// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-24 RealSense, Inc. All Rights Reserved.
#pragma once

#include "core/stream-interface.h"
#include "core/stream-profile-interface.h"
#include "core/video.h"
#include "core/motion.h"
#include "core/inference.h"
#include "core/stream-profile.h"
#include "core/tagged-profile.h"
#include "librealsense-exception.h"
#include "environment.h"


struct rs2_stream_profile
{
    librealsense::stream_profile_interface * profile;
    std::shared_ptr< librealsense::stream_profile_interface > clone;
};


namespace librealsense
{
    class stream : public stream_interface
    {
    public:
        stream(rs2_stream stream_type, int index = 0);

        int get_stream_index() const override;
        void set_stream_index(int index) override;

        rs2_stream get_stream_type() const override;
        void set_stream_type(rs2_stream stream) override;

        int get_unique_id() const override { return _uid; }
        void set_unique_id(int uid) override { _uid = uid; };

    private:
        int _index = 0;
        int _uid = 0;
        rs2_stream _type = RS2_STREAM_ANY;
    };

    namespace platform {
        struct stream_profile;
    }

    class backend_stream_profile
    {
    public:
        virtual platform::stream_profile const & get_backend_profile() const {
            throw not_implemented_exception( "not a backend profile!" );
        }

        virtual void to_stream( std::ostream & ) const {}

        virtual ~backend_stream_profile() = default;
    };

    class stream_profile_base : public virtual stream_profile_interface, public backend_stream_profile
    {
    public:
        stream_profile_base();

        int get_stream_index() const override;
        void set_stream_index(int index) override;

        rs2_stream get_stream_type() const override;
        void set_stream_type(rs2_stream stream) override;

        rs2_format get_format() const override;
        void set_format(rs2_format format) override;

        uint32_t get_framerate() const override;
        void set_framerate(uint32_t val) override;

        int get_tag() const override;
        void tag_profile(int tag) override;

        int get_unique_id() const override { return _uid; }
        void set_unique_id(int uid) override
        {
            _uid = uid;
        };

        const char * get_name() override { return _name.c_str(); }
        void set_name( const std::string & name ) override { _name = name; }

        rs2_stream_profile* get_c_wrapper() const override;

        void set_c_wrapper(rs2_stream_profile* wrapper) override;

        void create_snapshot(std::shared_ptr<stream_profile_interface>& snapshot) const override;
        void enable_recording(std::function<void(const stream_profile_interface&)> record_action) override;

    private:
        int _index = 1;
        int _uid = 0;
        rs2_stream _type = RS2_STREAM_ANY;
        rs2_format _format = RS2_FORMAT_ANY;
        uint32_t _framerate = 0;
        int _tag = profile_tag::PROFILE_TAG_ANY;
        rs2_stream_profile _c_wrapper;
        rs2_stream_profile* _c_ptr = nullptr;
        std::string _name;
    };

    class video_stream_profile : public virtual video_stream_profile_interface, public stream_profile_base, public extension_snapshot
    {
    public:
        explicit video_stream_profile()
            : _calc_intrinsics(
                  []() -> rs2_intrinsics
                  { throw not_implemented_exception( "No intrinsics are available for this stream profile!" ); } )
            , _width( 0 )
            , _height( 0 )
        {
        }

        rs2_intrinsics get_intrinsics() const override { return _calc_intrinsics(); }
        void set_intrinsics(std::function<rs2_intrinsics()> calc) override { _calc_intrinsics = calc; }

        uint32_t get_width() const override { return _width; }
        uint32_t get_height() const override { return _height; }
        void set_dims(uint32_t width, uint32_t height) override
        {
            _width = width;
            _height = height;
        }

        std::shared_ptr<stream_profile_interface> clone() const override
        {
            auto res = std::make_shared< video_stream_profile >();
            if( !res )
                throw librealsense::invalid_value_exception( "Stream profiles failed in clone." );
            auto id = environment::get_instance().generate_stream_id();

            res->set_unique_id( id );
            //LOG_DEBUG( "video_stream_profile::clone, id= " << id );
            res->set_dims(get_width(), get_height());
            std::function<rs2_intrinsics()> int_func = _calc_intrinsics;
            res->set_intrinsics([int_func]() { return int_func(); });
            res->set_framerate(get_framerate());
            environment::get_instance().get_extrinsics_graph().register_same_extrinsics(*res, *this);
            return res;
        }

