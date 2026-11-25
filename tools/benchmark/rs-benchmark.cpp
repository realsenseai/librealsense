// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-24 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <librealsense2-gl/rs_processing_gl.hpp>

#include <iostream>
#include <iomanip>
#include <map>
#include <set>
#include <cstring>
#include <chrono>
#include <numeric>
#include <math.h>
#include <fstream>

#include <common/cli.h>
#include "example-utils.hpp"

using namespace std;
using namespace chrono;
using namespace TCLAP;
using namespace rs2;

#if (defined(_WIN32) || defined(_WIN64))
#include <intrin.h>

string get_cpu()
{
    // Based on: https://weseetips.wordpress.com/tag/c-get-cpu-name/
    // Get extended ids.
    int CPUInfo[4] = { -1 };
    __cpuid(CPUInfo, 0x80000000);
    unsigned int nExIds = CPUInfo[0];

    // Get the information associated with each extended ID.
    char CPUBrandString[0x40] = { 0 };
    for (unsigned int i = 0x80000000; i <= nExIds; ++i)
    {
        __cpuid(CPUInfo, i);

        // Interpret CPU brand string and cache information.
        if (i == 0x80000002)
        {
            memcpy(CPUBrandString,
                CPUInfo,
                sizeof(CPUInfo));
        }
        else if (i == 0x80000003)
        {
            memcpy(CPUBrandString + 16,
                CPUInfo,
                sizeof(CPUInfo));
        }
        else if (i == 0x80000004)
        {
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
        }
    }

    char* ptr = CPUBrandString;
    while (*ptr == ' ') ptr++;
    return ptr;
}

#elif defined __linux__ || defined(__linux__)
string get_cpu() {
    // Based on: http://forums.codeguru.com/showthread.php?472578-How-to-get-the-cpu-information-on-linux
    string line;
    ifstream finfo("/proc/cpuinfo");
    while(getline(finfo,line))
    {
        stringstream str(line);
        string itype;
        string info;
        if (getline(str, itype, ':') && getline(str, info) && itype.substr(0, 10) == "model name") 
        {
            return info;
        }
    }
    return "unknown";
}
#elif __APPLE__
string get_cpu() { return "unknown"; }
#else
string get_cpu() { return "unknown"; }
#endif

class test
{
public:
    virtual frame prepare(frame f) { return f; };
    virtual frame process(frame f) = 0;
    virtual frame finish (frame f) { return f; };
    virtual const std::string& name() const = 0;
};

template<class T>
class pb_test : public test
{
public:
    pb_test(std::string name) 
        : _block(), _name(std::move(name)) {}

    frame process(frame f) override
    {
        return _block.process(f);
    }
    virtual const std::string& name() const override
    {
        return _name;
    }
private:
    T _block;
    std::string _name;
};

template<class T>
class gl_test : public pb_test<T>
{
public:
    gl_test(std::string name) 
        : pb_test<T>(std::move(name)) {}


    void flush()
    {
        glFlush();
        glFinish();
    }

    frame process(frame f) override
    {
        auto res = pb_test<T>::process(f);
        flush();
        return res;
    }
    frame prepare(frame f) override
    {
        auto res = _upload.process(f);
        flush();
        return res;
    }
    frame finish(frame f) override
    {
        _ptr = (void*)f.get_data();
        return f;
    }
private:
    gl::uploader _upload;
    volatile void* _ptr;
};

class suite
{
public:
    virtual void register_tests(stream_profile stream,
        vector<shared_ptr<test>>& tests) const = 0;
};

#define REGISTER_TEST(x) tests.push_back(make_shared<pb_test<x>>(#x))

