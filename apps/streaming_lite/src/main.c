/**
 * Copyright (C) 2017-2018, Decawave Limited, All Rights Reserved
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "imgmgr/imgmgr.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif
#if MYNEWT_VAL(BLE_ENABLED)
#include "bleprph/bleprph.h"
#endif

#include <uwb/uwb.h>
#include <uwb/uwb_ftypes.h>
#include <uwb_rng/uwb_rng.h>
#include <config/config.h>
#if MYNEWT_VAL(UWBCFG_ENABLED)
#include "uwbcfg/uwbcfg.h"
#endif
#include <uwb_transport/uwb_transport.h>
#if MYNEWT_VAL(TDMA_ENABLED)
#include <tdma/tdma.h>
#endif
#if MYNEWT_VAL(UWB_CCP_ENABLED)
#include <uwb_ccp/uwb_ccp.h>
#endif
#if MYNEWT_VAL(DW1000_DEVICE_0)
#include <dw1000/dw1000_gpio.h>
#include <dw1000/dw1000_hal.h>
#endif
#if MYNEWT_VAL(CONCURRENT_NRNG)
#include <nrng/nrng.h>
#endif

#include <crc/crc8.h>

#define DEST_ADDRESS 0x0000
#define TIMEOUT      0x15B2
//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif

uint8_t test[512 - sizeof(uwb_transport_frame_header_t) - 2];

void stream_timer(uwb_transport_instance_t * uwb_transport);

#if MYNEWT_VAL(UWBCFG_ENABLED)
static bool uwb_config_updated = false;

/**
 * @fn uwb_config_update
 *
 * Called from the main event queue as a result of the uwbcfg packet
 * having received a commit/load of new uwb configuration.
 */
int
uwb_config_updated_func()
{
    /* Workaround in case we're stuck waiting for ccp with the
     * wrong radio settings */
    struct uwb_dev * udev = uwb_dev_idx_lookup(0);
    struct uwb_ccp_instance *ccp = (struct uwb_ccp_instance*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_CCP);
    if (dpl_sem_get_count(&ccp->sem) == 0) {
        uwb_phy_forcetrxoff(udev);
        uwb_mac_config(udev, NULL);
        uwb_txrf_config(udev, &udev->config.txrf);
        uwb_start_rx(udev);
        return 0;
    }

    uwb_config_updated = true;
    return 0;
}
#endif

/*!
 * @fn slot_cb(struct dpl_event * ev)
 *
 * In this example, slot_cb schedules the turning of the transceiver to receive mode at the desired epoch.
 * The precise timing is under the control of the DW1000, and the dw_time variable defines this epoch.
 * Note the slot_cb itself is scheduled MYNEWT_VAL(OS_LATENCY) in advance of the epoch to set up the transceiver accordingly.
 *
 * Note: The epoch time is referenced to the Rmarker symbol, it is therefore necessary to advance the rxtime by the SHR_duration such
 * that the preamble are received. The transceiver, when transmitting adjust the txtime accordingly.
 *
 * input parameters
 * @param inst - struct dpl_event *
 *
 * output parameters
 *
 * returns none
 */
static struct dpl_callout stream_callback;
static void
stream_cb(struct dpl_event * ev)
{
    assert(ev);
    dpl_callout_reset(&stream_callback, OS_TICKS_PER_SEC/80);
    uwb_transport_instance_t * uwb_transport = (uwb_transport_instance_t *)dpl_event_get_arg(ev);

#if MYNEWT_VAL(UWB_TRANSPORT_ROLE) == 1
    stream_timer(uwb_transport);
#endif

    if(uwb_transport_dequeue_tx(uwb_transport,
            0, TIMEOUT) == true){
    }else{
        uwb_transport_listen(uwb_transport, UWB_BLOCKING,
      0, TIMEOUT);
    }
}

static uint16_t g_crc8;
static bool
uwb_transport_cb(struct uwb_dev * inst, uint16_t uid, struct dpl_mbuf * mbuf)
{
    uint16_t len = DPL_MBUF_PKTLEN(mbuf);
    dpl_mbuf_copydata(mbuf, 0, sizeof(test), test);
    dpl_mbuf_free_chain(mbuf);
    if (g_crc8 != crc8_calc(0, test, sizeof(test))){
        uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
        printf("{\"utime\": %lu,\"error\": \" crc mismatch len=%d, sizeof(test) = %d\"}\n",utime, len, sizeof(test));
    }
    return true;
}

