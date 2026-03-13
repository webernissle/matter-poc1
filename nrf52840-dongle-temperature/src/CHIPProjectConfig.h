/*
 * CHIPProjectConfig.h – Project-level Matter configuration overrides
 *
 * These values override the defaults from the connectedhomeip SDK.
 * The discriminator and passcode set here must match prj.conf.
 */
#pragma once

/* Use the nRF Connect SDK's KConfig values for device identity */
#define CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID         CONFIG_CHIP_DEVICE_VENDOR_ID
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID        CONFIG_CHIP_DEVICE_PRODUCT_ID
#define CHIP_DEVICE_CONFIG_DEVICE_HARDWARE_VERSION  CONFIG_CHIP_DEVICE_HARDWARE_VERSION
#define CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION  CONFIG_CHIP_DEVICE_SOFTWARE_VERSION

/* Enable BLE commissioning */
#define CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE 1

/* Thread (OpenThread) transport */
#define CHIP_DEVICE_CONFIG_ENABLE_THREAD 1
#define CHIP_DEVICE_CONFIG_THREAD_TASK_STACK_SIZE 8192

/* Logging */
#define CHIP_DEVICE_CONFIG_LOG_PROVISIONING 1
