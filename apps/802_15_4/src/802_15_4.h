
#include <stdio.h>
#include <uwb/uwb.h>

//---------- dummy structures to remove build errors
struct ieee802154_hw{
//XXX: TBD
};

struct sk_buff{
//XXX: TBD
};
//--------------

struct ieee802154_ops {
     int     (*start)(struct ieee802154_hw *hw);
//   void    (*stop)(struct ieee802154_hw *hw);

     int     (*xmit_async)(struct ieee802154_hw *hw, struct sk_buff *skb);
     int     (*recv_async)(struct ieee802154_hw *hw, struct sk_buff *skb);
//   int     (*ed)(struct ieee802154_hw *hw, u8 *level);
//   int     (*set_channel)(struct ieee802154_hw *hw, u8 page, u8 channel);
};

int start(struct ieee802154_hw *hw);
//void stop(struct ieee802154_hw *hw);
int xmit_async(struct ieee802154_hw *hw, struct sk_buff *skb);
int recv_async(struct ieee802154_hw *hw, struct sk_buff *skb);
//int ed(struct ieee802154_hw *hw, u8 *level);
//int set_channel(struct ieee802154_hw *hw, u8 page, u8 channel);
