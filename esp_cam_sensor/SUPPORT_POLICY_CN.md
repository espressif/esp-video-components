有关 esp_cam_sensor 的最新支持政策，请参考 [SUPPORT_POLICY](https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor/SUPPORT_POLICY_CN.md) 。

# 技术支持政策

本文件阐述了相机传感器集成的技术支持框架，旨在促进有效的项目规划。

## 相机传感器分类
根据输出数据格式，相机传感器主要分为两类：

*   **RAW 传感器（拜耳传感器）**
    此类传感器未集成图像信号处理器（ISP），输出未处理的拜耳模式（RAW）数据。需要在主机系统级芯片（SoC）上进行后续的ISP处理，以将此数据转换为可供显示的 YUV 或 RGB 格式图像。

*   **YUV/RGB 传感器**
    与 RAW 传感器相比，此类传感器内部集成了 ISP 管道，能够直接输出已处理的 YUV 或 RGB 格式视频数据，从而简化集成流程。

## 驱动开发指南
根据传感器类型的不同，驱动开发方法有所区别：

*   **YUV/RGB 传感器**：对于原生支持YUV或RGB输出的传感器，开发者通常可以参考提供的 [**Driver Development Guide**](https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor#steps-to-add-a-new-camera-sensor-driver) 来实现对应的驱动程序。

*   **RAW 传感器**：对于RAW传感器，实现基本成像功能仅是第一步。要达到目标图像质量，还需要进行后续专门的**图像质量（IQ）调优**。

## RAW 传感器的 ISP 图像质量调优
使用 RAW 传感器需要进行专门的**图像质量（IQ）调优**，这涉及优化 ISP 算法参数（通常通过 [JSON 文件](https://github.com/espressif/esp-video-components/tree/master/esp_ipa#3-json-configuration)配置）。此项工作的特点如下：

1.  **专业设备要求**：专业的调优必须使用标准化工具，包括24色色卡、多光源灯箱、色温计和光学测试卡等，以进行客观的图像质量分析。
2.  **资源密集型的定制化**：参数优化高度依赖于具体的**相机模组（传感器与镜头组合）** 和**目标应用场景**（例如生物识别、视频会议、机器视觉）。该过程需要合格的成像工程师进行大量、反复的测试和校准。

鉴于所需的专业知识、设备和时间投入，建议用户阅读 [Espressif Image Process Algorithm for ISP](https://github.com/espressif/esp-video-components/tree/master/esp_ipa#espressif-image-process-algorithm-for-isp)，并与我们的技术人员取得联系。

## 推荐合作流程
对于涉及 RAW 传感器且需要优化图像质量的项目，我们建议遵循以下合作步骤：

1.  **需求提交**：首先通过向我司[**业务发展团队**](https://www.espressif.com/en/contact-us/sales-questions) 或者 sales@espressif.com 提交详细的项目概要来启动合作，以便进行初步的技术与商业评估。
2.  **可行性评估**：基于提交的需求，共同评估项目范围、资源分配和合作模式。
3.  **调优排期**：在项目确认并达成一致后，将相应地安排专门的 ISP 调优资源。

为加快初步评估，请为业务团队准备以下信息：

| 信息类别 | 所需细节 |
| :--- | :--- |
| **目标平台** | 项目指定的ESP系列芯片/模组（如已确认） |
| **连接性需求** | 是否需要Wi-Fi 和/或 蓝牙低能耗（BLE）功能 |
| **相机选型状态** | 已确定的相机传感器或模组型号（如有） |
| **供应商支持** | 如果已选定传感器/模组，能否获得供应商现场应用工程师（FAE）的支持 |
| **传感器输出能力** | 如果已选定，确认的传感器输出格式是什么（例如：RAW10, RAW12） |
| **性能规格** | 应用所需的目标分辨率、帧率及期望的输出数据格式 |
| **光学规格** | 所需的景深（DoF）及水平/垂直视场角（FoV），如有特定要求 |
| **主要用例** | 主要应用（例如：条码扫描、人脸认证、视频流、工业检测） |

**注意**：提供全面且准确的信息将极大提升我们技术支持方案评估的效率和准确性。