/*****************************************************************
>file name : audio_pc.c
>author : lichao
>create time : Wed 22 May 2019 10:36:22 AM CST
*****************************************************************/
#include "uac_stream.h"
#include "app_config.h"
#include "audio_decoder.h"
#include "media/includes.h"
#include "audio_config.h"
#include "system/includes.h"
#include "audio_enc.h"
#include "audio_eq.h"
#include "audio_drc.h"
#include "app_config.h"
#include "audio_config.h"
#include "audio_dec.h"
#include "app_main.h"
#include "clock_cfg.h"

#if TCFG_UI_ENABLE
#include "ui/ui_api.h"
#endif


#if TCFG_APP_PC_EN

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
#define UAC_DEC_PCM_ENC_TYPE		AUDIO_CODING_SBC
#define UAC_DEC_PCM_ENC_CHANNEL		AUDIO_CH_LR
#else
#define UAC_DEC_PCM_ENC_TYPE		AUDIO_CODING_MP3
#define UAC_DEC_PCM_ENC_CHANNEL		AUDIO_CH_LR
#endif

struct usb_audio_handle {
    int top_size;
    int bottom_size;
    int begin_size;
};

#define RATE_INC_STEP       2
#define RATE_DEC_STEP       2

#define PC_EQ_SUPPORT_ASYNC		1

#ifndef CONFIG_EQ_SUPPORT_ASYNC
#undef PC_EQ_SUPPORT_ASYNC
#define PC_EQ_SUPPORT_ASYNC		0
#endif


#if PC_EQ_SUPPORT_ASYNC && TCFG_PC_MODE_EQ_ENABLE
#define PC_EQ_SUPPORT_32BIT		1
#else
#define PC_EQ_SUPPORT_32BIT		0
#endif


struct uac_dec_hdl {
    struct audio_decoder decoder;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;
    int begin_size;
    int top_size;
    int bottom_size;
    u8 start;
    u8 channel;
    u8 output_ch;
    u8 sync_start;
    u16 src_out_sr;
    u32 dec_no_out_sound : 1;	// 解码不直接输出声音（用于TWS转发）
    u16 sample_rate;
    u16 audio_new_rate;
    u16 usb_audio_max_speed;
    u16 usb_audio_min_speed;
    struct audio_src_handle *src_sync;
    u8 remain;
    u8 eq_remain;
#if TCFG_PC_MODE_EQ_ENABLE
    struct audio_eq *p_eq;
#endif
#if TCFG_PC_MODE_DRC_ENABLE
    struct audio_drc *p_drc;
#endif

    struct user_audio_parm *user_hdl;

#if  PC_EQ_SUPPORT_32BIT
	s16 *eq_out_buf;
	int eq_out_buf_len;
	int eq_out_points;
	int eq_out_total;
#endif


};


extern struct audio_decoder_task decode_task;
extern struct audio_mixer mixer;

static u16 sys_event_id = 0;
static struct uac_dec_hdl *uac_dec = NULL;

static u8 uac_dec_maigc = 0;

extern u16 uac_get_cur_vol(void);
extern u8 uac_get_mute(void);
extern void bt_tws_sync_volume();

void pc_eq_drc_open(struct uac_dec_hdl *dec, struct audio_fmt *fmt);
void pc_eq_drc_close(struct uac_dec_hdl *dec);

int uac_vol_switch(int vol)
{
    u16 valsum = vol * (SYS_MAX_VOL + 1) / 100;

    if (valsum > SYS_MAX_VOL) {
        valsum = SYS_MAX_VOL;
    }
    return valsum;
}

static void uac_dec_set_output_channel(struct uac_dec_hdl *dec)
{
    u8 dac_conn = app_audio_output_mode_get();
    if (dac_conn == DAC_OUTPUT_LR || dac_conn == DAC_OUTPUT_DUAL_LR_DIFF) {
        dec->output_ch = 2;
    } else {
        dec->output_ch = 1;
    }
}

static int uac_sync_output_handler(void *priv, void *buf, int len)
{
    struct uac_dec_hdl *dec = (struct uac_dec_hdl *)priv;
    int remain_len = len;
    int wlen = 0;
    s16 *data = (s16 *)buf;

#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
    if (dec->dec_no_out_sound) {
        return pcm2tws_enc_output(NULL, data, len);
    }
#endif

	wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
	return wlen;
}

