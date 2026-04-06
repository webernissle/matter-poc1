/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <bsp/esp-bsp.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_ota.h>
#include <nvs_flash.h>

#include <cstring>

#include <app_openthread_config.h>
#include <app_reset.h>
#include <common_macros.h>

// drivers implemented by this example
#include <drivers/shtc3.h>
#include <drivers/pir.h>

/** Log tag used by ESP logging macros in this translation unit. */
static const char *TAG = "app_main";

/** Default NodeLabel written to Basic Information cluster on endpoint 0. */
static constexpr const char *kHomeNodeLabel = "EasyClimate";
/** Default VendorName written to Basic Information cluster. */
static constexpr const char *kVendorName = "jumpbit.com";
/** Default ProductName written to Basic Information cluster. */
static constexpr const char *kProductName = "M5NanoC6 Env Sensor";
/** Default ProductLabel written to Basic Information cluster. */
static constexpr const char *kProductLabel = "Matter Temp+Humidity";
/** Default HardwareVersionString written to Basic Information cluster. */
static constexpr const char *kHardwareVersionString = "v1";
/** Default SoftwareVersionString written to Basic Information cluster. */
static constexpr const char *kSoftwareVersionString = "1.0.0";

#if CONFIG_ENABLE_TEST_SETUP_PARAMS
/** Example onboarding QR code shown when test setup parameters are enabled. */
static constexpr const char *kDefaultMatterQrCode = "MT:Y.K9042C00KA0648G00";
/** Example manual pairing code shown when test setup parameters are enabled. */
static constexpr const char *kDefaultMatterManualPairingCode = "34970112332";
#endif

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

/**
 * Publish temperature updates into the Matter Temperature Measurement cluster.
 *
 * The callback schedules work onto the Matter thread via SystemLayer, then updates
 * the MeasuredValue attribute using centi-degrees Celsius:
 * $value = temperature_{C} \times 100$.
 *
 * Official references:
 * - ESP-Matter project: https://github.com/espressif/esp-matter
 * - Matter TemperatureMeasurement cluster docs (SDK source):
 *   https://github.com/project-chip/connectedhomeip/tree/master/src/app/clusters/temperature-measurement
 */
static void temp_sensor_notification(uint16_t endpoint_id, float temp, void *user_data)
{
    // schedule the attribute update so that we can report it from matter thread
    chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, temp]() {
        attribute_t * attribute = attribute::get(endpoint_id,
                                                 TemperatureMeasurement::Id,
                                                 TemperatureMeasurement::Attributes::MeasuredValue::Id);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);
        val.val.i16 = static_cast<int16_t>(temp * 100);

        attribute::update(endpoint_id, TemperatureMeasurement::Id, TemperatureMeasurement::Attributes::MeasuredValue::Id, &val);
    });
}

/**
 * Publish humidity updates into the Matter Relative Humidity Measurement cluster.
 *
 * The callback schedules work onto the Matter thread and updates MeasuredValue
 * using centi-percent units:
 * $value = humidity_{\%} \times 100$.
 *
 * Official reference:
 * https://github.com/project-chip/connectedhomeip/tree/master/src/app/clusters/relative-humidity-measurement
 */
static void humidity_sensor_notification(uint16_t endpoint_id, float humidity, void *user_data)
{
    // schedule the attribute update so that we can report it from matter thread
    chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, humidity]() {
        attribute_t * attribute = attribute::get(endpoint_id,
                                                 RelativeHumidityMeasurement::Id,
                                                 RelativeHumidityMeasurement::Attributes::MeasuredValue::Id);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);
        val.val.u16 = static_cast<uint16_t>(humidity * 100);

        attribute::update(endpoint_id, RelativeHumidityMeasurement::Id, RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &val);
    });
}

/**
 * Publish PIR occupancy changes into the Matter Occupancy Sensing cluster.
 *
 * Official reference:
 * https://github.com/project-chip/connectedhomeip/tree/master/src/app/clusters/occupancy-sensor
 */
static void occupancy_sensor_notification(uint16_t endpoint_id, bool occupancy, void *user_data)
{
    // schedule the attribute update so that we can report it from matter thread
    chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, occupancy]() {
        attribute_t * attribute = attribute::get(endpoint_id,
                                                 OccupancySensing::Id,
                                                 OccupancySensing::Attributes::Occupancy::Id);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);
        val.val.b = occupancy;

        attribute::update(endpoint_id, OccupancySensing::Id, OccupancySensing::Attributes::Occupancy::Id, &val);
    });
}

