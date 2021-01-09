//g++ -Immalpp -I/opt/vc/include/interface/ -I/opt/vc/include/ -L/opt/vc/lib/ -lmmal -lmmal_util -lmmal_components -lmmal_core -lmmal_vc_client -std=c++17 test.cpp
//~/.leila/toolchains/rpi/x-tools/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-g++

#include <iostream>
#include <fstream>
#include <vector>

#include "mmalpp.h"

int main()
{
    /// Define components
    mmalpp::Component camera("vc.ril.camera");
    mmalpp::Component null_sink("vc.null_sink");

    MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request =
    {
        {MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)},
        MMAL_PARAMETER_CAMERA_SETTINGS, 1
    };

    /// Set parameters
    camera.control().parameter().set_header(&change_event_request.hdr);

    /// Passing callback with lambda expression
    camera.control().enable([](mmalpp::Generic_port& port, mmalpp::Buffer buffer){
        if (port.is_enabled())
            buffer.release();
    });

    MMAL_PARAMETER_CAMERA_CONFIG_T camConfig = {
        {MMAL_PARAMETER_CAMERA_CONFIG, sizeof ( camConfig ) },
        3280, // max_stills_w
        2464, // max_stills_h
        1, // stills_yuv422
        1, // one_shot_stills
        3280, // max_preview_video_w
        2464, // max_preview_video_h
        3, // num_preview_video_frames
        0, // stills_capture_circular_buffer_height
        0, // fast_preview_resume
        MMAL_PARAM_TIMESTAMP_MODE_ZERO // use_stc_timestamp
    };

    /// Set parameter to the control port
    camera.control().parameter().set_header(&camConfig.hdr);

    /// Set buffer_size and buffer_num to dfu value in the 2nd output port (still port)
    camera.output(2).set_default_buffer();

    /// Get still port's format
    MMAL_ES_FORMAT_T * format = camera.output(2).format();
    format->encoding = MMAL_ENCODING_RGB24;
    format->es->video.width = 640;
    format->es->video.height = 480;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = 640;
    format->es->video.crop.height = 480;
    format->es->video.frame_rate.num = 1;
    format->es->video.frame_rate.den = 1;

    /// Commit changes
    camera.output(2).commit();

    /// Enable component
    camera.enable();

    /// Enable null_sink component.
    null_sink.enable();

    camera.output(2).enable([](mmalpp::Generic_port& port, mmalpp::Buffer buffer){
        std::vector<u_char>& v = port.get_userdata_as<std::vector<u_char>>();

        /// Insert data into a vector.
        v.insert(v.end(), buffer.begin(), buffer.end());

        /// Notify when finished, You have to implement your notify_end_frame logic
        /// (e.g.: condition variable, mutex ,...).
        //if (buffer.flags() & (MMAL_BUFFER_HEADER_FLAG_EOS |
        //                      MMAL_BUFFER_HEADER_FLAG_FRAME_END))
        if (buffer.size() == 0){
            std::cout << "Finished. Press any key to continue " << buffer.size() << std::endl;
		  }
        buffer.release();

        if (port.is_enabled()){
            port.send_buffer(port.pool().queue().get_buffer());
		  }
    });
    
    camera.output(2).create_pool(camera.output(2).buffer_num_recommended(),
                                  camera.output(2).buffer_size_recommended());
    
    std::vector<u_char> v;
    camera.output(2).set_userdata(v);

    MMAL_PARAMETER_EXPOSUREMODE_T exp_mode =
    {
        {
            MMAL_PARAMETER_EXPOSURE_MODE,
            sizeof ( exp_mode )
        },
        MMAL_PARAM_EXPOSUREMODE_AUTO
    };

    /// Set some exposure mode parameter.
    camera.control().parameter().set_header(&exp_mode.hdr);

    /// Send the pool created above to the port.
    camera.output(2).send_all_buffers();
    
    /// Start capture just one frame
    camera.output(2).parameter().set_boolean(MMAL_PARAMETER_CAPTURE, true);

    std::cin.get(); /// Implement your waiting_for_frame-logic.

    /// When frame is captured it continues and write on file.
    std::cout << "Writing to file" << std::endl;

    std ::ofstream f("test.rgb", std::ios::binary);
    f.write(reinterpret_cast<char*>(v.data()), static_cast<std::streamsize>(v.size()));
    f.close();

    /// Before calling close you must disconnect all connections previously created.
    null_sink.disconnect();
//    camera.disconnect();//causes crash

    /// By calling close you will disable all ports and pools of each component.
    null_sink.close();
    camera.close();

    return 0;
}
