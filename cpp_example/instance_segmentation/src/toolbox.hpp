#ifndef _HAILO_COMMON_TOOLBOX_HPP_
#define _HAILO_COMMON_TOOLBOX_HPP_

#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <future>
#include <filesystem>
#include <opencv2/highgui.hpp>
#include <opencv2/core/matx.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp> 
#include "hailo/hailort.h"
#include "hailo/infer_model.hpp" 
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "hailo_infer.hpp"




// -------------------------- COLOR MACROS --------------------------
#define RESET         "\033[0m"
#define MAGENTA       "\033[35m"
#define BOLDGREEN     "\033[1m\033[32m"
#define BOLDBLUE      "\033[1m\033[34m"
#define BOLDMAGENTA   "\033[1m\033[35m"

extern std::vector<cv::Scalar> COLORS;
namespace fs = std::filesystem;
namespace hailo_utils {
    
struct InferenceResult {
    cv::Mat org_frame;
    std::vector<std::pair<uint8_t*, hailo_vstream_info_t>> output_data_and_infos;
    std::vector<std::shared_ptr<uint8_t>> output_guards;
};


struct CommandLineArgs {
    std::string detection_hef;
    std::string input_path;
    bool save;
    std::string batch_size;
};
// Callback types for task-specific processing
using PreprocessCallback = std::function<void(const std::vector<cv::Mat>&, std::vector<cv::Mat>&, uint32_t, uint32_t)>;
using PostprocessCallback = std::function<void(cv::Mat&, const std::vector<std::pair<uint8_t*, hailo_vstream_info_t>>&)>;

// Status/Error
hailo_status check_status(const hailo_status &status, const std::string &message);
hailo_status wait_and_check_threads(std::future<hailo_status> &f1, const std::string &name1,
                                    std::future<hailo_status> &f2, const std::string &name2,
                                    std::future<hailo_status> &f3, const std::string &name3);

// File / path
std::string get_hef_name(const std::string &path);

void init_video_capture(const std::string &input_path, cv::VideoCapture &capture,
    double &org_height, double &org_width, size_t &frame_count, size_t batch_size);

// CLI
std::string getCmdOption(int argc, char *argv[], const std::string &option);
bool has_flag(int argc, char *argv[], const std::string &flag);
CommandLineArgs parse_command_line_arguments(int argc, char **argv);

// ─────────────────────────────────────────────────────────────────────────────
// DISPLAY & VIDEO
// ─────────────────────────────────────────────────────────────────────────────

void print_net_banner(const std::string &detection_model_name,
                      const std::vector<hailort::InferModel::InferStream> &inputs,
                      const std::vector<hailort::InferModel::InferStream> &outputs);

cv::VideoCapture open_video_capture(const std::string &input_path, cv::VideoCapture capture,
                                    double &org_height, double &org_width, size_t &rame_count);
bool show_frame(const cv::Mat &frame_to_draw);

template<typename T>
class BoundedTSQueue {
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond_not_empty;
    std::condition_variable m_cond_not_full;
    const size_t m_max_size;
    bool m_stopped;

public:
    explicit BoundedTSQueue(size_t max_size) : m_max_size(max_size), m_stopped(false) {}
    ~BoundedTSQueue() { stop(); }

    BoundedTSQueue(const BoundedTSQueue&) = delete;
    BoundedTSQueue& operator=(const BoundedTSQueue&) = delete;

std::size_t size() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }
    void push(T item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_not_full.wait(lock, [this] { return m_queue.size() < m_max_size || m_stopped; });
        if (m_stopped) return;

        m_queue.push(std::move(item));
        m_cond_not_empty.notify_one();
    }

    bool pop(T &out_item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_not_empty.wait(lock, [this] { return !m_queue.empty() || m_stopped; });
        if (m_stopped && m_queue.empty()) {
            return false;
        }

        out_item = std::move(m_queue.front());
        m_queue.pop();
        m_cond_not_full.notify_one();
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopped = true;
        }
        m_cond_not_empty.notify_all();
        m_cond_not_full.notify_all();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// GENERIC PRE/POST PROCESSING FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────────

// Generic preprocessing functions that accept callbacks
void preprocess_video_frames(cv::VideoCapture &capture,
                          uint32_t width, uint32_t height, size_t batch_size,
                          std::shared_ptr<BoundedTSQueue<std::pair<std::vector<cv::Mat>, std::vector<cv::Mat>>>> preprocessed_batch_queue,
                          PreprocessCallback preprocess_callback);

// Generic postprocessing function that accepts callback
hailo_status run_post_process(
    int org_height,
    int org_width,
    size_t frame_count,
    cv::VideoCapture &capture,
    double fps,
    size_t batch_size,
    std::shared_ptr<BoundedTSQueue<InferenceResult>> results_queue,
    PostprocessCallback postprocess_callback);

// Generic inference function
hailo_status run_inference_async(HailoInfer& model,
                            std::chrono::duration<double>& inference_time,
                            std::shared_ptr<BoundedTSQueue<std::pair<std::vector<cv::Mat>, std::vector<cv::Mat>>>> preprocessed_batch_queue,
                            std::shared_ptr<BoundedTSQueue<InferenceResult>> results_queue);

// Generic preprocessing runner that accepts callback
hailo_status run_preprocess(const std::string& input_path, const std::string& hef_path, HailoInfer &model, 
                            cv::VideoCapture &capture,
                            size_t batch_size,
                            std::shared_ptr<BoundedTSQueue<std::pair<std::vector<cv::Mat>, std::vector<cv::Mat>>>> preprocessed_batch_queue,
                            PreprocessCallback preprocess_callback);

// Resource management
void release_resources(cv::VideoCapture &capture, cv::VideoWriter &video,
                      std::shared_ptr<BoundedTSQueue<std::pair<std::vector<cv::Mat>, std::vector<cv::Mat>>>> preprocessed_batch_queue,
                      std::shared_ptr<BoundedTSQueue<InferenceResult>> results_queue);

} // namespace hailo_utils

#endif
