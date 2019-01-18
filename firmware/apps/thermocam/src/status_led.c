#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "thermocam.h"


#define STATUS_LED_TASK_PRIO        (250)  /* 1 = highest, 255 = lowest */
#define STATUS_LED_STACK_SIZE       OS_STACK_ALIGN(64)
static struct os_task status_led_task;
static os_stack_t status_led_stack[STATUS_LED_STACK_SIZE];

#define STATUS_POWERED 0
#define STATUS_CONNECTED 1
#define STATUS_NOTIFY 2
#define NUMBER_OF_STATES 3

// When device is powered on, blink 1x100ms each 5 sec.
// When bluetooth connection is live, blinke 1s on/ 1s off.
// When BLE notify is enabled, led on constantly.
uint32_t led_on_time[] = {100, 1000, 1000};
uint32_t led_off_time[] = {4900, 1000, 0};

static void status_led_task_func(void *arg)
{

    // translate wait map from ms to ticks
    for(int i = 0 ; i < NUMBER_OF_STATES; ++i) {
        led_on_time[i] = led_on_time[i] * OS_TICKS_PER_SEC / 1000;
        led_off_time[i] = led_off_time[i] * OS_TICKS_PER_SEC / 1000;
    }

    hal_gpio_init_out(LED_1, 0);

    os_time_t last_change_ts = os_time_get();
    while(1)
    {
        os_time_t now_ts = os_time_get();

        // check status
        unsigned s = STATUS_POWERED;
        if(is_connected()) {
            s = STATUS_CONNECTED;
        }

        int current_led_state = hal_gpio_read(LED_1);
        uint32_t (*time_map)[] = (current_led_state ? &led_on_time : &led_off_time);
        uint32_t (*other_time_map)[] = (current_led_state ? &led_off_time : &led_on_time);

        if((*other_time_map)[s] > 0) {
            // only consider toggling the led if the other state would be on for a while
            if(now_ts - last_change_ts > (*time_map)[s]) {
                hal_gpio_toggle(LED_1);
                last_change_ts = now_ts;
            }
        }

        os_time_delay(OS_TICKS_PER_SEC / 10);
    }
}


void thermocam_status_led_init(void)
{
    THERMOCAM_LOG(INFO, "Status led task init\n");
    os_task_init(&status_led_task, "status_led", status_led_task_func, NULL,
                 STATUS_LED_TASK_PRIO, OS_WAIT_FOREVER, status_led_stack,
                 STATUS_LED_STACK_SIZE);

}