/**
 * Create and register factory reset button handling.
 *
 * Uses BSP button helpers and app reset utility to wire long-press reset behavior.
 *
 * Official references:
 * - ESP-BSP: https://github.com/espressif/esp-bsp
 * - ESP-IDF button component docs: https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/button.html
 */
static esp_err_t factory_reset_button_register()
{
    button_handle_t push_button;
    esp_err_t err = bsp_iot_button_create(&push_button, NULL, BSP_BUTTON_NUM);
    VerifyOrReturnError(err == ESP_OK, err);
    return app_reset_button_register(push_button);
}

/**
 * Open a basic commissioning window when the device has no fabrics.
 *
 * This is typically used after last-fabric removal to allow re-commissioning.
 *
 * Official reference:
 * https://project-chip.github.io/connectedhomeip-doc/guides/commissioning.html
 */
static void open_commissioning_window_if_necessary()
{
    VerifyOrReturn(chip::Server::GetInstance().GetFabricTable().FabricCount() == 0);

    chip::CommissioningWindowManager & commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
    VerifyOrReturn(commissionMgr.IsCommissioningWindowOpen() == false);

    // After removing last fabric, this example does not remove the Wi-Fi credentials
    // and still has IP connectivity so, only advertising on DNS-SD.
    CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(chip::System::Clock::Seconds16(300),
                                    chip::CommissioningWindowAdvertisement::kDnssdOnly);
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
    }
}

/**
 * Matter device event callback.
 *
 * Handles commissioning lifecycle and fabric events for logging and recovery.
 */
static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Fabric removed successfully");
        open_commissioning_window_if_necessary();
        break;

    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
        ESP_LOGI(TAG, "BLE deinitialized and memory reclaimed");
        break;

    default:
        break;
    }
}

/**
 * Identify cluster callback.
 *
 * Invoked when a commissioner/client triggers Identify for an endpoint.
 * This sample logs the request and does not drive a physical indicator.
 *
 * Official reference:
 * https://github.com/project-chip/connectedhomeip/tree/master/src/app/clusters/identify
 */
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}

/**
 * Global attribute update callback for ESP-Matter node attributes.
 *
 * This sample is telemetry-focused and accepts writes by returning success without
 * custom handling.
 */
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    // Since this is just a sensor and we don't expect any writes on our temperature sensor,
    // so, return success.
    return ESP_OK;
}

/**
 * Log onboarding codes for commissioning during bring-up/testing.
 *
 * If test setup params are disabled, prompts the user to query runtime values.
 */
static void log_commissioning_codes()
{
#if CONFIG_ENABLE_TEST_SETUP_PARAMS
    ESP_LOGI(TAG, "Matter QR Code: %s", kDefaultMatterQrCode);
    ESP_LOGI(TAG, "Matter manual pairing code: %s", kDefaultMatterManualPairingCode);
    ESP_LOGI(TAG, "Apple Home: Add Accessory, then scan the QR code (or use manual pairing code).");
#else
    ESP_LOGI(TAG, "Commissioning parameters are custom. Use 'matter onboardingcodes' in CHIP shell to print current QR/manual codes.");
#endif
}

/**
 * Populate Basic Information cluster metadata on endpoint 0.
 *
 * Updates vendor/product labels and software/hardware strings so controller apps
 * show meaningful device identity fields.
 *
 * Official reference:
 * https://github.com/project-chip/connectedhomeip/tree/master/src/app/clusters/basic-information
 */
