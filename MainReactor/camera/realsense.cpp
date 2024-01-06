#include <opencv2/opencv.hpp>
#include <librealsense2/rs.hpp>
#include "camera.h"
#include "camera_settings.h"
#include "realsense.h"

std::string getSerialFromSource(std::string source)
{
    // Get the string between the first two slashes, if it exists.
    // E.g. RS/1234/COLOR => "1234" or RS/COLOR => ""
    int firstSlash = source.find('/');
    int secondSlash = source.find('/', firstSlash + 1);
    if (firstSlash == std::string::npos || secondSlash == std::string::npos)
    {
        return "";
    }

    return source.substr(firstSlash + 1, secondSlash - firstSlash - 1);
}

////////////////////////////
// RealsenseCameraFactory //
////////////////////////////

void RealsenseCameraFactory::listCameras(std::vector<CameraInfo> &outCameraInfo) const
{
    rs2::context ctx;
    for (rs2::device &&dev : ctx.query_devices())
    {
        std::string deviceSource = "RS/" + std::string(dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));
        std::string deviceDescription = std::string(dev.get_info(RS2_CAMERA_INFO_NAME));
        outCameraInfo.push_back({ deviceSource + "/COLOR", deviceDescription + " (Color)" });
        outCameraInfo.push_back({ deviceSource + "/IR", deviceDescription + " (IR)" });
        outCameraInfo.push_back({ deviceSource + "/DEPTH", deviceDescription + " (Depth)" });
    }
}

bool RealsenseCameraFactory::constructCamera(const CameraSettings &settings, std::vector<std::shared_ptr<Runnable>> &outCameras)
{
    if (!settings.source.starts_with("RS/"))
    {
        return false;
    }
    
    std::unique_ptr<RealsenseCamera> camera;
    if (settings.source.ends_with("/COLOR"))
    {
        camera = std::make_unique<RealsenseColorCamera>(settings);
    }
    else if (settings.source.ends_with("/IR"))
    {
        camera = std::make_unique<RealsenseIRCamera>(settings);
    }
    else if (settings.source.ends_with("/DEPTH"))
    {
        camera = std::make_unique<RealsenseDepthCamera>(settings);
    }
    else
    {
        std::cerr << "ERROR: Failed to create realsense camera with unknown source: " << settings.source << std::endl;
        throw 1;
    }

    std::string serial = getSerialFromSource(settings.source);
    if (!realsenseDevices.contains(serial))
    {
        realsenseDevices[serial] = std::make_shared<Realsense>(serial);
        outCameras.push_back(realsenseDevices[serial]);
    }

    realsenseDevices[serial]->registerCamera(std::move(camera));
    return true;
}

///////////////
// Realsense //
///////////////

void Realsense::registerCamera(std::unique_ptr<RealsenseCamera> camera)
{
    const CameraSettings &cameraSettings = camera->getSettings();
    if (!cameraSettings.source.starts_with("RS/"))
    {
        std::cerr << "ERROR: Unrecognized realsense camera source: " << cameraSettings.source << std::endl;
        throw 1;
    }

    // Register the camera type.
    if (cameraSettings.source.ends_with("/COLOR"))
    {
        if(cameraSettings.exposure > 0)
            colorExposure = cameraSettings.exposure;
        if(cameraSettings.whiteBalance > 0)
            whiteBalance = cameraSettings.whiteBalance;
        cameras.emplace_back(std::move(camera));
        config.enable_stream(RS2_STREAM_COLOR, 848, 480, RS2_FORMAT_BGR8, 30);
        std::cout << "Registered a color camera" << std::endl;
        return;
    }
    else if (cameraSettings.source.ends_with("/IR"))
    {
        cameras.emplace_back(std::move(camera));
        config.enable_stream(RS2_STREAM_INFRARED, 2, 848, 480, RS2_FORMAT_Y8, 30);
        std::cout << "Registered an IR camera" << std::endl;
        return;
    }
    else if (cameraSettings.source.ends_with("/DEPTH"))
    {
        if(cameraSettings.rs_alignToColor)
            bAlign = true;
        cameras.emplace_back(std::move(camera));
        config.enable_stream(RS2_STREAM_DEPTH, 848, 480, RS2_FORMAT_Z16, 30);
        std::cout << "Registered a depth camera" << std::endl;
        return;
    }
    else
    {
        std::cerr << "ERROR: Unrecognized realsense camera source: " << cameraSettings.source << std::endl;
        throw 1;
    }
}

