/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#include "esp_partition.h"
#include "wear_levelling.h"
#include "esp_vfs_fat.h"
#if CONFIG_SOC_SDMMC_HOST_SUPPORTED
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#endif /* CONFIG_SOC_SDMMC_HOST_SUPPORTED */
#if CONFIG_EXAMPLE_SDMMC_DEBUG_PIN_CONNECTIONS
#include "sd_test_io.h"
#endif /* CONFIG_EXAMPLE_SDMMC_DEBUG_PIN_CONNECTIONS */
#include "example_video_common.h"
#if EXAMPLE_TINYUSB_MSC_STORAGE
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */
#if EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif /* EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO */

/**
 * @brief Example TinyUSB endpoint number
 */
#define EPNUM_MSC                               1
#define TUSB_DESC_TOTAL_LEN                     (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

#define EXAMPLE_IS_UHS1                         (CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_SDR50 || CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_DDR50)

/**
 * @brief Example SPI flash mount point
 */
#define SPI_FLASH_MOUNT_POINT                   CONFIG_EXAMPLE_SPI_FLASH_MOUNT_POINT

/**
 * @brief Example SPI flash partition label
 */
#define EXAMPLE_SPI_FLASH_PARTITION_LABEL       CONFIG_EXAMPLE_SPI_FLASH_PARTITION_LABEL

/**
 * @brief Example SDMMC mount point
 */
#define SDMMC_MOUNT_POINT                       CONFIG_EXAMPLE_SDMMC_MOUNT_POINT

/**
 * @brief Example format storage if mount failed
 */
#define EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED  CONFIG_EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED

/**
 * @brief Example format storage
 */
#define EXAMPLE_FORMAT_STORAGE                  CONFIG_EXAMPLE_FORMAT_STORAGE

/**
 * @brief Example FATFS max files
 */
#define FATFS_MAX_FILES                         5

/**
 * @brief Example FATFS SDMMC allocation unit size
 */
#define FATFS_SDMMC_ALLOCATION_UNIT_SIZE        (16 * 1024)

/**
 * @brief Example MSC max files
 */
#define MSC_MAX_FILES                           5

/**
 * @brief Example TinyUSB interface number
 */
enum {
    ITF_NUM_MSC = 0, /**< TinyUSB interface number is MSC */
    ITF_NUM_TOTAL
};

/**
 * @brief Example TinyUSB endpoint number
 */
enum {
    EDPT_CTRL_OUT = 0x00, /**< TinyUSB endpoint number is CTRL_OUT */
    EDPT_CTRL_IN  = 0x80, /**< TinyUSB endpoint number is CTRL_IN */

    EDPT_MSC_OUT  = 0x01, /**< TinyUSB endpoint number is MSC_OUT */
    EDPT_MSC_IN   = 0x81, /**< TinyUSB endpoint number is MSC_IN */
};

/**
 * @brief Example storage medium type
 */
typedef enum example_storage_medium_type {
    EXAMPLE_STORAGE_MEDIUM_SPI_FLASH, /**< Storage medium type is SPI flash */
    EXAMPLE_STORAGE_MEDIUM_MMC, /**< Storage medium type is SD/MMC card */
} example_storage_medium_type_t;

/**
 * @brief Example storage file system type
 */
typedef enum example_storage_fs_type {
    EXAMPLE_STORAGE_FS_TYPE_FATFS, /**< Storage file system type is FATFS */
    EXAMPLE_STORAGE_FS_TYPE_MSC, /**< Storage file system type is MSC */
} example_storage_fs_type_t;

/**
 * @brief Example storage medium
 */
typedef struct example_storage_medium {
    example_storage_medium_type_t medium_type; /**< Storage medium type */
    example_storage_fs_type_t fs_type; /**< Storage file system type */
    union {
        struct example_storage_fatfs_spiflash {
            wl_handle_t wl_handle; /**< Wear levelling handle */
        } spi_flash;

#if CONFIG_SOC_SDMMC_HOST_SUPPORTED
        struct example_storage_fatfs_mmc {
            sdmmc_host_t host; /**< SDMMC host handle */
            sdmmc_card_t *card; /**< SDMMC card handle */
#if EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
            sd_pwr_ctrl_handle_t pwr_ctrl_handle;  /*!< Power control handle */
#endif /* EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO */
        } mmc;
#endif /* CONFIG_SOC_SDMMC_HOST_SUPPORTED */
    };
} __attribute__((aligned(4))) example_storage_medium_t;

/**
 * @brief Example storage FATFS
 */
typedef struct example_storage_fatfs {
    example_storage_medium_t medium; /**< Storage medium */
} __attribute__((aligned(4))) example_storage_fatfs_t;

#if EXAMPLE_TINYUSB_MSC_STORAGE
/**
 * @brief Example storage MSC
 */
typedef struct example_storage_msc {
    example_storage_medium_t medium; /**< Storage medium */
    tinyusb_msc_storage_handle_t storage_hdl; /**< TinyUSB MSC storage handle */
} __attribute__((aligned(4))) example_storage_msc_t;
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */

