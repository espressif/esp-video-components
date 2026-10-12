/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "unity.h"
#include "esp_cam_sensor.h"
#include "esp_cam_ircut.h"
#include "esp_video_cam.h"
#include "esp_video_ioctl.h"

#define TEST_CTRL_ID_MAX 16

static esp_cam_ircut_mode_t s_ircut_mode;
static int s_ircut_set_count;
static uint32_t s_ircut_last_cmd;

static int stub_sensor_query_gain_vflip(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    (void)dev;

    if (qdesc->id != ESP_CAM_SENSOR_GAIN && qdesc->id != ESP_CAM_SENSOR_VFLIP) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
    qdesc->number.minimum = 0;
    qdesc->number.maximum = 16;
    qdesc->number.step = 1;
    qdesc->default_value = 0;
    return ESP_OK;
}

static int stub_sensor_query_none(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    (void)dev;
    (void)qdesc;

    return ESP_ERR_NOT_SUPPORTED;
}

static const esp_cam_sensor_ops_t s_sensor_ops_gain_vflip = {
    .query_para_desc = stub_sensor_query_gain_vflip,
};

static const esp_cam_sensor_ops_t s_sensor_ops_none = {
    .query_para_desc = stub_sensor_query_none,
};

static int stub_ircut_ioctl(esp_cam_ircut_device_t *dev, uint32_t cmd, void *arg)
{
    esp_cam_ircut_mode_t mode;

    if (!arg) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    s_ircut_last_cmd = cmd;
    if (cmd == ESP_CAM_IRCUT_IOC_SET_MODE) {
        mode = *(esp_cam_ircut_mode_t *)arg;
        if (mode != ESP_CAM_IRCUT_MODE_DAY && mode != ESP_CAM_IRCUT_MODE_NIGHT) {
            return ESP_ERR_INVALID_ARG;
        }
        s_ircut_mode = mode;
        dev->current_mode = mode;
        s_ircut_set_count++;
        return ESP_OK;
    }
    if (cmd == ESP_CAM_IRCUT_IOC_GET_MODE) {
        *(esp_cam_ircut_mode_t *)arg = dev->current_mode;
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

static const esp_cam_ircut_ops_t s_ircut_ops = {
    .priv_ioctl = stub_ircut_ioctl,
};

static void init_cam(esp_video_cam_t *cam, esp_cam_sensor_device_t *sensor, const esp_cam_sensor_ops_t *ops,
                     esp_cam_ircut_device_t *ircut)
{
    memset(sensor, 0, sizeof(*sensor));
    sensor->name = "stub";
    sensor->ops = ops;

    memset(cam, 0, sizeof(*cam));
    cam->sensor = sensor;
    cam->ircut = ircut;
}

static int enum_control_ids(esp_video_cam_t *cam, uint32_t *ids, int max_ids, struct v4l2_query_ext_ctrl *ircut_qctrl)
{
    struct v4l2_query_ext_ctrl qctrl;
    int count = 0;

    memset(&qctrl, 0, sizeof(qctrl));
    while (count < max_ids) {
        qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        esp_err_t ret = esp_video_cam_query_ext_ctrls(cam, &qctrl);
        if (ret != ESP_OK) {
            TEST_ASSERT_EQUAL_HEX32(ESP_ERR_INVALID_ARG, ret);
            break;
        }

        ids[count++] = qctrl.id;
        if (ircut_qctrl && qctrl.id == V4L2_CID_IRCUT_MODE) {
            *ircut_qctrl = qctrl;
        }
    }

    TEST_ASSERT_LESS_THAN_INT(max_ids, count);
    return count;
}

static void assert_ircut_query(const struct v4l2_query_ext_ctrl *qctrl)
{
    TEST_ASSERT_EQUAL_HEX32(V4L2_CID_IRCUT_MODE, qctrl->id);
    TEST_ASSERT_EQUAL_INT(V4L2_CTRL_TYPE_MENU, qctrl->type);
    TEST_ASSERT_EQUAL_STRING("IRCUT Mode", qctrl->name);
    TEST_ASSERT_EQUAL_INT64(ESP_CAM_IRCUT_MODE_UNKNOWN, qctrl->minimum);
    TEST_ASSERT_EQUAL_INT64(ESP_CAM_IRCUT_MODE_NIGHT, qctrl->maximum);
    TEST_ASSERT_EQUAL_UINT64(1, qctrl->step);
    TEST_ASSERT_EQUAL_INT64(ESP_CAM_IRCUT_MODE_DAY, qctrl->default_value);
}

TEST_CASE("IR-CUT control is enumerated by V4L2_CTRL_FLAG_NEXT_CTRL", "[video]")
{
    esp_video_cam_t cam;
    esp_cam_sensor_device_t sensor;
    esp_cam_ircut_device_t ircut;
    struct v4l2_query_ext_ctrl direct;
    struct v4l2_query_ext_ctrl enumerated;
    struct v4l2_ext_control control;
    struct v4l2_ext_controls controls;
    uint32_t ids[TEST_CTRL_ID_MAX];
    int count;

    memset(&ircut, 0, sizeof(ircut));
    ircut.name = "stub-ircut";
    ircut.ops = &s_ircut_ops;
    ircut.current_mode = ESP_CAM_IRCUT_MODE_DAY;
    init_cam(&cam, &sensor, &s_sensor_ops_gain_vflip, &ircut);

    memset(&direct, 0, sizeof(direct));
    direct.id = V4L2_CID_IRCUT_MODE;
    TEST_ESP_OK(esp_video_cam_query_ext_ctrls(&cam, &direct));
    assert_ircut_query(&direct);

    memset(&enumerated, 0, sizeof(enumerated));
    count = enum_control_ids(&cam, ids, TEST_CTRL_ID_MAX, &enumerated);
    TEST_ASSERT_EQUAL_INT(3, count);
    TEST_ASSERT_EQUAL_HEX32(V4L2_CID_GAIN, ids[0]);
    TEST_ASSERT_EQUAL_HEX32(V4L2_CID_VFLIP, ids[1]);
    TEST_ASSERT_EQUAL_HEX32(V4L2_CID_IRCUT_MODE, ids[2]);
    assert_ircut_query(&enumerated);

    memset(&control, 0, sizeof(control));
    memset(&controls, 0, sizeof(controls));
    control.id = enumerated.id;
    control.value = ESP_CAM_IRCUT_MODE_NIGHT;
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count = 1;
    controls.controls = &control;

    s_ircut_mode = ESP_CAM_IRCUT_MODE_UNKNOWN;
    TEST_ESP_OK(esp_video_cam_set_ext_ctrls(&cam, &controls));
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_NIGHT, s_ircut_mode);

    init_cam(&cam, &sensor, &s_sensor_ops_gain_vflip, NULL);
    memset(&direct, 0, sizeof(direct));
    direct.id = V4L2_CID_IRCUT_MODE;
    TEST_ESP_ERR(ESP_ERR_NOT_SUPPORTED, esp_video_cam_query_ext_ctrls(&cam, &direct));

    count = enum_control_ids(&cam, ids, TEST_CTRL_ID_MAX, NULL);
    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_EQUAL_HEX32(V4L2_CID_GAIN, ids[0]);
    TEST_ASSERT_EQUAL_HEX32(V4L2_CID_VFLIP, ids[1]);
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_NOT_EQUAL_HEX32(V4L2_CID_IRCUT_MODE, ids[i]);
    }

    init_cam(&cam, &sensor, &s_sensor_ops_none, &ircut);
    count = enum_control_ids(&cam, ids, TEST_CTRL_ID_MAX, &enumerated);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_EQUAL_HEX32(V4L2_CID_IRCUT_MODE, ids[0]);
    assert_ircut_query(&enumerated);
}

