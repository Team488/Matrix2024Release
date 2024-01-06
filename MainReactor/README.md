Main Reactor
============

![Now with 100% less design flaws!](https://i.stack.imgur.com/PjA4r.jpg)

[Based on the great sharecam example!](https://github.com/meirm/sharecam/tree/master/version4)

Some day soon, we'll have docs in this spot!

Shared Mat
==========

SharedMat.h is a utility class for sharing video streams efficiently
between processes. See `shared_reader.cpp` and `shared_writer.cpp`
for usage examples.

To try it out, start `shared_source` to capture the video stream
to a shared buffer. Then start as many `shared_reader`s as you'd
like. Each one will display the same video stream. `shared_reader`
takes the name of a filter to apply to the video stream - the only
options are 'blur' and 'canny', like so `./shared_reader blur`.

### How to setup python
1. Setup python and opencv
1. `pip install .`
1. Check out `sample_consumer.py` for info on how to use this

### Known issues
If a reader process crashes while holding the mutex, the source will deadlock.

### Setup

### macOS

1. Install [Homebrew](https://brew.sh)
1. `brew install pybind11`
1. `brew install automake`
1. `brew install cmake`
1. `brew install opencv`
1. `brew install cppzmq`
1. `brew install libusb`
1. Install [vcpkg](https://github.com/Microsoft/vcpkg)
1. `./bootstrap-vcpkg.sh`
1. `sudo ./vcpkg integrate install`
1. Install the [RealSense SDK](https://github.com/IntelRealSense/librealsense#building-librealsense---using-vcpkg)
  1. `./vcpkg install realsense2`
  1. A line is output similar to the following
    1. `CMake projects should use: "-DCMAKE_TOOLCHAIN_FILE=/Users/<user-name>/source/vcpkg/scripts/buildsystems/vcpkg.cmake"`
    1. You can add an environment variable like VCKPG_PATH to [your environment variables](https://apple.stackexchange.com/questions/356441/how-to-add-permanent-environment-variable-in-zsh)
1. Clone Main Reactor, go into the project directory
1. `mkdir build`
1. `cd build`
1. `cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_PATH`
