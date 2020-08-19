#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_online_cfg.h"
#include "audio_drc.h"
#include "common/Resample_api.h"
#include "audio_pitch.h"
#include "clock_cfg.h"
#include "audio_config.h"
#define LOG_TAG     "[APP-REVERB]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"
#include "effects_config.h"

#if ((defined(TCFG_SPEED_PITCH_ENABLE) && (TCFG_SPEED_PITCH_ENABLE))||(defined(TCFG_MIC_REC_PITCH_EN)&&(TCFG_MIC_REC_PITCH_EN))||(TCFG_REVERB_PITCH_EN)	)


//*******************变速变调接口***********************//
static PITCH_SHIFT_PARM picth_parm;

static void init_pitch_shift_parm()
{
    picth_parm.shiftv = PICTH_SHIFTV;//5000 ;             //pitch rate:  <8192(pitch up), >8192(pitch down)   ：调节范围4000到16000
#if (defined(TCFG_REVERB_SAMPLERATE_DEFUALT))
    picth_parm.sr = TCFG_REVERB_SAMPLERATE_DEFUALT;                             //配置输入audio的采样率
#else
    picth_parm.sr = 16000;                             //配置输入audio的采样率
#endif//TCFG_REVERB_SAMPLERATE_DEFUALT

    picth_parm.effect_v = EFFECT_PITCH_SHIFT;          //选移频效果
    picth_parm.formant_shift = PICTH_FORMANT_SHIFT; // 8192;      //3000到16000，或者0【省buf，效果只选EFFECT_PITCH_SHIFT】
}

PITCH_SHIFT_PARM *get_pitch_parm(void)
{
    return &picth_parm;
}

s_pitch_hdl *open_pitch(PITCH_SHIFT_PARM *param)
{
    s_pitch_hdl *p;
    p = zalloc(sizeof(s_pitch_hdl));
    if (!p) {
        printf("open picth zalloc err \n");
        return NULL;
    }
    p->ops = get_pitchshift_func_api();
    init_pitch_shift_parm();
    if (param) {
        p->databuf = zalloc(p->ops->need_buf(p->databuf, param));
        printf("picth sr%d \n", param->sr);
    } else {
        p->databuf = zalloc(p->ops->need_buf(p->databuf, get_pitch_parm()));
    }
    p->ops->open(p->databuf, get_pitch_parm());
    os_mutex_create(&p->mutex);
    printf("\n--func=%s\n", __FUNCTION__);
    return p;
}
void picth_run(s_pitch_hdl *pitch_hdl, s16 *indata, s16 *outdata, int len, u8 ch_num)
{
    if (pitch_hdl) {
        if (ch_num == 1) {
            os_mutex_pend(&pitch_hdl->mutex, 0);
            pitch_hdl->ops->run(pitch_hdl->databuf, indata, outdata, len / 2);
            os_mutex_post(&pitch_hdl->mutex);
        } else {
            int i, j;
            for (i = 0; i < len / 4; i++) { //合成后处理
                j = 2 * i;
                /* pitch_hdl->signal_buf[i] = (indata[j]>>1) + (indata[j+1]>>1);		 */
            }
            for (i = 0; i < len / 4; i++) { //取单声道数据
                /* pitch_hdl->signal_buf[i] = indata[i*2];	 */
                pitch_hdl->signal_buf[i] = indata[i * 2 + 1];
                if (i >= 511) {
                    log_e(" pich len err\n");
                }
            }
            os_mutex_pend(&pitch_hdl->mutex, 0);
            pitch_hdl->ops->run(pitch_hdl->databuf, pitch_hdl->signal_buf, pitch_hdl->signal_buf, len / 4);
            os_mutex_post(&pitch_hdl->mutex);

            for (i = (len / 4 - 1); i >= 0; i--) {
                j = 2 * i;
                /* outdata[j] = pitch_hdl->signal_buf[i]; */
                outdata[j + 1] = pitch_hdl->signal_buf[i];
            }
        }
    }

}
void close_pitch(s_pitch_hdl *pitch_hdl)
{
    if (pitch_hdl) {
        if (pitch_hdl->databuf) {
            free(pitch_hdl->databuf);
        }
        os_mutex_del(&pitch_hdl->mutex, 0);
        free(pitch_hdl);
    }
}
void update_pict_parm(s_pitch_hdl *pitch_hdl)
{
    if (pitch_hdl) {
        if (pitch_hdl->ops) {
            os_mutex_pend(&pitch_hdl->mutex, 0);
            pitch_hdl->ops->init(pitch_hdl->databuf, get_pitch_parm());
            os_mutex_post(&pitch_hdl->mutex);
        }
    }
}
//***************************************//



#endif

