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

## Image Quality (IQ) Tuning Preparation Guide

To ensure a smooth and efficient Image Quality (IQ) tuning process, please prepare and confirm the following information before submitting your tuning request:

### IQ Tuning Baseline Information Form

| Information Category | Required Details & Specifications |
| :--- | :--- |
| **Lens Specifications** | Focal length, Field of View (FOV), F-number (Aperture), Chief Ray Angle (CRA) curve, and Infrared (IR) filter specifications (e.g., 650nm / 850nm / 940nm). |
| **Illuminator / Supplemental Light (Optional)** | Light source type (White Light / 850nm IR / 940nm IR), driving method (PWM dimming / GPIO high-low level switch), maximum power, and beam angle. |
| **Display Specifications (Optional)** | Display panel model, material/technology (OLED / LCD), screen Gamma curve, and supported color space standards (e.g., sRGB). |
| **Ambient Light Sensor (Optional)** | The exact chip/part model of the ambient light sensor. |
| **IR-CUT Mechanism (Optional)** | The part model of the IR-CUT driver IC, along with the hardware and software linkage mechanism between the IR-CUT and the supplemental light. |
| **Focus Motor Specifications (Optional)** | The chip of the Auto-Focus (AF) motor. |

### Submission Guidelines & Prerequisites

* **Camera Module Verification**: Since optical characteristics have a decisive impact on final image quality, users must **fully define the camera module's FOV and focusing distance, and evaluate its baseline noise performance** prior to submitting a tuning request.
* **Golden Sample Management**: To ensure that the tuned image quality maintains excellent compatibility and consistency during mass production, users are required to ship the designated **Golden Sample** to the Project Manager (PM). This sample will serve as the absolute benchmark for quality assurance (QA) across subsequent production batches.
