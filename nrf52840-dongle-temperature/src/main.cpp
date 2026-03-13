/*
 * nRF52840 Dongle – Matter Temperature Sensor
 *
 * This application implements a Matter-compliant temperature sensor using the
 * nRF52840 Dongle's on-chip die temperature sensor (TEMP peripheral).
 *
 * Device type  : Temperature Sensor (0x0302)
 * Cluster      : Temperature Measurement (0x0402)
 * Transport    : Matter over Thread (IEEE 802.15.4 / OpenThread)
 *
 * The device joins a Thread network provided by the HomePod mini (which acts
 * as a Thread Border Router) during the Matter commissioning flow via BLE.
 * After commissioning the device is visible in Apple Home as a temperature
 * sensor and updates every 30 seconds.
 *
 * Build requirements:
 *   - nRF Connect SDK v2.6 or later
 *   - west tool
 *   - nRF52840 Dongle (PCA10059)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/usb/usb_device.h>

#include <app/server/Server.h>
#include <app/server/CommissioningWindowManager.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/clusters/temperature-measurement-server/temperature-measurement-server.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/nrfconnect/OTAImageProcessorImpl.h>

LOG_MODULE_REGISTER(nrf52840_temp_sensor, CONFIG_LOG_DEFAULT_LEVEL);

/* ------------------------------------------------------------------ */
/*  Constants                                                           */
/* ------------------------------------------------------------------ */

/** Matter endpoint for the temperature measurement cluster */
static constexpr chip::EndpointId kTempEndpointId = 1;

/** Update interval: 30 seconds */
static constexpr uint32_t kUpdateIntervalMs = 30000U;

/* ------------------------------------------------------------------ */
/*  Temperature measurement helper                                      */
/* ------------------------------------------------------------------ */

namespace {

/**
 * @brief Read the nRF52840 on-chip TEMP peripheral value.
 *
 * The Zephyr TEMP_NRF5 driver exposes the die temperature sensor under the
 * Zephyr sensor API.  Accuracy is ±0.25 °C (typical).
 *
 * @param[out] celsius  Temperature in degrees Celsius.
 * @return 0 on success, negative errno on failure.
 */
int ReadInternalTemperature(float &celsius)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(temp));
    if (!device_is_ready(dev)) {
        LOG_ERR("Temperature sensor device not ready");
        return -ENODEV;
    }

    int ret = sensor_sample_fetch(dev);
    if (ret != 0) {
        LOG_ERR("sensor_sample_fetch failed: %d", ret);
        return ret;
    }

    struct sensor_value val;
    ret = sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &val);
    if (ret != 0) {
        LOG_ERR("sensor_channel_get failed: %d", ret);
        return ret;
    }

    celsius = sensor_value_to_float(&val);
    return 0;
}

/**
 * @brief Update the Matter MeasuredValue attribute for the temperature cluster.
 *
 * Matter represents temperature as a signed 16-bit integer in units of 0.01 °C.
 * E.g. 2500 represents 25.00 °C.
 *
 * @param celsius  Temperature in degrees Celsius.
 */
void UpdateMatterTemperature(float celsius)
{
    /* Clamp to Matter attribute range [-273.15 °C, 327.67 °C] */
    if (celsius < -273.15f) { celsius = -273.15f; }
    if (celsius >  327.67f) { celsius =  327.67f; }

    int16_t matterTemp = static_cast<int16_t>(celsius * 100.0f);

    chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(
        kTempEndpointId, matterTemp);

    LOG_INF("Temperature: %.2f °C  (Matter raw: %" PRId16 ")", celsius, matterTemp);
}

} /* anonymous namespace */

/* ------------------------------------------------------------------ */
/*  Matter event handler                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Called by the Matter stack for platform-level events.
 */
static void AppEventHandler(const ChipDeviceEvent *event, intptr_t /* arg */)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        LOG_INF("Commissioning complete – device is now in the fabric");
        break;

    case chip::DeviceLayer::DeviceEventType::kThreadConnectivityChange:
        if (event->ThreadConnectivityChange.Result == chip::DeviceLayer::kConnectivity_Established) {
            LOG_INF("Thread network attached");
        } else {
            LOG_WRN("Thread network disconnected");
        }
        break;

    case chip::DeviceLayer::DeviceEventType::kCHIPoBLEConnectionClosed:
        LOG_INF("BLE commissioning connection closed");
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  Temperature update thread                                           */
/* ------------------------------------------------------------------ */

/** Stack and thread control block for the temperature update thread */
K_THREAD_STACK_DEFINE(s_tempThreadStack, 2048);
static struct k_thread s_tempThreadData;

