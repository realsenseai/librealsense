
#include <opencv2/imgproc.hpp>
#include <opencv2/rgbd/kinfu.hpp>

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <example.hpp>         // Include short list of convenience functions for rendering

#include <thread>
#include <queue>
#include <atomic>
#include <fstream>

using namespace cv;
using namespace cv::kinfu;

static float max_dist = 2.5;
static float min_dist = 0;


// Assigns an RGB value for each point in the pointcloud, based on the depth value
void colorize_pointcloud(const Mat points, Mat& color)
{
    // Define a vector of 3 Mat arrays which will hold the channles of points
    std::vector<Mat> channels(points.channels());
    split(points, channels);
    // Get the depth channel which we'll use to colorize the pointcloud
    color = channels[2];

    // Convert the depth matrix to unsigned char values
    float min = min_dist;
    float max = max_dist;
    color.convertTo(color, CV_8UC1, 255 / (max - min), -255 * min / (max - min));
    // Get an rgb value for each point
    applyColorMap(color, color, COLORMAP_JET);
}

// Handles all the OpenGL calls needed to display the point cloud
void draw_kinfu_pointcloud(glfw_state& app_state, Mat points, Mat normals)
{
    // Define new matrix which will later hold the coloring of the pointcloud
    Mat color;
    colorize_pointcloud(points, color);

    // OpenGL commands that prep screen for the pointcloud
    glLoadIdentity();
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    glClearColor(153.f / 255, 153.f / 255, 153.f / 255, 1);
    glClear(GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    gluPerspective(65, 1.3, 0.01f, 10.0f);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    gluLookAt(0, 0, 0, 0, 0, 1, 0, -1, 0);

    glTranslatef(0, 0, 1 + app_state.offset_y*0.05f);
    glRotated(app_state.pitch-20, 1, 0, 0);
    glRotated(app_state.yaw+5, 0, 1, 0);
    glTranslatef(0, 0, -0.5f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glBegin(GL_POINTS);
    // this segment actually prints the pointcloud
    for (int i = 0; i < points.rows; i++)
    {
        // Get point coordinates from 'points' matrix
        float x = points.at<float>(i, 0);
        float y = points.at<float>(i, 1);
        float z = points.at<float>(i, 2);

        // Get point coordinates from 'normals' matrix
        float nx = normals.at<float>(i, 0);
        float ny = normals.at<float>(i, 1);
        float nz = normals.at<float>(i, 2);

        // Get the r, g, b values for the current point
        uchar r = color.at<uchar>(i, 0);
        uchar g = color.at<uchar>(i, 1);
        uchar b = color.at<uchar>(i, 2);

        // Register color and coordinates of the current point
        glColor3ub(r, g, b);
        glNormal3f(nx, ny, nz);
        glVertex3f(x, y, z);
    }
    // OpenGL cleanup
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}


void export_to_ply(Mat points, Mat normals)
{
    // First generate a filename
    const size_t buffer_size = 50;
    char fname[buffer_size];
    time_t t = time(0);   // get time now
    struct tm * now = localtime(&t);
    strftime(fname, buffer_size, "%m%d%y %H%M%S.ply", now);
    std::cout << "exporting to" << fname << std::endl;

    // Get rgb values for points
    Mat color;
    colorize_pointcloud(points, color);

    // Write the ply file
    std::ofstream out(fname);
    out << "ply\n";
    out << "format binary_little_endian 1.0\n";
    out << "comment pointcloud saved from Realsense Viewer\n";
    out << "element vertex " << points.rows << "\n";
    out << "property float" << sizeof(float) * 8 << " x\n";
    out << "property float" << sizeof(float) * 8 << " y\n";
    out << "property float" << sizeof(float) * 8 << " z\n";

    out << "property float" << sizeof(float) * 8 << " nx\n";
    out << "property float" << sizeof(float) * 8 << " ny\n";
    out << "property float" << sizeof(float) * 8 << " nz\n";

    out << "property uchar red\n";
    out << "property uchar green\n";
    out << "property uchar blue\n";
    out << "end_header\n";
    out.close();

    out.open(fname, std::ios_base::app | std::ios_base::binary);
    for (int i = 0; i < points.rows; i++)
    {
        // write vertices
        out.write(reinterpret_cast<const char*>(&(points.at<float>(i, 0))), sizeof(float));
        out.write(reinterpret_cast<const char*>(&(points.at<float>(i, 1))), sizeof(float));
        out.write(reinterpret_cast<const char*>(&(points.at<float>(i, 2))), sizeof(float));

        // write normals
        out.write(reinterpret_cast<const char*>(&(normals.at<float>(i, 0))), sizeof(float));
        out.write(reinterpret_cast<const char*>(&(normals.at<float>(i, 1))), sizeof(float));
        out.write(reinterpret_cast<const char*>(&(normals.at<float>(i, 2))), sizeof(float));

        // write colors
        out.write(reinterpret_cast<const char*>(&(color.at<uchar>(i, 0))), sizeof(uint8_t));
        out.write(reinterpret_cast<const char*>(&(color.at<uchar>(i, 1))), sizeof(uint8_t));
        out.write(reinterpret_cast<const char*>(&(color.at<uchar>(i, 2))), sizeof(uint8_t));
    }
}


// Thread-safe queue for OpenCV's Mat objects
class mat_queue
{
public:
    void push(Mat& item)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        queue.push(item);
    }
    int try_get_next_item(Mat& item)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (queue.empty())
            return false;
        item = std::move(queue.front());
        queue.pop();
        return true;
    }
private:
    std::queue<Mat> queue;
    std::mutex _mtx;
};


