/**
 * This file contains utilities that make it easy to share image data using a
 * shared memory segment.
 * 
 * TODO: Protect readers from reading partial frames.
 * Probably what we want is for readers to copy a frame (while protected by a
 * shared lock) before doing any processing on it. The boost interprocess
 * mutex will deadlock if a process holding one crashes, so we'd probably
 * want to use a linux-specific robust lock.
 * 
 */
#ifndef SHARED_MAT_H 
#define SHARED_MAT_H 1

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <opencv2/opencv.hpp>

typedef struct {
  cv::Size  size;
  int       type;
  int       frame;
  boost::interprocess::managed_shared_memory::handle_t handle;
} SharedMatHeader;

/**
 * RAII helper for working with shared memory. Deletes the shared memory
 * when created and when destroyed. An object that contains this object is
 * guaranteed to have exclusive control of the named shared memory object.
 * 
 * Not intended for public use.
 */
class ShmRemover {
  public:
    ShmRemover() {};
    ShmRemover(const char *name) : name(name) { boost::interprocess::shared_memory_object::remove(name); }
    ~ShmRemover() { boost::interprocess::shared_memory_object::remove(name); }
    void setName(const char *name) { boost::interprocess::shared_memory_object::remove(name); }

  private:
    const char* name;
};

/**
 * Reads a matrix out of a named shared memory segment. 
 */
class SharedMat {
  public:
    SharedMat(const char *name);

    // Waits indefinitely for a new frame to be ready.
    bool waitForFrame();
    cv::Mat mat;

    // No implict copy constructor.
    SharedMat(const SharedMat& rhs) = delete;
    SharedMat& operator=(const SharedMat& rhs) = delete;

  private:
    SharedMatHeader *header;
    boost::interprocess::managed_shared_memory segment;
};

/**
 * Helper class for writing frames to a named shared memory segment.
 */
class SharedMatWriter {
  public:
    SharedMatWriter();
    SharedMatWriter(const char *name, const cv::Mat &frame);

    void updateMemory(const char *name, const cv::Mat &frame);

    /**
     * Updates the shared frame. This increments the frame counts
     * and notifies any consumers that the new frame is ready.
     */
    bool updateFrame(cv::VideoCapture &capture);

    /**
     * Updates the shared frame as above, just with a direct Mat clone.
     */
    bool updateFrame(const cv::Mat &inputMat);

    cv::Mat mat;

    // No implict copy constructor.
    SharedMatWriter(const SharedMatWriter &rhs) = delete;
    SharedMatWriter& operator=(const SharedMatWriter &rhs) = delete;

  private:
    ShmRemover remover;
    SharedMatHeader *header;
    boost::interprocess::managed_shared_memory segment;

    void updateMemory(const cv::Mat &frame);
    void wakeAll();
};


#endif // #ifndef SHARED_MAT_H