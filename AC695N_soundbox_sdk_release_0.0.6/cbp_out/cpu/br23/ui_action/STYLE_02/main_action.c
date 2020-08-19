#include "ui/ui.h"
#include "app_config.h"
#include "ui/ui_style.h"
#include "app_action.h"
#include "system/timer.h"

#if TCFG_SPI_LCD_ENABLE

#define STYLE_NAME  JL
REGISTER_UI_STYLE(STYLE_NAME)

extern int ui_hide_main(int id);
extern int ui_show_main(int id);
extern void key_ui_takeover(u8 on);


static const char * task_switch[4] = 
{
    APP_NAME_BT,										
    APP_NAME_MUSIC,									
    APP_NAME_FM,									
    APP_NAME_LINEIN,							
};

static int task_switch_check(const char *name)
{
    return true;
}


static int main_mode_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct window *window = (struct window *)ctr;
    static int id = 0;

    switch (e) {
        case ON_CHANGE_INIT:
            puts("\n***main_mode_onchange***\n");
            key_ui_takeover(1);
            /* ui_register_msg_handler(ID_WINDOW_VIDEO_REC, rec_msg_handler); */
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


REGISTER_UI_EVENT_HANDLER(ID_WINDOW_MAIN)
    .onchange = main_mode_onchange,
    .onkey = NULL,
    .ontouch = NULL,
};







static int menu_control_onkey(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    int sel_item = 0;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    switch (e->value) {
    case KEY_OK:
        puts("KEY_OK\n");
        sel_item = ui_grid_cur_item(grid);
        printf("sel_item = %d \n", sel_item);
        if(sel_item < sizeof(task_switch)/sizeof(task_switch[0]))
        {
            if(task_switch_check(task_switch[sel_item])){
                app_task_switch(task_switch[sel_item], ACTION_APP_MAIN, NULL);
            }
            else
            {

            }

        }
        break;
    case KEY_MENU:

        break;
    case KEY_DOWN:
    case KEY_UP:
        return FALSE;
        /*
         * 向后分发消息
         */

    default:
        /* ui_hide(BT_CTRL_LAYOUT); */
        break;
    }

    return true;
    /*
     * 不向后分发消息
     */
}

static int menu_control_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static int sel_item_last = -1;
    int sel_item = 0;
    switch (e) {
        case ON_CHANGE_INIT:
            break;
        case ON_CHANGE_SHOW_PROBE:
            /* break; */
        /* case ON_CHANGE_HIGHLIGHT: */
            /* puts("ON_CHANGE_HIGHLIGHT \n"); */
            sel_item = ui_grid_cur_item(grid);
            if(sel_item_last != sel_item)
            {
                printf("sel_item = %d \n", sel_item);
                /* ui_text_show_index_by_id(MENU_TEXT,sel_item); */
                sel_item_last = sel_item;
            }
            break;
    }
    return false;
}



REGISTER_UI_EVENT_HANDLER(MENU_MAIN_LIST)
    .onkey = menu_control_onkey,
    .onchange = menu_control_onchange,
    .ontouch = NULL,
};





#endif