class processing_blocks : public suite
{
public:
    void register_tests(stream_profile stream,
        vector<shared_ptr<test>>& tests) const override
    {
        if (stream.stream_type() == RS2_STREAM_DEPTH)
        {
            REGISTER_TEST(colorizer);
            REGISTER_TEST(pointcloud);
            REGISTER_TEST(spatial_filter);
            REGISTER_TEST(temporal_filter);
            REGISTER_TEST(disparity_transform);
            REGISTER_TEST(threshold_filter);
            REGISTER_TEST(decimation_filter);
        }
        if (stream.format() == RS2_FORMAT_YUYV)
        {
            REGISTER_TEST(yuy_decoder);
        }
    }
};

#define REGISTER_GL_TEST(x) tests.push_back(make_shared<gl_test<x>>(#x))

class gl_blocks : public suite
{
public:
    void register_tests(stream_profile stream,
        vector<shared_ptr<test>>& tests) const override
    {
        if (stream.stream_type() == RS2_STREAM_DEPTH)
        {
            REGISTER_GL_TEST(gl::colorizer);
            REGISTER_GL_TEST(gl::pointcloud);
        }
        if (stream.format() == RS2_FORMAT_YUYV)
        {
            REGISTER_GL_TEST(gl::yuy_decoder);
        }
    }
};