int main(int argc, char **argv)
{
    // Declare KinFu and params pointers
    Ptr<KinFu> kf;
    Ptr<Params> params = Params::defaultParams();

    // Create a pipeline and configure it
    rs2::pipeline p;
    rs2::config cfg;
    float depth_scale;
    cfg.enable_stream(RS2_STREAM_DEPTH, 1280, 720, RS2_FORMAT_Z16);
    auto profile = p.start(cfg);
    auto dev = profile.get_device();
    auto stream_depth = profile.get_stream(RS2_STREAM_DEPTH);

    // Get a new frame from the camera
    rs2::frameset data = p.wait_for_frames();
    auto d = data.get_depth_frame();
    
    for (rs2::sensor& sensor : dev.query_sensors())
    {
        if (rs2::depth_sensor dpt = sensor.as<rs2::depth_sensor>())
        {
            // Set some presets for better results
            dpt.set_option(RS2_OPTION_VISUAL_PRESET, RS2_RS400_VISUAL_PRESET_HIGH_DENSITY);
            // Depth scale is needed for the kinfu set-up
            depth_scale = dpt.get_depth_scale();
            break;
        }
    }

    // Declare post-processing filters for better results
    auto decimation = rs2::decimation_filter();
    auto spatial = rs2::spatial_filter();
    auto temporal = rs2::temporal_filter();

    auto clipping_dist = max_dist / depth_scale; // convert clipping_dist to raw depth units

    // Use decimation once to get the final size of the frame
    d = decimation.process(d);
    auto w = d.get_width();
    auto h = d.get_height();
    Size size = Size(w, h);

    auto intrin = stream_depth.as<rs2::video_stream_profile>().get_intrinsics();

    // Configure kinfu's parameters
    params->frameSize = size;
    params->intr = Matx33f(intrin.fx/2, 0, intrin.ppx/2,
                           0, intrin.fy/2, intrin.ppy/2,
                           0, 0, 1);
    params->depthFactor = 1 / depth_scale;

    // Initialize KinFu object
    kf = KinFu::create(params);

    bool after_reset = false;
    mat_queue points_queue;
    mat_queue normals_queue;

    window app(1280, 720, "RealSense KinectFusion Example");
    glfw_state app_state;
    register_glfw_callbacks(app, app_state);

    std::atomic_bool stopped(false);

    // This thread runs KinFu algorithm and calculates the pointcloud by fusing depth data from subsequent depth frames
    std::thread calc_cloud_thread([&]() {
        Mat _points;
        Mat _normals;
        try {
            while (!stopped)
            {
                rs2::frameset data = p.wait_for_frames(); // Wait for next set of frames from the camera

                auto d = data.get_depth_frame();
                // Use post processing to improve results
                d = decimation.process(d);
                d = spatial.process(d);
                d = temporal.process(d);

                // Set depth values higher than clipping_dist to 0, to avoid unnecessary noise in the pointcloud
                uint16_t* p_depth_frame = reinterpret_cast<uint16_t*>(const_cast<void*>(d.get_data()));
#pragma omp parallel for schedule(dynamic) //Using OpenMP to try to parallelise the loop
                for (int y = 0; y < h; y++)
                {
                    auto depth_pixel_index = y * w;
                    for (int x = 0; x < w; x++, ++depth_pixel_index)
                    {
                        // Check if the depth value of the current pixel is greater than the threshold
                        if (p_depth_frame[depth_pixel_index] > clipping_dist)
                        {
                            p_depth_frame[depth_pixel_index] = 0;
                        }
                    }
                }

                // Define matrices on the GPU for KinFu's use
                UMat points;
                UMat normals;
                // Copy frame from CPU to GPU
                Mat f(h, w, CV_16UC1, (void*)d.get_data());
                UMat frame(h, w, CV_16UC1);
                f.copyTo(frame);
                f.release();

                // Run KinFu on the new frame(on GPU)
                if (!kf->update(frame))
                {
                    kf->reset(); // If the algorithm failed, reset current state
                    // Save the pointcloud obtained before failure
                    export_to_ply(_points, _normals);

                    // To avoid calculating pointcloud before new frames were processed, set 'after_reset' to 'true'
                    after_reset = true;
                    points.release();
                    normals.release();
                    std::cout << "reset" << std::endl;
                }

                // Get current pointcloud
                if (!after_reset)
                {
                    kf->getCloud(points, normals);
                }

                if (!points.empty() && !normals.empty())
                {
                    // copy points from GPU to CPU for rendering
                    points.copyTo(_points);
                    points.release();
                    normals.copyTo(_normals);
                    normals.release();
                    // Push to queue for rendering
                    points_queue.push(_points);
                    normals_queue.push(_normals);
                }
                after_reset = false;
            }
        }
        catch (const std::exception& e) // Save pointcloud in case an error occurs (for example, camera disconnects)
        {
            export_to_ply(_points, _normals);
        }
    });
    
    // Main thread handles rendering of the pointcloud
    Mat points;
    Mat normals;
    while (app)
    {
        // Get the current state of the pointcloud
        points_queue.try_get_next_item(points);
        normals_queue.try_get_next_item(normals);
        if (!points.empty() && !normals.empty()) // points or normals might not be ready on first iterations
            draw_kinfu_pointcloud(app_state, points, normals);
    }
    stopped = true;
    calc_cloud_thread.join();

    // Save the pointcloud upon closing the app
    export_to_ply(points, normals);

    return 0;
}
