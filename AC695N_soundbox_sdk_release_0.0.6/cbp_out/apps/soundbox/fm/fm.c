#include "fm_server.h"


#define LOG_TAG_CONST       APP_FM
#define LOG_TAG             "[APP_FM]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


#if TCFG_APP_FM_EN

// key
extern u8 fm_key_event_get(struct key_event *key);

static int _key_event_opr(struct sys_event *event)
{
    int ret = true;
    int err = 0;
    int i, j;
    u8 temp = 0;
    struct key_event *key = &event->u.key;
    u8 key_event = fm_key_event_get(key);
    r_printf("key value:%d, event:%d \n", key->value, key->event);

#if (TCFG_SPI_LCD_ENABLE)
    extern int key_is_ui_takeover();
    if (key_is_ui_takeover()) {
        return false;
    }
#endif

    switch (key_event) {
    case  KEY_MUSIC_PP://暂停播放
        fm_server_msg_post(FM_MUSIC_PP);
        break;
    case  KEY_FM_SCAN_ALL_DOWN://全自动搜台
        fm_server_msg_post(FM_SCAN_ALL_DOWN);
        break;
    case  KEY_FM_SCAN_ALL_UP:
        fm_server_msg_post(FM_SCAN_ALL_UP);
        break;
    case  KEY_FM_SCAN_DOWN:
        fm_server_msg_post(FM_SCAN_DOWN);//半自动搜台
        break;
    case  KEY_FM_SCAN_UP:
        fm_server_msg_post(FM_SCAN_UP);//半自动搜台
        break;
    case  KEY_FM_PREV_STATION://下一台
        fm_server_msg_post(FM_PREV_STATION);
        break;
    case  KEY_FM_NEXT_STATION:
        fm_server_msg_post(FM_NEXT_STATION);
        break;
    case  KEY_FM_PREV_FREQ://下一个频率
        fm_server_msg_post(FM_PREV_FREQ);
        break;
    case  KEY_FM_NEXT_FREQ:
        fm_server_msg_post(FM_NEXT_FREQ);
        break;
    case KEY_VOL_UP:
        fm_server_msg_post(FM_VOLUME_UP);
        break;
    case KEY_VOL_DOWN:
        fm_server_msg_post(FM_VOLUME_DOWN);
        break;

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
#if TCFG_FM_INSIDE_ENABLE
// 对箱+内部fm，不开混响
    case KEY_REVERB_OPEN:
    case KEY_REVERB_DEEPVAL_UP:
    case KEY_REVERB_DEEPVAL_DOWN:
        ret = true;
        break;
#endif
#endif
#endif

    default:
        ret = false;
        break;
    }
    /* printf("fm_freq_cur:%x\n", __this->fm_freq_cur); */
    /* printf("fm_freq_channel_cur:%x\n", __this->fm_freq_channel_cur); */
    /* printf("fm_total_channel:%x\n", __this->fm_total_channel); */
    return ret;
}

// fm play event
static int _fm_event_opr(struct device_event *dev)
{
    int ret = false;
    int err = 0;

    switch (dev->event) {
    case AUDIO_PLAY_EVENT_END:
        log_info("AUDIO_PLAY_EVENT_END\n");
        break;
    case AUDIO_PLAY_EVENT_ERR:
        break;
    }

    return ret;
}

// tone play event
static int _tone_event_opr(struct device_event *dev)
{
    int ret = false;

    switch (dev->event) {
    case AUDIO_PLAY_EVENT_END:
        break;
    }

    return ret;
}

static int fm_event_handler(struct application *app, struct sys_event *event)
{
    int err = 0;
    switch (event->type) {
    case SYS_KEY_EVENT:
        return _key_event_opr(event);
        break;

    case SYS_DEVICE_EVENT:
        if ((u32)event->arg == DEVICE_EVENT_FROM_TONE) {
            _tone_event_opr(&event->u.dev);
        } else if ((u32)event->arg == DEVICE_EVENT_FROM_FM) {
            if (event->u.dev.event == DEVICE_EVENT_IN) {
                log_info("fm online \n");
            } else if (event->u.dev.event == DEVICE_EVENT_OUT) {
                log_info("fm offline \n");
                app_task_next();
            }
            return true;
        }
        return false;

    default:
        return false;
    }
}

static void fm_app_init(void)
{
    sys_key_event_enable();
    ui_update_status(STATUS_FM_MODE);
    clock_idle(FM_IDLE_CLOCK);
    fm_manage_init();
    fm_server_create();
#if (defined SMART_BOX_EN) && (SMART_BOX_EN)
	extern void function_change_inform(char *app_mode_name);
	function_change_inform(APP_NAME_FM);
#endif
}

static void fm_app_uninit(void)
{
    fm_sever_kill();
    fm_manage_close();
    /* tone_play_stop(); */
	tone_play_stop_by_index(IDEX_TONE_FM);
}


static int fm_state_machine(struct application *app, enum app_state state,
                            struct intent *it)
{
    int ret;
    switch (state) {
    case APP_STA_CREATE:
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
#if TCFG_FM_INSIDE_ENABLE
        reverb_pause();
        extern void bt_tws_sync_reverb_state(u8 status);
        bt_tws_sync_reverb_state(1);
#endif
#endif
#endif
        r_printf("APP_STA_CREATE\n");
        break;
    case APP_STA_START:
        if (!it) {
            break;
        }
        switch (it->action) {
        case ACTION_APP_MAIN:
            printf(" ACTION_FM_MAIN\n");
            fm_app_init();
            break;
        }
        break;
    case APP_STA_PAUSE:
        log_info("APP_STA_PAUSE\n");
        fm_manage_close();
        break;
    case APP_STA_RESUME:
        log_info("APP_STA_RESUME\n");
        break;
    case APP_STA_STOP:
        log_info("APP_STA_STOP\n");
        break;
    case APP_STA_DESTROY:
        log_info("APP_STA_DESTROY\n");
        fm_app_uninit();
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
#if TCFG_FM_INSIDE_ENABLE
        reverb_resume();
        extern void bt_tws_sync_reverb_state(u8 status);
        bt_tws_sync_reverb_state(0);
#endif
#endif
#endif
        break;
    }

    return 0;
}

static int fm_user_msg_deal(int msg, int argc, int *argv)
{
    return 0;
}

static const struct application_reg app_fm_reg = {
    .tone_name = TONE_FM,
    .tone_play_check = NULL,
    .tone_prepare = NULL,
    .enter_check = NULL,
    .exit_check = NULL,
    .user_msg = fm_user_msg_deal,
#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
    .tone_tws_cmd = SYNC_CMD_MODE_FM,
#endif
};

static const struct application_operation app_fm_ops = {
    .state_machine  = fm_state_machine,
    .event_handler 	= fm_event_handler,
};

REGISTER_APPLICATION(app_app_fm) = {
    .name 	= APP_NAME_FM,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_fm_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_fm_reg,
};


#endif

