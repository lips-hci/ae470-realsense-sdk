// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved
// original file: examples/capture
//
// Copyright(c) 2024 LIPS Corporation. All Rights Reserved.
//
// This example is to demo how to recover camera and realsense
// connection if network has problem
//
// Test case:
//   you can run this sample to query depth and color frames, and randomly
//   unplug ethernet cable from your PC/NB side to simulate broken network
//   or unstable network problems
//
//   This sample should reconnect camera and restart realsense tasks successfully
//   your depth streaming should go back
//

#include <chrono>
#include <thread>

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include "example.hpp"          // Include short list of convenience functions for rendering
#include <librealsense2/lips_ae4xx_dm.h> // ae4xx_restart()

bool safe_stop_pipeline(rs2::pipeline &p)
{
    try
    {
        std::cout << "pipeline safe stopping..." << std::endl;
        p.stop();
        std::cout << "pipeline safe stopped!" << std::endl;

        return true;
    }
    catch(const rs2::error &e)
    {
        std::cout << "pipeline stop - FAIL!" << std::endl;
        std::cerr << "RealSense error calling " << e.get_failed_function();
        std::cerr << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    }

    return false;
}

// Capture Example demonstrates how to
// capture depth and color video streams and render them to the screen
int main(int argc, char * argv[]) try
{
    rs2::log_to_console(RS2_LOG_SEVERITY_ERROR);
    // Create a simple OpenGL window for rendering:
    // 32:9 window because depth and color side-by-side
    window app(1280, 400, "LIPSedge AE4xx Camera Capture-safe Example");

    // Declare depth colorizer for pretty visualization of depth data
    rs2::colorizer color_map;
    // Declare rates printer for showing streaming rates of the enabled streams.
    //rs2::rates_printer printer;

    // Declare RealSense pipeline, encapsulating the actual device and sensors
    //rs2::pipeline pipe;

    int start_retry = 0;

    while (app) //ctrl+c can quit the window
    {
        // Declare rates printer for showing streaming rates of the enabled streams.
        rs2::rates_printer printer;

        // Declare RealSense pipeline, encapsulating the actual device and sensors
        rs2::pipeline pipe;

        // LIPS: customize stream resolutions to HD
        // Create a configuration for configuring the pipeline with a non default profile
        rs2::config cfg;

        // Start streaming with Depth and Color streams
        cfg.enable_stream(RS2_STREAM_DEPTH, 1280, 720); //AE450/470(rs D455) would prefer 848x480
        cfg.enable_stream(RS2_STREAM_COLOR, 1280, 720);
        // If a device is capable to stream IMU data, you can enable them in cfg

        try
        {
            // Start pipeline with chosen configuration
            auto profile = pipe.start(cfg);
            std::cout << "pipeline safe started." << std::endl;
            start_retry = 0;
            //DEBUG streams
            for (auto&& sp : profile.get_streams())
            {
                std::cout << " -> " << sp.stream_name() << " as " << sp.format() << " at " << sp.fps() << " Hz";
                if (auto vp = sp.as<rs2::video_stream_profile>())
                {
                    std::cout << "; Resolution: " << vp.width() << "x" << vp.height();
                }
                std::cout << std::endl;
            }
        }
        catch (const rs2::error & e)
        {
            std::cout << "pipeline start - FAIL! Keep retry..." << std::endl;
            std::cerr << "RealSense error calling " << e.get_failed_function();
            std::cerr << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;

            if( std::string("No device connected").compare(e.what()) == 0 )
            {
                if (++start_retry > 4 )
                {
                    start_retry = 0;
                    std::cout << "Oops! we have to reboot LIPSedge-AE4 camera. Please wait for 5 seconds..." << std::endl;
#if (AEMODEL==430)
                    ae4xx_restart("ae430", "192.168.0.100");
#else //AEMODEL==400
                    ae4xx_restart("ae400", "192.168.0.100");
#endif
                    std::this_thread::sleep_for(std::chrono::seconds(6));
                }
            }

            //since pipe didn't start correctedly, we don't have to stop it
            std::cout << "Trying to restart pipeline. Please wait..." << std::endl;
            continue;
        }

        try
        {
            while (app)// Application still alive?
            {
                rs2::frameset data = pipe.wait_for_frames(5000).// Wait for next set of frames from the camera
                                    apply_filter(printer).     // Print each enabled stream frame rate
                                    apply_filter(color_map);   // Find and colorize the depth data

                // The show method, when applied on frameset, break it to frames and upload each frame into a gl textures
                // Each texture is displayed on different viewport according to it's stream unique id
                app.show(data);
            }
        }
        catch (const rs2::error & e)
        {
            std::cerr << "RealSense error calling " << e.get_failed_function();
            std::cerr << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;

            std::cout << "Trying to stop pipeline. Please wait..." << std::endl;
            if (safe_stop_pipeline(pipe))
            {
                //Next step is to re-start pipe
                std::cout << "Trying to restart pipeline. Please wait..." << std::endl;
                continue;
            }
            else
            {
                std::cout << "We're doomed! Cannot recover pipeline..." << std::endl;
                break;
            }
        }
    }

    std::cout << "Bye Bye!" << std::endl;
    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function();
    std::cerr << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}