#ifdef CONFIG_EXAMPLE_SDMMC_DEBUG_PIN_CONNECTIONS
static const char *names[] = {
    "CLK",
    "CMD",
    "D0",
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    "D1",
    "D2",
    "D3",
#endif /* CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4 */
};
static const int pins[] = {
    EXAMPLE_SDMMC_CLK_PIN,
    EXAMPLE_SDMMC_CMD_PIN,
    EXAMPLE_SDMMC_D0_PIN,
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    EXAMPLE_SDMMC_D1_PIN,
    EXAMPLE_SDMMC_D2_PIN,
    EXAMPLE_SDMMC_D3_PIN,
#endif /* CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4 */
};
static const int pin_count = sizeof(pins) / sizeof(pins[0]);

#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE
static const int adc_channels[] = {
    CONFIG_EXAMPLE_ADC_CLK_PIN_CHANNEL,
    CONFIG_EXAMPLE_ADC_CMD_PIN_CHANNEL,
    CONFIG_EXAMPLE_ADC_D0_PIN_CHANNEL,
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    CONFIG_EXAMPLE_ADC_D1_PIN_CHANNEL,
    CONFIG_EXAMPLE_ADC_D2_PIN_CHANNEL,
    CONFIG_EXAMPLE_ADC_D3_PIN_CHANNEL,
#endif /* CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4 */
};
#endif /* CONFIG_EXAMPLE_ENABLE_ADC_FEATURE */

static pin_configuration_t config = {
    .names = names,
    .pins = pins,
#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE
    .adc_channels = adc_channels,
#endif /* CONFIG_EXAMPLE_ENABLE_ADC_FEATURE */
};
#endif /* CONFIG_EXAMPLE_SDMMC_DEBUG_PIN_CONNECTIONS */

#if EXAMPLE_TINYUSB_MSC_STORAGE
static const tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, // Espressif VID for development. Change for production!
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

static const uint8_t msc_fs_configuration_desc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

#if (TUD_OPT_HIGH_SPEED)
static const tusb_desc_device_qualifier_t device_qualifier = {
    .bLength = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved = 0
};

static const uint8_t msc_hs_configuration_desc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 512),
};
#endif /* TUD_OPT_HIGH_SPEED */

static const char *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: is supported language is English (0x0409)
    "TinyUSB",                      // 1: Manufacturer
    "TinyUSB Device",               // 2: Product
    "123456",                       // 3: Serials
    "Example MSC",                  // 4. MSC
};
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */

static const char *TAG = "example_storage";
static bool s_is_spiflash_initialized = false;
#if CONFIG_SOC_SDMMC_HOST_SUPPORTED
static bool s_is_mmc_initialized = false;
#endif /* CONFIG_SOC_SDMMC_HOST_SUPPORTED */

#if EXAMPLE_TINYUSB_MSC_STORAGE
/**
 * @brief Storage mount changed callback
 *
 * @param handle Storage handle
 * @param event Event information
 * @param arg User argument, provided during callback registration
 */
static void storage_mount_changed_cb(tinyusb_msc_storage_handle_t handle, tinyusb_msc_event_t *event, void *arg)
{
    switch (event->id) {
    case TINYUSB_MSC_EVENT_MOUNT_START:
        // Verify that all the files are closed before unmounting
        break;
    case TINYUSB_MSC_EVENT_MOUNT_COMPLETE:
        ESP_LOGI(TAG, "Storage mounted to application: %s", (event->mount_point == TINYUSB_MSC_STORAGE_MOUNT_APP) ? "Yes" : "No");
        break;
    case TINYUSB_MSC_EVENT_MOUNT_FAILED:
    case TINYUSB_MSC_EVENT_FORMAT_REQUIRED:
        ESP_LOGE(TAG, "Storage mount failed or format required");
        break;
    default:
        break;
    }
}
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */

/**
 * @brief Initialize SPI flash and wear levelling
 *
 * @param medium Example storage medium
 * @param mount_wl Whether to mount the SPI flash to wear levelling
 *
 * @return ESP_OK if success, otherwise error code
 */
static esp_err_t example_storage_init_spiflash(example_storage_medium_t *medium, bool mount_wl)
{
    esp_err_t ret;

    medium->medium_type = EXAMPLE_STORAGE_MEDIUM_SPI_FLASH;

    if (mount_wl) {
        ESP_LOGI(TAG, "Initializing SPI flash wear levelling");

        const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, EXAMPLE_SPI_FLASH_PARTITION_LABEL);
        ESP_RETURN_ON_FALSE(data_partition, ESP_ERR_NOT_FOUND, TAG, "Failed to find FATFS partition. Check the partition table.");
        ESP_GOTO_ON_ERROR(wl_mount(data_partition, &medium->spi_flash.wl_handle), fail0, TAG, "Failed to mount SPI flash to wear levelling");
    }

    return ESP_OK;

fail0:
    return ret;
}

