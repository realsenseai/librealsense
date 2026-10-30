/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/hpp/rs_nn.hpp>

static void nn_upload( const rs2::nn_loader & self, const std::vector< uint8_t > & file, rs2_nn_target target, int slot,
                       bool activate, std::function< void( float ) > callback )
{
    if( callback )
        self.nn_upload( file, target, slot, activate, std::move( callback ) );
    else
        self.nn_upload( file, target, slot, activate );
}

void init_nn( py::module & m )
{
    /** rs_nn.hpp **/
    BIND_ENUM( m, rs2_nn_target, RS2_NN_TARGET_COUNT, "Where an uploaded NN model is stored." )

    py::class_< rs2::nn_loader, rs2::device, py_holder< rs2::nn_loader > > nn_loader( m, "nn_loader",
        "Device that accepts NN model packages from the Perception Service" );
    nn_loader.def( py::init< rs2::device >(), "device"_a )
        .def( "nn_upload",
              []( const rs2::nn_loader & self, py::buffer data, rs2_nn_target target, int slot, bool activate,
                  std::function< void( float ) > callback )
              {
                  py::buffer_info info = data.request();
                  if( info.strides.size() != 1 || info.strides[0] != info.itemsize )
                      throw std::runtime_error( "data must be a contiguous bytes-like object" );
                  auto begin = static_cast< const uint8_t * >( info.ptr );
                  std::vector< uint8_t > file( begin, begin + info.size * info.itemsize );
                  py::gil_scoped_release release;
                  nn_upload( self, file, target, slot, activate, std::move( callback ) );
              },
              "Upload an NN model package (bytes), sent to the device as is. Stop the Perception sensor first. When the "
              "install is done the SDK resets the camera: the device disconnects and must be opened again, as after a "
              "firmware update. Firmware update is refused while the device installs a model, uploaded by any "
              "process. If the upload does not complete, wait a minute before resetting or reconnecting the camera: it "
              "may still be writing the model. Currently only "
              "target=flash, slot=0 and activate=True are supported. The callback receives the progress in [0, 1].",
              "data"_a, "target"_a = RS2_NN_TARGET_FLASH, "slot"_a = 0, "activate"_a = true, "callback"_a = nullptr )
        .def( "nn_upload",
              []( const rs2::nn_loader & self, py::object path, rs2_nn_target target, int slot, bool activate,
                  std::function< void( float ) > callback )
              {
                  // Python opens the file: any os.PathLike, and non-ASCII paths on Windows
                  std::string data
                      = py::module::import( "pathlib" ).attr( "Path" )( path ).attr( "read_bytes" )().cast< py::bytes >();
                  std::vector< uint8_t > file( data.begin(), data.end() );
                  py::gil_scoped_release release;
                  nn_upload( self, file, target, slot, activate, std::move( callback ) );
              },
              "Upload an NN model package from a file path (str or os.PathLike); see the bytes overload.",
              "path"_a, "target"_a = RS2_NN_TARGET_FLASH, "slot"_a = 0, "activate"_a = true, "callback"_a = nullptr )
        .def( "is_upload_supported",
              []( const rs2::nn_loader & self )
              {
                  std::string reason;
                  bool supported = self.is_upload_supported( &reason );
                  return std::make_tuple( supported, reason );
              },
              "Whether the device supports NN model upload (product, firmware, not locked). Returns (supported, reason).",
              py::call_guard< py::gil_scoped_release >() );
}