TEST_CASE("IR-CUT S_EXT_CTRLS G_EXT_CTRLS round-trip", "[video]")
{
    esp_video_cam_t cam;
    esp_cam_sensor_device_t sensor;
    esp_cam_ircut_device_t ircut;
    struct v4l2_ext_control control;
    struct v4l2_ext_controls controls;

    memset(&ircut, 0, sizeof(ircut));
    ircut.name = "stub-ircut";
    ircut.ops = &s_ircut_ops;
    ircut.current_mode = ESP_CAM_IRCUT_MODE_DAY;
    init_cam(&cam, &sensor, &s_sensor_ops_none, &ircut);

    memset(&control, 0, sizeof(control));
    memset(&controls, 0, sizeof(controls));
    control.id = V4L2_CID_IRCUT_MODE;
    control.value = ESP_CAM_IRCUT_MODE_NIGHT;
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count = 1;
    controls.controls = &control;

    s_ircut_mode = ESP_CAM_IRCUT_MODE_UNKNOWN;
    s_ircut_set_count = 0;
    TEST_ESP_OK(esp_video_cam_set_ext_ctrls(&cam, &controls));
    TEST_ASSERT_EQUAL_INT(1, s_ircut_set_count);
    TEST_ASSERT_EQUAL_HEX32(ESP_CAM_IRCUT_IOC_SET_MODE, s_ircut_last_cmd);
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_NIGHT, ircut.current_mode);

    /* Zero-initialized value must be overwritten by the current mode, not applied as UNKNOWN. */
    memset(&control, 0, sizeof(control));
    control.id = V4L2_CID_IRCUT_MODE;
    TEST_ESP_OK(esp_video_cam_get_ext_ctrls(&cam, &controls));
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_NIGHT, control.value);
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_NIGHT, ircut.current_mode);
    TEST_ASSERT_EQUAL_INT(1, s_ircut_set_count);
    TEST_ASSERT_EQUAL_HEX32(ESP_CAM_IRCUT_IOC_GET_MODE, s_ircut_last_cmd);
}