static int uac_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    struct uac_dec_hdl *dec = container_of(decoder, struct uac_dec_hdl, decoder);
    char err = 0;
    int rlen = len;
    int wlen = 0;

    if (!dec->remain) {
#if ((defined(DEC_OUT_OTHER_DATA) && (DEC_OUT_OTHER_DATA)))
        other_audio_dec_output(decoder, data, len, dec->output_ch, dec->sample_rate);
#endif

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
        if (dec->user_hdl) {
            u8 ch_num = dec->output_ch;
            user_audio_process_handler_run(dec->user_hdl, data, len, ch_num);
        }
#endif

    }
#if PC_EQ_SUPPORT_ASYNC
#if TCFG_PC_MODE_EQ_ENABLE
    if (dec->p_eq) {
        int eqlen = audio_eq_run(dec->p_eq, data, len);
        len -= eqlen;
        if (len == 0) {
            dec->remain = 0;
        } else {
            dec->remain = 1;
        }
        return eqlen;
    }
#endif
#endif


    if (!dec->remain) {
#if TCFG_PC_MODE_EQ_ENABLE
        if (dec->p_eq) {
            audio_eq_run(dec->p_eq, data, len);
        }
#endif
#if TCFG_PC_MODE_DRC_ENABLE
        if (dec->p_drc) {
            audio_drc_run(dec->p_drc, data, len);
        }
#endif
    }


    do {
        if (dec->src_sync) {
            wlen = audio_src_resample_write(dec->src_sync, data, rlen);
            /*printf("%d - %d\n", len, wlen);*/
            /* return wlen; */
#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
        } else if (dec->dec_no_out_sound) {
            wlen = pcm2tws_enc_output(NULL, data, rlen);
#endif
        } else {
            wlen = audio_mixer_ch_write(&dec->mix_ch, data, rlen);
        }
        if (!wlen) {
            err++;
            if (err < 2) {
                continue;
            }
            break;
        }
        err = 0;
        data += wlen / 2;
        rlen -= wlen;
    } while (rlen > 0);

    if (rlen == 0) {
        dec->remain = 0;
    } else {
        dec->remain = 1;
    }


    return len - rlen;
}

static void pcm_LR_to_mono(s16 *pcm_lr, s16 *pcm_mono, int points_len)
{
    s16 pcm_L;
    s16 pcm_R;
    int i = 0;

    for (i = 0; i < points_len; i++, pcm_lr += 2) {
        pcm_L = *pcm_lr;
        pcm_R = *(pcm_lr + 1);
        *pcm_mono++ = (s16)(((int)pcm_L + pcm_R) >> 1);
    }
}

static int uac_stream_read(struct audio_decoder *decoder, void *buf, u32 len)
{
    int rlen = 0;
    struct uac_dec_hdl *dec = container_of(decoder, struct uac_dec_hdl, decoder);

    if (!dec->sync_start) {
        if (uac_speaker_stream_size() < dec->begin_size) {
            return -1;
        }
        dec->sync_start = 1;
    }

    rlen = uac_speaker_read(NULL, (void *)buf, len);
    if (!rlen) {
        return -1;
    }

    if (dec->channel == 2 && dec->output_ch == 1) {
        pcm_LR_to_mono((s16 *)buf, (s16 *)buf, rlen >> 2);
        rlen >>= 1;
    }

    return rlen;
}

static const struct audio_dec_input uac_input = {
    .coding_type = AUDIO_CODING_PCM,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = uac_stream_read,
            /*.fseek = uac_stream_seek,*/
            /*.flen  = uac_stream_len,*/
        }
    }
};