bool Realsense::start()
{
    if (cameras.empty())
    {
        std::cerr << "ERROR: Unable to start Realsense with 0 cameras" << std::endl;
        throw 1;
    }

    if (serial.empty())
    {
        std::cout << "Starting Realsense" << std::endl;
    }
    else
    {
        std::cout << "Starting Realsense with serial: " << serial << std::endl;
        config.enable_device(serial);
    }

    rs2::pipeline_profile selection = pipe.start(config);
	rs2::device selected_device = selection.get_device();
    if (bAlign) {
        bAlign = hasColorAndDepth(selection.get_streams());
        if (!bAlign) std::cout << "Either Color or Depth not available - alignment disabled!" << std::endl;
    }

    //bAlign knows that color AND depth are configured, so set up the alignment
    //We align to color as to prevent the depth artifacts from poisoning the color stream
    if (bAlign) {
        rs2_stream alignTo = RS2_STREAM_COLOR;
        pAlignment.reset(new rs2::align(alignTo));
        depthScale = getDepthScale(selected_device);
    }

    ///TODO: This is unconfigured - just hard coded here. Needs to be pulled apart and fixed to allow for different entries.
    ///Leaving this code in place to show an example of "IR only sensor use" on a D435i
	// rs2::depth_sensor depth_sensor = selected_device.first<rs2::depth_sensor>();
	// //depth_sensor.set_option(RS2_OPTION_EMITTER_ENABLED, 0.f); // Disable emitter
	// depth_sensor.set_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE, 0.f);
	// depth_sensor.set_option(RS2_OPTION_EXPOSURE, 1000);

    ///TODO: Make this configurable!
    rs2::color_sensor color_sensor = selected_device.first<rs2::color_sensor>();
    color_sensor.set_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE, 0.f);
    color_sensor.set_option(RS2_OPTION_EXPOSURE, (float)colorExposure);
    color_sensor.set_option(RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE, 1.f);
    if(whiteBalance > 0) {
        color_sensor.set_option(RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE, 0.f);
        color_sensor.set_option(RS2_OPTION_WHITE_BALANCE, (float)whiteBalance);
    }


    // Throw out the first handful of frames.
    for(int i = 0; i < 30; ++i)
    {
        pipe.wait_for_frames();
    }

    std::cout << "Finished starting up Realsense!" << std::endl;

    return true;
}

bool Realsense::update()
{
    bool isAllSuccess = true;
    rs2::frameset frameset = pipe.wait_for_frames();

    //align if enabled
    if (bAlign) {
        frameset = pAlignment->process(frameset);
    }

    for (std::unique_ptr<RealsenseCamera> &camera : cameras)
    {
        bool isSuccess = camera->updateFrame(frameset);
        isAllSuccess &= isSuccess;
    }

    return isAllSuccess;
}

bool Realsense::hasColorAndDepth(const std::vector<rs2::stream_profile>& streams) {
    bool depth_stream_found = false;
    bool color_stream_found = false;
    for (rs2::stream_profile sp : streams)
    {
        rs2_stream profile_stream = sp.stream_type();
        if (profile_stream != RS2_STREAM_DEPTH)
        {
            if (profile_stream == RS2_STREAM_COLOR)
            {
                color_stream_found = true;
            }
        }
        else
        {
            depth_stream_found = true;
        }
    }

    if (depth_stream_found && color_stream_found)
        return true;

    return false;
}

float Realsense::getDepthScale(rs2::device dev)
{
    // Go over the device's sensors
    for (rs2::sensor& sensor : dev.query_sensors())
    {
        // Check if the sensor if a depth sensor
        if (rs2::depth_sensor dpt = sensor.as<rs2::depth_sensor>())
        {
            return dpt.get_depth_scale();
        }
    }
    throw std::runtime_error("Device does not have a depth sensor");
}

//////////////////////////
// RealsenseColorCamera //
//////////////////////////

RealsenseColorCamera::RealsenseColorCamera(const CameraSettings &settings)
{
    Camera::setup(settings);
}

CameraSettings RealsenseColorCamera::getActiveSettings()
{
    // TODO: what settings can we use for a Realsense camera?
    CameraSettings settings;
    return settings;
}

Mat RealsenseColorCamera::getFrameData(rs2::frameset &frameset)
{
    // TODO: use settings for resolution?
    return Mat(Size(848, 480), CV_8UC3, (void*)frameset.get_color_frame().get_data(), Mat::AUTO_STEP);
}

///////////////////////
// RealsenseIRCamera //
///////////////////////

RealsenseIRCamera::RealsenseIRCamera(const CameraSettings &settings)
{
    Camera::setup(settings);
}

CameraSettings RealsenseIRCamera::getActiveSettings()
{
    CameraSettings settings;
    return settings;
}

Mat RealsenseIRCamera::getFrameData(rs2::frameset &frameset)
{
    return Mat(Size(848, 480), CV_8UC1, (void*)frameset.get_infrared_frame().get_data(), Mat::AUTO_STEP);
}

//////////////////////////
// RealsenseDepthCamera //
//////////////////////////

RealsenseDepthCamera::RealsenseDepthCamera(const CameraSettings &settings)
    : depth_to_disparity(true), disparity_to_depth(false)
{
    Camera::setup(settings);

    if (settings.rs_decimationMagnitude > 0)
    {
        shouldDecimate = true;
        decimation_filter = new rs2::decimation_filter(settings.rs_decimationMagnitude);
    }
}

CameraSettings RealsenseDepthCamera::getActiveSettings()
{
    CameraSettings settings;
    return settings;
}

Mat RealsenseDepthCamera::getFrameData(rs2::frameset &frameset)
{
    rs2::frame filtered = frameset.get_depth_frame();
    if (shouldDecimate)
        filtered = decimation_filter->process(filtered);
    filtered = depth_to_disparity.process(filtered);
    filtered = spat_filter.process(filtered);
    filtered = temp_filter.process(filtered);
    filtered = disparity_to_depth.process(filtered);
    if (getSettings().rs_alignToColor)
        return Mat(Size(848, 480), CV_16UC1, (void*)filtered.get_data(), Mat::AUTO_STEP);
    return Mat(Size(848, 480), CV_16UC1, (void*)filtered.get_data(), Mat::AUTO_STEP);
}