TEST_CASE("IR-CUT fresh device G_EXT_CTRLS returns initial UNKNOWN mode", "[video]")
{
    esp_video_cam_t cam;
    esp_cam_sensor_device_t sensor;
    esp_cam_ircut_device_t ircut;
    struct v4l2_query_ext_ctrl qctrl;
    struct v4l2_ext_control control;
    struct v4l2_ext_controls controls;

    /* ap1511b_detect() calloc() leaves current_mode at UNKNOWN. Do not preset DAY. */
    memset(&ircut, 0, sizeof(ircut));
    ircut.name = "stub-ircut";
    ircut.ops = &s_ircut_ops;
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_UNKNOWN, ircut.current_mode);
    init_cam(&cam, &sensor, &s_sensor_ops_none, &ircut);

    memset(&qctrl, 0, sizeof(qctrl));
    qctrl.id = V4L2_CID_IRCUT_MODE;
    TEST_ESP_OK(esp_video_cam_query_ext_ctrls(&cam, &qctrl));
    assert_ircut_query(&qctrl);

    memset(&control, 0, sizeof(control));
    memset(&controls, 0, sizeof(controls));
    control.id = V4L2_CID_IRCUT_MODE;
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count = 1;
    controls.controls = &control;

    s_ircut_set_count = 0;
    s_ircut_last_cmd = 0;
    TEST_ESP_OK(esp_video_cam_get_ext_ctrls(&cam, &controls));
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_UNKNOWN, control.value);
    TEST_ASSERT_EQUAL_INT(ircut.current_mode, control.value);
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(qctrl.minimum, control.value);
    TEST_ASSERT_LESS_OR_EQUAL_INT64(qctrl.maximum, control.value);
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_UNKNOWN, ircut.current_mode);
    TEST_ASSERT_EQUAL_INT(0, s_ircut_set_count);
    TEST_ASSERT_EQUAL_HEX32(ESP_CAM_IRCUT_IOC_GET_MODE, s_ircut_last_cmd);
}