static int usb_audio_stream_sync(struct uac_dec_hdl *dec, int data_size)
{
    if (!dec->src_sync) {
        return 0;
    }
    u16 sr = dec->audio_new_rate;

    if (data_size < dec->bottom_size) {
        dec->audio_new_rate += RATE_INC_STEP;
        /*printf("rate inc\n");*/
    }

    if (data_size > dec->top_size) {
        dec->audio_new_rate -= RATE_DEC_STEP;
        /*printf("rate dec : %d\n", __this->audio_new_rate);*/
    }

    if (dec->audio_new_rate < dec->usb_audio_min_speed) {
        dec->audio_new_rate = dec->usb_audio_min_speed;
    } else if (dec->audio_new_rate > dec->usb_audio_max_speed) {
        dec->audio_new_rate = dec->usb_audio_max_speed;
    }

    if (sr != dec->audio_new_rate) {
        audio_hw_src_set_rate(dec->src_sync, dec->sample_rate, dec->audio_new_rate);
    }

    return 0;
}

static int uac_dec_probe_handler(struct audio_decoder *decoder)
{
    struct uac_dec_hdl *dec = container_of(decoder, struct uac_dec_hdl, decoder);
    int err = 0;

#if 0
    if (!dec->sync_start) {
        if (uac_speaker_stream_size() > dec->begin_size) {
            dec->sync_start = 1;
            return 0;
        } else {
            os_time_dly(2);
            audio_decoder_suspend(&dec->decoder, 0);
            /* audio_decoder_resume(&dec->decoder); */
            /* return 0;//临时 */
            return -EINVAL;
        }
    }
#else
    if (!dec->sync_start) {
        return 0;
    }
#endif

#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
    if (dec->dec_no_out_sound) {
        // localtws同步有微调处理
        return 0;
    }
#endif

    usb_audio_stream_sync(dec, uac_speaker_stream_size());
    return err;
}

static const struct audio_dec_handler uac_dec_handler = {
    .dec_probe  = uac_dec_probe_handler,
    .dec_output = uac_dec_output_handler,
};

static int uac_audio_sync_init(struct uac_dec_hdl *dec)
{
    dec->sync_start = 0;
    dec->begin_size = uac_speaker_stream_length() * 60 / 100;
    dec->top_size = uac_speaker_stream_length() * 70 / 100;
    dec->bottom_size = uac_speaker_stream_length() * 40 / 100;

    dec->audio_new_rate = dec->src_out_sr;//audio_output_rate(dec->sample_rate);
    printf("out_sr:%d, dsr:%d, \n", dec->audio_new_rate, dec->sample_rate);
    dec->usb_audio_max_speed = dec->audio_new_rate + 50;
    dec->usb_audio_min_speed = dec->audio_new_rate - 50;

#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
    if (dec->dec_no_out_sound) {
        if (dec->audio_new_rate == dec->sample_rate) {
            return -ENODEV;
        }
    }
#endif

    dec->src_sync = zalloc(sizeof(struct audio_src_handle));
    if (!dec->src_sync) {
        return -ENODEV;
    }
    audio_hw_src_open(dec->src_sync, dec->output_ch, SRC_TYPE_AUDIO_SYNC);

    audio_hw_src_set_rate(dec->src_sync, dec->sample_rate, dec->audio_new_rate);

    audio_src_set_output_handler(dec->src_sync, dec, uac_sync_output_handler);
    return 0;
}

static int uac_audio_sync_release(struct uac_dec_hdl *dec)
{
    if (dec->src_sync) {
        audio_hw_src_stop(dec->src_sync);
        audio_hw_src_close(dec->src_sync);
        local_irq_disable();
        free(dec->src_sync);
        dec->src_sync = NULL;
        local_irq_enable();
    }

    return 0;
}

static int uac_audio_close(void)
{
    if (!uac_dec || !uac_dec->start) {
        return 0;
    }

    uac_dec->start = 0;
    uac_dec->sync_start = 0;

    audio_decoder_close(&uac_dec->decoder);
    uac_audio_sync_release(uac_dec);
    if (!uac_dec->dec_no_out_sound) {
        audio_mixer_ch_close(&uac_dec->mix_ch);
        app_audio_state_exit(APP_AUDIO_STATE_MUSIC);
    }

#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
    if (uac_dec->dec_no_out_sound) {
        uac_dec->dec_no_out_sound = 0;
        pcm2tws_enc_close();
        encfile_tws_dec_close();
        local_tws_stop();

    }

#endif
    pc_eq_drc_close(uac_dec);
#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    if (uac_dec->user_hdl) {
        user_audio_process_close(uac_dec->user_hdl);
        uac_dec->user_hdl = NULL;
    }
