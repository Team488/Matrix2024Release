#include <zmq_addon.hpp>
#include "zmq_camera.h"

//////////////////////
// ZmqCameraFactory //
//////////////////////

bool ZmqCameraFactory::constructCamera(const CameraSettings &settings, std::vector<std::shared_ptr<Runnable>> &outCameras)
{
    if (!settings.source.starts_with("tcp://"))
    {
        return false;
    }

    if (std::count(settings.source.begin(), settings.source.end(), '/') < 3)
    {
        std::cerr << "ERROR: ZmqCamera source is missing topic: " << settings.source << std::endl;
        throw 1;
    }

    // Extract the address and the topic name from the source.
    // E.g. tcp://192.168.0.1:10012/camera
    //      address: tcp://192.168.0.1:10012
    //      topic: camera
    int pathDelimiterIndex = settings.source.rfind('/');
    std::string address = settings.source.substr(0, pathDelimiterIndex);
    std::string topic = settings.source.substr(pathDelimiterIndex + 1);

    outCameras.push_back(std::make_shared<ZmqCamera>(address, topic, settings));
    return true;
}

///////////////
// ZmqCamera //
///////////////

ZmqCamera::ZmqCamera(const std::string &address, const std::string &topic, const CameraSettings &settings)
{
    std::cout << "Starting ZmqCamera with address '" << address << "' and topic '" << topic << "'" << std::endl;
    socket = zmq::socket_t(context, ZMQ_SUB);
    socket.connect(address);
    socket.setsockopt(ZMQ_SUBSCRIBE, topic.c_str(), topic.size());
    if (!socket.connected())
    {
        // TODO: Fix this connected check. The check never fails, even with an incorrect IP or port.
        std::cerr << "ERROR: Failed to open ZmqCamera with source: " << settings.source << std::endl;
        throw 1;
    }

    Camera::setup(settings);
}

ZmqCamera::~ZmqCamera()
{
    socket.close();
}

bool ZmqCamera::update()
{
    zmq::recv_result_t result = zmq::recv_multipart_n(socket, messages.data(), messages.size());
    if (result.has_value() && result.value() == messages.size())
    {
        // TODO: do these leak memory?
        int dtype = CV_MAKETYPE(*(uint8_t *)messages[3].data(), *(uint8_t *)messages[4].data());
        Mat frame(
            *(uint32_t *)messages[1].data(),
            *(uint32_t *)messages[2].data(),
            dtype,
            messages[5].data());

        updateFrame(frame);
        return true;
    }

    return false;
}

CameraSettings ZmqCamera::getActiveSettings()
{
    CameraSettings settings;
    return settings;
}
