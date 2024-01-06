#include <pybind11/pybind11.h>
#include <opencv2/opencv.hpp>
#include <shared_mat/shared_mat.h>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)


namespace py = pybind11;

std::string determine_np_dtype(int depth)
{
    switch (depth) {
    case CV_8U: return py::format_descriptor<uint8_t>::format();
    case CV_8S: return py::format_descriptor<int8_t>::format();
    case CV_16U: return py::format_descriptor<uint16_t>::format();
    case CV_16S: return py::format_descriptor<int16_t>::format();
    case CV_32S: return py::format_descriptor<int32_t>::format();
    case CV_32F: return py::format_descriptor<float>::format();
    case CV_64F: return py::format_descriptor<double>::format();
    default:
        throw std::invalid_argument("Unsupported data type.");
    }
}

std::vector<std::size_t> determine_shape(cv::Mat& m)
{
    if (m.channels() == 1) {
        return {
            static_cast<size_t>(m.rows)
            , static_cast<size_t>(m.cols)
        };
    }

    return {
        static_cast<size_t>(m.rows)
        , static_cast<size_t>(m.cols)
        , static_cast<size_t>(m.channels())
    };
}

std::vector<std::size_t> deterimine_strides(cv::Mat& m)
{
    if (m.channels() == 1) {
        return {
            static_cast<size_t>(m.cols * m.elemSize1()),
            static_cast<size_t>(m.elemSize1())
        };
    }

    return {
        static_cast<size_t>(m.elemSize1() * m.channels() * m.cols),
        static_cast<size_t>(m.elemSize1() * m.channels()),
        static_cast<size_t>(m.elemSize1())
    };
}

PYBIND11_MODULE(_MainReactor, m) {
    m.doc() = "MainReactor"; // optional module docstring

    py::class_<cv::Mat>(m, "Mat", py::buffer_protocol())
        .def_buffer([](cv::Mat& im) -> pybind11::buffer_info {
            std::string format = determine_np_dtype(im.depth());
            std::vector<std::size_t> shape = determine_shape(im);
            std::vector<std::size_t> strides = deterimine_strides(im);

            return pybind11::buffer_info(
                // Pointer to buffer
                im.data,
                // Size of one scalar
                im.elemSize1(),
                // Python struct-style format descriptor
                format,
                // Number of dimensions.
                shape.size(),
                // Buffer dimensions
                shape,
                // Strides (in bytes) for each index
                strides
            );
        });

    py::class_<SharedMat>(m, "SharedMat")
        .def(py::init<const char *>())
        .def("waitForFrame", &SharedMat::waitForFrame)
        .def_readonly("mat", &SharedMat::mat);


    #ifdef VERSION_INFO
        m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
    #else
        m.attr("__version__") = "dev";
    #endif
}
