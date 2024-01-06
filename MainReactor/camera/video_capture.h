#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H 1

#include <opencv2/opencv.hpp>
#include <opencv2/videoio/registry.hpp>
#include <shared_mat/shared_mat.h>
#include <string>
#include <vector>
#include "camera.h"
#include "camera_settings.h"
#include "runnable.h"

class VideoCaptureCameraFactory : public CameraFactory
{
    public:
        virtual void listCameras(std::vector<CameraInfo> &outCameraInfo) const;
        virtual bool constructCamera(const CameraSettings &settings, std::vector<std::shared_ptr<Runnable>> &outCameras);
};

/* A camera that receives data through any of OpenCV's support backends, e.g. a USB camera, an MJPEG stream, an RTSP stream.
 * Support can also be added for GStreamer format strings if needed. Support can also be added to identify a USB camera from its product name or serial number.
 *
 * The source is expected to be of one of the following formats:
 *   0 (device index)
 *   http://IP:PORT/PATH (MJPEG stream)
 *   rtsp://IP:PORT/PATH (RTSP stream)
 */
class VideoCaptureCamera : public Camera, public virtual Runnable
{
    private:
        VideoCapture capture;
        void applySettings(const CameraSettings &settings);
        virtual Mat applyFilters(Mat &inMat) {return inMat;}

    public:
        VideoCaptureCamera(int deviceIndex, cv::VideoCaptureAPIs backend, const CameraSettings &settings);
        VideoCaptureCamera(const std::string &url, cv::VideoCaptureAPIs backend, const CameraSettings &settings);
        virtual bool update();
        virtual CameraSettings getActiveSettings();
};

#endif //ifndef VIDEO_CAPTURE_H