cmake_minimum_required(VERSION 3.1)
# If statement policy https://cmake.org/cmake/help/latest/policy/CMP0057.html
cmake_policy(SET CMP0057 NEW)

# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

project(MainReactor)

find_package(OpenCV 4 REQUIRED)
find_package(realsense2 REQUIRED)
find_package(Threads REQUIRED)

# https://cmake.org/cmake/help/latest/module/FindPython.html
find_package(Python COMPONENTS Interpreter Development)
# https://pybind11.readthedocs.io/en/stable/cmake/index.html
find_package(pybind11 CONFIG)

set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME     OFF)
find_package(Boost 1.65.0 REQUIRED COMPONENTS date_time filesystem)

include_directories(./ ${Boost_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})

# shared_mat
add_library(shared_mat SHARED shared_mat/shared_mat.cpp)
target_link_libraries(shared_mat ${Boost_LIBRARIES} ${OpenCV_LIBS})
if (UNIX AND NOT APPLE)
    # POSIX Real Time Extensions for Linux are required to build shared_mat
    target_link_libraries(shared_mat -lrt)
endif()

# camera
if (APPLE)
    # Add location of libusb libraries, required for realsense, and required linking option for Security
    # framework for some usb functions
    link_directories(/opt/homebrew/lib)
    add_link_options(-framework Security)
endif()
add_library(camera camera/camera.cpp camera/realsense.cpp camera/video_capture.cpp camera/zmq_camera.cpp)
target_link_libraries(camera shared_mat realsense2::realsense2 -ldl -lzmq ${Boost_LIBRARIES} ${OpenCV_LIBS})

# main_reactor
add_executable(main_reactor main_reactor.cpp)
target_link_libraries(main_reactor camera)

# sample_consumer
add_executable(sample_consumer sample_consumer.cpp)
target_link_libraries(sample_consumer shared_mat)
# only try to create python bindings if we have python and pybind
if (Python_FOUND AND pybind11_FOUND)
    # hardcode path to where the MainReactor package will be installed inside the
    # _MainReactor python library. Otherwise, python will not be able to find
    # shared_mat at runtime and fail to import MainReactor with a linker error
    SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    if (APPLE)
        # This RPATH is used within conda and venv, which is our virtual env solution on Macs.
        # Otherwise on a Mac you'd usually use "@loader_path"
        SET(CMAKE_INSTALL_RPATH "${Python_SITEARCH}/MainReactor")
    else()
        # This is the standard RPATH to use on Linux, resolving to the library's directory
        # at runtime.
        SET(CMAKE_INSTALL_RPATH "$ORIGIN/")
    endif()
    pybind11_add_module(_MainReactor MODULE python/MainReactor_python.cpp)
    target_link_libraries(_MainReactor PUBLIC shared_mat ${OpenCV_LIBS})
    install(TARGETS _MainReactor shared_mat DESTINATION MainReactor)
else()
    message(WARNING "Couldn't find python and pybind, skipping generating python modules")
endif()



# add_executable (videosend videosend.cpp)
# target_link_libraries(videosend ${OpenCV_LIBS})
# add_executable (videorec videorec.cpp)
# target_link_libraries(videorec ${OpenCV_LIBS})

# # Shared memory demos
# add_executable (shared_reader shared_reader.cpp)
# target_link_libraries(shared_reader shared_mat ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

# add_executable (shared_source shared_source.cpp)
# target_link_libraries(shared_source shared_mat ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

# #Camera demos
# add_executable (shared_camera_source shared_camera_source.cpp)
# target_link_libraries(shared_camera_source shared_mat camera ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

# add_executable (shared_camera_reader shared_camera_reader.cpp)
# target_link_libraries(shared_camera_reader shared_mat camera ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

# if (WIN32)
# else()
#     target_link_libraries(shared_reader -lrt)
#     target_link_libraries(shared_source -lrt)
#     target_link_libraries(shared_camera_source -lrt)
#     target_link_libraries(shared_camera_reader -lrt)
# endif()