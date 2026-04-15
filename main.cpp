#include <hailo/hailort.hpp>
#include <iostream>
#include "Myhailo.hpp"
#include "open_camera.hpp"


using namespace hailort;

int main(){
    setenv("DISPLAY", ":0", 1);
    model_path = "yolov8s.hef";

    constexpr size_t expected_input_size = 640 * 640 * 3; // UINT8 NHWC

    auto context_opt = Hailo_init();
    if (!context_opt) {
        std::cerr << "Failed to initialize Hailo context" << std::endl;
        return -1;
    }
    struct HailoContext context = std::move(context_opt.value());

    
    //启动摄像头
    auto cap_opt = open_camera();
    if (!cap_opt) {
        std::cerr << "Failed to open camera" << std::endl;  
        return -1;
    }

    cv::VideoCapture cap = std::move(cap_opt.value());

    cv::Mat bgr_frame, rgb_frame;
    cv::Mat canvas(640, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    const int target_w = 640;
    const int target_h = 640;
    

    while (true){
        cv::Mat NV12_frame;
        cap >> NV12_frame;
        if (NV12_frame.empty()) {
            std::cerr << "错误: 摄像头抓取到空帧" <<endl;
            return -1;
        }

        cv::cvtColor(NV12_frame, rgb_frame, cv::COLOR_YUV2RGB_NV12);
        

        const float scale = std::min(
			static_cast<float>(target_w) / static_cast<float>(rgb_frame.cols),
			static_cast<float>(target_h) / static_cast<float>(rgb_frame.rows));

		const int new_w = static_cast<int>(rgb_frame.cols * scale);
		const int new_h = static_cast<int>(rgb_frame.rows * scale);
		const int x_offset = (target_w - new_w) / 2;
		const int y_offset = (target_h - new_h) / 2;

        rgb_frame.copyTo(canvas(cv::Rect(x_offset, y_offset, new_w, new_h)));

        // 将预处理后的图像数据复制到输入缓冲
        if (context.input_buffer[0].size() != canvas.total() * canvas.elemSize()){
            std::cerr << "错误: 摄像头预处理后的输入大小与模型输入不一致" << std::endl;
            return -1;
        }

        std::memcpy(context.input_buffer[0].data(), canvas.data, context.input_buffer[0].size());

        auto run_status = context.configured_infer_model.run(context.bindings, std::chrono::milliseconds(1000));
        if (run_status != HAILO_SUCCESS) {
            std::cerr << "错误: 推理执行失败, status=" << run_status << std::endl;
            continue;
        }

        cv::Mat display_bgr = canvas.clone(); // 在预处理后的640x640画布上绘制结果
        const float *detections = reinterpret_cast<const float*>(context.output_buffer[0].data()); // 将输出缓冲按float数组解析
		const size_t detection_float_count = context.output_buffer[0].size() / sizeof(float); // 输出里一共有多少个float
		size_t detection_offset = 0; // 解析游标，每读取一个字段就向后移动
		const int class_count = 80; // COCO类别数
		const int max_boxes_per_class = 100; // 每个类别最多100个框
		const float score_threshold = 0.4f; // 画框阈值

		// NMS BY CLASS 输出布局：每个类别先给 count，再给该类别最多100个框(每框5个float)
		for (int class_id = 0; class_id < class_count; ++class_id) {
			if (detection_offset >= detection_float_count) {
				break;
			}

			const int box_count = static_cast<int>(detections[detection_offset++]); // 该类别有效框数量
			for (int box_index = 0; box_index < max_boxes_per_class; ++box_index) {
				if (detection_offset + 4 >= detection_float_count) {
					break;
				}

				const float ymin = detections[detection_offset++]; // 框上边界
				const float xmin = detections[detection_offset++]; // 框左边界
				const float ymax = detections[detection_offset++]; // 框下边界
				const float xmax = detections[detection_offset++]; // 框右边界
				const float score = detections[detection_offset++]; // 置信度

				// 超过该类别有效数量，或分数太低，则跳过
				if (box_index >= box_count || score < score_threshold) {
					continue;
				}

				// 某些模型给0~1归一化坐标，某些模型给像素坐标；这里做兼容
				auto to_pixel_x = [&](float value) {
					return static_cast<int>(value <= 1.5f ? value * target_w : value);
				};
				auto to_pixel_y = [&](float value) {
					return static_cast<int>(value <= 1.5f ? value * target_h : value);
				};

				// 将坐标裁剪到画布范围，避免越界绘制
				const int x1 = std::max(0, std::min(to_pixel_x(xmin), target_w - 1));
				const int y1 = std::max(0, std::min(to_pixel_y(ymin), target_h - 1));
				const int x2 = std::max(0, std::min(to_pixel_x(xmax), target_w - 1));
				const int y2 = std::max(0, std::min(to_pixel_y(ymax), target_h - 1));

				// 非法框（右下在左上之前）直接丢弃
				if (x2 <= x1 || y2 <= y1) {
					continue;
				}

				// 将类别ID映射为COCO标签文本
				const std::string label = (class_id >= 0 && class_id < static_cast<int>(coco_labels.size()))
					? coco_labels[class_id]
					: (std::string("cls ") + std::to_string(class_id));

				// 绘制框和标签
				cv::rectangle(display_bgr, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 255, 0), 2);
				cv::putText(display_bgr,
					label + " " + cv::format("%.2f", score),
					cv::Point(x1, std::max(0, y1 - 8)),
					cv::FONT_HERSHEY_SIMPLEX,
					0.5,
					cv::Scalar(0, 0, 255),
					1);
			}
		}
        cv::cvtColor(display_bgr, display_bgr, cv::COLOR_RGB2BGR); // 模型输入是RGB，OpenCV显示习惯是BGR，转换一下颜色空间
        cv::imshow("Hailo Stream", display_bgr);
        if (cv::waitKey(1) == 27) { // 按ESC键退出
            break;
        }
    }
    
    return 0;
}