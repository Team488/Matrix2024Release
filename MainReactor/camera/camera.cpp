#include "camera.h"

bool Camera::updateFrame(Mat &cameraFrame)
{
    // Unwarp and undistort
    // TODO: Implement this

    // Apply the filter set for this particular camera.
    Mat frameOut = applyFilters(cameraFrame);

    if (frameOut.empty())
    {
        std::cerr << "ERROR: Failed to update with empty frame." << std::endl;
        return false;
    }

    // Pipe it out!
    if (!isSharedMemInitialized)
    {
        shared.updateMemory(settings.name.c_str(), frameOut);
        isSharedMemInitialized = true;
    }
    else
    {
        shared.updateFrame(frameOut);
    }

    return true;
}

bool Camera::saveActiveSettings(const std::string &path)
{
    CameraSettings activeSettings = getActiveSettings();
    activeSettings.name = settings.name;
    activeSettings.source = settings.source;
    return activeSettings.save(path);
}
