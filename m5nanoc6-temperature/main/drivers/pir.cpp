/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <lib/support/CodeUtils.h>

#include <drivers/pir.h>

/**
 * GPIO pin connected to the PIR sensor digital output.
 *
 * Value is provided from Kconfig (`CONFIG_PIR_DATA_PIN`).
 */
#define PIR_SENSOR_PIN (static_cast<gpio_num_t>(CONFIG_PIR_DATA_PIN))

/**
 * Runtime context for the PIR sensor driver.
 */
typedef struct {
    /** Application-provided PIR configuration and callback details. */
    pir_sensor_config_t *config;
    /** True when the driver has been initialized. */
    bool is_initialized;
} pir_sensor_ctx_t;

/**
 * Global singleton PIR driver context.
 *
 * This implementation supports a single PIR sensor instance.
 */
static pir_sensor_ctx_t s_ctx;

/**
 * GPIO interrupt handler for PIR state changes.
 *
 * Marked with `IRAM_ATTR` so it can execute safely from interrupt context.
 * It reads the PIR pin level and notifies the application only when occupancy
 * state changes.
 *
 * Official references:
 * - ESP-IDF GPIO API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html
 * - ESP-IDF IRAM-safe interrupt guidance: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/intr_alloc.html
 */
static void IRAM_ATTR pir_gpio_handler(void *arg)
{
    /** Last reported occupancy state to suppress duplicate callbacks. */
    static bool occupancy = false;
    bool new_occupancy = gpio_get_level(PIR_SENSOR_PIN);

    // we only need to notify application layer if occupancy changed
    if (occupancy != new_occupancy) {
        occupancy = new_occupancy;
        if (s_ctx.config->cb) {
            s_ctx.config->cb(s_ctx.config->endpoint_id, new_occupancy, s_ctx.config->user_data);
        }
    }
}

/**
 * Configure the GPIO pin and register the ISR callback for PIR edge events.
 *
 * Steps:
 * 1. Reset and configure the pin as input.
 * 2. Enable interrupts on both rising and falling edges.
 * 3. Apply pulldown mode to define idle signal level.
 * 4. Install GPIO ISR service and register `pir_gpio_handler`.
 */
static void pir_gpio_init(gpio_num_t pin)
{
    gpio_reset_pin(pin);
    gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE);
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(pin, pir_gpio_handler, NULL);
}

/**
 * Initialize the PIR sensor driver.
 *
 * Validates input parameters, prevents double initialization, and sets up
 * GPIO interrupt-based occupancy detection.
 *
 * Returns:
 * - `ESP_OK` on success
 * - `ESP_ERR_INVALID_ARG` when `config` is null
 * - `ESP_ERR_INVALID_STATE` when already initialized
 */
esp_err_t pir_sensor_init(pir_sensor_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_ctx.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    pir_gpio_init(PIR_SENSOR_PIN);

    s_ctx.config = config;
    return ESP_OK;
}
