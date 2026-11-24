# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs


def test_nn_api_is_exposed():
    assert rs.extension.nn_loader
    assert [str( t ) for t in ( rs.nn_target.flash, rs.nn_target.ddr )] == ['nn_target.flash', 'nn_target.ddr']
    for name in ( 'nn_upload', 'is_upload_supported' ):
        assert hasattr( rs.nn_loader, name )
    for name in ( 'is_nn_loader', 'as_nn_loader' ):
        assert hasattr( rs.device, name )


def test_custom_nn_api_is_exposed():
    assert rs.stream.custom_nn
    assert rs.extension.custom_nn_frame
    assert rs.nn_dtype.s16 and rs.nn_dtype.f32
    for name in ( 'source_frame_id', 'nn_model_generation', 'nn_status', 'nn_model_id' ):
        assert hasattr( rs.frame_metadata_value, name )
    for name in ( 'tensor_count', 'tensor', 'tensors', 'status', 'status_message', 'generation', 'model_id',
                  'source_frame_id' ):
        assert hasattr( rs.custom_nn_frame, name )
    for name in ( 'name', 'dtype', 'shape', 'layout', 'data', 'dequant_scale', 'as_float' ):
        assert hasattr( rs.nn_tensor, name )
    for name in ( 'is_custom_nn_frame', 'as_custom_nn_frame' ):
        assert hasattr( rs.frame, name )
    assert hasattr( rs.composite_frame, 'get_custom_nn_frame' )
