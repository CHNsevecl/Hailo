#include "Myhailo.hpp"

#include <iostream>

std::optional<HailoContext> Hailo_init(const std::string& hef_path) {
	HailoContext context;

	std::cout << "[Step 1] 初始化 Hailo 设备并读取 HEF..." << std::endl;

	auto vdevice_exp = hailort::VDevice::create();
	if (!vdevice_exp) {
		std::cerr << "错误: 无法创建设备, status=" << vdevice_exp.status() << std::endl;
		return std::nullopt;
	}
	context.vdevice = vdevice_exp.release();

	auto infer_model_exp = context.vdevice->create_infer_model(hef_path);
	if (!infer_model_exp) {
		std::cerr << "错误: 无法加载 HEF: " << hef_path << ", status=" << infer_model_exp.status() << std::endl;
		return std::nullopt;
	}
	context.infer_model = infer_model_exp.release();

	context.input_names = context.infer_model->get_input_names();
	context.output_names = context.infer_model->get_output_names();

	std::cout << "input_names size = " << context.input_names.size() << std::endl;
	for (const auto &n : context.input_names) std::cout << "  " << n << std::endl;
	std::cout << "output_names size = " << context.output_names.size() << std::endl;
	for (const auto &n : context.output_names) std::cout << "  " << n << std::endl;

	if (context.input_names.empty() || context.output_names.empty()) {
		std::cerr << "错误: 模型输入或输出为空" << std::endl;
		return std::nullopt;
	}

	if (context.output_names.size() != 1) {
		std::cerr << "错误: 当前模型有 " << context.output_names.size() << " 个输出节点，"
				  << "此程序仅支持单输出模型，请检查模型文件" << std::endl;
		return std::nullopt;
	}
	std::cout << "模型输出节点数量检查通过 (仅支持单输出): " << context.output_names.size() << std::endl;

	for (const auto &name : context.input_names) {
		const size_t input_size = context.infer_model->input(name)->get_frame_size();
		std::cout << "  - " << name << ": " << input_size << " bytes" << std::endl;
	}

	std::cout << "输出节点数量: " << context.output_names.size() << std::endl;
	for (const auto &name : context.output_names) {
		const size_t output_size = context.infer_model->output(name)->get_frame_size();
		std::cout << "  - " << name << ": " << output_size << " bytes" << std::endl;
	}

	auto configured_infer_model_exp = context.infer_model->configure();
	if (!configured_infer_model_exp) {
		std::cerr << "错误: 无法配置模型, status=" << configured_infer_model_exp.status() << std::endl;
		return std::nullopt;
	}
	context.configured_infer_model = configured_infer_model_exp.release();

	auto bindings_exp = context.configured_infer_model.create_bindings();
	if (!bindings_exp) {
		std::cerr << "错误: 无法创建 bindings, status=" << bindings_exp.status() << std::endl;
		return std::nullopt;
	}
	context.bindings = bindings_exp.release();

	context.input_buffer.reserve(context.input_names.size());
	for (const auto &name : context.input_names) {
		const size_t input_size = context.infer_model->input(name)->get_frame_size();
		context.input_buffer.emplace_back(input_size);
		context.bindings.input(name)->set_buffer(hailort::MemoryView(context.input_buffer.back().data(), context.input_buffer.back().size()));
	}

	context.output_buffer.reserve(context.output_names.size());
	for (const auto &name : context.output_names) {
		const size_t output_size = context.infer_model->output(name)->get_frame_size();
		context.output_buffer.emplace_back(output_size);
		context.bindings.output(name)->set_buffer(hailort::MemoryView(context.output_buffer.back().data(), context.output_buffer.back().size()));
	}

	std::cout << "[Step 2 完成] 模型已配置，输入输出缓冲已绑定。" << std::endl;
	std::cout << "[Step 1 完成] 设备与模型检查通过。" << std::endl;

	return context;
}
