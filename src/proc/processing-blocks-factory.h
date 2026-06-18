// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/core/stream-profile.h>
#include <src/core/stream-profile-interface.h>

#include <vector>

#include "../types.h"

#include "identity-processing-block.h"

namespace librealsense
{

    class processing_block_factory
    {
    public:
        processing_block_factory() {}

        processing_block_factory(const std::vector<stream_profile>& from,
            const std::vector<stream_profile>& to,
            std::function<std::shared_ptr<processing_block>(void)> generate_func);

        bool operator==(const processing_block_factory& rhs) const;

        const std::vector<stream_profile> & get_source_info() const { return _source_info; }
        const std::vector<stream_profile> & get_target_info() const { return _target_info; }
        std::shared_ptr<processing_block> generate();
        
        static processing_block_factory create_id_pbf(rs2_format format, rs2_stream stream, int idx = 0);
        template<typename T, typename Fn>
        static std::vector<processing_block_factory> create_pbf_vector( rs2_format src, const std::vector<rs2_format>& dst, rs2_stream stream, Fn creator )
        {
            std::vector<processing_block_factory> rgb_factories;
            for( auto d : dst )
            {
                // register identity processing block if requested
                if( src == d )
                {
                    rgb_factories.push_back( { { {src} }, { {src, stream} }, [=]() { return creator( std::make_shared<identity_processing_block>() ); } } );
                    continue;
                }

                rgb_factories.push_back( { { {src} }, { {d, stream} }, [=]() { return creator( std::make_shared<T>( d )); } } );
            }

            return rgb_factories;
        }
        template<typename T>
        static std::vector<processing_block_factory> create_pbf_vector( rs2_format src, const std::vector<rs2_format>& dst, rs2_stream stream )
        {
            return create_pbf_vector< T >( src, dst, stream,
                []( std::shared_ptr< generic_processing_block > pb )
                {
                    return pb;
                } );
        }

        // Overload taking explicit target stream_profiles. Each target carries its own stream and
        // index (and is future-ready for more profile fields), for converters whose output is not on
        // the default stream index. Kept as a single 2-arg form so a braced rs2_format list still
        // resolves unambiguously to the vector<rs2_format> overloads above.
        template<typename T>
        static std::vector<processing_block_factory> create_pbf_vector( rs2_format src, const std::vector<stream_profile>& dst )
        {
            std::vector<processing_block_factory> rgb_factories;
            for( auto const & target : dst )
            {
                // register identity processing block if requested
                if( src == target.format )
                {
                    rgb_factories.push_back( { { {src} }, { target }, []() { return std::make_shared<identity_processing_block>(); } } );
                    continue;
                }

                rgb_factories.push_back( { { {src} }, { target }, [=]() { return std::make_shared<T>( target.format ); } } );
            }

            return rgb_factories;
        }

        stream_profiles find_satisfied_requests(const stream_profiles& sp, const stream_profiles& supported_profiles) const;
        bool has_source(const std::shared_ptr<stream_profile_interface>& source) const;

    protected:
        std::vector<stream_profile> _source_info;
        std::vector<stream_profile> _target_info;
        std::function<std::shared_ptr<processing_block>(void)> generate_processing_block;
    };
}
