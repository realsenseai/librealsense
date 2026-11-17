// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "rscore-pp-block-factory.h"

#include "proc/decimation-filter.h"
#include "proc/rotation-filter.h"
#include "proc/disparity-transform.h"
#include "proc/hdr-merge.h"
#include "proc/hole-filling-filter.h"
#include "proc/sequence-id-filter.h"
#include "proc/spatial-filter.h"
#include "proc/temporal-filter.h"
#include "proc/threshold.h"
#include "proc/dual-rgb-rectify-filter.h"

#include <rsutils/string/nocase.h>
#include <rsutils/json.h>


namespace librealsense {


std::shared_ptr< processing_block_interface >
rscore_pp_block_factory::create_pp_block( std::string const & name, rsutils::json const & settings )
{
    // These filters do not accept settings (nor are settings recorded in ros_writer)
    (void *)&settings;

    if( rsutils::string::nocase_equal( name, "Decimation Filter" ) )
        return std::make_shared< decimation_filter >();
    if( rsutils::string::nocase_equal( name, "Rotation Filter" ) )
        return std::make_shared< rotation_filter >();
    if( rsutils::string::nocase_equal( name, "HDR Merge" ) )  // and Hdr Merge
        return std::make_shared< hdr_merge >();
    if( rsutils::string::nocase_equal( name, "Filter By Sequence id" )    // name
        || rsutils::string::nocase_equal( name, "Sequence Id Filter" ) )  // extension
        return std::make_shared< sequence_id_filter >();
    if( rsutils::string::nocase_equal( name, "Threshold Filter" ) )
        return std::make_shared< threshold >();
    if( rsutils::string::nocase_equal( name, "Depth to Disparity" )     // name
        || rsutils::string::nocase_equal( name, "Disparity Filter" ) )  // extension
        return std::make_shared< disparity_transform >( true );
    if( rsutils::string::nocase_equal( name, "Disparity to Depth" ) )
        return std::make_shared< disparity_transform >( false );
    if( rsutils::string::nocase_equal( name, "Spatial Filter" ) )
        return std::make_shared< spatial_filter >();
    if( rsutils::string::nocase_equal( name, "Temporal Filter" ) )
        return std::make_shared< temporal_filter >();
    if( rsutils::string::nocase_equal( name, "Hole Filling Filter" ) )
        return std::make_shared< hole_filling_filter >();
    if( rsutils::string::nocase_equal( name, "RGB Rectification" ) )  // D401/D405 dual-RGB
        return std::make_shared< dual_rgb_rectify_filter >();

    return {};
}


}  // namespace librealsense