#if MYNEWT_VAL(UWB_TRANSPORT_ROLE) == 1
void
stream_timer(uwb_transport_instance_t * uwb_transport)
{
    for (uint8_t i = 0; i < 18; i++){
        uint16_t destination_uid = DEST_ADDRESS;
        struct dpl_mbuf * mbuf;
        if (uwb_transport->config.os_msys_mpool){
            mbuf = dpl_msys_get_pkthdr(sizeof(test), sizeof(uwb_transport_user_header_t));
        }
        else{
            mbuf = dpl_mbuf_get_pkthdr(uwb_transport->omp, sizeof(uwb_transport_user_header_t));
        }
        if (mbuf){
            dpl_mbuf_copyinto(mbuf, 0, test, sizeof(test));
            uwb_transport_enqueue_tx(uwb_transport, destination_uid, 0xDEAD, mbuf);
        }else{
            //break;
        }
    }
}
#endif


int main(int argc, char **argv){
    int rc;

    sysinit();

#if MYNEWT_VAL(UWBCFG_ENABLED)
    /* Register callback for UWB configuration changes */
    struct uwbcfg_cbs uwb_cb = {
        .uc_update = uwb_config_updated_func
    };
    uwbcfg_register(&uwb_cb);
    /* Load config from flash */
    conf_load();
#endif

    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    struct uwb_dev * udev = uwb_dev_idx_lookup(0);

#if MYNEWT_VAL(USE_DBLBUFFER)
        /* Make sure to enable double buffring */
        udev->config.dblbuffon_enabled = 1;
        udev->config.rxauto_enable = 0;
        uwb_set_dblrxbuff(udev, true);
#else
        udev->config.dblbuffon_enabled = 0;
        udev->config.rxauto_enable = 1;
        uwb_set_dblrxbuff(udev, false);
#endif

    struct _uwb_transport_instance * uwb_transport = (struct _uwb_transport_instance *)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_TRANSPORT);
    assert(uwb_transport);

    struct _uwb_transport_extension extension = {
        .tsp_code = 0xDEAD,
        .uwb_transport = uwb_transport,
        .extension_cb = uwb_transport_cb
    };

    uwb_transport_append_extension(uwb_transport, &extension);

#if MYNEWT_VAL(DW1000_DEVICE_0)
    // Using GPIO5 and GPIO6 to study timing.
    dw1000_gpio5_config_ext_txe( hal_dw1000_inst(0));
    dw1000_gpio6_config_ext_rxe( hal_dw1000_inst(0));
#endif

    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    printf("{\"utime\": %lu,\"exec\": \"%s\"}\n",utime,__FILE__);
    printf("{\"device_id\"=\"%lX\"",udev->device_id);
    printf(",\"panid=\"%X\"",udev->pan_id);
    printf(",\"addr\"=\"%X\"",udev->uid);
    printf(",\"part_id\"=\"%lX\"",(uint32_t)(udev->euid&0xffffffff));
    printf(",\"lot_id\"=\"%lX\"}\n",(uint32_t)(udev->euid>>32));
    printf("{\"utime\": %lu,\"msg\": \"frame_duration = %d usec\"}\n",utime,uwb_phy_frame_duration(udev, sizeof(test) + sizeof(uwb_transport_frame_header_t)));
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime,uwb_phy_SHR_duration(udev));
    printf("UWB_TRANSPORT_ROLE = %d\n",  MYNEWT_VAL(UWB_TRANSPORT_ROLE));

    for (uint16_t i=0; i < sizeof(test); i++)
        test[i] = i;

    g_crc8 = crc8_calc(0, test, sizeof(test));
    uwb_set_uid(udev, DEST_ADDRESS);
    dpl_callout_init(&stream_callback, dpl_eventq_dflt_get(), stream_cb, uwb_transport);
    dpl_callout_reset(&stream_callback, DPL_TICKS_PER_SEC);

    while (1) {
        dpl_eventq_run(dpl_eventq_dflt_get());
    }

    assert(0);
    return rc;
}

