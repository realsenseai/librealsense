/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/hpp/rs_processing.hpp>
#include <memory>

void init_processing(py::module &m) {
    /** rs_processing.hpp **/
    py::class_<rs2::frame_source> frame_source(m, "frame_source", "The source used to generate frames, which is usually done by the low level driver for each sensor. "
                                               "frame_source is one of the parameters of processing_block's callback function, which can be used to re-generate the "
                                               "frame and via frame_ready invoke another callback function to notify application frame is ready.");
    frame_source.def("allocate_video_frame", &rs2::frame_source::allocate_video_frame, "Allocate a new video frame with given params",
                     "profile"_a, "original"_a, "new_bpp"_a = 0, "new_width"_a = 0,
                     "new_height"_a = 0, "new_stride"_a = 0, "frame_type"_a = RS2_EXTENSION_VIDEO_FRAME)
        .def("allocate_motion_frame", &rs2::frame_source::allocate_motion_frame, "Allocate a new motion frame with given params",
            "profile"_a, "original"_a, "frame_type"_a = RS2_EXTENSION_MOTION_FRAME)
        .def("allocate_points", &rs2::frame_source::allocate_points, "profile"_a,
             "original"_a) // No docstring in C++
        .def("allocate_composite_frame", &rs2::frame_source::allocate_composite_frame,
             "Allocate composite frame with given params", "frames"_a)
        .def("frame_ready", &rs2::frame_source::frame_ready, "Invoke the "
             "callback funtion informing the frame is ready.", "result"_a);

    // Not binding frame_processor_callback, templated

    py::class_<rs2::frame_queue> frame_queue(m, "frame_queue", "Frame queues are the simplest cross-platform "
                                             "synchronization primitive provided by librealsense to help "
                                             "developers who are not using async APIs.");
    frame_queue.def(py::init<>())
        .def(py::init<unsigned int, bool>(), "capacity"_a, "keep_frames"_a = false)
        .def("enqueue", &rs2::frame_queue::enqueue, "Enqueue a new frame into the queue.", "f"_a)
        .def("wait_for_frame", &rs2::frame_queue::wait_for_frame, "Wait until a new frame "
             "becomes available in the queue and dequeue it.", "timeout_ms"_a = 5000, py::call_guard<py::gil_scoped_release>())
        .def("poll_for_frame", [](const rs2::frame_queue &self) {
            rs2::frame frame;
            self.poll_for_frame(&frame);
            return frame;
        }, "Poll if a new frame is available and dequeue it if it is")
        .def("try_wait_for_frame", [](const rs2::frame_queue &self, unsigned int timeout_ms) {
            rs2::frame frame;
            auto success = self.try_wait_for_frame(&frame, timeout_ms);
            return std::make_tuple(success, frame);
        }, "timeout_ms"_a = 5000, py::call_guard<py::gil_scoped_release>()) // No docstring in C++
        .def("__call__", &rs2::frame_queue::operator(), "Identical to calling enqueue.", "f"_a)
        .def("capacity", &rs2::frame_queue::capacity, "Return the capacity of the queue.")
        .def("size", &rs2::frame_queue::size, "Number of enqueued frames.")
        .def("keep_frames", &rs2::frame_queue::keep_frames, "Return whether or not the queue calls keep on enqueued frames.");

    py::class_<rs2::processing_block, rs2::options> processing_block(m, "processing_block", "Define the processing block workflow, inherit this class to "
                                                                     "generate your own processing_block.");
    processing_block.def(py::init([](std::function<void(rs2::frame, rs2::frame_source*)> processing_function) {
        return new rs2::processing_block([=](rs2::frame f, rs2::frame_source& fs) {processing_function(f, &fs); });
        }), "processing_function"_a)
        .def("start", [](rs2::processing_block& self, std::function<void(rs2::frame)> f) {
            self.start(f);
        }, "Start the processing block with callback function to inform the application the frame is processed.", "callback"_a)
        .def("invoke", &rs2::processing_block::invoke, "Ask processing block to process the frame", "f"_a)
        .def("supports", (bool (rs2::processing_block::*)(rs2_camera_info) const) &rs2::processing_block::supports, "Check if a specific camera info field is supported.")
        .def("get_info", &rs2::processing_block::get_info, "Retrieve camera specific information, like versions of various internal components.");
        /*.def("__call__", &rs2::processing_block::operator(), "f"_a)*/
        // supports(camera_info) / get_info(camera_info)?

    py::class_<rs2::filter, rs2::processing_block, rs2::filter_interface> filter(m, "filter", "Define the filter workflow, inherit this class to generate your own filter.");
    filter.def(py::init([](std::function<void(rs2::frame, rs2::frame_source&)> filter_function, int queue_size) {
            return new rs2::filter(filter_function, queue_size);
        }), "filter_function"_a, "queue_size"_a = 1)
        .def(BIND_DOWNCAST(filter, decimation_filter))
        .def( BIND_DOWNCAST( filter, rotation_filter ) )
        .def(BIND_DOWNCAST(filter, disparity_transform))
        .def(BIND_DOWNCAST(filter, hole_filling_filter))
        .def(BIND_DOWNCAST(filter, spatial_filter))
        .def(BIND_DOWNCAST(filter, temporal_filter))
        .def(BIND_DOWNCAST(filter, threshold_filter))
        .def(BIND_DOWNCAST(filter, hdr_merge))
        .def(BIND_DOWNCAST(filter, sequence_id_filter))
        .def("__nonzero__", &rs2::filter::operator bool) // Called to implement truth value testing in Python 2
        .def("__bool__", &rs2::filter::operator bool);   // Called to implement truth value testing in Python 3
        // get_queue?
        // is/as?

    py::class_<rs2::pointcloud, rs2::filter> pointcloud(m, "pointcloud", "Generates 3D point clouds based on a depth frame. Can also map textures from a color frame.");
    pointcloud.def(py::init<>())
        .def(py::init<rs2_stream, int>(), "stream"_a, "index"_a = 0)
        .def("calculate", &rs2::pointcloud::calculate, "Generate the pointcloud and texture mappings of depth map.", "depth"_a)
        .def("map_to", &rs2::pointcloud::map_to, "Map the point cloud to the given color frame.", "mapped"_a);

    py::class_<rs2::yuy_decoder, rs2::filter> yuy_decoder(m, "yuy_decoder", "Converts frames in raw YUY format to RGB. This conversion is somewhat costly, "
                                                          "but the SDK will automatically try to use SSE2, AVX, or CUDA instructions where available to "
                                                          "get better performance. Othere implementations (GLSL, OpenCL, Neon, NCS) should follow.");
    yuy_decoder.def(py::init<>());

    py::class_<rs2::threshold_filter, rs2::filter> threshold(m, "threshold_filter", "Depth thresholding filter. By controlling min and "
                                                             "max options on the block, one could filter out depth values that are either too large "
                                                             "or too small, as a software post-processing step");
    threshold.def(py::init<float, float>(), "min_dist"_a = 0.15f, "max_dist"_a = 4.f);

    py::class_<rs2::units_transform, rs2::filter> units_transform(m, "units_transform");
    units_transform.def(py::init<>());

    // rs2::asynchronous_syncer

    py::class_<rs2::syncer> syncer(m, "syncer", "Sync instance to align frames from different streams");
    auto poll_for_frame = []( const rs2::syncer & self ) {
        rs2::frameset frames;
        self.poll_for_frames( &frames );
        return frames;
    };
    auto wait_for_frame = []( const rs2::syncer& self, unsigned int timeout_ms ) {
        rs2::frameset fs;
        auto success = self.try_wait_for_frames( &fs, timeout_ms );
        return std::make_tuple( success, fs );
    };
    syncer.def( py::init< int >(), "queue_size"_a = 1 )
        .def( "wait_for_frames",
              &rs2::syncer::wait_for_frames,
              "Wait until a coherent set of frames becomes available",
              "timeout_ms"_a = 5000,
              py::call_guard< py::gil_scoped_release >() )
        .def( "wait_for_frame",  // same, but with a name that matches frame_queue!
              &rs2::syncer::wait_for_frames,
              "Wait until a coherent set of frames becomes available",
              "timeout_ms"_a = 5000,
              py::call_guard< py::gil_scoped_release >() )
        .def( "poll_for_frames", poll_for_frame, "Check if a coherent set of frames is available" )
        .def( "poll_for_frame",  poll_for_frame, "Check if a coherent set of frames is available" )  // same, but with a name that matches frame_queue!
        .def( "try_wait_for_frames",
              wait_for_frame,
              "timeout_ms"_a = 5000,
              py::call_guard< py::gil_scoped_release >() )
        .def( "try_wait_for_frame",  // same, but with a name that matches frame_queue!
              wait_for_frame,
              "timeout_ms"_a = 5000,
              py::call_guard< py::gil_scoped_release >() );
      /*.def("__call__", &rs2::syncer::operator(), "frame"_a)*/

    py::class_<rs2::align, rs2::filter> align(m, "align", "Performs alignment between depth image and another image.");
    align.def(py::init<rs2_stream>(), "To perform alignment of a depth image to the other, set the align_to parameter with the other stream type.\n"
              "To perform alignment of a non depth image to a depth image, set the align_to parameter to RS2_STREAM_DEPTH.\n"
              "Camera calibration and frame's stream type are determined on the fly, according to the first valid frameset passed to process().", "align_to"_a)
        .def("process", (rs2::frameset(rs2::align::*)(rs2::frameset)) &rs2::align::process, "Run thealignment process on the given frames to get an aligned set of frames", "frames"_a);

    py::class_<rs2::colorizer, rs2::filter> colorizer(m, "colorizer", "Colorizer filter generates color images based on input depth frame");
    colorizer.def(py::init<>())
        .def(py::init<float>(), "Possible values for color_scheme:\n"
             "0 - Jet\n"
             "1 - Classic\n"
             "2 - WhiteToBlack\n"
             "3 - BlackToWhite\n"
             "4 - Bio\n"
             "5 - Cold\n"
             "6 - Warm\n"
             "7 - Quantized\n"
             "8 - Pattern", "color_scheme"_a)
        .def("colorize", &rs2::colorizer::colorize, "Start to generate color image base on depth frame", "depth"_a)
        /*.def("__call__", &rs2::colorizer::operator())*/;

    py::class_<rs2::decimation_filter, rs2::filter> decimation_filter(m, "decimation_filter", "Performs downsampling by using the median with specific kernel size.");
    decimation_filter.def(py::init<>())
        .def(py::init<float>(), "magnitude"_a);

    py::class_< rs2::rotation_filter, rs2::filter > rotation_filter(m, "rotation_filter","Performs rotation of frames." );
    rotation_filter.def( py::init<>() ).def( py::init< std::vector< rs2_stream > >(), "value"_a );

    py::class_<rs2::temporal_filter, rs2::filter> temporal_filter(m, "temporal_filter", "Temporal filter smooths the image by calculating multiple frames "
                                                                  "with alpha and delta settings. Alpha defines the weight of current frame, and delta defines the"
                                                                  "threshold for edge classification and preserving.");
    temporal_filter.def(py::init<>())
        .def(py::init<float, float, int>(), "Possible values for persistence_control:\n"
             "1 - Valid in 8/8 - Persistency activated if the pixel was valid in 8 out of the last 8 frames\n"
             "2 - Valid in 2 / last 3 - Activated if the pixel was valid in two out of the last 3 frames\n"
             "3 - Valid in 2 / last 4 - Activated if the pixel was valid in two out of the last 4 frames\n"
             "4 - Valid in 2 / 8 - Activated if the pixel was valid in two out of the last 8 frames\n"
             "5 - Valid in 1 / last 2 - Activated if the pixel was valid in one of the last two frames\n"
             "6 - Valid in 1 / last 5 - Activated if the pixel was valid in one out of the last 5 frames\n"
             "7 - Valid in 1 / last 8 - Activated if the pixel was valid in one out of the last 8 frames\n"
             "8 - Persist Indefinitely - Persistency will be imposed regardless of the stored history(most aggressive filtering)",
             "smooth_alpha"_a, "smooth_delta"_a, "persistence_control"_a);

    py::class_<rs2::spatial_filter, rs2::filter> spatial_filter(m, "spatial_filter", "Spatial filter smooths the image by calculating frame with alpha and delta settings. "
                                                                "Alpha defines the weight of the current pixel for smoothing, and is bounded within [25..100]%. Delta "
                                                                "defines the depth gradient below which the smoothing will occur as number of depth levels.");
    spatial_filter.def(py::init<>())
        .def(py::init<float, float, float, float>(), "smooth_alpha"_a, "smooth_delta"_a, "magnitude"_a, "hole_fill"_a);;

    py::class_<rs2::disparity_transform, rs2::filter> disparity_transform(m, "disparity_transform", "Converts from depth representation "
                                                                          "to disparity representation and vice - versa in depth frames");
    disparity_transform.def(py::init<bool>(), "transform_to_disparity"_a = true);

    py::class_<rs2::hole_filling_filter, rs2::filter> hole_filling_filter(m, "hole_filling_filter", "The processing performed depends on the selected hole filling mode.");
    hole_filling_filter.def(py::init<>())
        .def(py::init<int>(), "Possible values for mode:\n"
             "0 - fill_from_left - Use the value from the left neighbor pixel to fill the hole\n"
             "1 - farest_from_around - Use the value from the neighboring pixel which is furthest away from the sensor\n"
             "2 - nearest_from_around - -Use the value from the neighboring pixel closest to the sensor", "mode"_a);

    py::class_<rs2::hdr_merge, rs2::filter> hdr_merge(m, "hdr_merge", "Merges depth frames with different sequence ID");
    hdr_merge.def(py::init<>());

    py::class_<rs2::sequence_id_filter, rs2::filter> sequence_id_filter(m, "sequence_id_filter", "Splits depth frames with different sequence ID");
    sequence_id_filter.def(py::init<>())
        .def(py::init<float>(), "sequence_id"_a);
    // rs2::rates_printer

    py::class_<rs2::embedded_filter, rs2::options> embedded_filter(m, "embedded_filter", "Define the embedded filter workflow.");
    embedded_filter.def( BIND_DOWNCAST( embedded_filter, embedded_decimation_filter ) )
        .def( BIND_DOWNCAST( embedded_filter, embedded_temporal_filter ) )
        .def( "__bool__", &rs2::embedded_filter::operator bool )  // Called to implement truth value testing in Python 3
        .def( "get_type", &rs2::embedded_filter::get_type, "Get the embedded filter type" );

    py::class_<rs2::embedded_decimation_filter, rs2::embedded_filter> embedded_decimation_filter(m, "embedded_decimation_filter",
        "Define the embedded decimation filter workflow.");

    py::class_<rs2::embedded_temporal_filter, rs2::embedded_filter> embedded_temporal_filter(m, "embedded_temporal_filter",
        "Define the embedded temporal filter workflow.");

    /** end rs_processing.hpp **/
}
