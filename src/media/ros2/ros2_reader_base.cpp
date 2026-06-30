// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "ros2_reader_base.h"

#include <zstd.h>

#include <src/context.h>
#include <src/sensor.h>
#include <src/stream.h>
#include <src/core/extension.h>


namespace librealsense
{
    ros2_reader_base::ros2_reader_base(const std::string& file, const std::shared_ptr<context> ctx)
        : m_metadata_parser_map(md_constant_parser::create_metadata_parser_map())
        , m_total_duration(0)
        , m_file_path(file)
        , m_context(ctx)
    {
        try
        {
            reset();
            m_total_duration    = get_file_duration();
            _first_timestamp_ns = _storage->get_metadata().starting_time.time_since_epoch().count();
        }
        catch (const std::exception& e)
        {
            throw io_exception(rsutils::string::from() << "Failed to create ros2 reader: " << e.what());
        }
    }

    device_snapshot ros2_reader_base::query_device_description(const nanoseconds& time)
    {
        return read_device_description(time);
    }

    nanoseconds ros2_reader_base::query_duration() const
    {
        return m_total_duration;
    }

    nanoseconds ros2_reader_base::get_file_duration()
    {
        auto meta = _storage->get_metadata();
        return nanoseconds(meta.duration.count());
    }

    std::shared_ptr<rosbag2_storage_plugins::SqliteStorage> ros2_reader_base::as_sqlite_storage()
    {
        auto as_sqlite = std::dynamic_pointer_cast<rosbag2_storage_plugins::SqliteStorage>(_storage);
        if (!as_sqlite)
            throw std::runtime_error("expected a SqliteStorage backend");
        return as_sqlite;
    }

    void ros2_reader_base::reset()
    {
        _storage = std::make_shared< rosbag2_storage_plugins::SqliteStorage >();
        _storage->open(m_file_path, rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY);
        m_frame_source = std::make_shared<frame_source>(32);
        m_frame_source->init(m_metadata_parser_map);
        _cache_valid = false;

        // Reapply streaming filter if it was previously set
        if (!_streaming_filter_topics.empty())
        {
            _storage->set_filter({ _streaming_filter_topics });
        }
    }

