虽说树莓派是很多新手会用到的第一个Linux主机，网上资粮很多。
但是Hailo 8L Hat，作为一个官方配件，上手困难，中文教程少，所以我把自己的结论写下来帮助其他人。



/*注意
  这是面向新手的，或者说只是我个人经验的分享，所以可能会有不准确、不专业的内容。如果有希望你能指出
/*



步骤一：更新系统与固件
  1、更新软件包列表和所有软件：

  
    sudo apt update && sudo apt full-upgrade -y
  2、重启

步骤二：启用PCIe Gen 3.0模式
  1、在终端中运行配置工具：
  
    sudo raspi-config
  2、依次选择：Advanced Options > PCIe Speed。
  3、系统会询问是否启用PCIe Gen 3.0模式，选择 Yes。
  4、选择 Finish 退出配置工具。当系统询问是否重启时，选择 Yes。
步骤三：安装Hailo软件包
  1、在终端中执行以下命令来安装所有必需的驱动、固件和库：
 
    sudo apt install hailo-all
  2、安装完成后，再次重启系统
步骤四：验证安装
  1、检查NPU状态：运行以下命令，它会识别并显示Hailo芯片的信息。
  
    hailortcli fw-control identify
    例如：
 
       Executing on device: 0001:01:00.0
            Identifying board
            Control Protocol Version: 2
            Firmware Version: 4.23.0 (release,app,extended context switch buffer)
            Logger Version: 0
            Board Name: Hailo-8
            Device Architecture: HAILO8L
            Serial Number: HLDDLBB243502526
            Part Number: HM21LB1C2LAE
            Product Name: HAILO-8L AI ACC M.2 B+M KEY MODULE EXT TMP

      同时，可能会在终端当前的位置创建一个hailo.log文件，不用在意，是保存返回得到的信息的
  2、检查PCIE硬件是否连接：

    lspci | grep Hailo


      例如：
        0001:01:00.0 Co-processor: Hailo Technologies Ltd. Hailo-8 AI Processor (rev 01)
    注意：其实第一个指令足够，同时注意你的设备时8还是8L，这一点通过第二个指令无法区分

    
         以我的为例“Device Architecture: HAILO8L”，这是8L设备，使用模型要注意
         
硬件和系统配置差不多到这里，在cpp使用的的源代码在树莓派系统一般自带，也会有已经编译好的模型。比如，我的自带hef格式的模型在“/usr/share/hailo-models”。
（Hailo NPU必须使用.hef格式的模型）
（如果你是8L型号，选择h8l模型，若是8型号，选择h8模型）
你可以使用指令查看模型数据：

    hailortcli parse-hef your_model.hef
  

  例子：
    hailortcli parse-hef yolov8s.hef
    
    Architecture HEF was compiled for: HAILO8L
    Network group name: yolov8s, Multi Context - Number of contexts: 4
        Network name: yolov8s/yolov8s
            VStream infos:
                Input  yolov8s/input_layer1 UINT8, NHWC(640x640x3)
                Output yolov8s/yolov8_nms_postprocess FLOAT32, HAILO NMS BY CLASS(number of classes: 80, maximum bounding boxes per class: 100, maximum frame size: 160320)
                Operation:
                    Op YOLOV8
                    Name: YOLOV8-Post-Process
                    Score threshold: 0.200
                    IoU threshold: 0.70
                    Classes: 80
                    Max bboxes per class: 100
                    Image height: 640
                    Image width: 640

  Architecture HEF was compiled for：模型是为8还是8L编译的\n
  INPUT：输入数据格式\n
  OUTPUT：输出格式\n
  Score threshold: 置信度阈值\n
  Classes：类别数\n

  /*
    注意：大部分模型都是640*640的输入，但大部分摄像头是640*480，不要直接拉伸传入，会失真。正确方式是：将摄像头画面放到640*640的黑色画布
  /*
