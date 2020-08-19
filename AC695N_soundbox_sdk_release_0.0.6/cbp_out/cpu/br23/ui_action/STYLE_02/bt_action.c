#include "ui/ui.h"
#include "app_config.h"
#include "ui/ui_style.h"
#include "app_action.h"
#include "system/timer.h"
#include "key_event_deal.h"
#include "audio_config.h"
#include "jiffies.h"
#include "audio_eq.h"
#if TCFG_SPI_LCD_ENABLE

#define STYLE_NAME  JL

extern int ui_hide_main(int id);
extern int ui_show_main(int id);
extern void key_ui_takeover(u8 on);

static int bt_mode_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct window *window = (struct window *)ctr;
    switch (e) {
        case ON_CHANGE_INIT:
            puts("\n***bt_mode_onchange***\n");
            key_ui_takeover(1);
            /*
             * 注册APP消息响应
             */
            break;
        case ON_CHANGE_RELEASE:
            /*
             * 要隐藏一下系统菜单页面，防止在系统菜单插入USB进入USB页面
             */
            break;
        default:
            return false;
    }
    return false;
}



REGISTER_UI_EVENT_HANDLER(ID_WINDOW_BT)
    .onchange = bt_mode_onchange,
    .onkey = NULL,
    .ontouch = NULL,
};

static int bt_layout_onkey(void *ctr, struct element_key_event *e)
{
    printf("bt_layout_onkey %d\n",e->value);
    switch (e->value) {
    case KEY_MENU:
        if(ui_get_disp_status_by_id(BT_MENU_LAYOUT)<=0)
        {
            printf("BT_MENU_LAYOUT\n");
            ui_show(BT_MENU_LAYOUT);
        }
        break;
    case KEY_UP:
    case KEY_DOWN:
        break;
    case KEY_VOL_UP:
        if(ui_get_disp_status_by_id(BT_VOL_LAYOUT)<=0)
        {
            ui_show(BT_VOL_LAYOUT);
        }
        break;
    case KEY_VOL_DOWN:
        /* printf("KEY_VOL_DOWN"); */
        if(ui_get_disp_status_by_id(BT_VOL_LAYOUT)<=0)
        {
            ui_show(BT_VOL_LAYOUT);
        }
        break;
    default:
        return false;
        break;
    }
    return false;
}




REGISTER_UI_EVENT_HANDLER(BT_LAYOUT)
    .onchange = NULL,
    .onkey = bt_layout_onkey,
    .ontouch = NULL,
};



static u16 bt_timer = 0;
static void bt_vol_timeout(void *p)
{
    int id = (int)(p);
    if (ui_get_disp_status_by_id(id) == TRUE) {
        ui_hide(id);
    }
    bt_timer = 0;
}


static int bt_vol_layout_onkey(void *ctr, struct element_key_event *e)
{
    printf("bt_vol_layout_onkey %d\n",e->value);
    struct unumber num;
    u8 vol;
    switch (e->value) {
    case KEY_MENU:
        break;
    case KEY_UP:
    case KEY_DOWN:
        break;
    case KEY_VOL_UP:
        /* volume_up(); */
        /* vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE); */
        vol = jiffies%100;
        num.numbs =  1;
        num.number[0] = vol;
        ui_number_update_by_id(BT_VOL_NUM,&num);

        if(!bt_timer)
            bt_timer  = sys_timeout_add((void*)BT_VOL_LAYOUT,bt_vol_timeout,3000);
        else
            sys_timer_modify(bt_timer,3000);

        break;
    case KEY_VOL_DOWN:
        /* volume_down(); */
        /* vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE); */

        vol = jiffies%100;
        num.numbs =  1;
        num.number[0] = vol;
        ui_number_update_by_id(BT_VOL_NUM,&num);

        if(!bt_timer)
            bt_timer  = sys_timeout_add((void*)BT_VOL_LAYOUT,bt_vol_timeout,3000);
        else
            sys_timer_modify(bt_timer,3000);


        break;
    default:
        return false;
        break;
    }
    return true;
}




REGISTER_UI_EVENT_HANDLER(BT_VOL_LAYOUT)
    .onchange = NULL,
    .onkey = bt_vol_layout_onkey,
    .ontouch = NULL,
};



static int bt_menu_list_onkey(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    int sel_item = 0;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    switch (e->value) {
        case KEY_OK:
            sel_item = ui_grid_cur_item(grid);
            switch(sel_item)
            {
                case 0:
                    ui_show(BT_MENU_EQ_LAYOUT);
                    break;
                case 1:
                    ui_hide(BT_MENU_LAYOUT);
                    break;
            }
            break;
        default:
            return false;
    }
    return TRUE;
}
 


REGISTER_UI_EVENT_HANDLER(BT_MENU_LIST)
    .onchange = NULL,
    .onkey = bt_menu_list_onkey,
    .ontouch = NULL,
};


static int bt_eq_menu_list_onkey(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    int sel_item = 0;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    switch (e->value) {
        case KEY_OK:
            sel_item = ui_grid_cur_item(grid);
            switch(sel_item)
            {
                case 0:
                    break;
                case 6:
                    ui_hide(BT_MENU_EQ_LAYOUT);
                    break;
            }
            break;
        default:
            return false;
    }
    return TRUE;
}
 

static const int eq_mode[] = {
    BT_EQ_NORMAL_PIC,
    BT_EQ_ROCK_PIC,
    BT_EQ_POP_PIC,  
    BT_EQ_CLASSIC_PIC,
    BT_EQ_JAZZ_PIC,
    BT_EQ_COUNTRY_PIC,
};


static int bt_eq_menu_list_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    int list = 0;
    switch (e) {
    case ON_CHANGE_INIT:
        printf("ON_CHANGE_INIT %d \n", grid->avail_item_num);
        int id =  eq_mode[jiffies%6];
        printf("id = %x \n",id);
        ui_pic_show_image_by_id(id,1);
        break;
    }
    return false;
}



REGISTER_UI_EVENT_HANDLER(BT_EQ_MENU_LIST)
    .onchange = bt_eq_menu_list_onchange,
    .onkey = bt_eq_menu_list_onkey,
    .ontouch = NULL,
};
















#endif
