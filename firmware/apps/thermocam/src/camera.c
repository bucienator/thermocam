#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_i2c.h"
#include "thermocam.h"

uint8_t thermocam_raw_image[128];
uint32_t thermocam_frame_cnt;

#define CAM_TASK_PRIO        (200)  /* 1 = highest, 255 = lowest */
#define CAM_STACK_SIZE       OS_STACK_ALIGN(1024)
static struct os_task camera_task;
static os_stack_t camera_task_stack[CAM_STACK_SIZE];

static void camera_task_func(void *arg)
{
    THERMOCAM_LOG(INFO, "Camera task started\n");

    int rc = 0;
    
    uint8_t b[2];
    
    struct hal_i2c_master_data pdata;
    pdata.address = 0x69;
    pdata.len = 2;
    pdata.buffer = b;
    b[0]=2;
    b[1]=0;
    rc = hal_i2c_master_write(0, &pdata, OS_TICKS_PER_SEC, 1);
    if(rc != 0) {
        THERMOCAM_LOG(ERROR, "I2C write error %d\n", rc);
    }
 
    while (1) {
        os_time_delay(OS_TICKS_PER_SEC);

        if(!is_notification_enabled()) {
            continue;
        }
        
        pdata.len = 1;
        pdata.buffer = b;
        b[0] = 0x80;
        rc = hal_i2c_master_write(0, &pdata, OS_TICKS_PER_SEC, 1);
        if(rc != 0) {
            THERMOCAM_LOG(ERROR, "I2C write error %d\n", rc);
            continue;
        }
        
        pdata.len=128;
        pdata.buffer=thermocam_raw_image;
        rc = hal_i2c_master_read(0, &pdata, OS_TICKS_PER_SEC, 1);
        if(rc != 0) {
            THERMOCAM_LOG(ERROR, "I2C read error %d\n", rc);
            continue;
        }

        // trigger notify of data change
        gatt_svr_notify();

        thermocam_frame_cnt++;
        int i;
        for(i = 0; i < 64; ++i) {
            //uint16_t val = ((uint16_t)b[i*2 + 1] << 8) | ((uint16_t)b[i*2]);
            //uint16_t absVal = (val & 0x7FF);
            //image[i] =  ((val & 0x800) ? 0 - (float)absVal : (float)absVal) / 4;
        }
    }
}

void thermocam_camera_init(void)
{
    THERMOCAM_LOG(INFO, "Camera task init\n");
    os_task_init(&camera_task, "cam", camera_task_func, NULL,
                 CAM_TASK_PRIO, OS_WAIT_FOREVER, camera_task_stack,
                 CAM_STACK_SIZE);
}