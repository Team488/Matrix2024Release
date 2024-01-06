#ifndef ZMQ_CAMERA_H
#define ZMQ_CAMERA_H 1

#include <opencv2/opencv.hpp>
#include <shared_mat/shared_mat.h>
#include <string>
#include <vector>
#include <zmq.hpp>
#include "camera.h"
#include "camera_settings.h"
#include "runnable.h"

class ZmqCameraFactory : public CameraFactory
{
    public:
        virtual bool constructCamera(const CameraSettings &settings, std::vector<std::shared_ptr<Runnable>> &outCameras);
};

/* A camera feed that receives frames via ZMQ.
 * The source is expected to be of the format: tcp://IP:PORT/TOPIC
 * The data received is expected to be: [ topic, height, width, bit_depth, channel_count, image_data ]
 */
class ZmqCamera : public Camera, public virtual Runnable
{
    private:
        zmq::context_t context;
        zmq::socket_t socket;
        std::array<zmq::message_t, 6> messages;
        virtual Mat applyFilters(Mat &inMat) {return inMat;}

    public:
        ZmqCamera(const std::string &address, const std::string &topic, const CameraSettings &settings);
        ~ZmqCamera();
        virtual bool update();
        virtual CameraSettings getActiveSettings();
};

#endif //ifndef ZMQ_CAMERA_H