TEST_CASE("IR-CUT S_EXT_CTRLS rejects readable UNKNOWN mode", "[video]")
{
    esp_video_cam_t cam;
    esp_cam_sensor_device_t sensor;
    esp_cam_ircut_device_t ircut;
    struct v4l2_query_ext_ctrl qctrl;
    struct v4l2_querymenu qmenu;
    struct v4l2_ext_control control;
    struct v4l2_ext_controls controls;

    memset(&ircut, 0, sizeof(ircut));
    ircut.name = "stub-ircut";
    ircut.ops = &s_ircut_ops;
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_UNKNOWN, ircut.current_mode);
    init_cam(&cam, &sensor, &s_sensor_ops_none, &ircut);

    memset(&qctrl, 0, sizeof(qctrl));
    qctrl.id = V4L2_CID_IRCUT_MODE;
    TEST_ESP_OK(esp_video_cam_query_ext_ctrls(&cam, &qctrl));
    assert_ircut_query(&qctrl);

    /* Range includes UNKNOWN so GET is in range. QUERYMENU marks it non-writable. */
    memset(&qmenu, 0, sizeof(qmenu));
    qmenu.id = V4L2_CID_IRCUT_MODE;
    qmenu.index = ESP_CAM_IRCUT_MODE_UNKNOWN;
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, esp_video_cam_query_menu(&cam, &qmenu));

    memset(&qmenu, 0, sizeof(qmenu));
    qmenu.id = V4L2_CID_IRCUT_MODE;
    qmenu.index = ESP_CAM_IRCUT_MODE_DAY;
    TEST_ESP_OK(esp_video_cam_query_menu(&cam, &qmenu));
    TEST_ASSERT_EQUAL_STRING("Day", (const char *)qmenu.name);

    memset(&qmenu, 0, sizeof(qmenu));
    qmenu.id = V4L2_CID_IRCUT_MODE;
    qmenu.index = ESP_CAM_IRCUT_MODE_NIGHT;
    TEST_ESP_OK(esp_video_cam_query_menu(&cam, &qmenu));
    TEST_ASSERT_EQUAL_STRING("Night", (const char *)qmenu.name);

    memset(&qmenu, 0, sizeof(qmenu));
    qmenu.id = V4L2_CID_IRCUT_MODE;
    qmenu.index = ESP_CAM_IRCUT_MODE_NIGHT + 1;
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, esp_video_cam_query_menu(&cam, &qmenu));

    memset(&control, 0, sizeof(control));
    memset(&controls, 0, sizeof(controls));
    control.id = V4L2_CID_IRCUT_MODE;
    control.value = ESP_CAM_IRCUT_MODE_UNKNOWN;
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count = 1;
    controls.controls = &control;

    s_ircut_mode = ESP_CAM_IRCUT_MODE_DAY;
    s_ircut_set_count = 0;
    s_ircut_last_cmd = 0;
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, esp_video_cam_set_ext_ctrls(&cam, &controls));
    TEST_ASSERT_EQUAL_INT(0, s_ircut_set_count);
    TEST_ASSERT_EQUAL_HEX32(0, s_ircut_last_cmd);
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_UNKNOWN, ircut.current_mode);
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_DAY, s_ircut_mode);

    memset(&control, 0, sizeof(control));
    control.id = V4L2_CID_IRCUT_MODE;
    TEST_ESP_OK(esp_video_cam_get_ext_ctrls(&cam, &controls));
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_UNKNOWN, control.value);
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(qctrl.minimum, control.value);
    TEST_ASSERT_LESS_OR_EQUAL_INT64(qctrl.maximum, control.value);

    control.value = qctrl.default_value;
    TEST_ESP_OK(esp_video_cam_set_ext_ctrls(&cam, &controls));
    TEST_ASSERT_EQUAL_INT(1, s_ircut_set_count);
    TEST_ASSERT_EQUAL_HEX32(ESP_CAM_IRCUT_IOC_SET_MODE, s_ircut_last_cmd);
    TEST_ASSERT_EQUAL_INT(ESP_CAM_IRCUT_MODE_DAY, ircut.current_mode);
}
