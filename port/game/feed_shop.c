/* shop_dialogues.c: feed branch, quantity panel, commit and reopen.
 * Business predicates are the initial branches of original talk scripts 365/368. */
#include "feed_shop.h"
#include "psx_data.h"
#include <stdlib.h>
#include <string.h>
bool hmbtn_feed_shop_expand(uint8_t *record,size_t size,const HmbtnNewGame *registration)
{
    if(!record || size!=8192 || !registration) return false;
    uint16_t words[HMBTN_MESSAGE_WORDS];unsigned count=0;
    for(unsigned i=0;i<HMBTN_MESSAGE_WORDS;++i) {
        const uint16_t code=hmbtn_read_le16(record+0x1800+i*2);
        const uint16_t *name=code==0x400A ? registration->player_name
            : code==0x400D ? registration->farm_name : NULL;
        if(name) {
            for(unsigned j=0;j<HMBTN_NAME_CHARS && name[j]!=0xFFFF;++j) {
                if(count>=HMBTN_MESSAGE_WORDS) return false;
                words[count++]=name[j];
            }
        } else {
            if(count>=HMBTN_MESSAGE_WORDS) return false;
            words[count++]=code;
        }
        if(code==0xFFFF) {
            for(unsigned j=0;j<count;++j) hmbtn_write_le16(record+0x1800+j*2,words[j]);
            return true;
        }
    }
    return false;
}
unsigned hmbtn_feed_shop_cost(const HmbtnFeedShop *s)
{
    const HmbtnFarmItem *item=hmbtn_farm_item(s->item);
    return item && item->buy_price>0 ? (unsigned)item->buy_price*s->quantity : 0;
}
static void start_message(HmbtnFeedShop *s,HmbtnFeedShopPhase phase,unsigned message)
{
    s->phase=phase;s->request=message;s->loaded=0;s->closing=false;
    s->message=(HmbtnMessage){0};s->repeat=0;s->previous_arrows=0;
}
static void next(HmbtnFeedShop *s,HmbtnFeedShopPhase phase,unsigned message)
{
    s->next=phase;s->next_request=message;s->closing=true;
    s->message.phase=HMBTN_MESSAGE_CLOSING;s->message.box_frame=0;
}
bool hmbtn_feed_shop_open(HmbtnFeedShop *s,HmbtnVillage *v,const HmbtnIntroActor *p,
    const HmbtnFarmClock *clock,unsigned area,unsigned held_item)
{
    if(s->phase!=HMBTN_SHOP_OFF || held_item || p->facing>3) return false;
    const bool barn=area==136;
    if(!barn && area!=128) return false;
    const unsigned id=barn ? 15 : 18;
    if(clock->weekday==(barn ? 1u : 0u) || clock->hour<(barn ? 9u : 11u)
            || clock->hour>=(barn ? 15u : 16u)) return false;
    const HmbtnIntroActor *merchant=&v->people[id].actor;
    if(!merchant->visible || v->people[id].location!=(barn ? 9u : 49u)) return false;
    /* farm_villager_find_box: 200 X/Z and 20 Y about the forward probe. */
    static const int step[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    if(abs(merchant->position[0]-p->position[0]-step[p->facing][0])>=200
            || abs(merchant->position[2]-p->position[2]-step[p->facing][1])>=200
            || abs(merchant->position[1]-p->position[1])>=20) return false;
    HmbtnPoultry *poultry=s->poultry;
    memset(s,0,sizeof(*s));s->poultry=poultry;s->date=*clock;
    s->merchant=id;s->item=barn ? 0x68 : 0x69;s->quantity=1;
    s->saved_facing=merchant->facing;s->sound=3;
    start_message(s,HMBTN_SHOP_GREETING,barn ? 0x62 : 0x44);return true;
}
void hmbtn_feed_shop_tick(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnFarmStatistics *stats,
    uint32_t pressed,uint32_t held)
{
    s->sound=-1;
    if(s->phase==HMBTN_SHOP_NAME) {
        hmbtn_new_game_tick(&s->editor,pressed,held);s->sound=s->editor.sound;
        if(!s->editor.finished || s->editor.message.phase!=HMBTN_MESSAGE_CLOSED) return;
        s->after_result=HMBTN_SHOP_SELECT;
        if(s->editor.choice) start_message(s,HMBTN_SHOP_RESULT,0x4E);
        else if(!hmbtn_poultry_room(s->poultry)) start_message(s,HMBTN_SHOP_RESULT,0x55);
        else if(stats->gold<hmbtn_feed_shop_cost(s)) start_message(s,HMBTN_SHOP_RESULT,0x4F);
        else if(hmbtn_poultry_buy(s->poultry,s->editor.name,&s->date)) {
            unsigned cost=hmbtn_feed_shop_cost(s);stats->gold-=cost;stats->ledger[0][1]+=cost;
            stats->animals[0]=hmbtn_poultry_count(s->poultry);
            start_message(s,HMBTN_SHOP_RESULT,0x57);
        }
        return;
    }
    if(s->phase==HMBTN_SHOP_OFF || s->loaded!=s->request) return;
    const bool input=s->phase==HMBTN_SHOP_SELECT || s->phase==HMBTN_SHOP_COUNT;
    const bool ready=s->message.phase==HMBTN_MESSAGE_WAIT_END;
    s->sound=hmbtn_message_tick(&s->message,input ? 0 : pressed,held);
    if(s->closing) {
        if(s->message.phase==HMBTN_MESSAGE_CLOSED) start_message(s,s->next,s->next_request);
        return;
    }
    if(!input) {
        if(s->message.phase!=HMBTN_MESSAGE_CLOSED) return;
        switch(s->phase) {
        case HMBTN_SHOP_GREETING:case HMBTN_SHOP_INFO:start_message(s,HMBTN_SHOP_SELECT,0x46);break;
        case HMBTN_SHOP_OFFER:
            if(s->selection==1) { s->phase=HMBTN_SHOP_NAME;hmbtn_name_editor_begin(&s->editor); }
            else start_message(s,HMBTN_SHOP_COUNT,0x4D);
            break;
        case HMBTN_SHOP_RESULT:start_message(s,s->after_result,s->after_result==HMBTN_SHOP_SELECT ? 0x46 : 0x4D);break;
        case HMBTN_SHOP_LEAVE:s->phase=HMBTN_SHOP_OFF;s->request=0;break;
        default:break;
        }
        return;
    }
    if(!ready) return;
    const bool barn=s->merchant==15;
    if(pressed&HMBTN_PAD_CIRCLE) { s->sound=1;next(s,HMBTN_SHOP_LEAVE,barn ? 0x6E : 0x4E);return; }
    if(pressed&HMBTN_PAD_TRIANGLE) {
        s->sound=0;s->after_result=s->phase;
        next(s,HMBTN_SHOP_RESULT,barn ? 0x63 : s->selection ? 0x4A : 0x47);return;
    }
    if(s->phase==HMBTN_SHOP_SELECT) {
        if(!barn && s->poultry && (pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN))) {
            s->selection^=1;s->item=s->selection ? 0x109 : 0x69;s->quantity=1;s->sound=5;
        } else if(pressed&HMBTN_PAD_CROSS) {
            s->sound=0;s->after_result=HMBTN_SHOP_SELECT;
            if(s->selection && !hmbtn_poultry_room(s->poultry)) next(s,HMBTN_SHOP_RESULT,0x55);
            else if(s->selection && stats->gold<hmbtn_feed_shop_cost(s)) next(s,HMBTN_SHOP_RESULT,0x4F);
            else next(s,HMBTN_SHOP_OFFER,barn ? 0x6C : s->selection ? 0x56 : 0x4B);
        }
        return;
    }
    const uint32_t arrows=held&(HMBTN_PAD_UP|HMBTN_PAD_DOWN|HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT);
    uint32_t action=pressed;
    if(arrows!=s->previous_arrows) { s->repeat=0;action|=arrows;s->previous_arrows=arrows; }
    else if(arrows && ++s->repeat>=12 && (s->repeat-12)%3==0) action|=arrows;
    int quantity=(int)s->quantity;
    if(action&HMBTN_PAD_RIGHT) ++quantity;
    else if(action&HMBTN_PAD_LEFT) --quantity;
    else if(action&HMBTN_PAD_UP) quantity+=10;
    else if(action&HMBTN_PAD_DOWN) quantity-=10;
    else if(pressed&HMBTN_PAD_CROSS) {
        const unsigned cost=hmbtn_feed_shop_cost(s),stock=barn ? 0 : 2;
        s->after_result=HMBTN_SHOP_COUNT;s->sound=0;
        if(stats->gold<cost) next(s,HMBTN_SHOP_RESULT,0x4F);
        else if(h->supplies[stock]>=1000 || s->quantity>=1000-h->supplies[stock])
            next(s,HMBTN_SHOP_RESULT,barn ? 0x6D : 0x4C);
        else {
            h->supplies[stock]+=s->quantity;stats->gold-=cost;stats->ledger[0][1]+=cost;
            s->after_result=HMBTN_SHOP_SELECT;next(s,HMBTN_SHOP_RESULT,barn ? 0x70 : 0x52);
        }
        return;
    }
    if(quantity!=(int)s->quantity) { s->quantity=quantity>99 ? 1 : quantity<=0 ? 99 : (unsigned)quantity;s->sound=5; }
}