    void ros2_reader_base::enable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids)
    {
        for (const auto& id : stream_ids) _enabled_streams.insert(id);
    }

    void ros2_reader_base::disable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids)
    {
        for (const auto& id : stream_ids) _enabled_streams.erase(id);
    }

    std::vector<std::shared_ptr<serialized_data>> ros2_reader_base::fetch_last_frames(const nanoseconds& seek_time)
    {
        // Jump to a short window before seek_time, scan it raw (no decompress), remember the last
        // frame (+ following metadata message) per stream - only those get decoded. Handed out as
        // clones since the consumer moves the holder out.
        // Window assumes every active stream emits >= 1 frame/sec.
        static const nanoseconds lookback( std::chrono::seconds( 1 ) );
        const auto from = seek_time > lookback ? seek_time - lookback : nanoseconds( 0 );
        as_sqlite_storage()->seek( static_cast<rcutils_time_point_value_t>( from.count() + _first_timestamp_ns ) );
        _cached_message = nullptr;
        _cache_valid = false;

        using msg_ptr = std::shared_ptr<rosbag2_storage::SerializedBagMessage>;
        std::map<stream_identifier, std::pair<msg_ptr, msg_ptr>> last; // stream -> {frame, metadata}
        stream_identifier last_sid{};
        bool expect_metadata = false;
        while (_storage->has_next())
        {
            auto msg = _storage->read_next(); // raw: no decompress
            if (!msg)
                continue;
            if (expect_metadata)
            {
                expect_metadata = false;
                // metadata follows its frame; native streams have none, so verify the topic
                if (msg->topic_name.find("/metadata") != std::string::npos)
                {
                    last[last_sid].second = msg;
                    continue;
                }
            }
            if (nanoseconds(static_cast<int64_t>(msg->time_stamp) - _first_timestamp_ns) > seek_time)
                break;
            stream_identifier sid;
            if (is_stream_topic(msg->topic_name, sid)
                && (_enabled_streams.empty() || _enabled_streams.count(sid)))
            {
                last[sid] = { msg, nullptr };
                last_sid = sid;
                expect_metadata = true;
            }
        }

        std::vector<std::shared_ptr<serialized_data>> frames;
        try
        {
            for (auto& kv : last)
            {
                auto frame_msg = kv.second.first;
                auto meta_msg  = kv.second.second;
                if (!frame_msg)
                    continue;
                decompress_if_needed(frame_msg);
                if (meta_msg)
                    decompress_if_needed(meta_msg);
                _cached_message = meta_msg;            // primed for ros2_reader's read_frame_metadata
                _cache_valid = (meta_msg != nullptr);
                auto sf = create_frame(frame_msg);
                if (sf && sf->frame)
                    frames.push_back(std::make_shared<serialized_frame>(sf->get_timestamp(), sf->stream_id, sf->frame.clone()));
            }
        }
        catch (...)
        {
            _cached_message = nullptr; // don't leak primed metadata if create_frame threw
            _cache_valid = false;
            throw;
        }
        _cached_message = nullptr; // don't leak the last primed metadata
        _cache_valid = false;
        return frames;
    }

    const std::string& ros2_reader_base::get_file_name() const
    {
        return m_file_path;
    }

    bool ros2_reader_base::is_zstd_compressed(const uint8_t* src, size_t src_size)
    {
        return src_size >= 4 && src[0] == 0x28 && src[1] == 0xB5 && src[2] == 0x2F && src[3] == 0xFD;
    }

    void ros2_reader_base::decompress_if_needed(std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg)
    {
        if (!msg || !msg->serialized_data || !msg->serialized_data->buffer || msg->serialized_data->buffer_length == 0)
            return;

        auto src = msg->serialized_data->buffer;
        auto src_size = msg->serialized_data->buffer_length;

        if (!is_zstd_compressed(src, src_size))
            return;

        auto frame_content_size = ZSTD_getFrameContentSize(src, src_size);
        if (frame_content_size == ZSTD_CONTENTSIZE_UNKNOWN || frame_content_size == ZSTD_CONTENTSIZE_ERROR)
            throw std::runtime_error("Failed to determine decompressed size for zstd-compressed message");

        // Guard against malformed frames claiming an absurd decompressed size — zstd's frame
        // header is untrusted input and a malicious file could request a huge allocation.
        constexpr size_t MAX_DECOMPRESSED_SIZE = 256 * 1024 * 1024; // 256 MB
        if (frame_content_size > MAX_DECOMPRESSED_SIZE)
            throw std::runtime_error(rsutils::string::from()
                << "Zstd decompressed size " << frame_content_size << " exceeds safety limit");

        auto decompressed_size = static_cast<size_t>(frame_content_size);

        // We create a new buffer for the decompressed data each time. We could use
        // a reusable member buffer like the writer does, but here the frame data may
        // still be in use when the metadata is read next and overwrite it — allowing
        // it to reallocate for simplicity for now.
        auto out = create_buffer(decompressed_size);

        auto result = ZSTD_decompress(out->buffer, out->buffer_capacity, src, src_size);
        if (ZSTD_isError(result))
            throw std::runtime_error(rsutils::string::from() << "Zstd decompression failed: " << ZSTD_getErrorName(result));

        out->buffer_length = result;
        msg->serialized_data = std::move(out);
    }

    void ros2_reader_base::prepare_for_streaming()
    {
        // Reopen storage to reset the filter, and apply relevant filters for streaming
        _storage = std::make_shared< rosbag2_storage_plugins::SqliteStorage >();
        _storage->open(m_file_path, rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY);

        auto stream_topics       = get_stream_topics();
        auto option_topics       = get_option_topics();
        auto notification_topics = get_notification_topics();

        _streaming_filter_topics.clear();
        _streaming_filter_topics.insert(_streaming_filter_topics.end(), stream_topics.begin(), stream_topics.end());
        _streaming_filter_topics.insert(_streaming_filter_topics.end(), option_topics.begin(), option_topics.end());
        _streaming_filter_topics.insert(_streaming_filter_topics.end(), notification_topics.begin(), notification_topics.end());

        _storage->set_filter({ _streaming_filter_topics });
    }

    void ros2_reader_base::seek_to_time(const nanoseconds& seek_time)
    {
        if (seek_time > m_total_duration)
        {
            throw invalid_value_exception( rsutils::string::from()
                                           << "Requested time is out of playback length. (Requested = "
                                           << seek_time.count() << ", Duration = " << m_total_duration.count() << ")" );
        }

        // Position the cursor at seek_time (indexed); paused frames are handled by fetch_last_frames.
        as_sqlite_storage()->seek(static_cast<rcutils_time_point_value_t>(seek_time.count() + _first_timestamp_ns));
        _cached_message = nullptr;   // lookahead stale after the jump
        _cache_valid = false;
    }

    bool ros2_reader_base::has_next_cached() const
    {
        // If we have a valid cached message, we have next
        if (_cache_valid)
            return true;

        return _storage->has_next();
    }

    std::shared_ptr<rosbag2_storage::SerializedBagMessage> ros2_reader_base::read_next_cached()
    {
        // If cache is valid, return cached message and mark as consumed
        if (_cache_valid)
        {
            _cache_valid = false;
            return _cached_message;
        }

        // Otherwise, read from storage and return immediately (no caching)
        if (!_storage->has_next())
            return nullptr;

        auto msg = _storage->read_next();
        decompress_if_needed(msg);
        return msg;
    }

    std::shared_ptr<rosbag2_storage::SerializedBagMessage> ros2_reader_base::peek_next_cached()
    {
        // If cache is valid, return cached message without consuming
        if (_cache_valid)
            return _cached_message;

        // Otherwise, read from storage and cache it
        if (!_storage->has_next())
            return nullptr;

        _cached_message = _storage->read_next();
        decompress_if_needed(_cached_message);
        _cache_valid = true;
        return _cached_message;
    }

    frame_holder ros2_reader_base::alloc_and_move_frame(std::vector<uint8_t>&& data,
        const stream_identifier& stream_id,
        frame_additional_data additional_data) const
    {
        auto frame_ext = frame_source::stream_to_frame_types(stream_id.stream_type);
        frame_interface* frame = m_frame_source->alloc_frame(
            { stream_id.stream_type, stream_id.stream_index, frame_ext },
            data.size(),
            std::move(additional_data),
            true);

        if (frame == nullptr)
        {
            LOG_WARNING("Failed to allocate new frame");
            return frame_holder{};
        }

        // Move the deserialized data directly — avoids a full memcpy of image data
        auto base_frame = static_cast<librealsense::frame*>(frame);
        base_frame->data = std::move(data);

        setup_frame(frame, stream_id);

        return frame_holder{ frame };
    }

}
