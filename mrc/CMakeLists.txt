cmake_minimum_required(VERSION 3.1)
# Enable C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
SET(OpenCV_DIR ../opencv/installation/OpenCV-master/lib/cmake/opencv4)

project (mrc)
find_package (OpenCV 4 REQUIRED)
FIND_PACKAGE(Threads REQUIRED)
find_package(realsense2 REQUIRED)

set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME     OFF)
find_package(Boost 1.65.0 REQUIRED)

include_directories(../MainReactor ${OpenCV_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
add_library(shared_mat ../MainReactor/shared_mat/shared_mat.cpp)
add_library(camera ../MainReactor/camera/camera.cpp ../MainReactor/camera/video_capture.cpp) # TODO: clean this up so we don't need to redefine all the files from the external lib

add_executable (mrc mrc.cpp)
target_link_libraries(mrc shared_mat camera ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT} -lrt)