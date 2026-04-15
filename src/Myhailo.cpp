#include <hailo/hailort.hpp>
#include <iostream>
#include <optional>
#include <opencv2/opencv.hpp>
#include "Myhailo.hpp"

using namespace hailort;

string model_path = "yolov8s.hef";


vector<string> coco_labels = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard",
    "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon",
    "bowl", "banana", "apple", "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza",
    "donut", "cake", "chair", "couch", "potted plant", "bed", "dining table", "toilet", "tv",
    "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven", "toaster",
    "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier",
    "toothbrush"
};

void Hailo_release(HailoContext &context){
    context.input_buffer.clear();
    context.output_buffer.clear();
    context.infer_model.reset();
    context.vdevice.reset();
}


std::optional<HailoContext> Hailo_init(){
    setenv("DISPLAY", ":0", 1);
    HailoContext context;

    //============创建硬件设备对象===============
    auto vdevice_exp = VDevice::create(); //创建设备对象，返回一个封装了状态的结果类型,此时设备还被保护在封装中
    if (!vdevice_exp) { //检查设备创建是否成功
        std::cerr << "错误: 无法创建设备, status=" << vdevice_exp.status() << std::endl;
        return std::nullopt;
    }
    auto vdevice = vdevice_exp.release(); //若硬件没问题（上面的判断），就把封装的配置取出，这是一个安全API方式
    context.vdevice = std::move(vdevice);

    //=========================================

    //=============加载 HEF 模型文件=============== 
    auto infer_model_exp = context.vdevice->create_infer_model(model_path); //加载模型文件，返回一个封装了状态的结果类型,此时模型还被保护在封装中
    if (!infer_model_exp) { //检查模型加载是否成功
        std::cerr << "错误: 无法加载 HEF: " << model_path <<endl;
        return std::nullopt;
    }
    auto infer_model = infer_model_exp.release(); //若模型没问题（上面的判断），就把封装的配置取出，这是一个安全API方式
    context.infer_model = std::move(infer_model);
    //=========================================

    //=============检查模型输入输出节点信息===============
    auto input_names = context.infer_model->get_input_names();
    auto output_names = context.infer_model->get_output_names();
    if (input_names.empty() || output_names.empty()) {
        std::cerr << "错误: 模型输入或输出为空" << std::endl;
        return std::nullopt;
    }

    std::cout << "模型输入节点数量: " << input_names.size() << std::endl;
    for (const auto &name : input_names){
        const size_t input_size = context.infer_model->input(name)->get_frame_size();
        std::cout << "  - " << name << ": " << input_size << " bytes" << std::endl;
    }

    std::cout << "模型输出节点数量: " << output_names.size() << std::endl;
    for (const auto &name : output_names){
        const size_t output_size = context.infer_model->output(name)->get_frame_size();
        std::cout << "  - " << name << ": " << output_size << " bytes" << std::endl;
    }   
    //=========================================

    //=============配置模型并绑定输入输出内存===============

    //================获得配置单==================
    auto configured_infer_model_exp = context.infer_model->configure();
    if (!configured_infer_model_exp) {
        std::cerr << "错误: 无法配置模型, status=" << configured_infer_model_exp.status() << std::endl;
        return std::nullopt;
    }
    auto configured_infer_model = configured_infer_model_exp.release();
    context.configured_infer_model = std::move(configured_infer_model);
    //=========================================

    //=======创建绑定对象，用于绑定输入输出内存========
    auto bindings_exp = context.configured_infer_model.create_bindings();
    if (!bindings_exp) {
        std::cerr << "错误: 无法创建绑定对象, status=" << bindings_exp.status() << std::endl;
        return std::nullopt;
    }
    auto bindings = bindings_exp.release(); 
    context.bindings = std::move(bindings);
    //=========================================

    //=======为输入输出节点分配内存并绑定========
    std::vector<std::vector<uint8_t>> input_buffers;
    std::vector<std::vector<uint8_t>> output_buffers;
    input_buffers.reserve(input_names.size());
    output_buffers.reserve(output_names.size());    

    // 输入绑定
    for (const auto &name : input_names){
        const size_t input_size = context.infer_model->input(name)->get_frame_size();//获得每个输入节点的字节大小
        input_buffers.emplace_back(input_size); //为每个输入节点分配内存
        context.bindings.input(name)->set_buffer(MemoryView(input_buffers.back().data(), input_buffers.back().size())); //将分配的内存绑定到模型输入
    }

    // 输出绑定
    for (const auto &name : output_names){
        const size_t output_size = context.infer_model->output(name)->get_frame_size();//获得每个输出节点的字节大小
        output_buffers.emplace_back(output_size); //为每个输出节点分配内存
        context.bindings.output(name)->set_buffer(MemoryView(output_buffers.back().data(), output_buffers.back().size())); //将分配的内存绑定到模型输出
    }
    context.input_buffer = std::move(input_buffers);
    context.output_buffer = std::move(output_buffers);
    //=========================================
    cout << "模型配置和内存绑定成功" << endl;

    return context;
}
