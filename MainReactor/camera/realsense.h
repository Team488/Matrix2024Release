#ifndef REALSENSE_H
#define REALSENSE_H 1

#include <vector>
#include <string>
#include "librealsense2/rs.hpp"
#include "camera.h"
#include "camera_settings.h"
#include "runnable.h"

/* A base class for Realsense cameras. Each Realsense device has multiple cameras of various types.
 * First create and set up the cameras, and then register them to a Realsense to run them.
 */
class RealsenseCamera : public Camera
{
    private:
        virtual Mat getFrameData(rs2::frameset &frameset) = 0;
        virtual Mat applyFilters(Mat &inMat) {return inMat;}

    public:
        virtual ~RealsenseCamera() = default;
        virtual bool update()
        {
            std::cerr << "ERROR: A RealsenseCamera cannot update on its own. Register it in a Realsense object and call update() on that." << std::endl;
            return false;
        }
        bool updateFrame(rs2::frameset &frameset)
        {
            Mat mat = getFrameData(frameset);
            return Camera::updateFrame(mat);
        }
};

/* A Realsense color camera. Create and set up this camera, and then register it to a Realsense to run it.
 * The source is expected to be of the format: RS/COLOR or RS/1234/COLOR
 */
class RealsenseColorCamera : public RealsenseCamera
{
    private:
        virtual Mat getFrameData(rs2::frameset &frameset);
    
    public:
        RealsenseColorCamera(const CameraSettings &settings);
        virtual CameraSettings getActiveSettings();
};

/* A Realsense infrared camera. Create and set up this camera, and then register it to a Realsense to run it.
 * The source is expected to be of the format: RS/IR or RS/1234/IR
 */
class RealsenseIRCamera : public RealsenseCamera
{
    private:
        virtual Mat getFrameData(rs2::frameset &frameset);
    
    public:
        RealsenseIRCamera(const CameraSettings &settings);
        virtual CameraSettings getActiveSettings();
};

/* A Realsense depth camera. Create and set up this camera, and then register it to a Realsense to run it.
 * The source is expected to be of the format: RS/DEPTH or RS/1234/DEPTH
 */
class RealsenseDepthCamera : public RealsenseCamera
{
    private:
        rs2::spatial_filter spat_filter;
        rs2::temporal_filter temp_filter;
        rs2::disparity_transform depth_to_disparity;
        rs2::disparity_transform disparity_to_depth;
        rs2::decimation_filter* decimation_filter;
        bool shouldDecimate;

        virtual Mat getFrameData(rs2::frameset &frameset);

    public:
        RealsenseDepthCamera(const CameraSettings &settings);
        virtual CameraSettings getActiveSettings();
};

/* A realsense device that has multiple cameras registered.
 * This is necessary because all RealsenseCameras from the same Realsense device must be updated together from the same frameset.
 * Thus you should set up multiple RealsenseCameras, then register them to a single Realsense.
 * This Realsense can then be started and updated as a single unit, and it will update each of its registered cameras.
 */
class Realsense : public virtual Runnable
{
    private:
        std::string serial;
        rs2::config config;
        rs2::pipeline pipe;
        std::vector<std::unique_ptr<RealsenseCamera>> cameras;
        std::unique_ptr<rs2::align> pAlignment;
        bool bAlign = false;
        bool hasColorAndDepth(const std::vector<rs2::stream_profile>& streams);
        float depthScale = -1.0;
        float getDepthScale(rs2::device dev);
        float colorExposure = -488.0;
        float whiteBalance = -488.0;

    public:
        Realsense(std::string serial) {this->serial = serial;}
        void registerCamera(std::unique_ptr<RealsenseCamera> camera);
        int getCameraCount() {return cameras.size();}
        const std::unique_ptr<RealsenseCamera> & getCamera(int index) {return cameras[index];}
        void alignToColor() {bAlign = true;} //one way door for configuration pre start
        virtual bool start();
        virtual bool update();
};

class RealsenseCameraFactory : public CameraFactory
{
    private:
        std::map<std::string, std::shared_ptr<Realsense>> realsenseDevices;

    public:
        virtual void listCameras(std::vector<CameraInfo> &outCameraInfo) const;
        virtual bool constructCamera(const CameraSettings &settings, std::vector<std::shared_ptr<Runnable>> &outCameras);
};

#endif //ifndef REALSENSE_H