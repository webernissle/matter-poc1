# CMakeLists.txt for nRF52840 Dongle Matter Temperature Sensor
# Requires: nRF Connect SDK v2.6 or later with Matter support
#
# Build with:
#   west build -b nrf52840dongle/nrf52840 -- -DCONF_FILE=prj.conf

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nrf52840_dongle_temperature_sensor
    VERSION 1.0.0
    DESCRIPTION "Matter Temperature Sensor for nRF52840 Dongle"
)

target_sources(app PRIVATE
    src/main.cpp
)
