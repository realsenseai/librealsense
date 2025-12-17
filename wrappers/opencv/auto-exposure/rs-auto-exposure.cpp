
#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <opencv2/opencv.hpp>   // Include OpenCV API

// Check intersect in persentage
bool isIntersects(const cv::Rect &r1, const cv::Rect &r2, float threshold)
{
    if ( !((r1 & r2).area() > 0) )
        return false;

    cv::Point tl, br;
    tl.x = std::max(r1.tl().x, r2.tl().x);
    tl.y = std::max(r1.tl().y, r2.tl().y);
    br.x = std::min(r1.br().x, r2.br().x);
    br.y = std::min(r1.br().y, r2.br().y);

    cv::Rect inter(tl.x, tl.y, br.x - tl.x, br.y - tl.y );

    int areaR1 = r1.width * r1.height;
    int areaInter = inter.width * inter.height;

    float overlapping = (float)areaInter/areaR1;

    return ( overlapping > threshold ) ? true : false;
}

cv::Rect getApproximateInteriorRect (const cv::Mat & image)
{
    cv::Mat grayImage;
    cv::cvtColor(image, grayImage, cv::COLOR_BGR2GRAY);

    std::vector<std::vector<cv::Point> > contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(grayImage, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_TC89_KCOS,
                     cv::Point(-1, -1));

    std::vector<double> areas;

    for (int i=0; i < contours.size(); i++)
        areas.push_back(contourArea(contours.at(i)));

    int maxContour = distance(areas.begin(), max_element(areas.begin(), areas.end()));

    cv::Mat contour = cv::Mat::zeros(grayImage.size(), CV_8UC1);
    cv::drawContours(contour, contours, maxContour, cv::Scalar(255));

    std::vector<cv::Point> locations;
    cv::findNonZero(contour, locations);

    cv::Rect interiorROI = cv::boundingRect(locations);

    interiorROI.x += interiorROI.width*1/3;
    interiorROI.width -= interiorROI.width*2/3;

    interiorROI.y += interiorROI.height*1/3;
    interiorROI.height -= interiorROI.height*2/3;

    return interiorROI;
}

int main(int argc, char * argv[]) try
{
    // Declare depth colorizer for pretty visualization of depth data
    rs2::colorizer color_map;

    // Aligning frames to color size
    rs2::align depthToColorAlignment(RS2_STREAM_COLOR);

    // Declare threshold filter for work with dots in range
    rs2::threshold_filter threshold;
    float threshold_min = 0.3;
    float threshold_max = 1.5;

    // Keep dots on the depth frame in range
    threshold.set_option(RS2_OPTION_MIN_DISTANCE, threshold_min);
    threshold.set_option(RS2_OPTION_MAX_DISTANCE, threshold_max);

    // set up color sensor for direct access
    rs2::context ctx;

    if ( ctx.query_devices().size() == 0 ) {
        std::cerr << "Camera doesn't connected" << std::endl;
        return 2;
    }

    rs2::device hwDevice = ctx.query_devices()[0];
    rs2::sensor colorSensor = hwDevice.query_sensors()[1];

    // enable auto exposure
    colorSensor.set_option( RS2_OPTION_ENABLE_AUTO_EXPOSURE, true);

    // Declare rectangle for store ROI for auto exposure
    cv::Rect cameraROI;

    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe;

    // Start streaming with default recommended configuration
    pipe.start();

    // skip first 30 fames for auto exposure
    for (int i=0; i<30; i++)
        pipe.wait_for_frames();

    while ( true )
    {
        // get set of frames
        rs2::frameset frames = pipe.wait_for_frames(); // Wait for next set of frames from the camera

        // align images
        frames = depthToColorAlignment.process(frames);

        // get color frame
        rs2::frame rs2Color = frames.get_color_frame();

        // get depth frame
        rs2::frame rs2Depth = frames.get_depth_frame();

        if ( !rs2Color || !rs2Depth )
            continue;

        // keep points in range from threshold_min to threshold_max
        rs2Depth = threshold.process(rs2Depth);

        // Query frame size (width and height)
        const int w = rs2Depth.as<rs2::video_frame>().get_width();
        const int h = rs2Depth.as<rs2::video_frame>().get_height();

        cv::Mat cvColor = cv::Mat( h, w, CV_8UC3, (void*)(rs2Color.get_data()) );
        cv::cvtColor(cvColor, cvColor, cv::COLOR_BGR2RGB);

        // Create OpenCV matrix of size (w,h) from the colorized depth data
        cv::Mat cvDepth(cv::Size(w, h), CV_8UC3, (void*)rs2Depth.apply_filter(color_map).get_data());

        // get current interior rectangle
        cv::Rect rect = getApproximateInteriorRect(cvDepth);

        // update auto-exposure ROI on the camera
        if (!isIntersects(rect, cameraROI, 0.7) ) {
            cameraROI = rect;

            rs2::region_of_interest roi{};
            roi.min_x = cameraROI.x;
            roi.max_x = cameraROI.x + cameraROI.width;
            roi.min_y = cameraROI.y;
            roi.max_y = cameraROI.y + cameraROI.height;

            if ( colorSensor.is<rs2::roi_sensor>() )
                colorSensor.as<rs2::roi_sensor>().set_region_of_interest(roi);
        }

        cv::rectangle(cvColor, cameraROI, {255,255,0}, 4 );
        cv::rectangle(cvDepth, cameraROI, {255,255,0}, 4 );

        // Rescale image for convenience
        cv::resize( cvColor, cvColor, cv::Size(), 0.5, 0.5);
        cv::resize( cvDepth, cvDepth, cv::Size(), 0.5, 0.5);

        cv::Mat showImage = cv::Mat::zeros(cvColor.size().height*2, cvColor.size().width, CV_8UC3 );

        cvDepth.copyTo(showImage(cv::Rect(0,0,cvColor.size().width, cvColor.size().height)));
        cvColor.copyTo(showImage(cv::Rect(0,cvColor.size().height,cvColor.size().width, cvColor.size().height)));

        // Update the window with new data
        cv::imshow("image", showImage);
        int k = cv::waitKey(1);

        if ( k == 'q' )
            break;
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