/**
 * @brief Deinitialize SPI flash and wear levelling
 *
 * @param medium Example storage medium
 * @param unmount_wl Whether to unmount the SPI flash wear levelling
 *
 * @return ESP_OK if success, otherwise error code
 */
static esp_err_t example_storage_deinit_spiflash(example_storage_medium_t *medium, bool unmount_wl)
{
    esp_err_t ret = ESP_OK;

    if (unmount_wl) {
        ESP_RETURN_ON_ERROR(wl_unmount(medium->spi_flash.wl_handle), TAG, "Failed to unmount SPI flash wear levelling");
    }

    return ret;
}

#if CONFIG_SOC_SDMMC_HOST_SUPPORTED
/**
 * @brief Initialize SD card configuration and create power control driver
 *
 * @param medium Example storage medium
 * @param slot_config SD card slot configuration pointer
 *
 * @return ESP_OK if success, otherwise error code
 */
static esp_err_t example_storage_init_mmc(example_storage_medium_t *medium, sdmmc_slot_config_t *slot_config)
{
    esp_err_t ret = ESP_OK;

    medium->medium_type = EXAMPLE_STORAGE_MEDIUM_MMC;

    /**
     * By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
     * For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
     * Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
     */

    sdmmc_host_t host_default = SDMMC_HOST_DEFAULT();
    medium->mmc.host = host_default;

    /**
     * By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
     * For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
     * Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
     */
#if CONFIG_EXAMPLE_SDMMC_SPEED_HS
    medium->mmc.host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
#elif CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_SDR50
    medium->mmc.host.slot = SDMMC_HOST_SLOT_0;
    medium->mmc.host.max_freq_khz = SDMMC_FREQ_SDR50;
    medium->mmc.host.flags &= ~SDMMC_HOST_FLAG_DDR;
#elif CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_DDR50
    medium->mmc.host.slot = SDMMC_HOST_SLOT_0;
    medium->mmc.host.max_freq_khz = SDMMC_FREQ_DDR50;
#endif /* CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_DDR50 */

    /**
     * For SoCs where the SD power can be supplied both via an internal or external (e.g. on-board LDO) power supply.
     * When using specific IO pins (which can be used for ultra high-speed SDMMC) to connect to the SD card
     * and the internal LDO power supply, we need to initialize the power supply first.
     */
#if EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = EXAMPLE_SD_PWR_CTRL_LDO_IO_ID,
    };

    ESP_RETURN_ON_ERROR(sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &medium->mmc.pwr_ctrl_handle), TAG, "Failed to create a new on-chip LDO power control driver");

    medium->mmc.host.pwr_ctrl_handle = medium->mmc.pwr_ctrl_handle;
#endif /* EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO */

    /**
     * This initializes the slot without card detect (CD) and write protect (WP) signals.
     * Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
     */
    sdmmc_slot_config_t slot_config_default = SDMMC_SLOT_CONFIG_DEFAULT();
    *slot_config = slot_config_default;

    /* For SD Card, set bus width to use */
#ifdef EXAMPLE_SDMMC_BUS_WIDTH_4
    slot_config->width = 4;
#else /* EXAMPLE_SDMMC_BUS_WIDTH_4 */
    slot_config->width = 1;
#endif  /* EXAMPLE_SDMMC_BUS_WIDTH_4 */

    /* On chips where the GPIOs used for SD card can be configured, set the user defined values */
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config->clk = EXAMPLE_SDMMC_CLK_PIN;
    slot_config->cmd = EXAMPLE_SDMMC_CMD_PIN;
    slot_config->d0 = EXAMPLE_SDMMC_D0_PIN;
#ifdef EXAMPLE_SDMMC_BUS_WIDTH_4
    slot_config->d1 = EXAMPLE_SDMMC_D1_PIN;
    slot_config->d2 = EXAMPLE_SDMMC_D2_PIN;
    slot_config->d3 = EXAMPLE_SDMMC_D3_PIN;
#endif  /* EXAMPLE_SDMMC_BUS_WIDTH_4 */
#endif  /* CONFIG_SOC_SDMMC_USE_GPIO_MATRIX */

    /**
     * This initializes the slot without card detect (CD) and write protect (WP) signals.
     * Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
     */
#if EXAMPLE_IS_UHS1
    slot_config->flags |= SDMMC_SLOT_FLAG_UHS1;
#endif /* EXAMPLE_IS_UHS1 */

    /**
     * Enable internal pullups on enabled pins. The internal pullups
     * are insufficient however, please make sure 10k external pullups are
     * connected on the bus. This is for debug / example purpose only.
     */
    slot_config->flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    return ret;
}

/**
 * @brief Deinitialize SD card configuration and delete power control driver
 *
 * @param medium Example storage medium
 *
 * @return ESP_OK if success, otherwise error code
 */
static esp_err_t example_storage_deinit_mmc(example_storage_medium_t *medium)
{
    esp_err_t ret = ESP_OK;

#if EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    ESP_RETURN_ON_ERROR(sd_pwr_ctrl_del_on_chip_ldo(medium->mmc.pwr_ctrl_handle), TAG, "Failed to delete on-chip LDO power control driver");
#endif /* EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO */

    return ret;
}
#endif /* CONFIG_SOC_SDMMC_HOST_SUPPORTED */

