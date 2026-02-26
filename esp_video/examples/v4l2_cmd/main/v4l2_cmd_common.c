/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/errno.h>

/**
 * @brief Decode the device name from the string
 *
 * @param dev_str Device name string
 * @param buffer Buffer to store the decoded device name
 * @param buffer_size Buffer size
 *
 * @return 0 on success, -1 on failure
 */
int decode_dev_name(const char *dev_str, char *buffer, size_t buffer_size)
{
    if (dev_str == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }

    if (isdigit((int)dev_str[0])) {
        char *endptr;
        errno = 0;
        long devno = strtol(dev_str, &endptr, 10);

        if (errno != 0 || endptr == dev_str) {
            return -1;
        } else if (devno < 0 || devno > UINT8_MAX) {
            return -1;
        } else if (*endptr != '\0') {
            return -1;
        }

        int len = snprintf(buffer, buffer_size, "/dev/video%d", (int)devno);
        if (len < 0 || len >= buffer_size) {
            return -1;
        }
    } else {
        const char dev_base[] = "/dev/video";

        if (strlen(dev_str) >= buffer_size) {
            return -1;
        } else if (memcmp(dev_str, dev_base, sizeof(dev_base) - 1) != 0) {
            return -1;
        }

        int len = snprintf(buffer, buffer_size, "%s", dev_str);
        if (len < 0 || len >= buffer_size) {
            return -1;
        }
    }

    return 0;
}
