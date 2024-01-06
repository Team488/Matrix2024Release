#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <shared_mat/shared_mat.h>

int main(int argc, const char **argv)
{
    // Accept shared matrix name as argument.
    std::string name;
    if (argc == 1)
    {
        std::cout << "Using default camera feed: COLOR_0" << std::endl;
        name = "COLOR_0";
    }
    else
    {
        std::cout << "Using camera feed: " << argv[1] << std::endl;
        name = argv[1];
    }

    // Subscribe to the shared matrix and display its frames.
    SharedMat sharedMat(name.c_str());
    while (1)
    {
        sharedMat.waitForFrame();
        cv::imshow(name, sharedMat.mat);
        
        if (cv::waitKey(1) == 27)
            break;
    }
}