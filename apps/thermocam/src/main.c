#include "sysinit/sysinit.h"
#include "config/config.h"
#include "host/ble_hs.h"
#include "thermocam.h"

 struct log thermocam_log;
 
int main(int argc, char **argv)
{
    log_register("thermocam", &thermocam_log, &log_console_handler, NULL, LOG_SYSLEVEL);
    log_register("ble_hs", &ble_hs_log, &log_console_handler, NULL, LOG_SYSLEVEL);

    thermocam_shell_init();
 
    sysinit();
	
    thermocam_ble_init();
    thermocam_camera_init();
    thermocam_gatt_svr_init();
	
	//conf_load();
	
    while (1) {
        /* Run the event queue to process background events */
        os_eventq_run(os_eventq_dflt_get());
    }
 
    return 0;
}
