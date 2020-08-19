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


static int test_timer =0;

static void test_timer_add(void *p)
{
    struct unumber num;
    printf(">>%s\n",__FUNCTION__);
    num.numbs =  2;
    num.number[0] = jiffies%100;
    num.number[1] = jiffies%100;
    ui_number_update_by_id(FM_CH,&num);
    num.numbs =  2;
    num.number[0] = jiffies%1000;
    num.number[1] = jiffies%10;
    ui_number_update_by_id(FM_FREQ,&num);
    ui_slider_set_persent_by_id(FM_SLIDER,jiffies%100);
}


static int fm_mode_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct window *window = (struct window *)ctr;
    switch (e) {
        case ON_CHANGE_INIT:
            puts("\n***fm_mode_onchange***\n");
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



REGISTER_UI_EVENT_HANDLER(ID_WINDOW_FM)
    .onchange = fm_mode_onchange,
    .onkey = NULL,
    .ontouch = NULL,
};






static int fm_layout_onchange(void *ctr, enum element_change_event e, void *arg)
{
    switch (e) {
    case ON_CHANGE_INIT:
        sys_timer_add(NULL,test_timer_add,500);
        break;
    }
    return false;
}


static int fm_layout_onkey(void *ctr, struct element_key_event *e)
{
    printf("fm_layout_onkey %d\n",e->value);
    switch (e->value) {
        case KEY_MENU:
            if(ui_get_disp_status_by_id(FM_MENU_LAYOUT)<=0)
            {
                printf("FM_MENU_LAYOUT\n");
                ui_show(FM_MENU_LAYOUT);
            }
            break;
        case KEY_UP:
        case KEY_DOWN:
            break;
        case KEY_VOL_UP:
        case KEY_VOL_DOWN:
            if(ui_get_disp_status_by_id(FM_VOL_LAYOUT)<=0)
            {
                ui_show(FM_VOL_LAYOUT);
            }
            break;
        default:
            return false;
            break;
    }
    return false;
}




REGISTER_UI_EVENT_HANDLER(FM_LAYOUT)
    .onchange = fm_layout_onchange,
    .onkey = fm_layout_onkey,
    .ontouch = NULL,
};




static int fm_menu_list_onkey(void *ctr, struct element_key_event *e)
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
                    /* ui_show(BT_MENU_EQ_LAYOUT); */
                    break;
                case 2:
                    ui_hide(FM_MENU_LAYOUT);
                    break;
            }
            break;
        default:
            return false;
    }
    return TRUE;
}
 


REGISTER_UI_EVENT_HANDLER(FM_MENU_LIST)
    .onchange = NULL,
    .onkey = fm_menu_list_onkey,
    .ontouch = NULL,
};



static u16 fm_timer = 0;
static void fm_vol_timeout(void *p)
{
    int id = (int)(p);
    if (ui_get_disp_status_by_id(id) == TRUE) {
        ui_hide(id);
    }
    fm_timer = 0;
}



static int fm_vol_layout_onkey(void *ctr, struct element_key_event *e)
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
        ui_number_update_by_id(FM_VOL_NUM,&num);

        if(!fm_timer)
            fm_timer  = sys_timeout_add((void*)FM_VOL_LAYOUT,fm_vol_timeout,3000);
        else
            sys_timer_modify(fm_timer,3000);
        break;

    case KEY_VOL_DOWN:
        /* volume_down(); */
        /* vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE); */

        vol = jiffies%100;
        num.numbs =  1;
        num.number[0] = vol;
        ui_number_update_by_id(FM_VOL_NUM,&num);
        if(!fm_timer)
            fm_timer  = sys_timeout_add((void*)FM_VOL_LAYOUT,fm_vol_timeout,3000);
        else
            sys_timer_modify(fm_timer,3000);
        break;

    default:
        return false;
        break;
    }
    return true;
}




REGISTER_UI_EVENT_HANDLER(FM_VOL_LAYOUT)
    .onchange = NULL,
    .onkey = fm_vol_layout_onkey,
    .ontouch = NULL,
};






#endif