#endif

    clock_set_cur();
    return 0;
}

static void uac_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        puts("USB AUDIO_DEC_EVENT_END\n");
        if ((u8)argv[1] != (u8)(uac_dec_maigc - 1)) {
            log_i("maigc err, %s\n", __FUNCTION__);
            break;
        }
        /*uac_audio_close();*/
        break;
    }
}


static int uac_audio_start(void)
{
    int err;
    struct audio_fmt f = {0};
    struct uac_dec_hdl *dec = uac_dec;

    if (!uac_dec) {
        return -EINVAL;
    }

    err = audio_decoder_open(&dec->decoder, &uac_input, &decode_task);
    if (err) {
        goto __err;
    }

    audio_decoder_set_handler(&dec->decoder, &uac_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, uac_dec_event_handler, uac_dec_maigc++);


    /* uac_dec_set_output_channel(dec); */
    dec->output_ch = audio_output_channel_num();

    f.coding_type = AUDIO_CODING_PCM;
    f.sample_rate = dec->sample_rate;
    f.channel = dec->output_ch;

    err = audio_decoder_set_fmt(&dec->decoder, &f);

    if (err) {
        goto __err;
    }

#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
    dec->dec_no_out_sound = 0;
    if (dec2tws_check_enable() == true) {
        audio_decoder_set_output_channel(&dec->decoder, UAC_DEC_PCM_ENC_CHANNEL);
        dec->output_ch = AUDIO_CH_IS_MONO(UAC_DEC_PCM_ENC_CHANNEL) ? 1 : 2;;
        struct audio_fmt enc_fmt = {0};
        enc_fmt.coding_type = UAC_DEC_PCM_ENC_TYPE;
        enc_fmt.bit_rate = 128;
        enc_fmt.channel = dec->output_ch;
        enc_fmt.sample_rate = audio_output_rate(dec->sample_rate);//dec->decoder.fmt.sample_rate;
        enc_fmt.sample_rate = app_audio_output_samplerate_select(enc_fmt.sample_rate, 1);
#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
        enc_fmt.sample_rate = pcm2tws_sbc_sample_rate_select(enc_fmt.sample_rate);
#endif

        int ret = pcm2tws_enc_open(&enc_fmt);
        if (ret == 0) {
#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
            pcm2tws_enc_set_output_handler(local_tws_output);
#else
            pcm2tws_enc_set_output_handler(encfile_tws_wfile);
            ret = encfile_tws_dec_open(&enc_fmt);
#endif
            if (ret == 0) {
                dec->dec_no_out_sound = 1;
                pcm2tws_enc_set_resume_handler(uac_dec_resume);
                audio_decoder_task_del_wait(&decode_task, &dec->wait);
                local_tws_start(&enc_fmt);
                audio_decoder_set_run_max(&dec->decoder, 20);
                dec->src_out_sr = enc_fmt.sample_rate;
                goto __dec_start;
            } else {
                pcm2tws_enc_close();
            }
        }
    }
#endif


    audio_mixer_ch_open(&dec->mix_ch, &mixer);
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, audio_output_rate(f.sample_rate));

    dec->src_out_sr = audio_output_rate(f.sample_rate);

__dec_start:
    pc_eq_drc_open(dec, &f);
    uac_audio_sync_init(dec);

    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);
    u8 vol = uac_vol_switch(uac_get_cur_vol());
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, vol, 0);
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    bt_tws_sync_volume();
#endif

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    struct user_audio_digital_parm  vol_parm = {0};
#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
    vol_parm.en  = true;
    vol_parm.vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
    vol_parm.vol_max = app_audio_get_max_volume();
    vol_parm.fade_step = 2;
#endif
    dec->user_hdl = user_audio_process_open((void *)&vol_parm, NULL, NULL);
#endif

    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err;
    }

    clock_set_cur();
    dec->start = 1;
    return 0;

__err:

    pc_eq_drc_close(dec);
#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    if (dec->user_hdl) {
        user_audio_process_close(dec->user_hdl);
        dec->user_hdl = NULL;
    }
