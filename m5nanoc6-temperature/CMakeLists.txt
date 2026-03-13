# CMakeLists.txt for M5NanoC6 Matter Temperature Sensor
# Requires: ESP-IDF v5.2+ and ESP Matter SDK

cmake_minimum_required(VERSION 3.16)

# Set the IDF_PATH and ESP_MATTER_PATH before including
# these are normally set as environment variables during build
if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR "ESP_MATTER_PATH environment variable not set. "
            "Please source the ESP Matter SDK setup script first.")
endif()

set(EXTRA_COMPONENT_DIRS
    $ENV{ESP_MATTER_PATH}/components
    $ENV{ESP_MATTER_PATH}/connectedhomeip/connectedhomeip/src/platform/ESP32
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(m5nanoc6_temperature_sensor)
