// License: Apache 2.0. See LICENSE file in root directory.
// Author: Christopher N. Hesse <raymanfx(at)gmail.com>

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <opencv2/opencv.hpp>   // Include OpenCV API
#include <opencv2/rgbd.hpp>

using namespace cv;

int main(int argc, char * argv[]) try
{
    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe;
    // Start streaming with default recommended configuration
    pipe.start();

    auto color_profile = pipe.get_active_profile().get_stream(RS2_STREAM_COLOR).as<rs2::video_stream_profile>();
    auto depth_profile = pipe.get_active_profile().get_stream(RS2_STREAM_DEPTH).as<rs2::video_stream_profile>();
    auto depth2color_extrin = depth_profile.get_extrinsics_to(color_profile);

    // camera matrix layout:
    // fx  0  cx
    // 0   fy cy
    // 0   0  1

    // rs-enumerate-devices -c
    // color intrinsics
    cv::Mat color_intrinsics(3, 3, CV_32FC1);
    color_intrinsics.at<float>(0, 0) = color_profile.get_intrinsics().fx;
    color_intrinsics.at<float>(0, 1) = 0;
    color_intrinsics.at<float>(0, 2) = color_profile.get_intrinsics().ppx;
    color_intrinsics.at<float>(1, 0) = 0;
    color_intrinsics.at<float>(1, 1) = color_profile.get_intrinsics().fy;
    color_intrinsics.at<float>(1, 2) = color_profile.get_intrinsics().ppy;
    color_intrinsics.at<float>(2, 0) = 0;
    color_intrinsics.at<float>(2, 1) = 0;
    color_intrinsics.at<float>(2, 2) = 1;

    // rs-enumerate-devices -c
    // color intrinsics
    cv::Mat depth_intrinsics(3, 3, CV_32FC1);
    depth_intrinsics.at<float>(0, 0) = depth_profile.get_intrinsics().fx;
    depth_intrinsics.at<float>(0, 1) = 0;
    depth_intrinsics.at<float>(0, 2) = depth_profile.get_intrinsics().ppx;
    depth_intrinsics.at<float>(1, 0) = 0;
    depth_intrinsics.at<float>(1, 1) = depth_profile.get_intrinsics().fy;
    depth_intrinsics.at<float>(1, 2) = depth_profile.get_intrinsics().ppy;
    depth_intrinsics.at<float>(2, 0) = 0;
    depth_intrinsics.at<float>(2, 1) = 0;
    depth_intrinsics.at<float>(2, 2) = 1;

    // rs-enumerate-devices -c
    // extrinsics (depth to color)
    cv::Mat extrin_rot(3, 3, CV_32FC1);
    cv::Mat extrin_trans(1, 3, CV_32FC1);
    extrin_rot.at<float>(0, 0) = depth2color_extrin.rotation[0];
    extrin_rot.at<float>(0, 1) = depth2color_extrin.rotation[3];
    extrin_rot.at<float>(0, 2) = depth2color_extrin.rotation[6];
    extrin_rot.at<float>(1, 0) = depth2color_extrin.rotation[1];
    extrin_rot.at<float>(1, 1) = depth2color_extrin.rotation[4];
    extrin_rot.at<float>(1, 2) = depth2color_extrin.rotation[7];
    extrin_rot.at<float>(2, 0) = depth2color_extrin.rotation[2];
    extrin_rot.at<float>(2, 1) = depth2color_extrin.rotation[5];
    extrin_rot.at<float>(2, 2) = depth2color_extrin.rotation[8];
    extrin_trans.at<float>(0, 0) = depth2color_extrin.translation[0];
    extrin_trans.at<float>(0, 1) = depth2color_extrin.translation[1];
    extrin_trans.at<float>(0, 2) = depth2color_extrin.translation[2];

    // rigid body transform
    // https://en.wikipedia.org/wiki/Camera_resectioning#Extrinsic_parameters
    cv::Mat Rt(4, 4, CV_32FC1);
    Rt.at<float>(0, 0) = extrin_rot.at<float>(0, 0);
    Rt.at<float>(0, 1) = extrin_rot.at<float>(0, 1);
    Rt.at<float>(0, 2) = extrin_rot.at<float>(0, 2);
    Rt.at<float>(0, 3) = extrin_trans.at<float>(0, 0);
    Rt.at<float>(1, 0) = extrin_rot.at<float>(1, 0);
    Rt.at<float>(1, 1) = extrin_rot.at<float>(1, 1);
    Rt.at<float>(1, 2) = extrin_rot.at<float>(1, 2);
    Rt.at<float>(1, 3) = extrin_trans.at<float>(0, 1);
    Rt.at<float>(2, 0) = extrin_rot.at<float>(2, 0);
    Rt.at<float>(2, 1) = extrin_rot.at<float>(2, 1);
    Rt.at<float>(2, 2) = extrin_rot.at<float>(2, 2);
    Rt.at<float>(2, 3) = extrin_trans.at<float>(0, 2);
    Rt.at<float>(3, 0) = 0;
    Rt.at<float>(3, 1) = 0;
    Rt.at<float>(3, 2) = 0;
    Rt.at<float>(3, 3) = 1;

    const auto c_window_name = "Color Image";
    const auto d_window_name = "Depth Image";
    const auto r_window_name = "Depth Image (registered)";
    namedWindow(c_window_name, WINDOW_AUTOSIZE);
    namedWindow(d_window_name, WINDOW_AUTOSIZE);
    namedWindow(r_window_name, WINDOW_AUTOSIZE);

    while (waitKey(1) < 0 && getWindowProperty(c_window_name, WND_PROP_AUTOSIZE) >= 0
        && getWindowProperty(d_window_name, WND_PROP_AUTOSIZE) >= 0
        && getWindowProperty(r_window_name, WND_PROP_AUTOSIZE) >= 0)
    {
        rs2::frameset data = pipe.wait_for_frames(); // Wait for next set of frames from the camera
        rs2::frame color = data.get_color_frame();
        rs2::depth_frame depth = data.get_depth_frame();

        // Query frame size (width and height)
        const int c_w = color.as<rs2::video_frame>().get_width();
        const int c_h = color.as<rs2::video_frame>().get_height();
        const int d_w = depth.as<rs2::video_frame>().get_width();
        const int d_h = depth.as<rs2::video_frame>().get_height();

        // Create OpenCV matrices of size (w,h)
        Mat color_image(Size(c_w, c_h), CV_8UC3, (void*)color.get_data(), Mat::AUTO_STEP);
        Mat depth_image(Size(d_w, d_h), CV_32FC1);

        // librealsense captures RGB but OpenCV expects BGR
        cvtColor(color_image, color_image, COLOR_RGB2BGR);

        // OpenCV expects depth data to be in meters for 32 bit float types
        for (size_t i = 0; i < d_h; i++) {
            for (size_t j = 0; j < d_w; j++) {
                depth_image.at<float>(i, j) = depth.get_distance(j, i);
            }
        }

        Mat registered;
        rgbd::registerDepth(depth_intrinsics, color_intrinsics, cv::Mat(), Rt, depth_image,
                            color_image.size(), registered, true /* dilate */);

        // Update the window with new data
        imshow(c_window_name, color_image);
        imshow(d_window_name, depth_image);
        imshow(r_window_name, registered);
    }

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
