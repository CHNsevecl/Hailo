
---

### 树莓派5 + Hailo-8L AI Hat 环境配置指南

虽说树莓派是很多新手会用到的第一个Linux主机，网上资料也很多。但**Hailo 8L AI Hat** 作为官方配件，上手还是有些门槛，且中文教程很少。所以我把自己的配置过程和结论写下来，希望能帮助到其他人。

> **注意**：本文档是面向新手的个人经验分享，可能会有不准确或不专业的地方。如果发现问题，欢迎指正！

---

### 步骤一：更新系统与固件

1. 更新软件包列表和所有软件：

   ```bash
   sudo apt update && sudo apt full-upgrade -y
   ```

2. **重启系统**：

   ```bash
   sudo reboot
   ```

---

### 步骤二：启用 PCIe Gen 3.0 模式

1. 运行树莓派配置工具：

   ```bash
   sudo raspi-config
   ```

2. 依次选择：`Advanced Options` > `PCIe Speed`。

3. 系统询问是否启用 PCIe Gen 3.0 模式，选择 **Yes**。

4. 选择 `Finish` 退出。当系统询问是否重启时，选择 **Yes**。

---

### 步骤三：安装 Hailo 软件包

1. 安装所有必需的驱动、固件和库：

   ```bash
   sudo apt install hailo-all
   ```

2. 安装完成后，**再次重启系统**。

---

### 步骤四：验证安装

#### 1. 检查 NPU 状态

运行以下命令，识别并显示 Hailo 芯片信息：

```bash
hailortcli fw-control identify
```

**示例输出**：

```
Executing on device: 0001:01:00.0
Identifying board
Control Protocol Version: 2
Firmware Version: 4.23.0 (release,app,extended context switch buffer)
Logger Version: 0
Board Name: Hailo-8
Device Architecture: HAILO8L          # 这里是关键：8L 还是 8
Serial Number: HLDDLBB243502526
Part Number: HM21LB1C2LAE
Product Name: HAILO-8L AI ACC M.2 B+M KEY MODULE EXT TMP
```

- 命令执行后，可能会在当前目录生成一个 `hailo.log` 文件，这是保存返回信息的日志文件，可以忽略。
- **重要**：通过输出中的 `Device Architecture` 字段可以确认你的设备是 **Hailo-8** 还是 **Hailo-8L**。`lspci` 命令无法区分这两者。

#### 2. 检查 PCIe 硬件连接（可选）

```bash
lspci | grep Hailo
```

**示例输出**：

```
0001:01:00.0 Co-processor: Hailo Technologies Ltd. Hailo-8 AI Processor (rev 01)
```

> 实际上，第一步的 `hailortcli fw-control identify` 已经足够验证硬件连接。

---

### 补充：模型相关说明

硬件和系统配置到这里就基本完成了。

- 树莓派系统中通常自带 C++ 开发环境，以及一些已经编译好的 **.hef** 格式模型（Hailo NPU 必须使用此格式）。
- 自带模型一般位于：`/usr/share/hailo-models`
- **选择模型时注意**：
  - 如果你是 **Hailo-8L** 型号，请选择 `h8l` 模型
  - 如果你是 **Hailo-8** 型号，请选择 `h8` 模型

#### 查看模型信息

使用以下命令查看 `.hef` 模型文件的信息：

```bash
hailortcli parse-hef your_model.hef
```

**示例**（以 `yolov8s.hef` 为例）：

```bash
hailortcli parse-hef yolov8s.hef
```

**示例输出及说明**：

```
Architecture HEF was compiled for: HAILO8L              # 模型是为 8 还是 8L 编译的
Network group name: yolov8s, Multi Context - Number of contexts: 4
    Network name: yolov8s/yolov8s
        VStream infos:
            Input  yolov8s/input_layer1 UINT8, NHWC(640x640x3)   # 输入数据格式
            Output yolov8s/yolov8_nms_postprocess FLOAT32, HAILO NMS BY CLASS(number of classes: 80, maximum bounding boxes per class: 100, maximum frame size: 160320)  # 输出格式
            Operation:
                Op YOLOV8
                Name: YOLOV8-Post-Process
                Score threshold: 0.200          # 置信度阈值
                IoU threshold: 0.70
                Classes: 80                     # 类别数
                Max bboxes per class: 100
                Image height: 640
                Image width: 640
```

#### 一个需要注意的点

> 大部分模型（如 YOLO 系列）的输入尺寸是 **640×640**，但很多常见摄像头的输出是 **640×480**。
>
> **请不要直接将画面拉伸到 640×640 再送入模型**，这样会导致图像失真，严重影响检测效果。
>
> **正确做法**：将 640×480 的画面放置到一个 **640×640 的黑色画布** 上（上下填充黑边），再进行推理。

---

### 现在开始cpp代码部分

---
### 为什么选择cpp？
这或许是我个人的问题，我在无hailo的情况下通过Python运行yolo，但是一直有内存溢出的问题（4G版本）因此我开始使用cpp，以方便内存管理。
事实证明，cpp的内存管理更加优秀，运行时可以将内存占用维持在 2-2.3G 左右，尤其是在hailo的配合下，CPU的压力瞬间从矩阵运算释放，你有更多空闲的性能做控制部分。

---

-源文件为src/MYhailo.cpp
-注释已经写上了，如果有问题，可以问问AI

