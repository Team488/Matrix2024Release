#include <string>
#include <thread>
#include <vector>
#include <boost/dll.hpp>
#include <boost/range/iterator_range.hpp>
#include <camera/camera_settings.h>
#include <camera/camera.h>
#include <camera/realsense.h>
#include <camera/runnable.h>
#include <camera/video_capture.h>
#include <camera/zmq_camera.h>

boost::filesystem::path CONFIG_PATH = boost::dll::program_location().parent_path().parent_path() / "config";

CameraFactory* FACTORIES[] = {
    new RealsenseCameraFactory(),
    new ZmqCameraFactory(),
    new VideoCaptureCameraFactory(),
};

void listConfigs()
{
    std::vector<boost::filesystem::path> configPaths;
    for (auto &entry : boost::make_iterator_range(boost::filesystem::directory_iterator(CONFIG_PATH), {}))
    {
        configPaths.push_back(entry.path());
    }
    std::sort(configPaths.begin(), configPaths.end());

    std::cout
        << std::endl
        << "+========================================================+" << std::endl
        << "|                     Available Configs                  |" << std::endl
        << "+=====================+=================+================+" << std::endl
        << "| Config              | Default Name    | Default Source  " << std::endl
        << "+---------------------+-----------------+-----------------" << std::endl;

    for (auto &configPath : configPaths)
    {
        std::string configName = configPath.stem().string();
        CameraSettings config;
        config.load(configPath.string());
        std::cout
            << "| "
            << configName
            << std::string(std::max(20 - (int)configName.size(), 1), ' ')
            << "| "
            << config.name
            << std::string(std::max(16 - (int)config.name.size(), 1), ' ')
            << "| "
            << config.source
            << std::endl;
    }

    std::cout
        << "+---------------------+-----------------+-----------------" << std::endl
        << std::endl;
}

void listCameras()
{
    std::vector<CameraInfo> cameraInfos;
    for (auto &factory : FACTORIES)
    {
        factory->listCameras(cameraInfos);
    }

    std::cout
        << std::endl
        << "+========================================================+" << std::endl
        << "|                     Available Cameras                  |" << std::endl
        << "+==========================+=============================+" << std::endl
        << "| Source                   | Description                  " << std::endl
        << "+--------------------------+------------------------------" << std::endl;

    for (auto &cameraInfo : cameraInfos)
    {
        std::cout
            << "| "
            << cameraInfo.source
            << std::string(std::max(25 - (int)cameraInfo.source.size(), 1), ' ')
            << "| "
            << cameraInfo.description
            << std::endl;
    }

    std::cout
        << "| (+ any network cameras)  |                              " << std::endl
        << "+--------------------------+------------------------------" << std::endl
        << std::endl;
}

void runCamera(const std::shared_ptr<Runnable> &camera)
{
    camera->start();
    while (1)
    {
        camera->update();
    }
}

int main(int argc, const char **argv)
{
    if (argc == 1 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
    {
        std::cout << std::endl
            << "MainReactor is a camera feed ingestor, accomplishing 2 goals:" << std::endl
            << std::endl
            << "  1. The camera frames are published to shared memory," << std::endl
            << "     allowing for multiple consumers to use each feed in real time," << std::endl
            << "     as opposed to a single consumer taking exclusive ownership over a device."
            << std::endl
            << "  2. The source and settings of each camera are abstracted from its usage," << std::endl
            << "     so MainReactor knows where each feed is coming from, but consumers only care how they're used." << std::endl
            << std::endl
            << std::endl
            << "Each config is loaded from the config folder, and each argument can specify" << std::endl
            << "an override for the config's name or source, so that camera feeds can be used" << std::endl
            << "for different purposes or sourced from different locations." << std::endl
            << "The name, whether from the config or from a command-line override, is the same name" << std::endl
            << "that a consumer must use to retrieve the frame from shared memory." << std::endl
            << std::endl
            << std::endl
            << "Usage: ./main_reactor config[:name][@source] ..." << std::endl
            << std::endl
            << "Sample Usage:" << std::endl
            << std::endl
            << "  ./main_reactor \\" << std::endl
            << "      rs_d435_color \\" << std::endl
            << "      rs_d435_depth \\" << std::endl
            << "      webcam:COLOR_0@https://192.168.0.14:8080/mjpeg \\" << std::endl
            << "      robot_front@tcp://192.168.0.14:10031 \\" << std::endl
            << "      webcam:COLOR_2@0" << std::endl
            << "      mac:COLOR_0" << std::endl
            << std::endl;
        listConfigs();
        listCameras();
        return 0;
    }

    // Assemble list of cameras, grouping realsense cameras by source.
    std::vector<std::shared_ptr<Runnable>> cameras;
    std::map<std::string, std::shared_ptr<Realsense>> realsenses;
    for (int i = 1; i < argc; ++i)
    {
        // Extract an input of the format "config[:name][@source]"
        std::cout << "Processing input: " << argv[i] << std::endl;
        const std::string input(argv[i]);
        std::string config = input, nameOverride = "", sourceOverride = "";
        int sourceDelimiterIndex = input.find('@');
        if (sourceDelimiterIndex != std::string::npos)
        {
            sourceOverride = input.substr(sourceDelimiterIndex + 1);
            config = input.substr(0, sourceDelimiterIndex);
            int nameDelimiterIndex = config.find(':');
            if (nameDelimiterIndex != std::string::npos)
            {
                nameOverride = config.substr(nameDelimiterIndex + 1);
                config = config.substr(0, nameDelimiterIndex);
            }
        }

        // Load the config.
        CameraSettings cameraSettings;
        boost::filesystem::path configPath = CONFIG_PATH / (config + ".json");
        std::cout << "  Config: " << configPath << std::endl;
        cameraSettings.load(configPath.string());
        if (!nameOverride.empty())
        {
            cameraSettings.name = nameOverride;
        }
        if (!sourceOverride.empty())
        {
            cameraSettings.source = sourceOverride;
        }

        // Create the camera.
        bool isCreated = false;
        for (auto &factory : FACTORIES)
        {
            isCreated = factory->constructCamera(cameraSettings, cameras);
            if (isCreated)
                break;
        }

        if (!isCreated)
        {
            std::cerr << "ERROR: Failed to create camera with input: " << input << std::endl;
            return 1;
        }
    }

    // Spawn camera threads.
    std::vector<std::thread> cameraThreads;
    for (const std::shared_ptr<Runnable> &camera : cameras)
    {
        cameraThreads.push_back(std::thread(runCamera, camera));
    }

    // Wait for all threads.
    for (std::thread &cameraThread : cameraThreads)
    {
        cameraThread.join();
    }
}
