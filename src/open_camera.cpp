#include <opencv2/opencv.hpp>
#include <optional>

using namespace std;
using namespace cv;

string pipeline =
    "libcamerasrc camera-name=/base/axi/pcie@1000120000/rp1/i2c@88000/imx708@1a ! "
    "video/x-raw, format=NV12, width=640, height=480, framerate=30/1 ! "
    "appsink drop=true max-buffers=1 sync=false";

optional<VideoCapture> open_camera() {
    VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        cerr << "错误: 无法打开摄像头" << endl;
        return nullopt;
    }
    return cap;
}
