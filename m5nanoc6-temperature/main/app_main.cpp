/*
 * M5NanoC6 Matter Temperature Sensor
 *
 * This application implements a Matter-compliant temperature sensor using the
 * ESP32-C6's built-in temperature sensor on the M5NanoC6 development board.
 *
 * Device type : Temperature Sensor (0x0302)
 * Cluster     : Temperature Measurement (0x0402)
 *
 * The device commissions into Apple Home via a HomePod mini acting as a
 * Thread/Matter border router (Wi-Fi path is used here since the ESP32-C6
 * supports Wi-Fi but not Thread natively – the device joins the same IP
 * network as the HomePod mini and uses Matter over Wi-Fi).
 *
 * Build requirements:
 *   - ESP-IDF v5.2 or later
 *   - ESP Matter SDK (https://github.com/espressif/esp-matter)
 */

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>

#include <driver/temperature_sensor.h>

static const char *TAG = "m5nanoc6_temp_sensor";

/* Endpoint ID assigned at runtime */
static uint16_t s_temperature_endpoint_id = 0;

/* Handle for the ESP32-C6 internal temperature sensor */
static temperature_sensor_handle_t s_temp_sensor = NULL;

/* ------------------------------------------------------------------ */
/*  Matter callbacks                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Called when a Matter platform event is emitted (e.g. commissioning).
 */
static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete – device is now in the fabric");
        break;
    case chip::DeviceLayer::DeviceEventType::kInternetConnectivityChange:
        if (event->InternetConnectivityChange.IPv4 == chip::DeviceLayer::kConnectivity_Established) {
            ESP_LOGI(TAG, "IPv4 connectivity established");
        }
        break;
    default:
        break;
    }
}

/**
 * @brief Identification callback (blink LED, etc. – left as log for PoC).
 */
static esp_err_t app_identification_cb(
    esp_matter::identification::callback_type_t type,
    uint16_t endpoint_id,
    uint8_t effect_id,
    uint8_t effect_variant,
    void *priv_data)
{
    ESP_LOGI(TAG, "Identify: endpoint=%u effect=%u variant=%u", endpoint_id, effect_id, effect_variant);
    return ESP_OK;
}

/**
 * @brief Attribute update callback – called before and after writes.
 */
static esp_err_t app_attribute_update_cb(
    esp_matter::attribute::callback_type_t type,
    uint16_t endpoint_id,
    uint32_t cluster_id,
    uint32_t attribute_id,
    esp_matter_attr_val_t *val,
    void *priv_data)
{
    if (type == esp_matter::attribute::PRE_UPDATE) {
        ESP_LOGD(TAG, "Attribute PRE_UPDATE: ep=%u cluster=0x%" PRIx32 " attr=0x%" PRIx32,
                 endpoint_id, cluster_id, attribute_id);
    }
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  Temperature update task                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief FreeRTOS task that periodically reads the internal temperature sensor
 *        and updates the Matter MeasuredValue attribute.
 */
static void temperature_update_task(void *pvParameters)
{
    while (true) {
        float celsius = 0.0f;
        esp_err_t err = temperature_sensor_get_celsius(s_temp_sensor, &celsius);
        if (err == ESP_OK) {
            /*
             * The Matter Temperature Measurement cluster represents temperature
             * as a signed 16-bit integer in units of 0.01 °C.
             * E.g. 2500 = 25.00 °C
             */
            int16_t matter_temp = (int16_t)(celsius * 100.0f);

            esp_matter_attr_val_t val = esp_matter_int16(matter_temp);
            esp_matter::attribute::update(
                s_temperature_endpoint_id,
                chip::app::Clusters::TemperatureMeasurement::Id,
                chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Id,
                &val);

            ESP_LOGI(TAG, "Temperature: %.2f °C  (Matter raw: %" PRId16 ")", celsius, matter_temp);
        } else {
            ESP_LOGW(TAG, "Temperature read failed: %s", esp_err_to_name(err));
        }

        /* Update interval: 30 seconds (Apple Home polls on its own schedule) */
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

/* ------------------------------------------------------------------ */
/*  Main entry point                                                    */
/* ------------------------------------------------------------------ */

extern "C" void app_main(void)
{
    esp_err_t err = ESP_OK;

    /* ----- NVS (required by Matter for persistent storage) ----- */
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* ----- Internal temperature sensor ----- */
    temperature_sensor_config_t ts_cfg = {
        .range_min = -10,   /* °C, calibrated range for the ESP32-C6 sensor */
        .range_max = 80,
        .clk_src   = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT,
    };
    err = temperature_sensor_install(&ts_cfg, &s_temp_sensor);
    ESP_ERROR_CHECK(err);

    err = temperature_sensor_enable(s_temp_sensor);
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "Internal temperature sensor enabled");

    /* ----- Matter node ----- */
    esp_matter::node::config_t node_cfg;
    /* The product name shown in Apple Home */
    strncpy(node_cfg.root_node.basic_information.node_label,
            "M5NanoC6 Temperature",
            sizeof(node_cfg.root_node.basic_information.node_label) - 1);

    esp_matter::node_t *node = esp_matter::node::create(
        &node_cfg,
        app_attribute_update_cb,
        app_identification_cb);

    if (!node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return;
    }

    /* ----- Temperature Sensor endpoint ----- */
    esp_matter::endpoint::temperature_sensor::config_t ep_cfg;
    /*
     * MeasuredValue: 0x8000 = invalid/unknown (will be updated by task).
     * MinMeasuredValue: -1000  = -10.00 °C
     * MaxMeasuredValue:  8000  =  80.00 °C
     */
    ep_cfg.temperature_measurement.measured_value    = (int16_t)0x8000; /* invalid until first reading */
    ep_cfg.temperature_measurement.min_measured_value = -1000;
    ep_cfg.temperature_measurement.max_measured_value =  8000;

    esp_matter::endpoint_t *endpoint = esp_matter::endpoint::temperature_sensor::create(
        node, &ep_cfg, ENDPOINT_FLAG_NONE, NULL);

    if (!endpoint) {
        ESP_LOGE(TAG, "Failed to create temperature sensor endpoint");
        return;
    }

    s_temperature_endpoint_id = esp_matter::endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Temperature sensor endpoint id: %u", s_temperature_endpoint_id);

    /* ----- Console (optional, useful for debugging over USB serial) ----- */
#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_handler();
    esp_matter::console::wifi_register_handler();
    esp_matter::console::init();
#endif

    /* ----- Start Matter stack ----- */
    err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Matter stack: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Matter stack started – waiting for commissioning");
    ESP_LOGI(TAG, "Use the QR code or manual pairing code printed above to commission via Apple Home");

    /* ----- Kick off the temperature update task ----- */
    xTaskCreate(
        temperature_update_task,
        "temp_update",
        4096,
        NULL,
        tskIDLE_PRIORITY + 5,
        NULL);
}
