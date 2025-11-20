#include "toolbox.hpp"
#include "hailo_infer.hpp"
namespace hailo_utils {
hailo_status check_status(const hailo_status &status, const std::string &message) {
    if (HAILO_SUCCESS != status) {
        std::cerr << message << " with status " << status << std::endl;
        return status;
    }
    return HAILO_SUCCESS;
}

hailo_status wait_and_check_threads(
    std::future<hailo_status> &f1, const std::string &name1,
    std::future<hailo_status> &f2, const std::string &name2,
    std::future<hailo_status> &f3, const std::string &name3)
{
    hailo_status status = f1.get();
    if (HAILO_SUCCESS != status) {
        std::cerr << name1 << " failed with status " << status << std::endl;
        return status;
    }

    status = f2.get();
    if (HAILO_SUCCESS != status) {
        std::cerr << name2 << " failed with status " << status << std::endl;
        return status;
    }

    status = f3.get();
    if (HAILO_SUCCESS != status) {
        std::cerr << name3 << " failed with status " << status << std::endl;
        return status;
    }

    return HAILO_SUCCESS;
}
std::string get_hef_name(const std::string &path)
{
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}


std::string getCmdOption(int argc, char *argv[], const std::string &option)
{
    std::string cmd;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (0 == arg.find(option, 0)) {
            std::size_t found = arg.find("=", 0) + 1;
            cmd = arg.substr(found);
            return cmd;
        }
    }
    return cmd;
}

bool has_flag(int argc, char *argv[], const std::string &flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == flag) {
            return true;
        }
    }
    return false;
}
std::string getCmdOptionWithShortFlag(int argc, char *argv[], const std::string &longOption, const std::string &shortOption) {
    std::string cmd;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == longOption || arg == shortOption) {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                cmd = argv[i + 1];
                return cmd;
            }
        }
    }
    return cmd;
}

CommandLineArgs parse_command_line_arguments(int argc, char** argv) {
    return {
        getCmdOptionWithShortFlag(argc, argv, "--net", "-n"),
        getCmdOptionWithShortFlag(argc, argv, "--input", "-i"),
        has_flag(argc, argv, "-s"),
        (getCmdOptionWithShortFlag(argc, argv, "--batch_size", "-b").empty() ? "1" : getCmdOptionWithShortFlag(argc, argv, "--batch_size", "-b"))
    };
}
void init_video_capture(const std::string& input_path, cv::VideoCapture &capture,
                               double &org_height, double &org_width, size_t &frame_count, size_t batch_size) {

        std::cout << "Opening camera" << std::endl;
        capture = open_video_capture(input_path, std::ref(capture), org_height, org_width, frame_count);
}

void print_net_banner(const std::string &detection_model_name,
    const std::vector<hailort::InferModel::InferStream> &inputs,
    const std::vector<hailort::InferModel::InferStream> &outputs)
{
    std::cout << BOLDMAGENTA << "-I-----------------------------------------------" << std::endl << RESET;
    std::cout << BOLDMAGENTA << "-I-  Network Name                               " << std::endl << RESET;
    std::cout << BOLDMAGENTA << "-I-----------------------------------------------" << std::endl << RESET;
    std::cout << BOLDMAGENTA << "-I   " << detection_model_name << std::endl << RESET;
    std::cout << BOLDMAGENTA << "-I-----------------------------------------------" << std::endl << RESET;
    for (auto &input : inputs) {
        auto shape = input.shape();
        std::cout << MAGENTA << "-I-  Input: " << input.name()
        << ", Shape: (" << shape.height << ", " << shape.width << ", " << shape.features << ")"
        << std::endl << RESET;
    }
    std::cout << BOLDMAGENTA << "-I-----------------------------------------------" << std::endl << RESET;
    for (auto &output : outputs) {
        auto shape = output.shape();
        std::cout << MAGENTA << "-I-  Output: " << output.name()
        << ", Shape: (" << shape.height << ", " << shape.width << ", " << shape.features << ")"
        << std::endl << RESET;
    }
    std::cout << BOLDMAGENTA << "-I-----------------------------------------------\n" << std::endl << RESET;
}

cv::VideoCapture open_video_capture(const std::string &input_path, cv::VideoCapture capture,
    double &org_height, double &org_width, size_t &frame_count) {
    capture.open(input_path, cv::CAP_ANY); 
    if (!capture.isOpened()) {
        throw std::runtime_error("Unable to read input file");
    }
    org_height = capture.get(cv::CAP_PROP_FRAME_HEIGHT);
    org_width = capture.get(cv::CAP_PROP_FRAME_WIDTH);
    frame_count = capture.get(cv::CAP_PROP_FRAME_COUNT);
    return capture;
}

bool show_frame(const cv::Mat &frame_to_draw)
{
    cv::imshow("Inference", frame_to_draw);

    if (cv::waitKey(1) == 'q') {
        std::cout << "Exiting" << std::endl;
        return false;
        }
    return true;
}

