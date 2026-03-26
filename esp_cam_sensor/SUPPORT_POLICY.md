The latest support policy for esp_cam_sensor can be found at [SUPPORT_POLICY](https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor/SUPPORT_POLICY.md) .

# Support Policy

This document outlines the support strategy for integrating camera sensors into your projects.

## Camera Sensor Categorization
Based on output data format, camera sensors are classified into two primary categories:

*   **RAW Sensor (Bayer Sensor)**
    This sensor type lacks an integrated Image Signal Processor (ISP) and outputs unprocessed Bayer pattern (RAW) data. Subsequent ISP processing on the host System-on-Chip (SoC) is required to convert this data into display-ready YUV or RGB format images.

*   **YUV/RGB Sensor**
    In contrast to RAW sensors, this category incorporates an internal ISP pipeline, enabling direct output of processed YUV or RGB format video data, thereby simplifying the integration process.

## Driver Development Guidelines
The driver development approach differs based on sensor type:

*   **YUV/RGB Sensor**: For sensors with native YUV or RGB output, developers can typically implement drivers for core functionality by consulting the provided [**Driver Development Guide**](https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor#steps-to-add-a-new-camera-sensor-driver).

*   **RAW Sensor**: For RAW sensors, achieving operational imaging is merely the first step. Attaining target image quality necessitates a subsequent, specialized **Image Quality (IQ) Tuning** process.

## ISP Image Quality Tuning for RAW Sensors
Utilizing a RAW sensor requires dedicated **Image Quality (IQ) Tuning**, which involves optimizing ISP algorithmic parameters (commonly configured via [JSON files](https://github.com/espressif/esp-video-components/tree/master/esp_ipa#3-json-configuration)). This activity is characterized by:

1.  **Specialized Equipment Prerequisite**: Professional tuning mandates standardized tools including 24-patch color charts, multi-illuminant light booths, color temperature meters, and optical test charts for objective image quality analysis.
2.  **Resource-Intensive Customization**: Parameter optimization is highly specific to the **camera module (sensor and lens combination)** and the **intended application scenario** (e.g., biometric recognition, video conferencing, machine vision). The process demands extensive, iterative testing and calibration by qualified imaging engineers.

Given the specialized expertise, equipment, and time investment required, it is advisable for users to refer to the relevant documentation [Espressif Image Process Algorithm for ISP](https://github.com/espressif/esp-video-components/tree/master/esp_ipa#espressif-image-process-algorithm-for-isp) and contact our technical support team.

## Recommended Engagement Workflow
For projects involving RAW sensors that require optimized image quality, we advise the following engagement sequence:

1.  **Requirements Submission**: Initiate contact by submitting a detailed project brief to our [**Business Development Team**](https://www.espressif.com/en/contact-us/sales-questions) or sales@espressif.com for preliminary technical-commercial review.
2.  **Feasibility Assessment**: Joint evaluation of project scope, resource allocation, and engagement model based on submitted requirements.
3.  **Tuning Scheduling**: Upon project confirmation and agreement, dedicated ISP tuning resources will be scheduled accordingly.

To expedite the initial assessment, please prepare the following information for the business team:

| Information Category | Details Requested |
| :--- | :--- |
| **Target Platform** | ESP series chip/module designated for the project (if confirmed) |
| **Connectivity Requirements** | Is Wi-Fi and/or Bluetooth Low Energy (BLE) functionality required? |
| **Camera Selection Status** | Identified camera sensor or module part number (if available) |
| **Vendor Support** | If a sensor/module is selected, is support available from the vendor's Field Application Engineer (FAE)? |
| **Sensor Output Capability** | If selected, what is the confirmed sensor output format (e.g., RAW10, RAW12)? |
| **Performance Specifications** | Target resolution, frame rate, and desired output data format for the application |
| **Optical Specifications** | Required depth of field (DoF) and horizontal/vertical field of view (FoV), if specified |
| **Primary Use Case** | Main application (e.g., barcode scanning, facial authentication, video streaming, industrial inspection) |

**Note**: Providing comprehensive and precise information will significantly enhance the efficiency and accuracy of our support proposal.