---
### 硬件启动（）

1、创建硬件设备对象
2、加载 HEF 模型文件
3、检查模型输入输出节点信息

### 配置模型并绑定输入输出内存
1、获得配置单
2、创建绑定对象，用于绑定输入输出内存
3、为输入输出节点分配内存并绑定

### 返回配置
你可以看到函数的结构是std::optional<HailoContext> Hailo_init()，这是我通过optional传回HailoContext这个结构体，这样函数中的配置的生命周期就和main函数绑定到一起，我们也可以通过结构体中的配置输入、获取数据

---


###编译方面

解决 Hailo 编译 YOLOv8 报错：expected conv but found concat layer

本文档记录了在使用 Hailo Model Zoo (hailomz) 将 YOLOv8 (ONNX) 模型编译为 Hailo 硬件模型 (HEF)
时常见的节点解析错误，以及相应的解决方案。

1. 报错现象

在使用 hailomz compile 编译自定义 YOLOv8 模型时，程序中断并打印出以下错误日志：

[info] According to recommendations, retrying parsing with end node names: ['/model.22/Concat', '/model.22/Sigmoid'].
...
hailo_sdk_client.sdk_backend.sdk_backend_exceptions.AllocatorScriptParserException: Error in the last layers of the model, expected conv but found concat layer.

2. 错误原因分析

这是一个由于后处理层（Post-processing）不兼容导致的经典错误。

  - YOLOv8 默认导出机制：Ultralytics 官方在导出 ONNX 模型时，会在网络末尾附加 Concat（拼接）和 Sigmoid
    等操作，将多个检测头（Head）的输出合并为一个单一的张量。
  - Hailo 硬件 NMS 限制：Hailo 芯片的硬件级非极大值抑制（NMS）模块不支持这种软件逻辑生成的拼接层。Hailo 编译器（及其对应的
    .alls 配置文件）严格要求输入 NMS 模块的是 最原始的 6 个卷积层 (Conv) 的输出（即 3 个特征图尺度的 bbox 回归分支和 3 个
    cls 分类分支）。
  - 冲突点：由于没有手动指定截断点，解析器回退到了模型最末端的 Concat 节点，导致 Hailo 在 NMS
    匹配阶段“期望找到卷积层，却找到了拼接层”，从而崩溃。

3. 解决方案

通过在编译命令中添加 --end-node-names 参数，强制 Hailo 解析器在拼接层发生之前提前截断模型图，直接提取这 6 个卷积层的输出。

修改前的失败命令：

hailomz compile yolov8s --hw-arch hailo8l --ckpt=best.onnx --calib-path dataset --classes 1 --performance 

修改后的成功命令：

hailomz compile yolov8s --hw-arch hailo8l --ckpt=best.onnx --calib-path dataset --classes 1 --performance --end-node-names /model.22/cv2.0/cv2.0.2/Conv /model.22/cv3.0/cv3.0.2/Conv /model.22/cv2.1/cv2.1.2/Conv /model.22/cv3.1/cv3.1.2/Conv /model.22/cv2.2/cv2.2.2/Conv /model.22/cv3.2/cv3.2.2/Conv

执行上述命令后，日志中会出现 End nodes mapped from original model: '/model.22/cv2... 并顺利进入
Calibration（量化校准）阶段。

4. 进阶排错：如何寻找自定义模型的 End Nodes？

如果你修改了 YOLOv8 的网络结构（例如增加了 P2 极小目标检测头，或者更改了网络深度），上述节点名称可能会发生变化。

你可以使用 Hailo 提供的 parser 工具自动侦测并打印正确的端点名称：

hailo parser onnx best.onnx

运行后，查看终端输出的日志。Hailo 通常会自动识别 YOLOv8 结构并给出提示，类似如下：

[info] NMS structure of yolov8 was detected. [info] In order to use HailoRT
post-processing capabilities, these end node names should be used:
/model.xx/cv... 

直接复制这段提示中以 /Conv 结尾的节点列表，替换到 --end-node-names 参数后即可。

PS:这是因为官方的模型编译时有yaml文件，把Conv层都标注好了。所以个人模型编译时也要写yaml文件或手动添加--end-node-names

5. 补充说明：量化校准警告 (Calibration Warning)

在成功编译的过程中，可能会看到如下黄色警告：

[warning] Reducing optimization level to 0 ... because there's less data than
the recommended amount (1024), and there's no available GPU

  - 原因：由于编译环境处于纯 CPU 状态，且校准图片（calib_images）数量不足官方推荐的 1024 张（如仅有 64 张）。
  - 影响：编译器为了节省纯 CPU 运算的时间，自动关闭了高级量化优化。这不会导致编译失败，依旧会输出 .hef
    模型，但在最终硬件上推理时，可能会出现一定的精度下降。
  - 建议：如果是为了快速打通部署流程，此警告可忽略；如果是为了生成最终的生产环境模型，建议提供 500~1000 张真实场景图片作为校准集，并在配置了
    NVIDIA GPU 的机器上进行编译。



### 结语

以上就是在树莓派5上配置 Hailo-8L AI Hat 的基础流程。希望这份记录能帮你少走一些弯路！

如果后续有关于 C++ 推理代码、模型转换或性能调优的实践，也欢迎继续补充。
