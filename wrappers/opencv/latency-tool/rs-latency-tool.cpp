// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "latency-detector.h"
#include <iostream>

// This demo is presenting one way to estimate latency without access to special equipment
// See ReadMe.md for more information
int main(int argc, char * argv[]) try
{
    using namespace cv;
    using namespace rs2;

    // Start RealSense camera
    // Uncomment the configuration you wish to test
    pipeline pipe;
    config cfg;
    //cfg.enable_stream(RS2_STREAM_COLOR, RS2_FORMAT_BGR8);
    //cfg.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_BGR8, 30);
    cfg.enable_stream(RS2_STREAM_COLOR, 1920, 1080, RS2_FORMAT_BGR8, 30);
    //cfg.enable_stream(RS2_STREAM_INFRARED, 640, 480, RS2_FORMAT_Y8);
    //cfg.enable_stream(RS2_STREAM_INFRARED, 1280, 720, RS2_FORMAT_Y8);
    pipe.start(cfg);

    // To test with a regular webcam, comment-out the previous block
    // And uncomment the following code:
    //context ctx;
    //auto dev = ctx.query_devices().front();
    //auto sensor = dev.query_sensors().front();
    //auto sps = sensor.get_stream_profiles();
    //stream_profile selected;
    //for (auto& sp : sps)
    //{
    //    if (auto vid = sp.as<video_stream_profile>())
    //    {
    //        if (vid.format() == RS2_FORMAT_BGR8 &&
    //            vid.width() == 640 && vid.height() == 480 &&
    //            vid.fps() == 30)
    //            selected = vid;
    //    }
    //}
    //sensor.open(selected);
    //syncer pipe;
    //sensor.start(pipe);

    const auto window_name = "Display Image";
    namedWindow(window_name, WINDOW_NORMAL);
    setWindowProperty(window_name, WND_PROP_FULLSCREEN, WINDOW_FULLSCREEN);

    const int display_w = 1280;
    const int digits = 16;
    const int circle_w = (display_w - 120) / (digits + 2);
    const int display_h = 720;

    bit_packer packer(digits);

    detector d(digits, display_w);

    while (getWindowProperty(window_name, WND_PROP_AUTOSIZE) >= 0)
    {
        // Wait for frameset from the camera
        for (auto f : pipe.wait_for_frames())
        {
            // Unfortunately depth frames do not register well using this method
            if (f.get_profile().stream_type() != RS2_STREAM_DEPTH)
            {
                d.submit_frame(f);
                break;
            }
        }

        // Commit to the next clock value before starting rendering the frame
        auto next_value = d.get_next_value();

        // Start measuring rendering time for the next frame
        // We substract rendering time since during that time
        // the clock is already captured, but there is zero chance 
        // for the clock to appear on screen (until done rendering)
        d.begin_render();

        Mat display = Mat::zeros(display_h, display_w, CV_8UC1);

        // Copy preview image generated by the detector (to not waste time in the main thread)
        d.copy_preview_to(display);

        // Pack the next value into binary form
        packer.try_pack(next_value);

        // Render the clock encoded in circles
        for (int i = 0; i < digits + 2; i++)
        {
            const int rad = circle_w / 2 - 6;
            // Always render the corner two circles (as markers)
            if (i == 0 || i == digits + 1 || packer.get()[i - 1])
            {
                circle(display, Point(50 + circle_w * i + rad, 70 + rad),
                    rad, Scalar(255, 255, 255), -1, 8);
            }
        }

        // Display current frame. WaitKey is doing the actual rendering
        imshow(window_name, display);
        if (waitKey(1) >= 0) break;

        // Report rendering of current frame is done
        d.end_render();
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