#endif

    return err;
}



static int uac_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

    if (event == AUDIO_RES_GET) {
        err = uac_audio_start();
    } else if (event == AUDIO_RES_PUT) {
        uac_audio_close();
    }

    return err;
}

static void uac_speaker_stream_rx_handler(int event, void *data, int len)
{
    if (uac_dec) {
        audio_decoder_resume(&uac_dec->decoder);
    }
}

extern struct uac_stream_info *puac_speaker_info;
static int usb_audio_play_open(void *_info)
{
    struct uac_dec_hdl *dec;

    if (uac_dec) {
        return 0;
    }

    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }

    uac_dec = dec;

    dec->sample_rate = (u32)_info & 0xFFFFFF;
    dec->channel = (u32)_info >> 24;
    printf("usb_audio_play_open sr:%d ch:%d\n", dec->sample_rate, dec->channel);
    set_uac_speaker_rx_handler(dec, uac_speaker_stream_rx_handler);
    dec->wait.priority = 2;
    dec->wait.preemption = 1;
    dec->wait.handler = uac_wait_res_handler;
    audio_decoder_task_add_wait(&decode_task, &dec->wait);

    clock_add(DEC_PCM_CLK);

    return 0;
}

static int usb_audio_play_close(void *arg)
{
    int err = 0;

    if (!uac_dec) {
        return 0;
    }

    if (uac_dec->start) {
        uac_audio_close();
    }


    audio_decoder_task_del_wait(&decode_task, &uac_dec->wait);

    clock_remove(DEC_PCM_CLK);

    local_irq_disable();
    free(uac_dec);
    uac_dec = NULL;
    local_irq_enable();

    return 0;
}


int uac_dec_restart(int magic)
{
    if ((!uac_dec) || (magic != uac_dec_maigc)) {
        return -1;
    }
    int _info = (uac_dec->channel << 24) | uac_dec->sample_rate;
    usb_audio_play_close(NULL);
    int err = usb_audio_play_open((void *)_info);
    return err;
}

int uac_dec_push_restart(void)
{
    if (!uac_dec) {
        return false;
    }
    int argv[3];
    argv[0] = (int)uac_dec_restart;
    argv[1] = 1;
    argv[2] = (int)uac_dec_maigc;
    os_taskq_post_type(os_current_task(), Q_CALLBACK, ARRAY_SIZE(argv), argv);
    return true;
}

void uac_dec_resume(void)
{
    if (uac_dec && (uac_dec->start)) {
        audio_decoder_resume(&uac_dec->decoder);
    }
}
int uac_dec_no_out_sound(void)
{
    if (uac_dec && uac_dec->dec_no_out_sound) {
        return true;
    }
    return false;
}


int usb_audio_mic_open(void *_info);
int usb_audio_mic_close(void *arg);
void usb_audio_mic_set_gain(int gain);

static int usb_device_event_handler(u8 event, int value)
{
    switch (event) {
    case USB_AUDIO_PLAY_OPEN:
        /*tone_play_stop();*/
        usb_audio_play_open((void *)value);
        break;
    case USB_AUDIO_PLAY_CLOSE:
        usb_audio_play_close((void *)value);
        break;
    case USB_AUDIO_MIC_OPEN:
        usb_audio_mic_open((void *)value);
        break;
    case USB_AUDIO_MIC_CLOSE:
        usb_audio_mic_close((void *)value);
        break;
    case USB_AUDIO_SET_MIC_VOL:
        usb_audio_mic_set_gain(value);
        break;
    case USB_AUDIO_SET_PLAY_VOL:
        app_audio_set_volume(APP_AUDIO_STATE_MUSIC, value, 1);
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        bt_tws_sync_volume();
#endif

#if TCFG_UI_ENABLE
        u8 vol = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
        ui_set_tmp_menu(MENU_MAIN_VOL, 1000, vol, NULL);
#endif //TCFG_UI_ENABLE
        break;
    default:
        break;
    }
    return 0;
}

