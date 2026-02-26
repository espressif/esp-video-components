/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include "esp_err.h"
#include "esp_log.h"
#include "example_video_common.h"
#include "esp_console.h"
#include "v4l2_cmd.h"

#if CONFIG_EXAMPLE_SELECT_ESP32P4_FUNCTION_EV_BOARD_V1_5
#ifndef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#pragma message("ESP32-P4-Function-EV-Board V1.5 uses the USB serial port; please select ESP_CONSOLE_USB_SERIAL_JTAG to enable the console USB serial port.")
#endif
#endif

#if EXAMPLE_TINYUSB_MSC_STORAGE
static example_storage_handle_t s_storage_handle;

/**
 * @brief Get the storage handle
 *
 * @return Storage handle
 */
example_storage_handle_t example_storage_get_handle(void)
{
    return s_storage_handle;
}
#endif

void app_main(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(example_video_init());

#if EXAMPLE_TINYUSB_MSC_STORAGE
#if CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH
    ESP_ERROR_CHECK(example_mount_msc_to_spiflash(&s_storage_handle));
#elif CONFIG_EXAMPLE_STORAGE_MEDIA_SDMMC
    ESP_ERROR_CHECK(example_mount_msc_to_mmc(&s_storage_handle));
#else
#error Unsupported storage media
#endif
#else
#pragma message("TinyUSB MSC storage is not enabled")
#endif

    repl_config.prompt = CONFIG_EXAMPLE_V4L2_CMD_PROMPT;
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#else
#error Unsupported console type
#endif

#ifdef CONFIG_EXAMPLE_V4L2_CMD_CTL
    v4l2_cmd_ctl_register();
#endif

#ifdef CONFIG_EXAMPLE_V4L2_CMD_BF
    v4l2_cmd_bf_register();
#endif

#ifdef CONFIG_EXAMPLE_V4L2_CMD_CCM
    v4l2_cmd_ccm_register();
#endif

#ifdef CONFIG_EXAMPLE_V4L2_CMD_GAMMA
    v4l2_cmd_gamma_register();
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
