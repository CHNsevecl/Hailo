#include "Myhailo.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>

// 修改 1: 参数改为 const hailort::Buffer&
void SaveOutputToFile(const hailort::Buffer& buffer) {
    const float* data = reinterpret_cast<const float*>(buffer.data());
    size_t float_count = buffer.size() / sizeof(float);

    std::ofstream outFile("hailo_debug_output.txt");
    if (!outFile.is_open()) return;

    // 直接写入原始数据，每个数据用逗号分隔
    for (size_t i = 0; i < float_count; ++i) {
        outFile << data[i];
        if (i < float_count - 1) {
            outFile << ", ";
        }
    }
    outFile << std::endl;  // 可选：在末尾添加换行符

    outFile.close();
}

std::optional<HailoContext> Hailo_init(const std::string& hef_path) {
	HailoContext context;
	std::cout << "[Step 1] 初始化 Hailo 设备并读取 HEF..." << std::endl;

	auto vdevice_exp = hailort::VDevice::create();
	if (!vdevice_exp) return std::nullopt;
	context.vdevice = vdevice_exp.release();

	auto infer_model_exp = context.vdevice->create_infer_model(hef_path);
	if (!infer_model_exp) return std::nullopt;
	context.infer_model = infer_model_exp.release();

	context.input_names = context.infer_model->get_input_names();
	context.output_names = context.infer_model->get_output_names();

	auto configured_infer_model_exp = context.infer_model->configure();
	if (!configured_infer_model_exp) return std::nullopt;
	context.configured_infer_model = configured_infer_model_exp.release();

	auto bindings_exp = context.configured_infer_model.create_bindings();
	if (!bindings_exp) return std::nullopt;
	context.bindings = bindings_exp.release();

	// 修改 2: 使用 hailort::Buffer::create_shared 分配对齐内存 (Input)
	for (const auto &name : context.input_names) {
        const size_t input_size = context.infer_model->input(name)->get_frame_size();
        auto buf_exp = hailort::Buffer::create(input_size); 
        
        if (buf_exp) {
            // 使用 emplace_back + std::move
            context.input_buffer.emplace_back(std::move(buf_exp.value())); 
            
            // 绑定时使用刚存进去的那个 buffer
            context.bindings.input(name)->set_buffer(hailort::MemoryView(
                context.input_buffer.back().data(), 
                context.input_buffer.back().size()
            ));
        }
    }

	// 修改 3: 使用 hailort::Buffer::create_shared 分配对齐内存 (Output)
	for (const auto &name : context.output_names) {
        const size_t output_size = context.infer_model->output(name)->get_frame_size();
        auto buf_exp = hailort::Buffer::create(output_size);
        
        if (buf_exp) {
            // 使用 emplace_back + std::move
            context.output_buffer.emplace_back(std::move(buf_exp.value()));
            
            // 绑定
            context.bindings.output(name)->set_buffer(hailort::MemoryView(
                context.output_buffer.back().data(), 
                context.output_buffer.back().size()
            ));
        }
    }

	std::cout << "[Step 1 & 2 完成] 内存对齐缓冲已绑定。" << std::endl;
	return context;
}


std::optional<std::vector<Detection>> ParseDetections(HailoContext& hailo_context, int target_w, int target_h, cv::Mat& RGB_frame, const std::vector<std::string>& class_names) {
	std::vector<Detection> results;
	if (!RGB_frame.isContinuous()) RGB_frame = RGB_frame.clone();

    // 修改 4: 指针拷贝方式微调
	if (hailo_context.input_buffer[0].size() != RGB_frame.total() * RGB_frame.elemSize()) {
		std::cerr << "错误: 输入大小不匹配" << std::endl;
		return std::nullopt;
	}
	std::memcpy(hailo_context.input_buffer[0].data(), RGB_frame.data, hailo_context.input_buffer[0].size());
	
    // 预擦除（使用实际大小）
	// std::memset(hailo_context.output_buffer[0].data(), 0xEE, hailo_context.output_buffer[0].size());

	auto run_status = hailo_context.configured_infer_model.run(hailo_context.bindings, std::chrono::milliseconds(1000));
	// SaveOutputToFile(hailo_context.output_buffer[0]); 

	if (run_status != HAILO_SUCCESS) return std::nullopt;

    // 修改 5: 解析逻辑保持不变，但使用 .data()
	const float *detections = reinterpret_cast<const float*>(hailo_context.output_buffer[0].data());

    int NMS_index = 0; // NMS结果起始位置
	for (size_t class_id = 0; class_id < class_names.size(); ++class_id) {
        int class_count = detections[NMS_index];

        if(class_count == 0){
            // std::cout << "类别 " << class_names[class_id] << " 没有检测到目标。" << " "<< "NMS_index: " << NMS_index << "class_count: " << class_count << std::endl;
            NMS_index += 1; // 移动到下一个类别的NMS结果起始位置
            continue;
        }
        else{
            // std::cout << "类别 " << class_names[class_id] << " 检测到 " << class_count << " 个目标。" << " "<< "NMS_index: " << NMS_index << "class_count: " << class_count << std::endl;
            for (int box_index = NMS_index; box_index < NMS_index + class_count; box_index++) {
                const float ymin  = detections[box_index + 1];
                const float xmin  = detections[box_index + 2];
                const float ymax  = detections[box_index + 3];
                const float xmax  = detections[box_index + 4];
                const float score = detections[box_index + 5];

                if (score < score_threshold) continue;

                auto to_pixel_x = [&](float value) { return static_cast<int>(value <= 1.5f ? value * target_w : value); };
                auto to_pixel_y = [&](float value) { return static_cast<int>(value <= 1.5f ? value * target_h : value); };

                const int x1 = std::max(0, std::min(to_pixel_x(xmin), target_w - 1));
                const int y1 = std::max(0, std::min(to_pixel_y(ymin), target_h - 1));
                const int x2 = std::max(0, std::min(to_pixel_x(xmax), target_w - 1));
                const int y2 = std::max(0, std::min(to_pixel_y(ymax), target_h - 1));

                if (x2 <= x1 || y2 <= y1) continue;

                Detection det;
                det.label = class_names[class_id];
                det.upper = cv::Point(x1, y1);
                det.lower = cv::Point(x2, y2);
                det.score = score;
                results.push_back(det);
            }
            NMS_index += 1 + class_count * 5; // 移动到下一个类别的NMS结果起始位置
        }
	}
	return results;
}

cv::Mat Stream_process(const cv::Mat& BGR_frame, int target_w, int target_h) {
    const float scale = std::min(static_cast<float>(target_w) / BGR_frame.cols, static_cast<float>(target_h) / BGR_frame.rows);
    const int new_w = static_cast<int>(BGR_frame.cols * scale);
    const int new_h = static_cast<int>(BGR_frame.rows * scale);
    const int x_offset = (target_w - new_w) / 2;
    const int y_offset = (target_h - new_h) / 2;

    cv::Mat resized_frame;
    cv::resize(BGR_frame, resized_frame, cv::Size(new_w, new_h));
    cv::Mat letterbox_bgr(target_h, target_w, CV_8UC3, cv::Scalar(114, 114, 114));
    resized_frame.copyTo(letterbox_bgr(cv::Rect(x_offset, y_offset, new_w, new_h)));
    cv::Mat RGB_frame;
    cv::cvtColor(letterbox_bgr, RGB_frame, cv::COLOR_BGR2RGB);
    return RGB_frame;
}