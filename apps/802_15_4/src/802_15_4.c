
#include <uwb_transport/uwb_transport.h>
#include <crc/crc8.h>

#include "802_15_4.h"

#define DEST_ADDRESS 0x0000
#define TIMEOUT      0x15B2

uint8_t test[512 - sizeof(uwb_transport_frame_header_t) - 2];

static uint16_t g_crc8;

void
stream_timer(uwb_transport_instance_t * uwb_transport);

int
start(struct ieee802154_hw *hw){
    for (uint16_t i=0; i < sizeof(test); i++)
        test[i] = i;

    g_crc8 = crc8_calc(0, test, sizeof(test));

#if MYNEWT_VAL(UWB_TRANSPORT_ROLE) == 0
    struct uwb_dev * udev = uwb_dev_idx_lookup(0);
    uwb_set_uid(udev, DEST_ADDRESS);
#endif

    return 0;
}

/*
void
stop(struct ieee802154_hw *hw){
// XXX: TBD
}
*/

int
xmit_async(struct ieee802154_hw *hw, struct sk_buff *skb){
    struct uwb_dev * udev = uwb_dev_idx_lookup(0);
    struct _uwb_transport_instance * uwb_transport = (struct _uwb_transport_instance *)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_TRANSPORT);
    assert(uwb_transport);

#if MYNEWT_VAL(UWB_TRANSPORT_ROLE) == 1
    stream_timer(uwb_transport);
#endif

    if(uwb_transport_dequeue_tx(uwb_transport,
            0, TIMEOUT) == true){
    }
    return 0;
}

int
recv_async(struct ieee802154_hw *hw, struct sk_buff *skb){

    struct uwb_dev * udev = uwb_dev_idx_lookup(0);
    struct _uwb_transport_instance * uwb_transport = (struct _uwb_transport_instance *)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_TRANSPORT);
    assert(uwb_transport);
    uwb_transport_listen(uwb_transport, UWB_BLOCKING, 0, TIMEOUT);
    return 0;
}
/*

int
ed(struct ieee802154_hw *hw, u8 *level){


}

int
set_channel(struct ieee802154_hw *hw, u8 page, u8 channel){

}
*/

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