static void usb_audio_event_handler(struct sys_event *event, void *priv)
{
    switch (event->type) {
    case SYS_DEVICE_EVENT:
        if ((u32)event->arg == DEVICE_EVENT_FROM_UAC) {
            log_d("usb device event : %d %x\n", event->u.dev.event, event->u.dev.value);
            usb_device_event_handler(event->u.dev.event, event->u.dev.value);
        }
        return;
    default:
        break;
    }
    return;
}

// usb audio 过程中播放提示音
int usb_audio_and_tone_play(u8 start)
{
    /*
    if (__this->onoff) {
        if (start) { // 开始播放提示音
            usb_audio_play_close((void *)__this->play_info);
        } else { // 提示音播放完成
            USB_AUDIO_MUTEX_PEND(0);
            uac_speaker_stream_buf_clear();
            USB_AUDIO_MUTEX_POST();
            usb_audio_play_open((void *)__this->play_info);
        }
    }
    */
    return 0;//__this->onoff;
}

__attribute__((weak))int audio_dev_init()
{
    return 0;
}

int usb_audio_demo_init(void)
{
    int err = 0;

    audio_dev_init();
    sys_event_id = register_sys_event_handler(SYS_ALL_EVENT, 2, NULL, usb_audio_event_handler);

    return 0;
}

void usb_audio_demo_exit(void)
{
    if (sys_event_id) {
        unregister_sys_event_handler(sys_event_id);
        sys_event_id = 0;
        usb_audio_play_close(NULL);
        usb_audio_mic_close(NULL);
    }
}

#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
/*
 *pc歌数字音量调节
 * */
void pc_user_digital_volume_set(u8 vol)
{
    if (uac_dec && uac_dec->user_hdl && uac_dec->user_hdl->dvol_hdl) {
        user_audio_digital_volume_set(uac_dec->user_hdl->dvol_hdl, vol);
    }
}

u8 pc_user_audio_digital_volume_get()
{
    if (!uac_dec) {
        return 0;
    }
    if (!uac_dec->user_hdl) {
        return 0;
    }
    if (!uac_dec->user_hdl->dvol_hdl) {
        return 0;
    }
    return user_audio_digital_volume_get(uac_dec->user_hdl->dvol_hdl);
}

/*
 *user_vol_max:音量级数
 *user_vol_tab:自定义音量表,自定义表长user_vol_max+1
 *注意：如需自定义音量表，须在volume_set前调用 ,否则会在下次volume_set时生效
 */
void pc_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max)
{
    if (uac_dec && uac_dec->user_hdl && uac_dec->user_hdl->dvol_hdl) {
        user_audio_digital_set_volume_tab(uac_dec->user_hdl->dvol_hdl, user_vol_tab, user_vol_max);
    }
}



#endif


#if PC_EQ_SUPPORT_32BIT
void pc_eq_32bit_out(struct uac_dec_hdl *dec)
{
    int wlen = 0;

	if (dec->src_sync) {
		wlen = audio_src_resample_write(dec->src_sync, &dec->eq_out_buf[dec->eq_out_points], (dec->eq_out_total-dec->eq_out_points)*2);
	} else{
		wlen = audio_mixer_ch_write(&dec->mix_ch, &dec->eq_out_buf[dec->eq_out_points], (dec->eq_out_total-dec->eq_out_points)*2);
	}
	dec->eq_out_points += wlen/2;
}
   #endif /*PC_EQ_SUPPORT_32BIT*/


