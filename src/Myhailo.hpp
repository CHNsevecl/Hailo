#ifndef __MYHAILO_HPP__
#define __MYHAILO_HPP__ 
#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <hailo/hailort.hpp>
#include <opencv2/opencv.hpp>


using namespace std;
using namespace hailort;


struct HailoContext {
    shared_ptr<VDevice> vdevice;
    shared_ptr<InferModel> infer_model;
    ConfiguredInferModel configured_infer_model;
    ConfiguredInferModel::Bindings bindings;
    vector<vector<uint8_t>> input_buffer;
    vector<vector<uint8_t>> output_buffer;
};

extern string model_path;

extern string pipeline;

extern vector<string> coco_labels;

std::optional<HailoContext> Hailo_init();


#endif