static void set_basic_information_metadata()
{
    auto update_basic_str = [](uint32_t attribute_id, const char *value) {
        esp_matter_attr_val_t val = esp_matter_char_str(const_cast<char *>(value), strlen(value));
        return attribute::update(0, BasicInformation::Id, attribute_id, &val);
    };

    esp_err_t err = update_basic_str(BasicInformation::Attributes::VendorName::Id, kVendorName);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set VendorName, err:%d", err);
    }

    err = update_basic_str(BasicInformation::Attributes::ProductName::Id, kProductName);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set ProductName, err:%d", err);
    }

    err = update_basic_str(BasicInformation::Attributes::ProductLabel::Id, kProductLabel);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set ProductLabel, err:%d", err);
    }

    err = update_basic_str(BasicInformation::Attributes::HardwareVersionString::Id, kHardwareVersionString);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set HardwareVersionString, err:%d", err);
    }

    err = update_basic_str(BasicInformation::Attributes::SoftwareVersionString::Id, kSoftwareVersionString);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set SoftwareVersionString, err:%d", err);
    }

    err = update_basic_str(BasicInformation::Attributes::NodeLabel::Id, kHomeNodeLabel);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set NodeLabel, err:%d", err);
    }
}

extern "C" void app_main()
{
    /**
     * Main firmware entrypoint.
     *
     * Flow:
     * 1. Initialize NVS and reset button.
     * 2. Create Matter node and set Basic Information metadata.
     * 3. Create temperature, humidity, and occupancy endpoints.
     * 4. Initialize SHTC3 and PIR sensor drivers with endpoint callbacks.
     * 5. Configure OpenThread (when enabled) and start Matter stack.
     * 6. Print commissioning/onboarding codes.
     *
     * Official references:
     * - ESP-Matter: https://docs.espressif.com/projects/esp-matter/en/latest/
     * - ESP-IDF app_main entrypoint model:
     *   https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/startup.html
     */
    /* Initialize the ESP NVS layer */
    nvs_flash_init();

    /* Initialize push button on the dev-kit to reset the device */
    esp_err_t err = factory_reset_button_register();
    ABORT_APP_ON_FAILURE(ESP_OK == err, ESP_LOGE(TAG, "Failed to initialize reset button, err:%d", err));

    /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    set_basic_information_metadata();

    // add temperature sensor device
    temperature_sensor::config_t temp_sensor_config;
    endpoint_t * temp_sensor_ep = temperature_sensor::create(node, &temp_sensor_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(temp_sensor_ep != nullptr, ESP_LOGE(TAG, "Failed to create temperature_sensor endpoint"));

    // add the humidity sensor device
    humidity_sensor::config_t humidity_sensor_config;
    endpoint_t * humidity_sensor_ep = humidity_sensor::create(node, &humidity_sensor_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(humidity_sensor_ep != nullptr, ESP_LOGE(TAG, "Failed to create humidity_sensor endpoint"));

    /**
     * Static SHTC3 driver configuration retained for the app lifetime.
     *
     * Contains endpoint bindings for temperature and humidity callbacks.
     */
    static shtc3_sensor_config_t shtc3_config = {
        .temperature = {
            .cb = temp_sensor_notification,
            .endpoint_id = endpoint::get_id(temp_sensor_ep),
        },
        .humidity = {
            .cb = humidity_sensor_notification,
            .endpoint_id = endpoint::get_id(humidity_sensor_ep),
        },
    };
    err = shtc3_sensor_init(&shtc3_config);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to initialize temperature sensor driver"));

    // add the occupancy sensor
    occupancy_sensor::config_t occupancy_sensor_config;
    occupancy_sensor_config.occupancy_sensing.occupancy_sensor_type =
        chip::to_underlying(OccupancySensing::OccupancySensorTypeEnum::kPir);
    occupancy_sensor_config.occupancy_sensing.occupancy_sensor_type_bitmap =
        chip::to_underlying(OccupancySensing::OccupancySensorTypeBitmap::kPir);
    occupancy_sensor_config.occupancy_sensing.feature_flags =
        chip::to_underlying(OccupancySensing::Feature::kPassiveInfrared);

    endpoint_t * occupancy_sensor_ep = occupancy_sensor::create(node, &occupancy_sensor_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(occupancy_sensor_ep != nullptr, ESP_LOGE(TAG, "Failed to create occupancy_sensor endpoint"));

    /**
     * Static PIR driver configuration retained for the app lifetime.
     *
     * Contains occupancy callback and associated endpoint binding.
     */
    static pir_sensor_config_t pir_config = {
        .cb = occupancy_sensor_notification,
        .endpoint_id = endpoint::get_id(occupancy_sensor_ep),
    };
    err = pir_sensor_init(&pir_config);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to initialize occupancy sensor driver"));

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    /* Set OpenThread platform config */
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&config);
#endif

    /* Matter start */
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    log_commissioning_codes();
}
