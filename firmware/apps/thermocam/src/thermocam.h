#pragma once

#include "host/ble_gatt.h"

// shell.c
void thermocam_shell_init();

// camera.c
extern uint8_t thermocam_raw_image[128];
extern uint32_t thermocam_frame_cnt;

void thermocam_camera_init();

// main.c
#include "log/log.h"
extern struct log thermocam_log;

#define THERMOCAM_LOG_MODULE  (LOG_MODULE_PERUSER + 0)
#define THERMOCAM_LOG(lvl, ...) \
    LOG_ ## lvl(&thermocam_log, THERMOCAM_LOG_MODULE, __VA_ARGS__)

// ble.c
bool has_connected_peer();
void thermocam_ble_init();

// misc.c
void print_bytes(const uint8_t *bytes, int len);
void print_addr(const void *addr);

// gatt_svr.c
extern const ble_uuid128_t gatt_svr_svc_thermo_cam_uuid;
extern uint16_t gatt_svr_chr_thermo_img_handle;
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
void gatt_svr_set_peer_to_notify(uint16_t conn_handle);
bool is_notification_enabled();
void gatt_svr_notify();
int thermocam_gatt_svr_init();

// status_led.c
void thermocam_status_led_init();