#include "esp_sensor.h"

esp_err_t esp_sensor_ioctl(esp_sensor_device_t handle, uint32_t cmd, void *value, size_t *size)
{
    esp_err_t ret = ESP_OK;
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_sensor_ops_t *ops = (esp_sensor_ops_t *)handle;
    switch (cmd) {
    case CAM_SENSOR_G_NAME:
        if (ops->get_name) {
            ops->get_name(value, size);
        }
        break;
    case CAM_SENSOR_G_FORMAT_ARRAY:
        if (ops->query_support_formats(value)) {
            ret = ESP_FAIL;
        }
        break;
    case CAM_SENSOR_G_FORMAT:
        if (ops->get_format(value)) {
            ret = ESP_FAIL;
        }
        break;
    case CAM_SENSOR_G_CAP:
        if (ops->query_support_capability(value)) {
            ret = ESP_FAIL;
        }
        break;
    case CAM_SENSOR_S_FORMAT:
        if (ops->set_format(value)) {
            ret = ESP_FAIL;
        }
        break;
    default:
        if (ops->priv_ioctl(cmd, value)) {
            ret = ESP_FAIL;
        }
        break;
    }
    return ret;
}