/**
 * @brief Initialize SPI flash and mount to FATFS
 *
 * @param handle_ret Example storage handle pointer
 *
 * @return ESP_OK if success, otherwise error code
 *
 * @note: This function is not thread-safe.
 */
esp_err_t example_mount_fatfs_to_spiflash(example_storage_handle_t *handle_ret)
{
    esp_err_t ret;
    example_storage_fatfs_t *fatfs;

    ESP_RETURN_ON_FALSE(handle_ret != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    ESP_RETURN_ON_FALSE(s_is_spiflash_initialized == false, ESP_ERR_INVALID_STATE, TAG, "SPI flash is already initialized");

    fatfs = (example_storage_fatfs_t *)heap_caps_malloc(sizeof(example_storage_fatfs_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (fatfs == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for fatfs");
        return ESP_ERR_NO_MEM;
    }

    ESP_GOTO_ON_ERROR(example_storage_init_spiflash(&fatfs->medium, false), fail0, TAG, "Failed to initialize SPI flash");
    fatfs->medium.fs_type = EXAMPLE_STORAGE_FS_TYPE_FATFS;

    ESP_LOGI(TAG, "Mounting FATFS to SPI flash");

    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = FATFS_MAX_FILES, // Number of files that can be open at a time
#if EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED
        .format_if_mount_failed = true, // If true, try to format the partition if mount fails
#else /* EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED */
        .format_if_mount_failed = false, // If false, do not format the partition if mount fails
#endif /* EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED */
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE, // Size of allocation unit, cluster size.
        .use_one_fat = false, // Use only one FAT table (reduce memory usage), but decrease reliability of file system in case of power failure.
    };

    // Mount FATFS filesystem located on "storage" partition in read-write mode
    ESP_GOTO_ON_ERROR(esp_vfs_fat_spiflash_mount_rw_wl(SPI_FLASH_MOUNT_POINT, EXAMPLE_SPI_FLASH_PARTITION_LABEL, &mount_config, &fatfs->medium.spi_flash.wl_handle), fail1, TAG, "Failed to mount FATFS");

#if EXAMPLE_FORMAT_STORAGE
    ESP_GOTO_ON_ERROR(esp_vfs_fat_spiflash_format_rw_wl(SPI_FLASH_MOUNT_POINT, EXAMPLE_SPI_FLASH_PARTITION_LABEL), fail2, TAG, "Failed to format storage");
#endif /* EXAMPLE_FORMAT_STORAGE */

    *handle_ret = fatfs;
    s_is_spiflash_initialized = true;

    return ESP_OK;

#if EXAMPLE_FORMAT_STORAGE
fail2:
    esp_vfs_fat_spiflash_unmount_rw_wl(SPI_FLASH_MOUNT_POINT, fatfs->medium.spi_flash.wl_handle);
#endif /* EXAMPLE_FORMAT_STORAGE */
fail1:
    example_storage_deinit_spiflash(&fatfs->medium, false);
fail0:
    heap_caps_free(fatfs);
    return ret;
}

/**
 * @brief Unmount the SPI flash from FATFS and free the resources
 *
 * @param handle Example storage handle
 *
 * @return ESP_OK if success, otherwise error code
 *
 * @note: This function is not thread-safe.
 */
esp_err_t example_unmount_fatfs_in_spiflash(example_storage_handle_t handle)
{
    esp_err_t ret = ESP_OK;
    example_storage_fatfs_t *fatfs = (example_storage_fatfs_t *)handle;

    ESP_RETURN_ON_FALSE(fatfs != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    ESP_RETURN_ON_FALSE(s_is_spiflash_initialized == true, ESP_ERR_INVALID_STATE, TAG, "SPI flash is not initialized");

    ESP_RETURN_ON_ERROR(esp_vfs_fat_spiflash_unmount_rw_wl(SPI_FLASH_MOUNT_POINT, fatfs->medium.spi_flash.wl_handle), TAG, "Failed to unmount SPI flash");
    ESP_RETURN_ON_ERROR(example_storage_deinit_spiflash(&fatfs->medium, false), TAG, "Failed to deinitialize SPI flash");

    heap_caps_free(fatfs);

    s_is_spiflash_initialized = false;

    ESP_LOGI(TAG, "Unmounted SPI flash from FATFS");

    return ret;
}

#if EXAMPLE_TINYUSB_MSC_STORAGE
/**
 * @brief Initialize MSC and mount to SPI flash
 *
 * @param handle_ret Example storage handle pointer
 *
 * @return ESP_OK if success, otherwise error code
 *
 * @note: This function is not thread-safe.
 */
esp_err_t example_mount_msc_to_spiflash(example_storage_handle_t *handle_ret)
{
    esp_err_t ret;
    example_storage_msc_t *msc;

    ESP_RETURN_ON_FALSE(handle_ret != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    ESP_RETURN_ON_FALSE(s_is_spiflash_initialized == false, ESP_ERR_INVALID_STATE, TAG, "SPI flash is already initialized");

    msc = (example_storage_msc_t *)heap_caps_malloc(sizeof(example_storage_msc_t), MALLOC_CAP_INTERNAL);
    if (msc == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for msc");
        return ESP_ERR_NO_MEM;
    }

    ESP_GOTO_ON_ERROR(example_storage_init_spiflash(&msc->medium, true), fail0, TAG, "Failed to initialize SPI flash");
    msc->medium.fs_type = EXAMPLE_STORAGE_FS_TYPE_MSC;

    ESP_LOGI(TAG, "Mounting MSC to SPI flash");

    tinyusb_msc_storage_config_t storage_cfg = {
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,       // Initial mount point to USB
        .fat_fs = {
            .base_path = SPI_FLASH_MOUNT_POINT,             // Use default base path
            .config = {
                .max_files = MSC_MAX_FILES,              // Maximum number of files that can be opened simultaneously
            },
#if EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED
            .do_not_format = false,                         // If false, format the partition if mount fails
#else /* EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED */
            .do_not_format = true,                          // If true, do not format the partition if mount fails
#endif /* EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED */
        },
        .medium = {
            .wl_handle = msc->medium.spi_flash.wl_handle,
        },
    };

    ESP_GOTO_ON_ERROR(tinyusb_msc_new_storage_spiflash(&storage_cfg, &msc->storage_hdl), fail1, TAG, "Failed to initialize MSC in SPI flash");
    ESP_GOTO_ON_ERROR(tinyusb_msc_set_storage_callback(storage_mount_changed_cb, NULL), fail2, TAG, "Failed to set storage callback");

#if EXAMPLE_FORMAT_STORAGE
    ESP_GOTO_ON_ERROR(tinyusb_msc_format_storage(msc->storage_hdl), fail2, TAG, "Failed to format storage");
#endif /* EXAMPLE_FORMAT_STORAGE */

    ESP_LOGI(TAG, "USB MSC initialization");
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.device = &descriptor_config;
    tusb_cfg.descriptor.full_speed_config = msc_fs_configuration_desc;
    tusb_cfg.descriptor.string = string_desc_arr;
    tusb_cfg.descriptor.string_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = msc_hs_configuration_desc;
    tusb_cfg.descriptor.qualifier = &device_qualifier;
#endif /* TUD_OPT_HIGH_SPEED */
    ESP_GOTO_ON_ERROR(tinyusb_driver_install(&tusb_cfg), fail2, TAG, "Failed to install TinyUSB driver");

    ESP_LOGI(TAG, "Mount storage...");
    ESP_GOTO_ON_ERROR(tinyusb_msc_set_storage_mount_point(msc->storage_hdl, TINYUSB_MSC_STORAGE_MOUNT_APP), fail3, TAG, "Failed to set MSC mount point to application");

    s_is_spiflash_initialized = true;
    *handle_ret = msc;

    return ESP_OK;

fail3:
    tinyusb_driver_uninstall();
    // fall through
fail2:
    tinyusb_msc_delete_storage(msc->storage_hdl);
fail1:
    example_storage_deinit_spiflash(&msc->medium, true);
fail0:
    heap_caps_free(msc);
    return ret;
}

/**
 * @brief Unmount MSC in SPI flash
 *
 * @param handle Example storage handle
 *
 * @return ESP_OK if success, otherwise error code
 *
 * @note: This function is not thread-safe.
 */
esp_err_t example_unmount_msc_from_spiflash(example_storage_handle_t handle)
{
    esp_err_t ret = ESP_OK;
    example_storage_msc_t *msc = (example_storage_msc_t *)handle;

    ESP_RETURN_ON_FALSE(msc != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    ESP_RETURN_ON_FALSE(s_is_spiflash_initialized == true, ESP_ERR_INVALID_STATE, TAG, "SPI flash is not initialized");

    ESP_RETURN_ON_ERROR(tinyusb_driver_uninstall(), TAG, "Failed to uninstall TinyUSB driver");
    ESP_RETURN_ON_ERROR(tinyusb_msc_delete_storage(msc->storage_hdl), TAG, "Failed to delete SPI flash storage");
    ESP_RETURN_ON_ERROR(example_storage_deinit_spiflash(&msc->medium, true), TAG, "Failed to deinitialize SPI flash");

    heap_caps_free(msc);

    s_is_spiflash_initialized = false;

    return ret;
}
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */

#if CONFIG_SOC_SDMMC_HOST_SUPPORTED
/**
 * @brief Initialize SD card and mount FATFS to SD card
 *
 * @param handle_ret Example storage handle pointer
 *
 * @return ESP_OK if success, otherwise error code
 *
 * @note: This function is not thread-safe.
 */
esp_err_t example_mount_fatfs_to_mmc(example_storage_handle_t *handle_ret)
{
    esp_err_t ret;
    example_storage_fatfs_t *fatfs;
    sdmmc_slot_config_t slot_config;

    ESP_RETURN_ON_FALSE(handle_ret != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    ESP_RETURN_ON_FALSE(s_is_mmc_initialized == false, ESP_ERR_INVALID_STATE, TAG, "MMC is already initialized");

    fatfs = (example_storage_fatfs_t *)heap_caps_malloc(sizeof(example_storage_fatfs_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (fatfs == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for fatfs");
        return ESP_ERR_NO_MEM;
    }

    ESP_GOTO_ON_ERROR(example_storage_init_mmc(&fatfs->medium, &slot_config), fail0, TAG, "Failed to initialize SD card configuration");
    fatfs->medium.fs_type = EXAMPLE_STORAGE_FS_TYPE_FATFS;

    ESP_LOGI(TAG, "Mounting SD card to FATFS");

    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = FATFS_MAX_FILES, // Number of files that can be open at a time
#if EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED
        .format_if_mount_failed = true, // If true, try to format the partition if mount fails
#else /* EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED */
        .format_if_mount_failed = false, // If false, do not format the partition if mount fails
#endif /* EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED */
        .allocation_unit_size = FATFS_SDMMC_ALLOCATION_UNIT_SIZE, // Size of allocation unit, cluster size.
    };

    // Mount FATFS filesystem located on "storage" partition in read-write mode
    ESP_GOTO_ON_ERROR(esp_vfs_fat_sdmmc_mount(SDMMC_MOUNT_POINT, &fatfs->medium.mmc.host, &slot_config, &mount_config, &fatfs->medium.mmc.card), fail1, TAG, "Failed to mount FATFS");

#if EXAMPLE_FORMAT_STORAGE
    ESP_GOTO_ON_ERROR(esp_vfs_fat_sdcard_format(SDMMC_MOUNT_POINT, fatfs->medium.mmc.card), fail2, TAG, "Failed to format storage");
#endif /* EXAMPLE_FORMAT_STORAGE */

    sdmmc_card_print_info(stdout, fatfs->medium.mmc.card);

    *handle_ret = fatfs;
    s_is_mmc_initialized = true;

    return ESP_OK;

#if EXAMPLE_FORMAT_STORAGE
fail2:
    esp_vfs_fat_sdcard_unmount(SDMMC_MOUNT_POINT, fatfs->medium.mmc.card);
#endif /* EXAMPLE_FORMAT_STORAGE */
fail1:
    if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "Failed to mount filesystem. "
                 "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
    } else {
        ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                 "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));

#ifdef CONFIG_EXAMPLE_SDMMC_DEBUG_PIN_CONNECTIONS
        check_sd_card_pins(&config, pin_count);
#endif /* CONFIG_EXAMPLE_SDMMC_DEBUG_PIN_CONNECTIONS */
    }
    example_storage_deinit_mmc(&fatfs->medium);
fail0:
    heap_caps_free(fatfs);
    return ret;
}

/**
 * @brief Unmount FATFS in SD card and deinitialize SD card
 *
 * @param handle Example storage handle
 *
 * @return ESP_OK if success, otherwise error code
 *
 * @note: This function is not thread-safe.
 */
esp_err_t example_unmount_fatfs_in_mmc(example_storage_handle_t handle)
{
    esp_err_t ret = ESP_OK;
    example_storage_fatfs_t *fatfs = (example_storage_fatfs_t *)handle;

    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    ESP_RETURN_ON_FALSE(s_is_mmc_initialized == true, ESP_ERR_INVALID_STATE, TAG, "MMC is not initialized");

    ESP_RETURN_ON_ERROR(esp_vfs_fat_sdcard_unmount(SDMMC_MOUNT_POINT, fatfs->medium.mmc.card), TAG, "Failed to unmount SD card");
    ESP_RETURN_ON_ERROR(example_storage_deinit_mmc(&fatfs->medium), TAG, "Failed to deinitialize SD card");

    heap_caps_free(fatfs);

    s_is_mmc_initialized = false;

    return ret;
}

#if EXAMPLE_TINYUSB_MSC_STORAGE
/**
 * @brief Initialize MSC and mount to SD card
 *
 * @param handle_ret Example storage handle pointer
 *
 * @return ESP_OK if success, otherwise error code
 *
 * @note: This function is not thread-safe.
 */
esp_err_t example_mount_msc_to_mmc(example_storage_handle_t *handle_ret)
{
    esp_err_t ret;
    example_storage_msc_t *msc;
    sdmmc_slot_config_t slot_config;

    ESP_RETURN_ON_FALSE(handle_ret != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    ESP_RETURN_ON_FALSE(s_is_mmc_initialized == false, ESP_ERR_INVALID_STATE, TAG, "MMC is already initialized");

    msc = (example_storage_msc_t *)heap_caps_malloc(sizeof(example_storage_msc_t) + sizeof(sdmmc_card_t), MALLOC_CAP_INTERNAL);
    if (msc == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for msc");
        return ESP_ERR_NO_MEM;
    }
    msc->medium.mmc.card = (sdmmc_card_t *)(((uint8_t *)msc) + sizeof(example_storage_msc_t));

    ESP_GOTO_ON_ERROR(example_storage_init_mmc(&msc->medium, &slot_config), fail0, TAG, "Failed to initialize SD card configuration");
    msc->medium.fs_type = EXAMPLE_STORAGE_FS_TYPE_MSC;

    ESP_GOTO_ON_ERROR((*msc->medium.mmc.host.init)(), fail1, TAG, "Host Config Init fail");
    ESP_GOTO_ON_ERROR(sdmmc_host_init_slot(msc->medium.mmc.host.slot, &slot_config), fail2, TAG, "Host init slot fail");

    ESP_GOTO_ON_ERROR(sdmmc_card_init(&msc->medium.mmc.host, msc->medium.mmc.card), fail2, TAG, "Failed to initialize SD card");

    ESP_LOGI(TAG, "Mounting MSC to SD card");

    tinyusb_msc_storage_config_t storage_cfg = {
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,       // Initial mount point to USB
        .fat_fs = {
            .base_path = SDMMC_MOUNT_POINT,                 // Use default base path
            .config.max_files = MSC_MAX_FILES,              // Maximum number of files that can be opened simultaneously
#if EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED
            .do_not_format = false,                         // If false, format the partition if mount fails
#else /* EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED */
            .do_not_format = true,                          // If true, do not format the partition if mount fails
#endif /* EXAMPLE_FORMAT_STORAGE_IF_MOUNT_FAILED */
            .format_flags = 0,                              // No special format flags
        },
        .medium = {
            .card = msc->medium.mmc.card,
        },
    };

    ESP_GOTO_ON_ERROR(tinyusb_msc_new_storage_sdmmc(&storage_cfg, &msc->storage_hdl), fail2, TAG, "Failed to initialize MSC in SD card");
    ESP_GOTO_ON_ERROR(tinyusb_msc_set_storage_callback(storage_mount_changed_cb, NULL), fail3, TAG, "Failed to set storage callback");

#if EXAMPLE_FORMAT_STORAGE
    ESP_GOTO_ON_ERROR(tinyusb_msc_format_storage(msc->storage_hdl), fail3, TAG, "Failed to format storage");
#endif /* EXAMPLE_FORMAT_STORAGE */

    ESP_LOGI(TAG, "USB MSC initialization");
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.device = &descriptor_config;
    tusb_cfg.descriptor.full_speed_config = msc_fs_configuration_desc;
    tusb_cfg.descriptor.string = string_desc_arr;
    tusb_cfg.descriptor.string_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = msc_hs_configuration_desc;
    tusb_cfg.descriptor.qualifier = &device_qualifier;
#endif /* TUD_OPT_HIGH_SPEED */
    ESP_GOTO_ON_ERROR(tinyusb_driver_install(&tusb_cfg), fail3, TAG, "Failed to install TinyUSB driver");

    ESP_LOGI(TAG, "Mount storage...");
    ESP_GOTO_ON_ERROR(tinyusb_msc_set_storage_mount_point(msc->storage_hdl, TINYUSB_MSC_STORAGE_MOUNT_APP), fail4, TAG, "Failed to mount SD card to %s", SDMMC_MOUNT_POINT);

    s_is_mmc_initialized = true;
    *handle_ret = msc;

    return ESP_OK;

fail4:
    tinyusb_driver_uninstall();
fail3:
    tinyusb_msc_delete_storage(msc->storage_hdl);
fail2:
    if (msc->medium.mmc.host.flags & SDMMC_HOST_FLAG_DEINIT_ARG) {
        msc->medium.mmc.host.deinit_p(msc->medium.mmc.host.slot);
    } else {
        (*msc->medium.mmc.host.deinit)();
    }
fail1:
    example_storage_deinit_mmc(&msc->medium);
fail0:
    heap_caps_free(msc);
    return ret;
}

/**
 * @brief Unmount MSC in SD card
 *
 * @param handle Example storage handle
 *
 * @return ESP_OK if success, otherwise error code
 *
 * @note: This function is not thread-safe.
 */
esp_err_t example_unmount_msc_from_mmc(example_storage_handle_t handle)
{
    esp_err_t ret = ESP_OK;
    example_storage_msc_t *msc = (example_storage_msc_t *)handle;

    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    ESP_RETURN_ON_FALSE(s_is_mmc_initialized == true, ESP_ERR_INVALID_STATE, TAG, "MMC is not initialized");

    ESP_RETURN_ON_ERROR(tinyusb_driver_uninstall(), TAG, "Failed to uninstall TinyUSB driver");
    ESP_RETURN_ON_ERROR(tinyusb_msc_delete_storage(msc->storage_hdl), TAG, "Failed to delete SD card storage");

    if (msc->medium.mmc.host.flags & SDMMC_HOST_FLAG_DEINIT_ARG) {
        ESP_RETURN_ON_ERROR(msc->medium.mmc.host.deinit_p(msc->medium.mmc.host.slot), TAG, "Failed to deinitialize SD card host");
    } else {
        ESP_RETURN_ON_ERROR((*msc->medium.mmc.host.deinit)(), TAG, "Failed to deinitialize SD card host");
    }

    ESP_RETURN_ON_ERROR(example_storage_deinit_mmc(&msc->medium), TAG, "Failed to deinitialize SD card");

    heap_caps_free(msc);

    s_is_mmc_initialized = false;

    return ret;
}
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */
#endif /* CONFIG_SOC_SDMMC_HOST_SUPPORTED */

#if EXAMPLE_TINYUSB_MSC_STORAGE
/**
 * @brief Check if the MSC storage is in use by USB host
 *
 * @param handle Example storage handle
 * @param is_in_use Pointer to store the result
 *
 * @return ESP_OK if success, otherwise error code
 *
 * @note: This function is not thread-safe.
 */
esp_err_t example_msc_storage_in_use_by_usb_host(example_storage_handle_t handle, bool *is_in_use)
{
    tinyusb_msc_mount_point_t mount_point;
    example_storage_msc_t *msc = (example_storage_msc_t *)handle;
    bool msc_initialized;

    ESP_RETURN_ON_FALSE(msc != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    ESP_RETURN_ON_FALSE(is_in_use != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid is_in_use pointer");

    msc_initialized = (msc->medium.medium_type == EXAMPLE_STORAGE_MEDIUM_SPI_FLASH && s_is_spiflash_initialized);
#if CONFIG_SOC_SDMMC_HOST_SUPPORTED
    msc_initialized |= (msc->medium.medium_type == EXAMPLE_STORAGE_MEDIUM_MMC && s_is_mmc_initialized);
#endif /* CONFIG_SOC_SDMMC_HOST_SUPPORTED */
    ESP_RETURN_ON_FALSE(msc_initialized, ESP_ERR_INVALID_STATE, TAG, "MSC is not initialized");
    ESP_RETURN_ON_ERROR(tinyusb_msc_get_storage_mount_point(msc->storage_hdl, &mount_point), TAG, "Failed to get storage mount point");

    *is_in_use = mount_point == TINYUSB_MSC_STORAGE_MOUNT_USB;
    return ESP_OK;
}
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */

/**
 * @brief Get the capacity of the storage
 *
 * @param handle Example storage handle
 * @param capacity Pointer to store the capacity of the storage in bytes
 *
 * @return ESP_OK if success, otherwise error code
 *
 * @note: This function is not thread-safe.
 */
esp_err_t example_storage_get_capacity(example_storage_handle_t handle, uint64_t *capacity)
{
    esp_err_t ret = ESP_FAIL;
    example_storage_medium_t *medium = (example_storage_medium_t *)handle;

    ESP_RETURN_ON_FALSE(medium != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    ESP_RETURN_ON_FALSE(capacity != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid capacity pointer");

    bool storage_initialized = (medium->medium_type == EXAMPLE_STORAGE_MEDIUM_SPI_FLASH && s_is_spiflash_initialized);
#if CONFIG_SOC_SDMMC_HOST_SUPPORTED
    storage_initialized |= (medium->medium_type == EXAMPLE_STORAGE_MEDIUM_MMC && s_is_mmc_initialized);
#endif /* CONFIG_SOC_SDMMC_HOST_SUPPORTED */
    ESP_RETURN_ON_FALSE(storage_initialized, ESP_ERR_INVALID_STATE, TAG, "Storage is not initialized");

    if (medium->fs_type == EXAMPLE_STORAGE_FS_TYPE_FATFS) {
        uint64_t out_total_bytes;
        uint64_t out_free_bytes;
        const char *mount_point = medium->medium_type == EXAMPLE_STORAGE_MEDIUM_SPI_FLASH ? SPI_FLASH_MOUNT_POINT : SDMMC_MOUNT_POINT;

        ESP_RETURN_ON_ERROR(esp_vfs_fat_info(mount_point, &out_total_bytes, &out_free_bytes), TAG, "Failed to get storage capacity");
        *capacity = out_total_bytes;
        ret = ESP_OK;
    } else if (medium->fs_type == EXAMPLE_STORAGE_FS_TYPE_MSC) {
#if EXAMPLE_TINYUSB_MSC_STORAGE
        example_storage_msc_t *msc = (example_storage_msc_t *)handle;
        uint32_t sector_count;
        uint32_t sector_size;

        ESP_RETURN_ON_ERROR(tinyusb_msc_get_storage_capacity(msc->storage_hdl, &sector_count), TAG, "Failed to get storage capacity");
        ESP_RETURN_ON_ERROR(tinyusb_msc_get_storage_sector_size(msc->storage_hdl, &sector_size), TAG, "Failed to get storage sector size");

        *capacity = (uint64_t)sector_count * sector_size;
        ret = ESP_OK;
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */
    }

    return ret;
}
