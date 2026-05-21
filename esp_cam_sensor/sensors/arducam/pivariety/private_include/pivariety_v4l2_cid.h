/*
 * SPDX-FileCopyrightText: 2026 Arducam Electronic Technology (Nanjing) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define V4L2_CTRL_CLASS_USER                     0x00980000
#define V4L2_CID_BASE                            (V4L2_CTRL_CLASS_USER | 0x900)
#define V4L2_CID_ARDUCAM_BASE                    (V4L2_CID_BASE + 0x1000)

#define V4L2_CID_BRIGHTNESS                      (V4L2_CID_BASE+0)
#define V4L2_CID_CONTRAST                        (V4L2_CID_BASE+1)

#define V4L2_CID_RED_BALANCE                     (V4L2_CID_BASE+14)
#define V4L2_CID_BLUE_BALANCE                    (V4L2_CID_BASE+15)
#define V4L2_CID_EXPOSURE                        (V4L2_CID_BASE+17)

#define V4L2_CID_GAIN                            (V4L2_CID_BASE+19)
#define V4L2_CID_HFLIP                           (V4L2_CID_BASE+20)
#define V4L2_CID_VFLIP                           (V4L2_CID_BASE+21)

#define V4L2_CID_POWER_LINE_FREQUENCY            (V4L2_CID_BASE+24)

#define V4L2_CID_HUE                             (V4L2_CID_BASE+3)
#define V4L2_CID_SATURATION                      (V4L2_CID_BASE+2)
#define V4L2_CID_SHARPNESS                       (V4L2_CID_BASE+27)
#define V4L2_CID_GAMMA                           (V4L2_CID_BASE+16)
#define V4L2_CID_BACKLIGHT_COMPENSATION          (V4L2_CID_BASE+28)

#define V4L2_CTRL_CLASS_CAMERA                   0x009a0000
#define V4L2_CID_CAMERA_CLASS_BASE               (V4L2_CTRL_CLASS_CAMERA | 0x900)
#define V4L2_CID_FOCUS_ABSOLUTE                  (V4L2_CID_CAMERA_CLASS_BASE + 10)

#define V4L2_CID_ARDUCAM_EXT_TRI                 (V4L2_CID_ARDUCAM_BASE + 1)
#define V4L2_CID_ARDUCAM_TEGRA_DISABLE_TIMEOUT   (V4L2_CID_ARDUCAM_BASE + 2)
#define V4L2_CID_ARDUCAM_TEGRA_TIMEOUT           (V4L2_CID_ARDUCAM_BASE + 3)
#define V4L2_CID_ARDUCAM_TEGRA_DISABLE_ALIGN     (V4L2_CID_ARDUCAM_BASE + 4)
#define V4L2_CID_ARDUCAM_FACE_DETECTION          (V4L2_CID_ARDUCAM_BASE + 5)
#define V4L2_CID_ARDUCAM_FRAME_RATE              (V4L2_CID_ARDUCAM_BASE + 6)
#define V4L2_CID_ARDUCAM_EFFECTS                 (V4L2_CID_ARDUCAM_BASE + 7)
#define V4L2_CID_ARDUCAM_IRCUT                   (V4L2_CID_ARDUCAM_BASE + 8)
#define V4L2_CID_ARDUCAM_HDR                     (V4L2_CID_ARDUCAM_BASE + 9)
#define V4L2_CID_ARDUCAM_PAN_X_ABSOLUTE          (V4L2_CID_ARDUCAM_BASE + 10)
#define V4L2_CID_ARDUCAM_PAN_Y_ABSOLUTE          (V4L2_CID_ARDUCAM_BASE + 11)
#define V4L2_CID_ARDUCAM_ZOOM_PAN_SPEED          (V4L2_CID_ARDUCAM_BASE + 12)
#define V4L2_CID_ARDUCAM_DENOISE                 (V4L2_CID_ARDUCAM_BASE + 13)

#define V4L2_CTRL_CLASS_IMAGE_SOURCE             0x009e0000 /* Image source controls */
#define V4L2_CTRL_CLASS_IMAGE_PROC               0x009f0000 /* Image processing controls */

/* Image source controls */
#define V4L2_CID_IMAGE_SOURCE_CLASS_BASE         (V4L2_CTRL_CLASS_IMAGE_SOURCE | 0x900)
#define V4L2_CID_IMAGE_SOURCE_CLASS              (V4L2_CTRL_CLASS_IMAGE_SOURCE | 1)

#define V4L2_CID_VBLANK                          (V4L2_CID_IMAGE_SOURCE_CLASS_BASE + 1)
#define V4L2_CID_HBLANK                          (V4L2_CID_IMAGE_SOURCE_CLASS_BASE + 2)
#define V4L2_CID_ANALOGUE_GAIN                   (V4L2_CID_IMAGE_SOURCE_CLASS_BASE + 3)
#define V4L2_CID_DIGITAL_GAIN                    (V4L2_CID_IMAGE_SOURCE_CLASS_BASE + 5)

/* Image processing controls */

#define V4L2_CID_IMAGE_PROC_CLASS_BASE           (V4L2_CTRL_CLASS_IMAGE_PROC | 0x900)
#define V4L2_CID_IMAGE_PROC_CLASS                (V4L2_CTRL_CLASS_IMAGE_PROC | 1)

#define V4L2_CID_PIXEL_RATE                      (V4L2_CID_IMAGE_PROC_CLASS_BASE + 2)

/*
 *
 * Selection interface definitions
 *
*/
/* Current cropping area */
#define V4L2_SEL_TGT_CROP              0x0000
/* Default cropping area */
#define V4L2_SEL_TGT_CROP_DEFAULT      0x0001
/* Cropping bounds */
#define V4L2_SEL_TGT_CROP_BOUNDS       0x0002
/* Native frame size */
#define V4L2_SEL_TGT_NATIVE_SIZE       0x0003
/* Current composing area */
#define V4L2_SEL_TGT_COMPOSE           0x0100
/* Default composing area */
#define V4L2_SEL_TGT_COMPOSE_DEFAULT   0x0101
/* Composing bounds */
#define V4L2_SEL_TGT_COMPOSE_BOUNDS    0x0102
/* Current composing area plus all padding pixels */
#define V4L2_SEL_TGT_COMPOSE_PADDED    0x0103
