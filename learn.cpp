#include <hailo/hailort.hpp>
#include <iostream>
#include <opencv2/opencv.hpp>


using namespace hailort;

int main() {
    setenv("DISPLAY", ":0", 1);
	const std::string hef_path = "yolov8s.hef";
	constexpr size_t expected_input_size = 640 * 640 * 3; // UINT8 NHWC

	std::cout << "[Step 1] 初始化 Hailo 设备并读取 HEF..." << std::endl;
    //=============创建硬件设备对象===============
	auto vdevice_exp = VDevice::create();
	if (!vdevice_exp) {
		std::cerr << "错误: 无法创建设备, status=" << vdevice_exp.status() << std::endl;
		return -1;
	}
	auto vdevice = vdevice_exp.release();//若硬件没问题（上面的判断），就把封装的配置取出，这是一个安全API方式
    //=========================================


    //=============加载 HEF 模型文件===============
	auto infer_model_exp = vdevice->create_infer_model(hef_path);
	if (!infer_model_exp) {
		std::cerr << "错误: 无法加载 HEF: " << hef_path << ", status=" << infer_model_exp.status() << std::endl;
		return -1;
	}
	auto infer_model = infer_model_exp.release();//若模型没问题（上面的判断），就把封装的配置取出，这是一个安全API方式
    //=========================================

    //=============检查模型输入输出节点信息===============
	auto input_names = infer_model->get_input_names();
	auto output_names = infer_model->get_output_names();
    std::cout << "input_names size = " << input_names.size() << std::endl;
    for (const auto &n : input_names) std::cout << "  " << n << std::endl;
    std::cout << "output_names size = " << output_names.size() << std::endl;
    for (const auto &n : output_names) std::cout << "  " << n << std::endl;

	if (input_names.empty() || output_names.empty()) {
		std::cerr << "错误: 模型输入或输出为空" << std::endl;
		return -1;
	}
    //=========================================


    for (const auto &name : input_names) {
		const size_t input_size = infer_model->input(name)->get_frame_size();
		std::cout << "  - " << name << ": " << input_size << " bytes" << std::endl;
	}
	std::cout << "期望字节数(640x640x3): " << expected_input_size << std::endl;

	std::cout << "输出节点数量: " << output_names.size() << std::endl;
	for (const auto &name : output_names) {
		const size_t output_size = infer_model->output(name)->get_frame_size();
		std::cout << "  - " << name << ": " << output_size << " bytes" << std::endl;
	}

	//=============第 2 步: 配置模型并绑定输入输出内存===============

    //================获得配置单==================
	auto configured_infer_model_exp = infer_model->configure();
	if (!configured_infer_model_exp) {
		std::cerr << "错误: 无法配置模型, status=" << configured_infer_model_exp.status() << std::endl;
		return -1;
	}
	auto configured_infer_model = configured_infer_model_exp.release();
    //=========================================

    //================创建绑定对象，用于绑定输入输出缓冲(buffers)==================
	auto bindings_exp = configured_infer_model.create_bindings();
	if (!bindings_exp) {
		std::cerr << "错误: 无法创建 bindings, status=" << bindings_exp.status() << std::endl;
		return -1;
	}
	auto bindings = bindings_exp.release();
    //=========================================

    //=================输入绑定==================
	std::vector<std::vector<uint8_t>> input_buffers;//存放输入字节的容器，生命周期要长于推理调用
    input_buffers.reserve(input_names.size());
    for(const auto &name : input_names){
        const size_t input_size = infer_model->input(name)->get_frame_size();
        input_buffers.emplace_back(input_size);
        bindings.input(name)->set_buffer(MemoryView(input_buffers.back().data(), input_buffers.back().size()));
    }
	
    //==========================================

    //=================输出绑定==================
	std::vector<std::vector<uint8_t>> output_buffers;
	output_buffers.reserve(output_names.size());
	for (const auto &name : output_names) {
		const size_t output_size = infer_model->output(name)->get_frame_size();
		output_buffers.emplace_back(output_size);
		bindings.output(name)->set_buffer(MemoryView(output_buffers.back().data(), output_buffers.back().size()));
	}
    //==========================================

	//=============第 3 步: 启动视频流并持续推理===============
    std::string pipeline = 
        "libcamerasrc camera-name=/base/axi/pcie@1000120000/rp1/i2c@88000/imx708@1a ! "
        "video/x-raw, format=NV12, width=640, height=480, framerate=30/1 ! "
        "appsink drop=true max-buffers=1 sync=false";

	cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
	if (!cap.isOpened()) {
		std::cerr << "错误: 无法打开摄像头" << std::endl;
		return -1;
	}

	cv::namedWindow("Hailo Stream", cv::WINDOW_AUTOSIZE);

	const int target_w = 640;
	const int target_h = 640;
	const std::vector<std::string> coco_labels = {
		"person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
		"fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
		"elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
		"skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle",
		"wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
		"broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant", "bed",
		"dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven",
		"toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
	};
	

	while (true) {
		cv::Mat frame_NV12;
		cap >> frame_NV12;
		if (frame_NV12.empty()) {
			std::cerr << "错误: 摄像头抓取到空帧" << std::endl;
			break;
		}

		cv::Mat frame_bgr;
		cv::cvtColor(frame_NV12, frame_bgr, cv::COLOR_YUV2BGR_NV12);

		const float scale = std::min(
			static_cast<float>(target_w) / static_cast<float>(frame_bgr.cols),
			static_cast<float>(target_h) / static_cast<float>(frame_bgr.rows));

		const int new_w = static_cast<int>(frame_bgr.cols * scale);
		const int new_h = static_cast<int>(frame_bgr.rows * scale);
		const int x_offset = (target_w - new_w) / 2;
		const int y_offset = (target_h - new_h) / 2;

		cv::Mat resized_bgr;
		cv::resize(frame_bgr, resized_bgr, cv::Size(new_w, new_h));

		cv::Mat letterbox_bgr(target_h, target_w, CV_8UC3, cv::Scalar(0, 0, 0));
		resized_bgr.copyTo(letterbox_bgr(cv::Rect(x_offset, y_offset, new_w, new_h)));

		cv::Mat resized_rgb;
		cv::cvtColor(letterbox_bgr, resized_rgb, cv::COLOR_BGR2RGB);
		if (!resized_rgb.isContinuous()) {
			resized_rgb = resized_rgb.clone();
		}

		if (input_buffers[0].size() != resized_rgb.total() * resized_rgb.elemSize()) {
			std::cerr << "错误: 摄像头预处理后的输入大小与模型输入不一致" << std::endl;
			break;
		}

		std::memcpy(input_buffers[0].data(), resized_rgb.data, input_buffers[0].size());

		auto run_status = configured_infer_model.run(bindings, std::chrono::milliseconds(1000));
		if (run_status != HAILO_SUCCESS) {
			std::cerr << "错误: 推理执行失败, status=" << run_status << std::endl;
			continue;
		}
        //=========================================
        
		cv::Mat display_bgr = letterbox_bgr.clone(); // 在预处理后的640x640画布上绘制结果
		const float *detections = reinterpret_cast<const float*>(output_buffers[0].data()); // 将输出缓冲按float数组解析
		const size_t detection_float_count = output_buffers[0].size() / sizeof(float); // 输出里一共有多少个float
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

		// cv::putText(display_bgr, "Hailo Streaming... press q to quit", cv::Point(20, 40),
		// 	cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);
		cv::imshow("Hailo Stream", display_bgr);
		

		if (cv::waitKey(1) == 'q') {
			break;
		}
	}

	cap.release();
	cv::destroyAllWindows();
	std::cout << "[Step 3 完成] 视频流已结束。" << std::endl;
    
	std::cout << "[Step 2 完成] 模型已配置，输入输出缓冲已绑定。" << std::endl;

	std::cout << "[Step 1 完成] 设备与模型检查通过。" << std::endl;

    std::cout << std::endl;
	return 0;
}