/**
 * @brief Zephyr thread entry point – periodically reads and publishes
 *        temperature.
 */
static void TemperatureUpdateThread(void * /*p1*/, void * /*p2*/, void * /*p3*/)
{
    while (true) {
        float celsius = 0.0f;
        if (ReadInternalTemperature(celsius) == 0) {
            chip::DeviceLayer::PlatformMgr().LockChipStack();
            UpdateMatterTemperature(celsius);
            chip::DeviceLayer::PlatformMgr().UnlockChipStack();
        }
        k_sleep(K_MSEC(kUpdateIntervalMs));
    }
}

/* ------------------------------------------------------------------ */
/*  Endpoint / cluster initialisation                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief Declare endpoint 1 as a Temperature Sensor device type and
 *        initialise the Temperature Measurement cluster.
 *
 * This is done via the ZAP-generated data-model declarations.  The
 * nRF Connect SDK Matter examples use DECLARE_DYNAMIC_ENDPOINT macros;
 * here we rely on the static ZAP configuration provided by the SDK's
 * temperature sensor sample as a starting point.
 *
 * MeasuredValue    – updated every 30 s by TemperatureUpdateThread
 * MinMeasuredValue – -1000  (–10.00 °C, practical for die temp)
 * MaxMeasuredValue – 12500  (125.00 °C, nRF52840 absolute max)
 */
static void InitTemperatureEndpoint(void)
{
    /* Set initial attribute values */
    chip::app::Clusters::TemperatureMeasurement::Attributes::MinMeasuredValue::Set(
        kTempEndpointId, static_cast<chip::app::DataModel::Nullable<int16_t>>(-1000));

    chip::app::Clusters::TemperatureMeasurement::Attributes::MaxMeasuredValue::Set(
        kTempEndpointId, static_cast<chip::app::DataModel::Nullable<int16_t>>(12500));

    /* Mark MeasuredValue as invalid until first reading */
    chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::SetNull(
        kTempEndpointId);
}

/* ------------------------------------------------------------------ */
/*  Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    int ret = 0;

    LOG_INF("nRF52840 Dongle Matter Temperature Sensor starting...");

    /* ----- USB CDC ACM (console via USB) ----- */
    ret = usb_enable(NULL);
    if (ret != 0) {
        LOG_ERR("USB enable failed: %d", ret);
        /* Non-fatal – continue without USB console */
    }

    /* Wait a moment for USB to enumerate before printing more logs */
    k_sleep(K_MSEC(500));

    /* ----- Matter platform initialisation ----- */
    ret = chip::DeviceLayer::PlatformMgr().InitChipStack();
    if (ret != CHIP_NO_ERROR) {
        LOG_ERR("InitChipStack failed: %" CHIP_ERROR_FORMAT, ret.Format());
        return -1;
    }

    /* Register the event handler BEFORE starting the stack */
    chip::DeviceLayer::PlatformMgr().AddEventHandler(AppEventHandler, 0);

    /* ----- Device attestation credentials (example / test DAC) ----- */
    chip::Credentials::SetDeviceAttestationCredentialsProvider(
        chip::Credentials::Examples::GetExampleDACProvider());

    /* ----- Initialise the temperature cluster attributes ----- */
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    InitTemperatureEndpoint();
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

    /* ----- Start Matter server ----- */
    static chip::CommonCaseDeviceServerInitParams serverParams;
    (void) serverParams.InitializeStaticResourcesBeforeServerInit();

    ret = chip::Server::GetInstance().Init(serverParams);
    if (ret != CHIP_NO_ERROR) {
        LOG_ERR("Server init failed: %" CHIP_ERROR_FORMAT, ret.Format());
        return -1;
    }

    /* ----- Start Matter stack (runs in its own Zephyr thread) ----- */
    ret = chip::DeviceLayer::PlatformMgr().StartEventLoopTask();
    if (ret != CHIP_NO_ERROR) {
        LOG_ERR("StartEventLoopTask failed: %" CHIP_ERROR_FORMAT, ret.Format());
        return -1;
    }

    LOG_INF("Matter stack started – waiting for commissioning via BLE");
    LOG_INF("Use Apple Home to scan the QR code or enter the manual pairing code");

    /* ----- Start the temperature update thread ----- */
    k_thread_create(
        &s_tempThreadData,
        s_tempThreadStack,
        K_THREAD_STACK_SIZEOF(s_tempThreadStack),
        TemperatureUpdateThread,
        NULL, NULL, NULL,
        K_PRIO_PREEMPT(5), 0, K_NO_WAIT);

    k_thread_name_set(&s_tempThreadData, "temp_update");

    /* Main thread hands off; Matter and Zephyr threads take over */
    return 0;
}
