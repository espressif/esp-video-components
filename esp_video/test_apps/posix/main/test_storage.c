/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "unity.h"

#include "example_video_common.h"

TEST_CASE("FATFS mount and unmount in SPI flash", "[video]")
{
    example_storage_handle_t handle;
    int count = 32;
    const char *mount_point = CONFIG_EXAMPLE_SPI_FLASH_MOUNT_POINT;

    for (int i = 0; i < count; i++)  {
        TEST_ESP_OK(example_mount_fatfs_to_spiflash(&handle));

        char name[64];
        snprintf(name, sizeof(name), "%s/test_%04d.txt", mount_point, i);
        int fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0666);
        TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
        int ret = close(fd);
        TEST_ASSERT_EQUAL_INT(0, ret);
        ret = unlink(name);
        TEST_ASSERT_EQUAL_INT(0, ret);

        printf("File %s testing is passed\n", name);

        TEST_ESP_OK(example_unmount_fatfs_in_spiflash(handle));
    }
}

#if EXAMPLE_TINYUSB_MSC_STORAGE
TEST_CASE("MSC mount and unmount in SPI flash", "[video]")
{
    example_storage_handle_t handle;
    int count = 32;
    const char *mount_point = CONFIG_EXAMPLE_SPI_FLASH_MOUNT_POINT;

    for (int i = 0; i < count; i++)  {
        TEST_ESP_OK(example_mount_msc_to_spiflash(&handle));

        char name[64];
        snprintf(name, sizeof(name), "%s/test_%04d.txt", mount_point, i);
        int fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0666);
        TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
        int ret = close(fd);
        TEST_ASSERT_EQUAL_INT(0, ret);
        ret = unlink(name);
        TEST_ASSERT_EQUAL_INT(0, ret);

        printf("File %s testing is passed\n", name);

        TEST_ESP_OK(example_unmount_msc_from_spiflash(handle));
    }
}
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */

#if CONFIG_SOC_SDMMC_HOST_SUPPORTED
TEST_CASE("FATFS mount and unmount in SD card", "[video]")
{
    example_storage_handle_t handle;
    int count = 32;
    const char *mount_point = CONFIG_EXAMPLE_SDMMC_MOUNT_POINT;

    for (int i = 0; i < count; i++)  {
        TEST_ESP_OK(example_mount_fatfs_to_mmc(&handle));

        char name[64];
        snprintf(name, sizeof(name), "%s/test_%04d.txt", mount_point, i);
        int fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0666);
        TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
        int ret = close(fd);
        TEST_ASSERT_EQUAL_INT(0, ret);
        ret = unlink(name);
        TEST_ASSERT_EQUAL_INT(0, ret);

        printf("File %s testing is passed\n", name);

        TEST_ESP_OK(example_unmount_fatfs_in_mmc(handle));
    }
}

#if EXAMPLE_TINYUSB_MSC_STORAGE
TEST_CASE("MSC mount and unmount in SD card", "[video]")
{
    example_storage_handle_t handle;
    int count = 32;
    const char *mount_point = CONFIG_EXAMPLE_SDMMC_MOUNT_POINT;

    for (int i = 0; i < count; i++)  {
        TEST_ESP_OK(example_mount_msc_to_mmc(&handle));

        char name[64];
        snprintf(name, sizeof(name), "%s/test_%04d.txt", mount_point, i);
        int fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0666);
        TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
        int ret = close(fd);
        TEST_ASSERT_EQUAL_INT(0, ret);
        ret = unlink(name);
        TEST_ASSERT_EQUAL_INT(0, ret);

        printf("File %s testing is passed\n", name);

        TEST_ESP_OK(example_unmount_msc_from_mmc(handle));
    }
}
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */
#endif /* CONFIG_SOC_SDMMC_HOST_SUPPORTED */
