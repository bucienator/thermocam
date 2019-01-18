#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bsp/bsp.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "thermocam.h"

/* 97b8fca2-45a8-478c-9e85-cc852af2e950 */
const ble_uuid128_t gatt_svr_svc_thermo_cam_uuid =
        BLE_UUID128_INIT(0x50, 0xe9, 0xf2, 0x2a, 0x85, 0xcc, 0x85, 0x9e,
                         0x8c, 0x47, 0xa8, 0x45, 0xa2, 0xfc, 0xb8, 0x97);

/* 52e66cfc-9dd2-4932-8e81-7eaf2c6e2c53 */
static const ble_uuid128_t gatt_svr_chr_thermo_img_uuid =
        BLE_UUID128_INIT(0x53, 0x2c, 0x6e, 0x2c, 0xaf, 0x7e, 0x81, 0x8e,
                         0x32, 0x49, 0xd2, 0x9d, 0xfc, 0x6c, 0xe6, 0x52);


uint16_t gatt_svr_chr_thermo_img_handle;
static uint16_t ble_svc_thermo_cam_conn_handle = BLE_HS_CONN_HANDLE_NONE;

static int
gatt_svr_chr_access_thermo_cam(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Thermo camera. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_thermo_cam_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Thermal image. */
            .uuid = &gatt_svr_chr_thermo_img_uuid.u,
            .access_cb = gatt_svr_chr_access_thermo_cam,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &gatt_svr_chr_thermo_img_handle,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        0, /* No more services. */
    },
};

static int gatt_svr_chr_access_thermo_cam(uint16_t conn_handle, uint16_t attr_handle,
                                          struct ble_gatt_access_ctxt *ctxt,
                                          void *arg)
{
    const ble_uuid_t *uuid;
    int rc;

    uuid = ctxt->chr->uuid;

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_thermo_img_uuid.u) == 0) {
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);

        rc = os_mbuf_append(ctxt->om, &thermocam_raw_image, sizeof thermocam_raw_image);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    /* Unknown characteristic; the nimble stack should not have called this
     * function.
     */
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        THERMOCAM_LOG(INFO, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        THERMOCAM_LOG(INFO, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        THERMOCAM_LOG(INFO, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

/**
 * This function must be called with the connection handlewhen a gap
 * connect event is received in order to send notifications to the
 * client.
 *
 * @params conn_handle          The connection handle for the current
 *                                  connection.
 */
void gatt_svr_on_gap_connect(uint16_t conn_handle)
{
    ble_svc_thermo_cam_conn_handle = conn_handle;
}

bool is_connected()
{
    return ble_svc_thermo_cam_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

void gatt_svr_notify()
{
    if(is_connected()) {
        ble_gattc_notify(ble_svc_thermo_cam_conn_handle, gatt_svr_chr_thermo_img_handle);
    }
}

int thermocam_gatt_svr_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
