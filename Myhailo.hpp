#ifndef __MYHAILO_HPP__
#define __MYHAILO_HPP__ 
#include <stdint.h>
#include <stdlib.h>
#include <opencv2/opencv.hpp>

using namespace std;


string model_path = "yolov8s.hef";

string pipeline = 
    "libcamerasrc camera-name=/base/axi/pcie@1000120000/rp1/i2c@88000/imx708@1a ! "
    "video/x-raw, format=NV12, width=640, height=480, framerate=30/1 ! "
    "appsink drop=true max-buffers=1 sync=false";

vector<string> coco_labels = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard",
    "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon",
    "bowl", "banana", "apple", "sandwich", "orange", "broccoli", "carrot","hot dog","pizza",
    "donut","cake","chair","couch","potted plant","bed","dining table","toilet","tv",
    "laptop","mouse","remote","keyboard","cell phone","microwave","oven","toaster",
    "sink","refrigerator","book","clock","vase","scissors","teddy bear","hair drier",
    "toothbrush"
};






#endif