static int pc_eq_output(void *priv, s16 *data, u32 len)
{
    int wlen = 0;
    int rlen = len;
    struct uac_dec_hdl *dec = priv;
#if PC_EQ_SUPPORT_ASYNC

    if (!dec->eq_remain) {
#if PC_EQ_SUPPORT_32BIT
		if (dec->eq_out_buf && (dec->eq_out_points < dec->eq_out_total)) {
			pc_eq_32bit_out(dec);
			if (dec->eq_out_points < dec->eq_out_total) {
				return 0;
			}
		}
#endif /*PC_EQ_SUPPORT_32BIT*/

#if TCFG_PC_MODE_DRC_ENABLE
        if (dec->p_drc) {
            audio_drc_run(dec->p_drc, data, len);
        }
#endif
    }

#if PC_EQ_SUPPORT_32BIT
		if ((!dec->eq_out_buf) || (dec->eq_out_buf_len < len/2)) {
			if (dec->eq_out_buf) {
				free(dec->eq_out_buf);
			}
			dec->eq_out_buf_len = len/2;
			dec->eq_out_buf = malloc(dec->eq_out_buf_len);
			ASSERT(dec->eq_out_buf);
		}
		s32 *idat = data;
		s16 *odat = dec->eq_out_buf;
		for (int i=0; i<len/4; i++) {
			s32 outdat = *idat++;
			if (outdat > 32767) {
				outdat = 32767;
			} else if (outdat < -32768) {
				outdat = -32768;
			}
			*odat++ = outdat;
		}
		dec->eq_out_points = 0;
	   	dec->eq_out_total = len/4;

		pc_eq_32bit_out(dec);
		return len;
#endif /*PC_EQ_SUPPORT_32BIT*/

        if (dec->src_sync) {
            wlen = audio_src_resample_write(dec->src_sync, data, len);
        } else {
            wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
        }
    if (wlen == len) {
        dec->eq_remain = 0;
    } else {
        dec->eq_remain = 1;
    }
    return wlen;
#endif
    return len;
}



void pc_eq_drc_open(struct uac_dec_hdl *dec, struct audio_fmt *fmt)
{
    if (!dec) {
        return;
    }
#if TCFG_PC_MODE_EQ_ENABLE
    dec->p_eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (dec->p_eq) {
        dec->p_eq->eq_ch = (struct hw_eq_ch *)((int)dec->p_eq + sizeof(struct audio_eq));
        struct audio_eq_param pc_eq_param = {0};
        pc_eq_param.channels = dec->channel;
        pc_eq_param.online_en = 1;
        pc_eq_param.mode_en = 1;
        pc_eq_param.remain_en = 1;
        pc_eq_param.max_nsection = EQ_SECTION_MAX;
        pc_eq_param.cb = eq_get_filter_info;
#if PC_EQ_SUPPORT_ASYNC
        pc_eq_param.no_wait = 1;//异步
#endif
        pc_eq_param.eq_name = 0;
        audio_eq_open(dec->p_eq, &pc_eq_param);
        audio_eq_set_samplerate(dec->p_eq, fmt->sample_rate);
        audio_eq_set_output_handle(dec->p_eq, pc_eq_output, dec);
#if PC_EQ_SUPPORT_32BIT
		audio_eq_set_info(dec->p_eq, pc_eq_param.channels, 1);
#endif

        audio_eq_start(dec->p_eq);
    }

#endif


#if TCFG_PC_MODE_DRC_ENABLE


    dec->p_drc = malloc(sizeof(struct audio_drc));
    if (dec->p_drc) {
        struct audio_drc_param drc_param = {0};
        drc_param.channels = dec->channel;
        drc_param.online_en = 1;
        drc_param.remain_en = 1;
        drc_param.cb = drc_get_filter_info;
        drc_param.stero_div = 0;

        drc_param.drc_name = 0;
        audio_drc_open(dec->p_drc, &drc_param);
        audio_drc_set_samplerate(dec->p_drc, fmt->sample_rate);
#if PC_EQ_SUPPORT_32BIT
		audio_drc_set_32bit_mode(dec->p_drc, 1);
#endif

        audio_drc_set_output_handle(dec->p_drc, NULL, NULL);
        audio_drc_start(dec->p_drc);
    }
#endif

}


void pc_eq_drc_close(struct uac_dec_hdl *dec)
{
    if (!dec) {
        return;
    }
#if TCFG_PC_MODE_EQ_ENABLE
    if (dec->p_eq) {
        audio_eq_close(dec->p_eq);
        free(dec->p_eq);
        dec->p_eq = NULL;
    }
#endif

#if TCFG_PC_MODE_DRC_ENABLE
    if (dec->p_drc) {
        audio_drc_close(dec->p_drc);
        free(dec->p_drc);
        dec->p_drc = NULL;
    }
#endif

#if PC_EQ_SUPPORT_32BIT
    if (dec->eq_out_buf) {
        free(dec->eq_out_buf);
        dec->eq_out_buf = NULL;
    }
#endif


}
#endif /* TCFG_APP_PC_EN */

