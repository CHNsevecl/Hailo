#ifndef OPEN_CAMERA_HPP
#define OPEN_CAMERA_HPP

#include <opencv2/opencv.hpp>
#include <optional>

std::optional<cv::VideoCapture> open_camera();

#endif // OPEN_CAMERA_HPP