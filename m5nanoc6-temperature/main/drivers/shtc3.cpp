/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <driver/i2c_master.h>

#include <lib/support/CodeUtils.h>

#include <drivers/shtc3.h>

static const char * TAG = "sht3x";

#define I2C_MASTER_SCL_IO CONFIG_SHTC3_I2C_SCL_PIN
#define I2C_MASTER_SDA_IO CONFIG_SHTC3_I2C_SDA_PIN
#define I2C_MASTER_FREQ_HZ 100000   /*!< I2C master clock frequency */

#define SHT3X_SENSOR_ADDR 0x44      /*!< I2C address of SHT3x/SHT30/SHT31 sensor */

typedef struct {
    shtc3_sensor_config_t *config;
    esp_timer_handle_t timer;
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t i2c_dev;
    bool is_initialized = false;
} shtc3_sensor_ctx_t;

static shtc3_sensor_ctx_t s_ctx;

static void i2c_scan_devices()
{
    ESP_LOGI(TAG, "Scanning I2C bus for devices...");
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        esp_err_t err = i2c_master_probe(s_ctx.i2c_bus, addr, 20);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "I2C device found at 0x%02X", addr);
        }
    }
}

static esp_err_t shtc3_init_i2c()
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = (gpio_num_t) I2C_MASTER_SDA_IO,
        .scl_io_num = (gpio_num_t) I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_ctx.i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C master bus, err:%d", err);
        return err;
    }

    i2c_scan_devices();

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHT3X_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    err = i2c_master_bus_add_device(s_ctx.i2c_bus, &dev_config, &s_ctx.i2c_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SHT3x device to I2C bus, err:%d", err);
        i2c_del_master_bus(s_ctx.i2c_bus);
        s_ctx.i2c_bus = NULL;
        return err;
    }

    return ESP_OK;
}

static esp_err_t shtc3_read(uint8_t *data, size_t size)
{
    // Single-shot measurement, high repeatability, clock stretching disabled.
    uint8_t cmd_data[] = {0x24, 0x00};
    esp_err_t err = i2c_master_transmit(s_ctx.i2c_dev, cmd_data, sizeof(cmd_data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SHT3x write command failed, err:%d", err);
        i2c_master_bus_reset(s_ctx.i2c_bus);
        return err;
    }

    // Wait for measurement to complete
    vTaskDelay(pdMS_TO_TICKS(20));

    err = i2c_master_receive(s_ctx.i2c_dev, data, size, 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SHT3x read data failed, err:%d", err);
        return err;
    }

    return ESP_OK;
}

// Temperature in degree Celsius
static float shtc3_get_temp(uint16_t raw_temp)
{
    return 175.0f * (static_cast<float>(raw_temp) / 65535.0f) - 45.0f;
}

// Humidity in percentage
static float shtc3_get_humidity(uint16_t raw_humidity)
{
    return 100.0f * (static_cast<float>(raw_humidity) / 65535.0f);
}

static esp_err_t shtc3_get_read_temp_and_humidity(float & temp, float & humidity)
{
    // foreach temperature and humidity: two bytes data, one byte for checksum
    uint8_t data[6] = {0};

    esp_err_t err = shtc3_read(data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }

    uint16_t raw_temp = (data[0] << 8) | data[1];
    uint16_t raw_humidity = (data[3] << 8) | data[4];

    temp = shtc3_get_temp(raw_temp);
    humidity = shtc3_get_humidity(raw_humidity);

    return ESP_OK;
}

static void timer_cb_internal(void *arg)
{
    auto *ctx = (shtc3_sensor_ctx_t *) arg;
    if (!(ctx && ctx->config)) {
        return;
    }

    float temp, humidity;
    esp_err_t err = shtc3_get_read_temp_and_humidity(temp, humidity);
    if (err != ESP_OK) {
        return;
    }

    ESP_LOGI(TAG, "Temperature: %.2f C, Humidity: %.2f %%", temp, humidity);

    if (ctx->config->temperature.cb) {
        ctx->config->temperature.cb(ctx->config->temperature.endpoint_id, temp, ctx->config->user_data);
    }
    if (ctx->config->humidity.cb) {
        ctx->config->humidity.cb(ctx->config->humidity.endpoint_id, humidity, ctx->config->user_data);
    }
}

esp_err_t shtc3_sensor_init(shtc3_sensor_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    // we need at least one callback so that we can start notifying application layer
    if (config->temperature.cb == NULL || config->humidity.cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_ctx.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = shtc3_init_i2c();
    if (err != ESP_OK) {
        return err;
    }

    // Probe the sensor with a soft reset to confirm it is reachable on the bus.
    // SHT3x soft reset command: 0x30A2.
    uint8_t soft_reset[] = {0x30, 0xA2};
    err = i2c_master_transmit(s_ctx.i2c_dev, soft_reset, sizeof(soft_reset), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SHT3x not responding (soft reset probe failed), err:%d", err);
        i2c_del_master_bus(s_ctx.i2c_bus);
        s_ctx.i2c_bus = NULL;
        return err;
    }
    // Allow sensor to complete reset before first measurement
    vTaskDelay(pdMS_TO_TICKS(1));

    // keep the pointer to config
    s_ctx.config = config;

    esp_timer_create_args_t args = {
        .callback = timer_cb_internal,
        .arg = &s_ctx,
    };

    err = esp_timer_create(&args, &s_ctx.timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create failed, err:%d", err);
        return err;
    }

    err = esp_timer_start_periodic(s_ctx.timer, config->interval_ms * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_start_periodic failed: %d", err);
        return err;
    }

    s_ctx.is_initialized = true;
    ESP_LOGI(TAG, "sht3x initialized successfully");

    return ESP_OK;
}
