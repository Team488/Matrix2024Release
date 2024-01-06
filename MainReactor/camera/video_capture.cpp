#include "video_capture.h"

///////////////////////////////
// VideoCaptureCameraFactory //
///////////////////////////////

void VideoCaptureCameraFactory::listCameras(std::vector<CameraInfo> &outCameraInfo) const
{
    // TODO: list all found camera devices
}

bool VideoCaptureCameraFactory::constructCamera(const CameraSettings &settings, std::vector<std::shared_ptr<Runnable>> &outCameras)
{
    // Find the specific camera backend if requested
    VideoCaptureAPIs cameraBackend = CAP_ANY;
    if (!settings.cameraBackend.empty())
    {
        for (VideoCaptureAPIs availableCameraBackend : cv::videoio_registry::getCameraBackends())
        {
            std::string availableCameraBackendName = cv::videoio_registry::getBackendName(cameraBackend);
            if (availableCameraBackendName == settings.cameraBackend)
            {
                cameraBackend = availableCameraBackend;
                break;
            }
        }
    }

    try
    {
        // Try to treat the source as a device index.
        int sourceIndex = std::stoi(settings.source);
        outCameras.push_back(std::make_shared<VideoCaptureCamera>(sourceIndex, cameraBackend, settings));
        return true;
    }
    catch(const std::exception& e)
    {
        // Try to treat the source as a URL.
        if (settings.source.find("://") != std::string::npos)
        {
            outCameras.push_back(std::make_shared<VideoCaptureCamera>(settings.source, cameraBackend, settings));
            return true;
        }
        else
        {
            // TODO: identify video capture device by product name or serial number.
            std::cerr <<  "ERROR: Unrecognized VideoCaptureCamera source: " << settings.source << std::endl;
            throw 1;
        }
    }
}

////////////////////////
// VideoCaptureCamera //
////////////////////////

VideoCaptureCamera::VideoCaptureCamera(int deviceIndex, cv::VideoCaptureAPIs backend, const CameraSettings &settings)
{
    std::cout << "Starting VideoCaptureCamera with index " << deviceIndex << " and backend '" << backend << "'" << std::endl;
    capture.open(deviceIndex, backend);
    if (!capture.isOpened())
    {
        std::cerr << "ERROR: Failed to open VideoCaptureCamera source: " << settings.source << std::endl;
        throw 1;
    }

    applySettings(settings);
}

VideoCaptureCamera::VideoCaptureCamera(const std::string &url, cv::VideoCaptureAPIs backend, const CameraSettings &settings)
{
    std::cout << "Starting VideoCaptureCamera with URL '" << url << "' and backend '" << backend << "'." << std::endl;
    capture.open(url, backend);
    if (!capture.isOpened())
    {
        std::cerr << "ERROR: Failed to open VideoCaptureCamera source: " << settings.source << std::endl;
        throw 1;
    }

    applySettings(settings);
}

void VideoCaptureCamera::applySettings(const CameraSettings &settings)
{
    Camera::setup(settings);
    
    // Try to set the camera settings.
    if (settings.exposure != -488)
        capture.set(CAP_PROP_EXPOSURE, settings.exposure);
    if (settings.contrast != -488)
        capture.set(CAP_PROP_CONTRAST, settings.contrast);
    if (settings.brightness != -488)
        capture.set(CAP_PROP_BRIGHTNESS, settings.brightness);
    if (settings.gain != -488)
        capture.set(CAP_PROP_GAIN, settings.gain);
    if (settings.saturation != -488)
        capture.set(CAP_PROP_SATURATION, settings.saturation);
}

bool VideoCaptureCamera::update()
{
    Mat frame;
    capture >> frame;
    updateFrame(frame);
    return true;
}

CameraSettings VideoCaptureCamera::getActiveSettings()
{
    CameraSettings settings;
    settings.exposure   = capture.get(CAP_PROP_EXPOSURE);
    settings.contrast   = capture.get(CAP_PROP_CONTRAST);
    settings.brightness = capture.get(CAP_PROP_BRIGHTNESS);
    settings.gain       = capture.get(CAP_PROP_GAIN);
    settings.saturation = capture.get(CAP_PROP_SATURATION);
    settings.cameraBackend = capture.getBackendName();
    return settings;
}