void preprocess_video_frames(cv::VideoCapture &capture,
                          uint32_t width, uint32_t height, size_t batch_size,
                          std::shared_ptr<BoundedTSQueue<std::pair<std::vector<cv::Mat>, std::vector<cv::Mat>>>> preprocessed_batch_queue,
                          PreprocessCallback preprocess_callback) {
    std::vector<cv::Mat> org_frames;
    std::vector<cv::Mat> preprocessed_frames;
    while (true) {
        cv::Mat org_frame;
        capture >> org_frame;
        if (org_frame.empty()) {
            preprocessed_batch_queue->stop();
            break;
        }
        org_frames.push_back(org_frame);
        
        if (org_frames.size() == batch_size) {
            preprocessed_frames.clear();
            preprocess_callback(org_frames, preprocessed_frames, width, height);
            preprocessed_batch_queue->push(std::make_pair(org_frames, preprocessed_frames));
            org_frames.clear();
        }
    }
}

hailo_status run_post_process(
    int org_height,
    int org_width,
    size_t frame_count,
    cv::VideoCapture &capture,
    double fps,
    size_t batch_size,
    std::shared_ptr<BoundedTSQueue<InferenceResult>> results_queue,
    PostprocessCallback postprocess_callback) {
    
    cv::VideoWriter video;

    int i = 0;
    while (true) {
        InferenceResult output_item;

        if (!results_queue->pop(output_item)) {
            break;
        }
        auto& frame_to_draw = output_item.org_frame;

        if (!output_item.output_data_and_infos.empty() && postprocess_callback) {
            postprocess_callback(frame_to_draw, output_item.output_data_and_infos);
        }
        
        video.write(frame_to_draw);
        
        if (!show_frame(frame_to_draw)) {
            break; // break the loop if input is from camera and user pressed 'q' 
        }
        i++;
    }
    release_resources(capture, video, nullptr, results_queue);
    return HAILO_SUCCESS;
}

hailo_status run_inference_async(HailoInfer& model,
                            std::chrono::duration<double>& inference_time,
                            std::shared_ptr<BoundedTSQueue<std::pair<std::vector<cv::Mat>, std::vector<cv::Mat>>>> preprocessed_batch_queue,
                            std::shared_ptr<BoundedTSQueue<InferenceResult>> results_queue) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    bool jobs_submitted = false;

    std::cerr << "inference: "<<  results_queue->size() << std::endl;
    while (true) {
        std::pair<std::vector<cv::Mat>, std::vector<cv::Mat>> preprocessed_frame_items;
        if (!preprocessed_batch_queue->pop(preprocessed_frame_items)) {
            break;
        }
        // Pass as parameters the device input and a lambda that captures the original frame and uses the provided output buffers.
        model.infer(
                    preprocessed_frame_items.second,
                    [org_frames = preprocessed_frame_items.first, queue = results_queue](
                        const hailort::AsyncInferCompletionInfo &info,
                        const std::vector<std::pair<uint8_t*, hailo_vstream_info_t>> &output_data_and_infos,
                        const std::vector<std::shared_ptr<uint8_t>> &output_guards)
                    {
                        for (size_t i = 0; i < org_frames.size(); ++i) {
                            InferenceResult output_item;
                            output_item.org_frame = org_frames[i];
                            output_item.output_guards.push_back(output_guards[i]);
                            output_item.output_data_and_infos.push_back(output_data_and_infos[i]);
                            queue->push(output_item);
                        }
                    });
        jobs_submitted = true;
    }
    if(jobs_submitted){
        model.wait_for_last_job();
    }
    results_queue->stop();
    auto end_time = std::chrono::high_resolution_clock::now();

    inference_time = end_time - start_time;

    return HAILO_SUCCESS;
}

hailo_status run_preprocess(const std::string& input_path, const std::string& hef_path, HailoInfer &model, 
                            cv::VideoCapture &capture, size_t batch_size,
                            std::shared_ptr<BoundedTSQueue<std::pair<std::vector<cv::Mat>, std::vector<cv::Mat>>>> preprocessed_batch_queue,
                            PreprocessCallback preprocess_callback) {

    auto model_input_shape = model.get_infer_model()->hef().get_input_vstream_infos().release()[0].shape;
    print_net_banner(get_hef_name(hef_path), std::ref(model.get_inputs()), std::ref(model.get_outputs()));
    preprocess_video_frames(capture, model_input_shape.width, model_input_shape.height, batch_size, preprocessed_batch_queue, preprocess_callback);
    return HAILO_SUCCESS;
}

void release_resources(cv::VideoCapture &capture, cv::VideoWriter &video, 
                      std::shared_ptr<BoundedTSQueue<std::pair<std::vector<cv::Mat>, std::vector<cv::Mat>>>> preprocessed_batch_queue,
                      std::shared_ptr<BoundedTSQueue<InferenceResult>> results_queue) {
    capture.release();
    cv::destroyAllWindows();
    
    if (preprocessed_batch_queue) {
        preprocessed_batch_queue->stop();
    }
    if (results_queue) {
        results_queue->stop();
    }
}
}