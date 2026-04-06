/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#include "esp_openthread_types.h"

/**
 * Default OpenThread radio configuration for ESP platform bring-up.
 *
 * Uses native radio mode provided by ESP32-C6/H2 OpenThread integration.
 *
 * Official references:
 * - ESP-IDF OpenThread API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/network/esp_openthread.html
 * - OpenThread project docs: https://openthread.io/guides
 */
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()                                           \
    {                                                                                   \
        .radio_mode = RADIO_MODE_NATIVE,                                                \
    }

/**
 * Default OpenThread host connection configuration.
 *
 * `HOST_CONNECTION_MODE_NONE` is used for an embedded single-chip setup where
 * Thread stack and host run on the same SoC.
 */
#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()                                            \
    {                                                                                   \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,                              \
    }

/**
 * Default OpenThread platform port configuration.
 *
 * - `storage_partition_name`: NVS partition used for persistent OpenThread data.
 * - `netif_queue_size`: network interface queue depth.
 * - `task_queue_size`: OpenThread task queue depth.
 */
#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()                                            \
    {                                                                                   \
        .storage_partition_name = "nvs",                                                \
        .netif_queue_size = 10,                                                         \
        .task_queue_size = 10,                                                          \
    }
#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD
