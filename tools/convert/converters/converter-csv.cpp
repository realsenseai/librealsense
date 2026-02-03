// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.


#include "converter-csv.hpp"
#include <iomanip>
#include <algorithm>
#include <iostream>

using namespace rs2::tools::converter;

converter_csv::motion_pose_frame_record::motion_pose_frame_record(rs2_stream stream_type, int stream_index,
    unsigned long long frame_number, 
    long long frame_ts, long long backend_ts, long long arrival_time,
    double p1, double p2, double p3,
    double p4, double p5, double p6, double p7) :
    _stream_type(stream_type),
    _stream_idx(stream_index),
    _frame_number(frame_number),
    _frame_ts(frame_ts),
    _backend_ts(backend_ts),
    _arrival_time(arrival_time),
    _params({ p1, p2, p3, p4, p5, p6, p7 }){}

std::string converter_csv::motion_pose_frame_record::to_string() const
{
    std::stringstream ss;
    ss << rs2_stream_to_string(_stream_type) << "," << _frame_number << ","
        << std::fixed << std::setprecision(3) << _frame_ts << "," << _backend_ts << "," << _arrival_time;

    // IMU and Pose frame hold the sample data in addition to the frame's header attributes
    size_t specific_attributes = 0;
    if (val_in_range(_stream_type, { RS2_STREAM_GYRO,RS2_STREAM_ACCEL }))
        specific_attributes = 3;
    if (val_in_range(_stream_type, { RS2_STREAM_POSE }))
        specific_attributes = 7;

    for (auto i = 0; i < specific_attributes; i++)
        ss << "," << _params[i];
    ss << std::endl;
    return ss.str().c_str();
}

converter_csv::converter_csv(const std::string& filePath, rs2_stream streamType)
    : _filePath(filePath)
    , _streamType(streamType)
    , _imu_pose_collection()
{
}

void converter_csv::convert_depth(rs2::depth_frame& depthframe)
{
    if (frames_map_get_and_set(depthframe.get_profile().stream_type(), depthframe.get_profile().stream_index(), depthframe.get_frame_number())) {
        return;
    }

    start_worker(
        [this, depthframe] {

            std::stringstream filename;
            filename << _filePath
                << "_" << depthframe.get_profile().stream_name()
                << "_" << std::setprecision(14) << std::fixed << depthframe.get_timestamp()
                << ".csv";

            std::stringstream metadata_file;
            metadata_file << _filePath
                << "_" << depthframe.get_profile().stream_name()
                << "_metadata_" << std::setprecision(14) << std::fixed << depthframe.get_timestamp()
                << ".txt";

            std::string filenameS = filename.str();
            std::string metadataS = metadata_file.str();

            add_sub_worker(
                [filenameS, metadataS, depthframe] {
                    std::ofstream fs(filenameS, std::ios::trunc);

                    if (fs) {
                        for (int y = 0; y < depthframe.get_height(); y++) {
                            auto delim = "";

                            for (int x = 0; x < depthframe.get_width(); x++) {
                                fs << delim << depthframe.get_distance(x, y);
                                delim = ",";
                            }
                            fs << '\n';
                        }
                        fs.flush();
                    }
                    metadata_to_txtfile(depthframe, metadataS);
                });
            wait_sub_workers();
        });
}

std::string converter_csv::get_time_string() const
{
    auto t = time(nullptr);
    char buffer[20] = {};
    const tm* time = localtime(&t);
    if (nullptr != time)
        strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", time);

    return std::string(buffer);
}

void converter_csv::save_motion_pose_data_to_file()
{
    if (!_imu_pose_collection.size())
    {
        std::cout << "No imu or pose data collected" << std::endl;
        return;
    }

    // Report amount of frames collected
    std::vector<uint64_t> frames_per_stream;
    for (const auto& kv : _imu_pose_collection)
        frames_per_stream.emplace_back(kv.second.size());

    std::sort(frames_per_stream.begin(), frames_per_stream.end());

    // Serialize and store data into csv-like format
    static auto time_Str = get_time_string();
    std::stringstream filename;
    filename << _filePath  << "_" << time_Str <<"_imu_pose.csv";
    std::ofstream csv(filename.str());
    if (!csv.is_open())
        throw std::runtime_error(stringify() << "Cannot open the requested output file " << _filePath << ", please check permissions");

    for (const auto& elem : _imu_pose_collection)
    {
        csv << "\n\nStream Type,F#,HW Timestamp (ms),Backend Timestamp(ms),Host Timestamp(ms)"
            << (val_in_range(elem.first.first, { RS2_STREAM_GYRO,RS2_STREAM_ACCEL }) ? ",3DOF_x,3DOF_y,3DOF_z" : "")
            << (val_in_range(elem.first.first, { RS2_STREAM_POSE }) ? ",t_x,t_y,t_z,r_x,r_y,r_z,r_w" : "")
            << std::endl; 

        for (auto i = 0; i < elem.second.size(); i++)
            csv << elem.second[i].to_string();
    }

}

void converter_csv::convert_motion_pose(rs2::frame& f)
{
    if (frames_map_get_and_set(f.get_profile().stream_type(), f.get_profile().stream_index(), f.get_frame_number())) {
        return;
    }

	const auto stream_uid = std::make_pair(f.get_profile().stream_type(),
        f.get_profile().stream_index());

    long long frame_timestamp = 0LL;
    if (f.supports_frame_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP))
        frame_timestamp = f.get_frame_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP);

    long long backend_timestamp = 0LL;
    if (f.supports_frame_metadata(RS2_FRAME_METADATA_BACKEND_TIMESTAMP))
        backend_timestamp = f.get_frame_metadata(RS2_FRAME_METADATA_BACKEND_TIMESTAMP);

    long long time_of_arrival = 0LL;
    if (f.supports_frame_metadata(RS2_FRAME_METADATA_TIME_OF_ARRIVAL))
        time_of_arrival = f.get_frame_metadata(RS2_FRAME_METADATA_TIME_OF_ARRIVAL);
    
    motion_pose_frame_record record{ f.get_profile().stream_type(),
                                f.get_profile().stream_index(),
                                f.get_frame_number(),
                                frame_timestamp,
                                backend_timestamp,
                                time_of_arrival};

    if (auto motion = f.as<rs2::motion_frame>())
    {
        auto axes = motion.get_motion_data();
        record._params = { axes.x, axes.y, axes.z };
    }

    if (auto pf = f.as<rs2::pose_frame>())
    {
        auto pose = pf.get_pose_data();
        record._params = { pose.translation.x, pose.translation.y, pose.translation.z,
                pose.rotation.x,pose.rotation.y,pose.rotation.z,pose.rotation.w };
    }

    _imu_pose_collection[stream_uid].emplace_back(record);
}

void converter_csv::convert(rs2::frame& frame)
{
    if (!(_streamType == rs2_stream::RS2_STREAM_ANY || frame.get_profile().stream_type() == _streamType))
        return;

    auto depthframe = frame.as<rs2::depth_frame>();
    if (depthframe)
    {
        convert_depth(depthframe);
        return;
    }

    auto motionframe = frame.as<rs2::motion_frame>();
    auto poseframe = frame.as<rs2::pose_frame>();
    if (motionframe || poseframe)
    {
        convert_motion_pose(frame);
        return;
    }
}

void converter_csv::flush()
{
    save_motion_pose_data_to_file();
}
