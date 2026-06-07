#ifndef __MYHAILO_HPP__
#define __MYHAILO_HPP__

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <hailo/hailort.hpp>

struct HailoContext {
    std::shared_ptr<hailort::VDevice> vdevice;
    std::shared_ptr<hailort::InferModel> infer_model;
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    std::vector<std::vector<uint8_t>> input_buffer;
    std::vector<std::vector<uint8_t>> output_buffer;
    hailort::ConfiguredInferModel configured_infer_model;
    hailort::ConfiguredInferModel::Bindings bindings;
};

std::optional<HailoContext> Hailo_init(const std::string& hef_path);

#endif
