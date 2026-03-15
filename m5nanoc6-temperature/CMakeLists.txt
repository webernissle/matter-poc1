# CMakeLists.txt for M5NanoC6 Matter Temperature Sensor
# Requires: ESP-IDF v5.5.3+ and ESP Matter SDK v1.5

cmake_minimum_required(VERSION 3.16)

# Set the IDF_PATH and ESP_MATTER_PATH before including
# these are normally set as environment variables during build
if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR "ESP_MATTER_PATH environment variable not set. "
            "Please source the ESP Matter SDK setup script first.")
endif()

set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH ${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip)

# Include ESP Matter SDK components and connectedhomeip (provides chip component)
set(EXTRA_COMPONENT_DIRS
    "${MATTER_SDK_PATH}/config/esp32/components"
    "${ESP_MATTER_PATH}/components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(m5nanoc6_temperature_sensor)
