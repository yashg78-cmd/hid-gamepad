#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "joystick";

#define JOYSTICK_LEFT_X_CHANNEL  ADC_CHANNEL_2  // GPIO3
#define JOYSTICK_LEFT_Y_CHANNEL  ADC_CHANNEL_8  // GPIO9
#define JOYSTICK_RIGHT_X_CHANNEL  ADC_CHANNEL_0  // GPIO1
#define JOYSTICK_RIGHT_Y_CHANNEL  ADC_CHANNEL_1  // GPIO2
#define JOYSTICK_RIGHT_SW_PIN     GPIO_NUM_13
#define JOYSTICK_LEFT_SW_PIN     GPIO_NUM_5



/************* TinyUSB descriptors ****************/

#define TUSB_DESC_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(1))
};

const char* hid_string_descriptor[] = {
    (char[]){0x09, 0x04},
    "TinyUSB",
    "ESP32 Controller",
    "123456",
    "HID Gamepad",
};

static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

/********* TinyUSB HID callbacks ***************/

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
}

/********* Application ***************/

typedef struct {
    int8_t x;
    int8_t y;
    bool z;
} joystick_t;

static joystick_t joystick_read(adc_oneshot_unit_handle_t adc_handle, adc_channel_t X_CHANNEL, adc_channel_t Y_CHANNEL, gpio_num_t SW_PIN)
{
    joystick_t joystick_val;
    int raw_x, raw_y;
    adc_oneshot_read(adc_handle, X_CHANNEL, &raw_x);
    adc_oneshot_read(adc_handle, Y_CHANNEL, &raw_y);

    int sw_val = gpio_get_level(SW_PIN);
    joystick_val.z = sw_val == 0 ? true : false;

    //ESP_LOGI(TAG, "X: %d | Y: %d | SW: %s", raw_x, raw_y, joystick_val.z ? "PRESSED" : "RELEASED");

    joystick_val.x = (int8_t)((raw_x - 2048) * 127 / 2048);
    joystick_val.y = (int8_t)((raw_y - 2048) * 127 / 2048);

    return joystick_val;
    
}

static uint32_t buttons_read(void){
    // read buttons
    bool a_pressed      = (gpio_get_level(GPIO_NUM_38)  == 0);
    bool b_pressed      = (gpio_get_level(GPIO_NUM_39)  == 0);
    bool x_pressed      = (gpio_get_level(GPIO_NUM_40)  == 0);
    bool y_pressed      = (gpio_get_level(GPIO_NUM_21)  == 0);

    bool lb_pressed     = (gpio_get_level(GPIO_NUM_7) == 0);
    bool rb_pressed     = (gpio_get_level(GPIO_NUM_41) == 0);
    bool start_pressed  = (gpio_get_level(GPIO_NUM_48) == 0);
    bool select_pressed = (gpio_get_level(GPIO_NUM_15) == 0);

    // build buttons bitmask
    uint32_t buttons = 0;
    buttons |= (a_pressed      ? (1UL << 0) : 0);
    buttons |= (b_pressed      ? (1UL << 1) : 0);
    buttons |= (x_pressed      ? (1UL << 2) : 0);
    buttons |= (y_pressed      ? (1UL << 3) : 0);
    buttons |= (lb_pressed     ? (1UL << 4) : 0);
    buttons |= (rb_pressed     ? (1UL << 5) : 0);
    buttons |= (start_pressed  ? (1UL << 6) : 0);
    buttons |= (select_pressed ? (1UL << 7) : 0);

    return buttons;
}
static uint8_t dpad_read(void){
    // read d-pad
    bool dpad_up    = (gpio_get_level(GPIO_NUM_16) == 0);
    bool dpad_down  = (gpio_get_level(GPIO_NUM_8) == 0);
    bool dpad_left  = (gpio_get_level(GPIO_NUM_18) == 0);
    bool dpad_right = (gpio_get_level(GPIO_NUM_17) == 0);

    
    // build hat switch
    uint8_t hat = GAMEPAD_HAT_CENTERED;
    if      (dpad_up   && dpad_right) hat = GAMEPAD_HAT_UP_RIGHT;
    else if (dpad_up   && dpad_left)  hat = GAMEPAD_HAT_UP_LEFT;
    else if (dpad_down && dpad_right) hat = GAMEPAD_HAT_DOWN_RIGHT;
    else if (dpad_down && dpad_left)  hat = GAMEPAD_HAT_DOWN_LEFT;
    else if (dpad_up)                 hat = GAMEPAD_HAT_UP;
    else if (dpad_down)               hat = GAMEPAD_HAT_DOWN;
    else if (dpad_left)               hat = GAMEPAD_HAT_LEFT;
    else if (dpad_right)              hat = GAMEPAD_HAT_RIGHT;

    return hat;
}
static void controller_send(adc_oneshot_unit_handle_t adc_handle)
{

    // read joysticks
    joystick_t left  = joystick_read(adc_handle, JOYSTICK_LEFT_X_CHANNEL, JOYSTICK_LEFT_Y_CHANNEL, JOYSTICK_LEFT_SW_PIN);
    joystick_t right = joystick_read(adc_handle, JOYSTICK_RIGHT_X_CHANNEL, JOYSTICK_RIGHT_Y_CHANNEL, JOYSTICK_RIGHT_SW_PIN);

    uint32_t buttons = buttons_read();

    uint8_t hat = dpad_read();

    tud_hid_gamepad_report(
        1,
        left.x,   // left joystick X
        left.y,   // left joystick Y
        right.x,  // right joystick X
        right.y,  // right joystick Y
        0,        // left trigger
        0,        // right trigger
        hat,      // hat switch
        buttons | (left.z  ? (1UL << 8) : 0)   // buttons + joystick clicks
                | (right.z ? (1UL << 9) : 0)
);
}

