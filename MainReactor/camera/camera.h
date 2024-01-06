#ifndef CAMERA_H
#define CAMERA_H 1

#include <opencv2/opencv.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <stdio.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <streambuf>
#include <shared_mat/shared_mat.h>
#include <utils/picojson.h>
#include "camera_settings.h"
#include "runnable.h"

using namespace picojson;
using namespace cv;

struct CameraInfo {
    std::string source;
    std::string description;
};

class CameraFactory {
    public:
        virtual void listCameras(std::vector<CameraInfo> &outCameraInfo) const {}
        virtual bool constructCamera(const CameraSettings &settings, std::vector<std::shared_ptr<Runnable>> &outCameras) = 0;
};

/* A camera built on top of SharedMat. To use it, create a new class that extends Camera with the filters you want and the necessary logic
 * for starting the camera and updating each frame. A shared matrix is published using settings.name as the shared memory name.
 *
 * TODO: Add a means for Cameras to tell their recipients what type of data they should expect
 */
class Camera {
    private:
        Mat cameraMatrix;
        Mat cameraDistortion;
        Mat cameraExtrinsics; //Camera Frame from Robot Frame
        double camSensorH, camSensorV;
        int resolutionH, resolutionV;
        double lensFocalLength;
        CameraSettings settings;
        virtual Mat applyFilters(Mat &inMat) = 0;
        SharedMatWriter shared; // undistored and filtered output
        bool isSharedMemInitialized;

    public:
        // Setters
        // TODO: get these from config
        void setSensorDimensions (double H, double V) {camSensorH = H; camSensorV = V;}
        void setCameraResolution (int H, int V) {resolutionH = H; resolutionV = V;}
        void setLensFocalLength (double lensFL) {lensFocalLength = lensFL;}

        // Getters
        const CameraSettings & getSettings() const {return settings;}
        double getCameraRatio() const {return resolutionH/resolutionV;}
        double getHorizontalView() const {return 2*atan2(camSensorH, (2*lensFocalLength));}
        double getVerticalView() const {return 2*atan2(camSensorV, (2*lensFocalLength));}

        // Config
        bool saveActiveSettings(const std::string &path);
        virtual CameraSettings getActiveSettings() = 0;
        
        // Lifecycle
        //bool loadCameraSettings (VideoCapture &capture, const std::string& path);
        //Saver
        //bool saveCameraSettings (const std::string& path, CameraSettings settings);
        //bool saveCameraSettings (const std::string& path, const VideoCapture& cap, const std::string& camType);
        //CameraSettings getCurrentSettings(const VideoCapture& cap, const std::string& type);
        //Operations
        //bool setup(VideoCapture &capture, Mat &cameraFrame, std::string path = "../config.json");
        virtual ~Camera() = default;
        virtual bool setup(const CameraSettings &newSettings) {settings = newSettings; return true;}
        bool updateFrame(Mat &inMat);
};

#endif //ifndef CAMERA_H