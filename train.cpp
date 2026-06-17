#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>
#include "./src/Myhailo.hpp"

// /usr/share/hailo-models/yolov8s_h8l.hef
int main(){
    setenv("DISPLAY", ":0", 1);

    std::optional<HailoContext> hailo_context = Hailo_init(hef_path);
    if (!hailo_context) {
        std::cerr << "Error: Failed to initialize Hailo context." << std::endl;
        return -1;
    }

    std::string pipeline = 
    "libcamerasrc camera-name=/base/axi/pcie@1000120000/rp1/i2c@88000/imx708@1a ! "
    "video/x-raw, format=NV12, width=640, height=640 ! " // 传感器输出
    "videoconvert ! "                                   // 硬件加速转换
    "video/x-raw, format=BGR ! "                        // 直接输出 BGR
    "appsink drop=true max-buffers=1";

    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video stream." << std::endl;
        return -1;
    }

    while (true){
        cv::Mat BGR_frame;
        if (!cap.read(BGR_frame)) {
            std::cerr << "Error: Could not read frame from video stream." << std::endl;
            break;
        }

        cv::Mat display_frame = Stream_process(BGR_frame, target_w, target_h);
        
        
        std::optional<std::vector<Detection>> detections_opt = ParseDetections(*hailo_context, target_w, target_h, display_frame, class_names);
        cv::cvtColor(display_frame, display_frame, cv::COLOR_RGB2BGR); // 将RGB转回BGR以便OpenCV显示

        if (!detections_opt) {
            std::cerr << "Error: Failed to parse detections." << std::endl;
            break;
        }
        const auto& detections = *detections_opt;

        
        for (const auto& det : detections) {
            if(det.label == "Blue_Laser") {
                cv::Point center((det.upper.x + det.lower.x) / 2, (det.upper.y + det.lower.y) / 2);
                cv::circle(display_frame, center, 2, cv::Scalar(255, 0, 0), -1); // 在检测到的激光点位置画一个绿色圆点
                cv::putText(display_frame,                                      // 图像
                    det.label + " " + cv::format("%.2f", det.score),                    // 文本内容
                    cv::Point(center.x - 20, std::max(0, center.y - 10)),       // 文本位置
                    cv::FONT_HERSHEY_SIMPLEX,                                   // 字体类型
                    0.5,                                                        // 字体大小
                    cv::Scalar(0, 255, 0),                                      // 颜色（BGR）
                    1                                                           // 线条粗细
                );
            }
            else if(det.label != "Blue_Laser") {
                cv::rectangle(display_frame, det.upper, det.lower, cv::Scalar(0, 255, 0), 2); // 在检测到的画布位置画一个绿色矩形框
                cv::putText(display_frame,                                      // 图像
                    det.label + " " + cv::format("%.2f", det.score),                    // 文本内容
                    cv::Point(det.upper.x, std::max(0, det.upper.y - 10)),       // 文本位置
                    cv::FONT_HERSHEY_SIMPLEX,                                   // 字体类型
                    0.5,                                                        // 字体大小
                    cv::Scalar(0, 255, 0),                                      // 颜色（BGR）
                    1                                                           // 线条粗细
                );
            }
        }
       
        cv::imshow("Hailo Inference", display_frame);
        if (cv::waitKey(1) == 27) { // Exit on 'ESC' key press
            break;
        }
    }
    
}