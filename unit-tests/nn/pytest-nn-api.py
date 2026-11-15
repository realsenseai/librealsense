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