int main(int argc, char** argv) try
{
    rs2::rum::set_app_name( "rs-benchmark" );
    auto settings = rs2::cli( "rs-benchmark tool" )
        .process( argc, argv );
    rs2::context ctx( settings.dump() );

    std::string serial;
    rs2_stream second_stream;
    if( ! device_with_streams( ctx, { RS2_STREAM_DEPTH }, serial ) )
        return EXIT_SUCCESS;

    if( device_with_streams( ctx, { RS2_STREAM_COLOR }, serial ) )
        second_stream = RS2_STREAM_COLOR;
    else if( device_with_streams( ctx, { RS2_STREAM_INFRARED }, serial ) )
        second_stream = RS2_STREAM_INFRARED;
    else
    {
        std::cout<< " Connect a Depth Camera that supports either RGB or Infrared streams." <<std::endl;
        return EXIT_SUCCESS;
    }

    glfwInit();
    glfwWindowHint(GLFW_VISIBLE, 0);
    auto win = glfwCreateWindow(100,100,"offscreen",0,0);
    glfwMakeContextCurrent(win);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    
    auto renderer = (const char*)glGetString(GL_RENDERER);
    auto version = (const char*)glGetString(GL_VERSION);

    cout << endl;
    cout << "|            |     |" << endl;
    cout << "|------------|-----|" << endl;

    cout << "|**CPU** |" << get_cpu() << " |" << endl;
    cout << "|**GPU** | " << renderer << " |" << endl;
    cout << "|**Graphics Driver** |" << version << " |" << endl;

    vector<shared_ptr<suite>> suites;
    suites.push_back(make_shared<processing_blocks>());
    
#ifndef __APPLE__
    gl::init_processing(win, true);
    suites.push_back(make_shared<gl_blocks>());
#endif

    pipeline p( ctx );
    config cfg;
    if (!serial.empty())
        cfg.enable_device(serial);
    cfg.enable_stream(RS2_STREAM_DEPTH);
    if(second_stream == RS2_STREAM_COLOR)
        cfg.enable_stream(RS2_STREAM_COLOR, RS2_FORMAT_YUYV, 30);
    else
        cfg.enable_stream(RS2_STREAM_INFRARED);
    auto prof = p.start(cfg);
    auto dev = prof.get_device();
    auto name = dev.get_info(RS2_CAMERA_INFO_NAME);
    cout << "|**Device Name** |" << name << " |" << endl << endl;
    cout.precision(3);

    for (auto stream : prof.get_streams())
    {
        cout << "**Stream Type**: " << stream.stream_name();
        if (auto vs = stream.as<video_stream_profile>())
        {
            cout << ", **Resolution**: " << vs.width() << " x " << vs.height() << endl;
        }
        auto fps = stream.fps();

        vector<shared_ptr<test>> procs;
        for (auto&& suite : suites)
            suite->register_tests(stream, procs);

        vector<frame> frames;
        for (int i = 0; i < 5 * fps; i++)
        {
            auto fs = p.wait_for_frames();
            for (auto&& f : fs)
                if (f.get_profile().unique_id() == stream.unique_id())
                {
                    f.keep();
                    frames.push_back(f);
                }
        }

        cout << endl;
        cout << "|Filter Name |Step |Median(m)   |Mean(m)  |STD(m)  |Max(m)  | Max FPS |" << endl;
        cout << "|------------|-----|------------|---------|--------|--------|---------|" << endl;

        string last_name = "";
        for (auto&& test : procs)
        {
            map<string, vector<double>> steps;

            for (auto&& f : frames)
            {
                auto p1 = high_resolution_clock::now();
                auto f1 = test->prepare(f);
                auto p2 = high_resolution_clock::now();
                auto f2 = test->process(f1);
                auto p3 = high_resolution_clock::now();
                test->finish(f2);
                auto p4 = high_resolution_clock::now();

                auto prep = duration_cast<microseconds>(p2 - p1).count();
                auto proc = duration_cast<microseconds>(p3 - p2).count();
                auto down = duration_cast<microseconds>(p4 - p3).count();
                auto total = duration_cast<microseconds>(p4 - p1).count();

                steps[" Upload"].push_back(prep * 0.001);
                steps["Calculate"].push_back(proc * 0.001);
                steps["Download"].push_back(down * 0.001);
                steps["Total"].push_back(total * 0.001);
            }

            int printed_steps = 0;
            for (auto&& sm : steps)
            {
                if (sm.first == "Total" && printed_steps < 2) continue;

                auto& m = sm.second;
                double max = *max_element(m.begin(), m.end());
                double sum = accumulate(m.begin(), m.end(), 0.0);
                double mean = sum / m.size();
                double sq_sum = inner_product(m.begin(), m.end(), m.begin(), 0.0);
                double stdev = sqrt(sq_sum / m.size() - mean * mean);
                sort(m.begin(), m.end());
                double median = m[m.size() / 2];

                vector<int> fps_values{ 6, 15, 30, 60, 90 };

                auto expected_max = mean + 1.645 * stdev; // 95-percentile - camera spec allows up to 5% outliers

                int best_fps = 1;
                for (int fps : fps_values)
                {
                    auto max_allowed = 1000.0 / fps;
                    if (expected_max < max_allowed) best_fps = fps;
                }

                if (sm.first == "Calculate" || median > 0.001)
                {
                    bool is_new = last_name != test->name();
                    bool is_total = sm.first == "Total";
                    cout << "|" << (is_new ? test->name() : "") 
                        << " |" << (is_total ? "**" : "") << sm.first << (is_total ? "**" : "") << " |"
                        << fixed << median << " |" << mean << " |"
                        << stdev << " |" << max << " |";

                    if (best_fps == 90) cout << "90 ![90](https://placehold.it/15/35ff4d/000000?text=+)";
                    else if (best_fps == 60) cout << "60 ![60](https://placehold.it/15/6fe837/000000?text=+)";
                    else if (best_fps == 30) cout << "30 ![30](https://placehold.it/15/82c13e/000000?text=+)";
                    else if (best_fps == 15) cout << "15 ![15](https://placehold.it/15/eff70c/000000?text=+)";
                    else if (best_fps == 6) cout << "6 ![6](https://placehold.it/15/d6a726/000000?text=+)";
                    else cout << "? ![unknown](https://placehold.it/15/d65d26/000000?text=+)";

                    cout << " |" << endl;

                    printed_steps++;
                    last_name = test->name();
                }
            }
        }
        cout << endl;
    }

    return EXIT_SUCCESS;
}
catch (const error & e)
{
    cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << endl;
    return EXIT_FAILURE;
}
catch( const exception & e )
{
    cerr << e.what() << endl;
    return EXIT_FAILURE;
}
catch( ... )
{
    cerr << "some error" << endl;
    return EXIT_FAILURE;
}