static void sw_config(int sw_pin){
     // Configure SW pin as input with pull-up
    gpio_config_t sw_conf = {
        .pin_bit_mask = (1ULL << sw_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&sw_conf);
}

static void joystick_config(int joystick_pin, adc_oneshot_unit_handle_t adc_handle ){
    adc_oneshot_chan_cfg_t joy_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_oneshot_config_channel(adc_handle, joystick_pin, &joy_config);
}


void app_main(void)
{
   sw_config(JOYSTICK_LEFT_SW_PIN); //configuring left joystick switch
   sw_config(JOYSTICK_RIGHT_SW_PIN); //configuring left joystick switch
   // face buttons
    sw_config(GPIO_NUM_38);
    sw_config(GPIO_NUM_39);
    sw_config(GPIO_NUM_40);
    sw_config(GPIO_NUM_21);
    sw_config(GPIO_NUM_7);
    sw_config(GPIO_NUM_41);
    sw_config(GPIO_NUM_48);
    sw_config(GPIO_NUM_15);

    // d-pad
    sw_config(GPIO_NUM_16);
    sw_config(GPIO_NUM_8);
    sw_config(GPIO_NUM_18);
    sw_config(GPIO_NUM_17);

    // Initialize ADC
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config, &adc_handle);

    joystick_config(JOYSTICK_LEFT_X_CHANNEL, adc_handle);
    joystick_config(JOYSTICK_LEFT_Y_CHANNEL, adc_handle);
    joystick_config(JOYSTICK_RIGHT_X_CHANNEL, adc_handle);
    joystick_config(JOYSTICK_RIGHT_Y_CHANNEL, adc_handle);

    // Initialize TinyUSB
    ESP_LOGI(TAG, "USB initialization");
    const tinyusb_config_t tusb_cfg = {
        .port = TINYUSB_PORT_FULL_SPEED_0,
        .task = {
                .size = 4096,
                .priority = 5,
                .xCoreID = 0,
        },
        .descriptor = {
            .device = NULL,
            .qualifier = NULL,
            .string = hid_string_descriptor,
            .string_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
            .full_speed_config = hid_configuration_descriptor,
            .high_speed_config = NULL,
        },
        .event_cb = NULL,
        .event_arg = NULL,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");

    while (1) {
        if (tud_mounted()) {
            controller_send(adc_handle);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}