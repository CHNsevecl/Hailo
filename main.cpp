#include <hailo/hailort.hpp>
#include <iostream>
#include "Myhailo.hpp"


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
    


    
}