        bool operator==(const video_stream_profile& other) const
        {
            return get_height() == other.get_height() &&
                get_width() == other.get_width() &&
                get_framerate() == other.get_framerate() &&
                get_stream_index() == other.get_stream_index() &&
                get_stream_type() == other.get_stream_type() &&
                get_unique_id() == other.get_unique_id() &&
                get_format() == other.get_format();
        }

        void update(std::shared_ptr<extension_snapshot> ext) override
        {
            return; //TODO: apply changes here
        }

    private:
        std::function<rs2_intrinsics()> _calc_intrinsics;
        uint32_t _width, _height;
    };


    class motion_stream_profile : public motion_stream_profile_interface, public stream_profile_base, public extension_snapshot
    {
    public:
        explicit motion_stream_profile()
            : _calc_intrinsics(
                []() -> rs2_motion_device_intrinsic
                { throw not_implemented_exception( "No intrinsics are available for this stream profile!" ); } )
        {
        }

        rs2_motion_device_intrinsic get_intrinsics() const override { return _calc_intrinsics(); }
        void set_intrinsics(std::function<rs2_motion_device_intrinsic()> calc) override { _calc_intrinsics = calc; }

        void update(std::shared_ptr<extension_snapshot> ext) override
        {
            return; //TODO: apply changes here
        }

        std::shared_ptr<stream_profile_interface> clone() const override
        {
            auto res = std::make_shared< motion_stream_profile >();
            res->set_unique_id(environment::get_instance().generate_stream_id());
            std::function<rs2_motion_device_intrinsic()> init_func = _calc_intrinsics;
            res->set_intrinsics([init_func]() { return init_func(); });
            res->set_framerate(get_framerate());
            environment::get_instance().get_extrinsics_graph().register_same_extrinsics(*res, *this);
            return res;
        }

    private:
        std::function<rs2_motion_device_intrinsic()> _calc_intrinsics;
    };

    class pose_stream_profile : public pose_stream_profile_interface, public stream_profile_base, public extension_snapshot
    {
    public:
        explicit pose_stream_profile()
        {
        }

        void update(std::shared_ptr<extension_snapshot> ext) override { /*Nothing to do here*/ }

        std::shared_ptr< stream_profile_interface > clone() const override
        {
            auto res = std::make_shared< pose_stream_profile >();
            res->set_unique_id( environment::get_instance().generate_stream_id() );
            res->set_framerate( get_framerate() );
            return res;
        }

    };

    class inference_stream_profile : public inference_stream_profile_interface, public stream_profile_base, public extension_snapshot
    {
    public:
        explicit inference_stream_profile()
        {
        }

        void update( std::shared_ptr< extension_snapshot > ext ) override { /*Nothing to do here*/ }

        std::shared_ptr< stream_profile_interface > clone() const override
        {
            auto res = std::make_shared< inference_stream_profile >();
            res->set_unique_id( environment::get_instance().generate_stream_id() );
            res->set_framerate( get_framerate() );
            return res;
        }
    };

    inline stream_profile to_profile(const stream_profile_interface* sp)
    {
        auto fps = static_cast<uint32_t>(sp->get_framerate());
        if (auto vid = dynamic_cast<const video_stream_profile*>(sp))
        {
            return { sp->get_format(), sp->get_stream_type(), sp->get_stream_index(), vid->get_width(), vid->get_height(), fps };
        }
        
        return { sp->get_format(), sp->get_stream_type(), sp->get_stream_index(), 0, 0, fps };
    }

    inline std::vector<stream_profile> to_profiles(const std::vector<std::shared_ptr<stream_profile_interface>>& vec)
    {
        std::vector<stream_profile> res;
        for (auto&& p : vec) res.push_back(to_profile(p.get()));
        return res;
    }
}

namespace std
{
    template<>
    struct hash<std::shared_ptr<librealsense::video_stream_profile>>
    {
        size_t operator()(const std::shared_ptr<librealsense::video_stream_profile>& k) const
        {
            using std::hash;

            return (hash<uint32_t>()(k->get_height()))
                ^ (hash<uint32_t>()(k->get_width()))
                ^ (hash<uint32_t>()(k->get_framerate()))
                ^ (hash<uint32_t>()(k->get_stream_index()))
                ^ (hash<uint32_t>()(k->get_stream_type()))
                ^ (hash<uint32_t>()(k->get_unique_id()))
                ^ (hash<uint32_t>()(k->get_format()));
        }
    };

    inline bool operator==(const std::shared_ptr<librealsense::video_stream_profile>& a, const std::shared_ptr<librealsense::video_stream_profile>& b)
    {
        if (!a.get() || !b.get()) return a.get() == b.get();
        return *a == *b;
    }
}
