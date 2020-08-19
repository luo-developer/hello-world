#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "classic/tws_api.h"
#include "btctrler/lmp_api.h"
#include "effectrs_sync.h"
#include "audio_eq.h"
#include "audio_drc.h"
#include "app_config.h"
#include "audio_config.h"
#include "aec_user.h"
#include "audio_enc.h"
#include "audio_dec.h"
#include "app_main.h"
#include "encode/encode_write.h"
#include "audio_digital_vol.h"
#include "common/Resample_api.h"
#include "clock_cfg.h"
#include "audio_link.h"
#include "audio_reverb.h"
#include "audio_mix_splicing.h"
#include "audio_dec_board_cfg.h"

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
#include "fm_emitter/fm_emitter_manage.h"
#endif

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif


#define ESCO_SRC_EN			0

#if (defined(DEC_MIX_ENABLE) && DEC_MIX_ENABLE)
#define A2DP_SRC_EN			1
#else
#define A2DP_SRC_EN			0
#endif

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
#define TCFG_ESCO_LIMITER	1
#else
#define TCFG_ESCO_LIMITER	1
#endif

#if TCFG_ESCO_LIMITER
#include "limiter_noiseGate_api.h"
/*限幅器上限*/
#define LIMITER_THR	 -10000 /*-12000 = -12dB,放大1000倍,(-10000参考)*/
/*小于CONST_NOISE_GATE的当成噪声处理,防止清0近端声音*/
#define LIMITER_NOISE_GATE  -30000 /*-12000 = -12dB,放大1000倍,(-30000参考)*/
/*低于噪声门限阈值的增益 */
#define LIMITER_NOISE_GAIN  (0 << 30) /*(0~1)*2^30*/
u8 *limiter_noiseGate_buf = NULL;
#endif/*TCFG_ESCO_LIMITER*/

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
RS_STUCT_API *sw_src_api;
u8 *sw_src_buf;
void *fm_sync = NULL;
#endif

#define FM_AEC_REF_TYPE             0     // 0:SRC 前拿数      1:SRC 后拿数

#define AUDIO_CODEC_SUPPORT_SYNC	1

#define A2DP_RX_AND_AUDIO_DELAY     0

#define A2DP_EQ_SUPPORT_ASYNC		1

#ifndef CONFIG_EQ_SUPPORT_ASYNC
#undef A2DP_EQ_SUPPORT_ASYNC
#define A2DP_EQ_SUPPORT_ASYNC		0
#endif

#if A2DP_EQ_SUPPORT_ASYNC && TCFG_BT_MUSIC_EQ_ENABLE
#define A2DP_EQ_SUPPORT_32BIT		1
#else
#define A2DP_EQ_SUPPORT_32BIT		0
#endif

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC) && (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#undef A2DP_EQ_SUPPORT_32BIT
#define A2DP_EQ_SUPPORT_32BIT		0
#endif

#if (TCFG_USER_TWS_ENABLE == 0)
#define BT_DEC_SYNC_EN				0	// 在解码通道做同步，TWS不能使用
#else
#define BT_DEC_SYNC_EN				0
#endif

#if BT_DEC_SYNC_EN

#undef ESCO_SRC_EN
#define ESCO_SRC_EN					1
#undef A2DP_SRC_EN
#define A2DP_SRC_EN					1
#undef AUDIO_CODEC_SUPPORT_SYNC
#define AUDIO_CODEC_SUPPORT_SYNC	0

#define BT_DEC_RATE_MAX_STEP       100
#define BT_DEC_RATE_INC_STEP       2
#define BT_DEC_RATE_DEC_STEP       2

#endif /*BT_DEC_SYNC_EN*/

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)

extern int audio_sbc_enc_get_rate(void);
extern int audio_sbc_enc_get_channel_num(void);
extern int audio_sbc_enc_check_empty_len(void);
extern int audio_sbc_enc_write(s16 *data, int len);
extern int audio_sbc_enc_is_work(void);
extern int audio_sbc_enc_reset_buf(u8 flag);


#endif /* (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT) */

#define TEST_POINTS  0 //单独测试解码输出1024个点，需要的时间,用于评估解码器大概运行时间,分析仪取时间时，使用10次翻转的平均值计算

#define AUDIO_DEC_SRC_OUT_BUF_LEN			(1024+4*2)
#define AUDIO_DEC_SRC_IN_BUF_LEN(isr,osr)	((isr) * ((AUDIO_DEC_SRC_OUT_BUF_LEN-4*2)/2) / (osr))

struct tone_dec_hdl {
    struct audio_decoder decoder;
    void (*handler)(void *, int argc, int *argv);
    void *priv;
};

struct a2dp_dec_hdl {
    struct audio_decoder decoder;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;
    enum audio_channel ch_type;
    u8 start;
    u8 ch;
    u8 header_len;
#if AUDIO_CODEC_SUPPORT_SYNC
    u8 sync_step;
    u8 preempt_state;
#if TCFG_USER_TWS_ENABLE
    u8 tws_confirm;
    int tws_together_time;
    u32 wait_time;
#endif
#endif
    u16 seqn;
    u16 sample_rate;
    int timer;
    u32 coding_type;
    /* struct audio_src_handle *hw_src; */
    u8 tws_ch_type;
    u8 remain;
    u8 eq_remain;
    u32 quad_pcm_point_offset;
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
    s16 *rr_rl_source;
#endif
    s16 *quad_data;
    struct user_audio_parm *user_hdl;

#if A2DP_SRC_EN
#if BT_DEC_SYNC_EN
    u32 read_time;
    int begin_size;
    int top_size;
    int bottom_size;
    int audio_new_rate;
    int rate_offset;
    s16 adjust_step;
    u8 sync_start;
#endif
    u16 src_out_sr;
    struct audio_src_handle *hw_src;
#endif

#if A2DP_EQ_SUPPORT_32BIT
    s16 *eq_out_buf;
    int eq_out_buf_len;
    int eq_out_points;
    int eq_out_total;
#endif
};


struct esco_dec_hdl {
    struct audio_decoder decoder;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;
    u32 coding_type;
    u8 *frame;
    u8 frame_len;
    u8 offset;
    u8 data_len;
    u8 start;
    u8 enc_start;
#if AUDIO_CODEC_SUPPORT_SYNC
    u8 sync_step;
    u8 preempt_state;
#endif
    u8 esco_len;
    u8 frame_get;
    u8 tws_mute_en;
    u8 remain;
    u16 sample_rate;
    u32 hash;
    int data[15];/*ALIGNED(4)*/
#if ESCO_SRC_EN
#if BT_DEC_SYNC_EN
    u32 read_time;
    int begin_size;
    int top_size;
    int bottom_size;
    int audio_new_rate;
    int rate_offset;
    s16 adjust_step;
    u8 sync_start;
#endif
    u16 src_out_sr;
    struct audio_src_handle *hw_src;
#endif
    u8 *sw_src_buf2k;
    s16 *ext_buf;
    u32 ext_buf_offset;
    u32 ext_buf_remain;

    struct user_audio_parm *user_hdl;
};

#if TCFG_BT_MUSIC_EQ_ENABLE
struct audio_eq *a2dp_eq = NULL;
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
struct audio_eq *a2dp_eq2 = NULL;
#endif
#endif
#endif

#if TCFG_PHONE_EQ_ENABLE
struct audio_eq *esco_eq = NULL;
#endif

#if TCFG_ESCO_PLC
#include "PLC.h"
#define PLC_FRAME_LEN	60
void *esco_plc = NULL;
#endif

#if TCFG_BT_MUSIC_DRC_ENABLE
struct audio_drc *a2dp_drc = NULL;
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
struct audio_drc *a2dp_drc2 = NULL;
#endif
#endif
#endif

#if TCFG_AUDIO_OUT_EQ_ENABLE || TCFG_AUDIO_OUT_DRC_ENABLE
#define AUDIO_OUT_EFFECT_ENABLE			1	// 音频输出时的音效处理
#else
#define AUDIO_OUT_EFFECT_ENABLE			0
#endif

#if AUDIO_OUT_EFFECT_ENABLE

#if ((AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC) && (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR))
#define AUIDO_OUT_EFFECT_FOUR_CH		1	// 4路
#else
#define AUIDO_OUT_EFFECT_FOUR_CH		0
#endif

#if (defined (TCFG_MIXER_EXT_ENABLE) && (TCFG_MIXER_EXT_ENABLE)) && (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1)) && AUIDO_OUT_EFFECT_FOUR_CH
#define AUIDO_OUT_EFFECT_MUL_CH			1	// 多个单独的声道
#undef AUIDO_OUT_EFFECT_FOUR_CH
#define AUIDO_OUT_EFFECT_FOUR_CH		0
#else
#define AUIDO_OUT_EFFECT_MUL_CH			0
#endif

#define AUDIO_OUT_EQ_SUPPORT_ASYNC		1

#if	((!defined(CONFIG_EQ_SUPPORT_ASYNC)) || AUIDO_OUT_EFFECT_MUL_CH)
#undef AUDIO_OUT_EQ_SUPPORT_ASYNC
#define AUDIO_OUT_EQ_SUPPORT_ASYNC		0
#endif

#if TCFG_AUDIO_OUT_EQ_ENABLE
#define AUDIO_OUT_EQ_USE_SPEC_NUM		0//2	// 使用特定的eq段
#else
#define AUDIO_OUT_EQ_USE_SPEC_NUM		0
#endif

struct audio_out_effect_hdl {
#if TCFG_AUDIO_OUT_EQ_ENABLE
    struct audio_eq *eq;
#endif
#if TCFG_AUDIO_OUT_DRC_ENABLE
    struct audio_drc *drc;
#endif
#if AUDIO_OUT_EQ_USE_SPEC_NUM
    int EQ_Coeff_table[AUDIO_OUT_EQ_USE_SPEC_NUM * 5];
#endif
#if AUIDO_OUT_EFFECT_MUL_CH
#if TCFG_AUDIO_OUT_EQ_ENABLE
    struct audio_eq *mul_eq[AUIDO_OUT_EFFECT_MUL_CH];
#endif
#if TCFG_AUDIO_OUT_DRC_ENABLE
    struct audio_drc *mul_drc[AUIDO_OUT_EFFECT_MUL_CH];
#endif
#endif
#if AUIDO_OUT_EFFECT_FOUR_CH
#if TCFG_AUDIO_OUT_EQ_ENABLE
    struct audio_eq *four_ch_eq;
#endif
#if TCFG_AUDIO_OUT_DRC_ENABLE
    struct audio_drc *four_ch_drc;
#endif
    s16 four_ch_buf[2][128 * 4];	// 和mix_buff相同
    s16 four_ch_buf_out[128 * 4 * 2];	// 和mix_buff相同
    u16 four_ch_points;
    u16 four_ch_offset;
    u8  four_ch_eq_work;
#endif
#if AUDIO_OUT_EQ_SUPPORT_ASYNC
    u8 eq_async_remain;
#endif
    u8 remain;
};
struct audio_out_effect_hdl *audio_out_effect = NULL;

#endif /*AUDIO_OUT_EFFECT_ENABLE*/

#if (defined(AUDIO_OUTPUT_AUTOMUTE) && (AUDIO_OUTPUT_AUTOMUTE == ENABLE))
struct audio_automute *automute = NULL;
#endif

void *audio_sync = NULL;

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#define BNUM   2
#else
#define BNUM   1
#endif

s16 mix_buff[128 * 4 * BNUM];
#if (defined (TCFG_MIXER_EXT_ENABLE) && (TCFG_MIXER_EXT_ENABLE))
s16 mix_buff_filt[128 * 4 * BNUM];
#endif//TCFG_MIXER_EXT_ENABLE

#define DEC_RUN_BY_ITSELF               0
#define DEC_PREEMTED_BY_PRIORITY        1
#define DEC_RESUME_BY_OUTSIDE           2
#define DEC_PREEMTED_BY_OUTSIDE         3


#if AUDIO_CODEC_SUPPORT_SYNC
s16 dac_sync_buff[256 * BNUM];
#endif

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#define MAX_SRC_NUMBER      6
#else
#define MAX_SRC_NUMBER      3
#endif
#if (defined (DEC_MIX_ENABLE) && (DEC_MIX_ENABLE))
u8 audio_src_hw_filt[SRC_FILT_POINTS * SRC_CHI * 2 * MAX_SRC_NUMBER * 2] ALIGNED(8);
#else
u8 audio_src_hw_filt[SRC_FILT_POINTS * SRC_CHI * 2 * MAX_SRC_NUMBER] ALIGNED(8);
#endif

static u16 dac_idle_delay_max = 10000;
static u16 dac_idle_delay_cnt = 10000;
static struct tone_dec_hdl *tone_dec = NULL;
struct audio_decoder_task decode_task = {0};
struct audio_mixer mixer;
extern struct dac_platform_data dac_data;

#if (defined(TCFG_DEC2TWS_TASK_ENABLE) && (TCFG_DEC2TWS_TASK_ENABLE))
struct audio_decoder_task decode_task_local = {0};
#endif

struct a2dp_dec_hdl *a2dp_dec = NULL;
struct esco_dec_hdl *esco_dec = NULL;


static u8 audio_a2dp_dec_get_volume_defualt(u8 state)
{
    return app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
}

REGISTER_DEC_BOARD_PARAM(a2dp_dec_param) = {
    .name = "a2dp_dec_param",
    .d_vol_en = 1,
    .d_vol_max = (void *)app_audio_get_max_volume,
    .d_vol_cur = (void *)audio_a2dp_dec_get_volume_defualt,
    .d_vol_fade_step = 2,
    .voice_cut_en  = 0,
};



extern int pcm_decoder_enable();
extern int cvsd_decoder_init();
extern int msbc_decoder_init();
extern int g729_decoder_init();
extern int sbc_decoder_init();
extern int mp3_decoder_init();
extern int wma_decoder_init();
extern int wav_decoder_init();
extern int mty_decoder_init();
extern int flac_decoder_init();
extern int ape_decoder_init();
extern int m4a_decoder_init();
extern int amr_decoder_init();
extern int dts_decoder_init();
extern int mp3pick_decoder_init();
extern int wmapick_decoder_init();
extern int aac_decoder_init();
extern int g726_decoder_init();
extern int midi_decoder_init();

extern int platform_device_sbc_init();

extern int latform_device_sbc_init();

int lmp_private_get_esco_data_len();
void *lmp_private_get_esco_packet(int *len, u32 *hash);
void lmp_private_free_esco_packet(void *packet);
extern int lmp_private_esco_suspend_resume(int flag);

extern int a2dp_media_get_total_buffer_size();
extern int a2dp_media_get_packet(u8 **frame);
extern int a2dp_media_get_remain_buffer_size();
extern int a2dp_media_get_remain_play_time(u8 include_tws);
extern int a2dp_media_get_total_data_len();
extern int a2dp_media_get_packet_num();
extern int a2dp_media_clear_packet_before_seqn(u16 seqn_number);

extern void *a2dp_media_fetch_packet(int *len, void *prev_packet);
extern void a2dp_media_free_packet(u8 *_packet);
extern int a2dp_media_channel_exist(void);

extern u32 timer_get_ms(void);

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
/* extern int audio_dualeq_to_quadeq_output_sync(void *src, struct audio_eq *eq[2], struct audio_drc *drc[2], struct audio_mixer_ch *ch, s16 *data, int len); */
/* extern int audio_dualeq_to_quadeq_output_async(struct audio_eq *eq[2], struct audio_drc *drc[2], struct audio_mixer_ch *ch, s16 *data, int len); */
/* extern int audio_dual_to_quad_output(struct audio_mixer_ch *ch, s16 *data, int len); */
int a2dp_eq_output2(void *priv, s16 *data, u32 len);
int a2dp_eq_output_async(void *priv, s16 *data, u32 len);
/* void dual_to_quad_fun(s16 *_quad_pcm, s16 *_dual_FL_FR, s16 *_dual_RL_RR, int points); */
int a2dp_eq_get_source(void *priv, s16 *buf, len);
#endif

static void bt_audio_mixer_ch_event_handler(void *priv, int event);

void a2dp_eq_drc_open(struct a2dp_dec_hdl *dec, struct audio_fmt *fmt);
void a2dp_eq_drc_close(struct a2dp_dec_hdl *dec);

/* void aec_estimate_dump(u16 DlyEst) */
/* { */
/* printf("DlyEst:%d\n", DlyEst); */
/* } */

void audio_resume_all_decoder(void)
{
    audio_decoder_resume_all(&decode_task);
#if (defined(TCFG_DEC2TWS_TASK_ENABLE) && (TCFG_DEC2TWS_TASK_ENABLE))
    audio_decoder_resume_all(&decode_task_local);
#endif
}

// 激活并执行
void audio_resume_all_decoder_run(int limit_mix_num, int cnt, int time_out)
{
    if (limit_mix_num && (audio_mixer_get_ch_num(&mixer) < limit_mix_num)) {
        return ;
    }
    while (cnt--) {
        audio_decoder_resume_all_by_sem(&decode_task, time_out);
#if (defined(TCFG_DEC2TWS_TASK_ENABLE) && (TCFG_DEC2TWS_TASK_ENABLE))
        audio_decoder_resume_all_by_sem(&decode_task_local, time_out);
#endif
    }
}

void audio_resume_all_mix(void)
{
    audio_resume_all_decoder_run(1, 2, 2);
}

void audio_src_isr_deal(void)
{
    audio_resume_all_decoder();
}

extern void audio_reverb_set_src_by_dac_sync(int in_rate, int out_rate);
extern void audio_linein_set_src_by_dac_sync(int in_rate, int out_rate);
extern void audio_usb_set_src_by_dac_sync(int in_rate, int out_rate);
void audio_dac_sync_src_deal(int in_rate, int out_rate)
{

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
    audio_reverb_set_src_by_dac_sync(in_rate, out_rate);
#endif
#if (defined(TCFG_LINEIN_ENABLE) && (TCFG_LINEIN_ENABLE))
    audio_linein_set_src_by_dac_sync(in_rate, out_rate);
#endif
#if (defined(TCFG_PC_ENABLE) && (TCFG_PC_ENABLE))
    audio_usb_set_src_by_dac_sync(in_rate, out_rate);
#endif

}

#define AUDIO_DECODE_TASK_WAKEUP_TIME	4	//ms
#if AUDIO_DECODE_TASK_WAKEUP_TIME
#include "timer.h"
static void audio_decoder_wakeup_timer(void *priv)
{
    //putchar('k');
    audio_resume_all_decoder();
}
int audio_decoder_task_add_probe(struct audio_decoder_task *task)
{
    if (task->wakeup_timer == 0) {
        task->wakeup_timer = sys_hi_timer_add(NULL, audio_decoder_wakeup_timer, AUDIO_DECODE_TASK_WAKEUP_TIME);
        log_i("audio_decoder_task_add_probe:%d\n", task->wakeup_timer);
    }
    return 0;
}
int audio_decoder_task_del_probe(struct audio_decoder_task *task)
{
    log_i("audio_decoder_task_del_probe\n");
    if (audio_decoder_task_wait_state(task) > 0) {
        /*解码任务列表还有任务*/
        return 0;
    }
    if (task->wakeup_timer) {
        log_i("audio_decoder_task_del_probe:%d\n", task->wakeup_timer);
        sys_hi_timer_del(task->wakeup_timer);
        task->wakeup_timer = 0;
    }
    return 0;
}

int audio_decoder_wakeup_modify(int msecs)
{
    if (decode_task.wakeup_timer) {
        sys_hi_timer_modify(decode_task.wakeup_timer, msecs);
    }

    return 0;
}
#endif/*AUDIO_DECODE_TASK_WAKEUP_TIME*/

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
static u8 audio_dec_bt_emitter_empty_en = 0;
void audio_dec_bt_emitter_check_empty_en(u8 en)
{
    if (bt_user_priv_var.emitter_or_receiver == BT_EMITTER_EN) {
        audio_dec_bt_emitter_empty_en = en;
    } else {
        audio_dec_bt_emitter_empty_en = 0;
    }
}
#endif /* (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT) */

void *a2dp_play_sync_open(struct audio_decoder *dec, u8 channel, u32 sample_rate, u32 output_rate, u32 coding_type);
void *esco_play_sync_open(struct audio_decoder *dec, u8 channel, u32 sample_rate, u32 output_rate);

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
void *local_fm_sync_open(u16 sample_rate, u16 output_rate, u8 channel, u32 total_size);
void *local_fm_sync_output_addr(void *sync);
int local_fm_sync_by_emitter(void *sync, void *data, int len, int emitter_data_len);
void local_fm_sync_close(void *sync);
#endif
//void audio_adc_init(void);
void *audio_adc_open(void *param, const char *source);
int audio_adc_sample_start(void *adc);
void audio_fade_in_fade_out(u8 left_gain, u8 right_gain);
int audio_output_start(u32 sample_rate, u8 reset_rate);

u32 audio_output_rate(int input_rate)
{
#if (defined(DEC_MIX_ENABLE) && DEC_MIX_ENABLE)
    return 44100;
#endif//DEC_MIX_ENABLE

#if ((defined(DEC_OUT_OTHER_DATA) && (DEC_OUT_OTHER_DATA)) || (defined(TCFG_IIS_ENABLE) && (TCFG_IIS_ENABLE)))
    return TCFG_IIS_OUTPUT_SR;
#endif
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC)

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
    return 44100;
#endif
    /*采样率不匹配的时候，选择DAC采样率，第2个参数：0 - 低一级采样率，1 - 高一级采样率*/
    return app_audio_output_samplerate_select(input_rate, 1);
#elif (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    return 41667;
#elif (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
    if (bt_user_priv_var.emitter_or_receiver == BT_EMITTER_EN) {
        return audio_sbc_enc_get_rate();
    } else {
        /*采样率不匹配的时候，选择DAC采样率，第2个参数：0 - 低一级采样率，1 - 高一级采样率*/
        return app_audio_output_samplerate_select(input_rate, 1);
    }
#elif (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_IIS)
    return TCFG_IIS_OUTPUT_SR;
#else
    return 44100;
#endif
}

u32 audio_output_channel_num(void)
{
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC)
    /*根据DAC输出的方式选择输出的声道*/
    u8 dac_connect_mode = app_audio_output_mode_get();
    if (dac_connect_mode == DAC_OUTPUT_LR || dac_connect_mode == DAC_OUTPUT_DUAL_LR_DIFF || dac_connect_mode == DAC_OUTPUT_FRONT_LR_REAR_LR) {
        return 2;
    } else {
        return 1;
    }
#elif (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    return  2;
#elif (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
    if (bt_user_priv_var.emitter_or_receiver == BT_EMITTER_EN) {
        return  audio_sbc_enc_get_channel_num();
    } else {
        u8 dac_connect_mode = app_audio_output_mode_get();
        if (dac_connect_mode == DAC_OUTPUT_LR || dac_connect_mode == DAC_OUTPUT_DUAL_LR_DIFF || dac_connect_mode == DAC_OUTPUT_FRONT_LR_REAR_LR) {
            return 2;
        } else {
            return 1;
        }
    }
#elif (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_IIS)
    return TCFG_IIS_OUTPUT_CH_NUM ? 2 : 1;
#else
    return  2;
#endif
}

int audio_output_set_start_volume(u8 state)
{
    s16 vol_max = get_max_sys_vol();
    if (state == APP_AUDIO_STATE_CALL) {
        vol_max = app_var.aec_dac_gain;
    }
    app_audio_state_switch(state, vol_max);
    return 0;
}

#if AUDIO_CODEC_SUPPORT_SYNC

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
#define FM_SYNC_OUTPUT_SIZE             512
static struct audio_src_base_handle *audio_fm_src = NULL;
static void *fm_sync_buffer = NULL;
static int fm_sync_remain = 0;
static int fm_sync_output_len = 0;
static int fm_emitter_sync_write(struct audio_src_base_handle *src, void *data, int len)
{
    int wlen = 0;
    int remain_len = len;
    int offset = 0;
    u16 insr = 1;
    u16 outsr = 1;

    if (esco_dec) {
        /*通话的时候后级不需要跟手机做同步，只需保证fm tx_buf不为空即可*/
        u16 esco_data_sr = 16000;
        if (esco_dec->coding_type == AUDIO_CODING_CVSD) {
            esco_data_sr = 8000;
        }
        insr = (int)(esco_data_sr * 1.566f); //25056(16k) or 12528(8k)
        outsr = 65250;// 1.566*41666.66..
        audio_src_base_set_rate(src, insr, outsr);
    } else {
        audio_wireless_sync_get_rate(audio_sync, &insr, &outsr);
        audio_src_base_set_rate(src, insr, outsr);
    }
    if (fm_sync_remain) {
        wlen = fm_emitter_cbuf_write((u8 *)fm_sync_buffer + (fm_sync_output_len - fm_sync_remain), fm_sync_remain);
        fm_sync_remain -= wlen;
        if (fm_sync_remain) { //上次都没输出完，直接返回0
            return 0;
        }
    }
    return audio_src_base_write(src, (u8 *)data + offset, remain_len);
    /* while (remain_len) { */
    /* wlen = audio_src_base_write(src, (u8 *)data + offset, remain_len); */

    /* remain_len -= wlen; */
    /* offset += wlen; */
    /* } */
    /* audio_src_base_data_flush_out(src); */
    /* return len  */
}

static int fm_emitter_sync_event_handler(void *priv, enum audio_src_event event, void *arg)
{
    struct audio_src_buffer *b = (struct audio_src_buffer *)arg;
    int wlen = 0;

    switch (event) {
    case SRC_EVENT_GET_OUTPUT_BUF:
        if (fm_sync_remain) {
            b->addr = 0;
            b->len = 0;
        } else {
            b->addr = fm_sync_buffer;
            b->len = FM_SYNC_OUTPUT_SIZE;//sizeof(fm_sync_buffer);
        }
        /*     if (!b->addr) { */
        /* log_e("no fm sync buffer to output\n"); */
        /* } */
        break;
    case SRC_EVENT_OUTPUT_DONE:
    case SRC_EVENT_INPUT_DONE:
    case SRC_EVENT_ALL_DONE:
        fm_sync_output_len = b->len;
        wlen = fm_emitter_cbuf_write((u8 *)b->addr, b->len);
        fm_sync_remain = b->len - wlen;
        if (wlen) {
#if FM_AEC_REF_TYPE == 1
            /*DownSampling*/
            if (sw_src_api) {
                u16 points_per_ch = (b->len >> 2);
                s16 *data = (s16 *)b->addr;
                for (int i = 0; i < points_per_ch; i++) {
                    data[i] = data[i * 2];
                }
                int rdlen = sw_src_api->run(sw_src_buf, data, points_per_ch, data);
                audio_aec_refbuf(data, rdlen << 1);
            }
#endif
        }
        break;
    default:
        break;
    }

    return 0;
}

static struct audio_src_base_handle *fm_emitter_sync_src_open(u8 channel, u16 insr, u16 outsr)
{
    struct audio_src_base_handle *src = zalloc(sizeof(struct audio_src_base_handle));

    if (!src) {
        return NULL;
    }
    audio_src_base_open(src, channel, SRC_TYPE_AUDIO_SYNC);

    audio_src_base_set_rate(src, insr, outsr);

    audio_src_base_set_event_handler(src, NULL, fm_emitter_sync_event_handler);

    return src;
}

static void fm_emitter_sync_src_close(struct audio_src_base_handle *src)
{
    if (src) {
        audio_src_base_stop(src);
        audio_src_base_close(src);
        free(src);
    }
}
#endif

static void *audio_sync_a2dp_open(struct a2dp_dec_hdl *dec, int sample_rate, int out_sample_rate, u32 coding_type)
{
    u8 channel = audio_output_channel_num();

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC)
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
    channel = 4;
#endif
#endif
    if (!audio_sync) {
        /*音频同步声道配置为最后一级输出的声道数*/
        audio_sync = a2dp_play_sync_open(&dec->decoder, channel, sample_rate, out_sample_rate, coding_type);
    }

    if (audio_sync) {
        audio_mixer_ch_set_event_handler(&dec->mix_ch, dec, bt_audio_mixer_ch_event_handler);
        audio_wireless_sync_info_init(audio_sync, sample_rate, out_sample_rate, channel);
    }

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    if (!audio_fm_src) {
        audio_fm_src = fm_emitter_sync_src_open(channel, sample_rate, out_sample_rate);
    }

    if (!fm_sync_buffer) {
        fm_sync_buffer = zalloc(FM_SYNC_OUTPUT_SIZE);
    }
#endif

    clock_add(SYNC_CLK);
    return audio_sync;
}

static void *audio_sync_esco_open(struct esco_dec_hdl *dec, int sample_rate, int out_sample_rate)
{
    u8 channel = audio_output_channel_num();
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC)
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
    channel = 4;
#endif
#endif
    if (!audio_sync) {
        audio_sync = esco_play_sync_open(&dec->decoder, channel, sample_rate, out_sample_rate);
    }

    if (audio_sync) {
        audio_mixer_ch_set_event_handler(&dec->mix_ch, dec, bt_audio_mixer_ch_event_handler);
        audio_wireless_sync_info_init(audio_sync, sample_rate, out_sample_rate, channel);
    }

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    if (!audio_fm_src) {
        audio_fm_src = fm_emitter_sync_src_open(channel, sample_rate, out_sample_rate);
    }

    if (!fm_sync_buffer) {
        fm_sync_buffer = zalloc(FM_SYNC_OUTPUT_SIZE);
    }
#endif


    clock_add(SYNC_CLK);
    return audio_sync;
}

static void audio_sync_close(void)
{
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    if (audio_fm_src) {
        fm_emitter_sync_src_close(audio_fm_src);
        audio_fm_src = NULL;
    }

    if (fm_sync_buffer) {
        free(fm_sync_buffer);
        fm_sync_buffer = NULL;
    }
#endif

    if (audio_sync) {
        audio_output_sync_stop();
        audio_wireless_sync_close(audio_sync);
        audio_sync = NULL;
    }

    clock_remove(SYNC_CLK);
}

#endif

u32 audio_output_data(s16 *data, u16 len)
{
    int wlen = len;
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    if (audio_sync) {
#if AUDIO_CODEC_SUPPORT_SYNC
        wlen = fm_emitter_sync_write(audio_fm_src, data, len);
#endif
    } else {
        wlen = fm_emitter_cbuf_write((u8 *)data, len);
    }

#elif (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)

    /* wlen = bt_emitter_output_write((u8 *)data, len); */
    int sbc_len = len;
    if (audio_dec_bt_emitter_empty_en && (audio_sbc_enc_is_work() == false)) {
        sbc_len = audio_sbc_enc_check_empty_len();
    }
    if (!sbc_len) {
        return 0;
    }
    if (len > sbc_len) {
        len = sbc_len;
    }
    u8 ch_out = audio_sbc_enc_get_channel_num();
    u8 ch_dac = 1;
    u8 dac_connect_mode = app_audio_output_mode_get();
    if (dac_connect_mode == DAC_OUTPUT_LR || dac_connect_mode == DAC_OUTPUT_DUAL_LR_DIFF || dac_connect_mode == DAC_OUTPUT_FRONT_LR_REAR_LR) {
        ch_dac = 2;
    }
    if ((ch_out == 2) && (ch_dac == 1)) {
        wlen = app_audio_output_write(data, len >> 1);
        wlen <<= 1;
    } else if ((ch_out == 1) && (ch_dac == 2)) {
        wlen = app_audio_output_write(data, len << 1);
        wlen >>= 1;
    } else {
        wlen = app_audio_output_write(data, len);
    }
    if (wlen) {
        int olen = audio_sbc_enc_write((u8 *)data, wlen);
        if ((olen != 0) && (wlen != olen)) {
            putchar('&');
        }
    }

#else

    wlen = app_audio_output_write(data, len);
    if (wlen != len) {
        //putchar('&');
    }

#endif

#if (defined(AUDIO_OUTPUT_AUTOMUTE) && (AUDIO_OUTPUT_AUTOMUTE == ENABLE))
    if (automute && wlen) {
        audio_automute_run(automute, data, wlen);
    }
#endif

    return wlen;
}

int audio_output_start(u32 sample_rate, u8 reset_rate)
{
    if (reset_rate) {
        app_audio_output_samplerate_set(sample_rate);
    }
    app_audio_output_start();

#if (defined(AUDIO_OUTPUT_AUTOMUTE) && (AUDIO_OUTPUT_AUTOMUTE == ENABLE))
    audio_automute_onoff(automute, 1, 0);
#endif

    return 0;
}

void audio_output_stop(void)
{
#if (defined(AUDIO_OUTPUT_AUTOMUTE) && (AUDIO_OUTPUT_AUTOMUTE == ENABLE))
    audio_automute_onoff(automute, 0, 1);
#endif
    app_audio_output_stop();

}

struct audio_src_handle *audio_hw_resample_open(void *priv, int (*output_handler)(void *, void *, int),
        u8 channel, u16 input_sample_rate, u16 output_sample_rate)
{
    struct audio_src_handle *hdl;
    hdl = zalloc(sizeof(struct audio_src_handle));
    if (hdl) {
        audio_hw_src_open(hdl, channel, SRC_TYPE_RESAMPLE);
        audio_hw_src_set_rate(hdl, input_sample_rate, output_sample_rate);
        audio_src_set_output_handler(hdl, priv, output_handler);
    }

    return hdl;
}

void audio_hw_resample_close(struct audio_src_handle *hdl)
{
    if (hdl) {
        audio_hw_src_stop(hdl);
        audio_hw_src_close(hdl);
        free(hdl);
    }
}

static u8 bt_dec_idle_query()
{
    if (a2dp_dec || esco_dec) {
        return 0;
    }

    return 1;
}
REGISTER_LP_TARGET(bt_dec_lp_target) = {
    .name = "bt_dec",
    .is_idle = bt_dec_idle_query,
};

#if A2DP_EQ_SUPPORT_32BIT
void a2dp_eq_32bit_out(struct a2dp_dec_hdl *dec)
{
    int wlen = 0;
#if A2DP_SRC_EN
    if (dec->hw_src) {
        wlen = audio_src_resample_write(dec->hw_src, &dec->eq_out_buf[dec->eq_out_points], (dec->eq_out_total - dec->eq_out_points) * 2);
    } else
#endif
    {
        wlen = audio_mixer_ch_write(&dec->mix_ch, &dec->eq_out_buf[dec->eq_out_points], (dec->eq_out_total - dec->eq_out_points) * 2);
    }
    dec->eq_out_points += wlen / 2;
}


int a2dp_eq_32bit_to_16bit(struct a2dp_dec_hdl *dec, s32 *data, u32 len)
{

    if ((!dec->eq_out_buf) || (dec->eq_out_buf_len < len / 2)) {
        if (dec->eq_out_buf) {
            free(dec->eq_out_buf);
        }
        dec->eq_out_buf_len = len / 2;
        dec->eq_out_buf = malloc(dec->eq_out_buf_len);
        ASSERT(dec->eq_out_buf);
    }
    s32 *idat = data;
    s16 *odat = dec->eq_out_buf;
    for (int i = 0; i < len / 4; i++) {
        s32 outdat = *idat++;
        if (outdat > 32767) {
            outdat = 32767;
        } else if (outdat < -32768) {
            outdat = -32768;
        }
        *odat++ = outdat;
    }
    dec->eq_out_points = 0;
    dec->eq_out_total = len / 4;

    a2dp_eq_32bit_out(dec);
    return len;
}

#endif /*A2DP_EQ_SUPPORT_32BIT*/

static int a2dp_eq_output(void *priv, s16 *data, u32 len)
{
    int wlen = 0;
    int rlen = len;
    struct a2dp_dec_hdl *dec = priv;

#if A2DP_EQ_SUPPORT_ASYNC
    if (!dec->eq_remain) {

#if A2DP_EQ_SUPPORT_32BIT
        if (dec->eq_out_buf && (dec->eq_out_points < dec->eq_out_total)) {
            a2dp_eq_32bit_out(dec);
            if (dec->eq_out_points < dec->eq_out_total) {
                return 0;
            }
        }
#endif /*A2DP_EQ_SUPPORT_32BIT*/

#if TCFG_BT_MUSIC_DRC_ENABLE

        if (a2dp_drc) {
            audio_drc_run(a2dp_drc, data, len);
        }
#endif

#if A2DP_EQ_SUPPORT_32BIT
        return a2dp_eq_32bit_to_16bit(dec, data, len);
#endif /*A2DP_EQ_SUPPORT_32BIT*/
    }

    /* printf("dec:0x%x, a2dp:0x%x ", dec, a2dp_dec); */
    /* printf("data:0x%x, len:%d ", data, len); */
#if A2DP_SRC_EN
    if (dec->hw_src) {
        wlen = audio_src_resample_write(dec->hw_src, data, len);
    } else
#endif
    {
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

#if AUDIO_OUT_EFFECT_ENABLE

static int mix_output_handler(struct audio_mixer *mixer, s16 *data, u16 len);
static int mix_output_ext_handler(struct audio_mixer *mixer, s16 *data, u16 len, u8 idx);

#if AUDIO_OUT_EQ_USE_SPEC_NUM

static struct eq_seg_info audio_out_eq_tab[AUDIO_OUT_EQ_USE_SPEC_NUM] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 125,   0 << 20, (int)(0.7f * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},
};

static int audio_out_eq_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    if (!audio_out_effect) {
        return -1;
    }
    local_irq_disable();
    for (int i = 0; i < AUDIO_OUT_EQ_USE_SPEC_NUM; i++) {
        eq_seg_design(&audio_out_eq_tab[i], sr, &audio_out_effect->EQ_Coeff_table[5 * i]);
    }
    local_irq_enable();

    info->L_coeff = info->R_coeff = (void *)audio_out_effect->EQ_Coeff_table;
    info->L_gain = info->R_gain = 0;
    info->nsection = AUDIO_OUT_EQ_USE_SPEC_NUM;
    return 0;
}
int audio_out_eq_spec_set_info(u8 idx, int freq, int gain)
{
    if (idx >= AUDIO_OUT_EQ_USE_SPEC_NUM) {
        return false;
    }
    if (gain > 12) {
        gain = 12;
    }
    if (gain < -12) {
        gain = -12;
    }
    if (freq) {
        audio_out_eq_tab[idx].freq = freq;
    }
    audio_out_eq_tab[idx].gain = gain << 20;

    printf("audio out eq, idx:%d, freq:%d,%d, gain:%d,%d \n", idx, freq, audio_out_eq_tab[idx].freq, gain, audio_out_eq_tab[idx].gain);

    local_irq_disable();
    if (audio_out_effect && audio_out_effect->eq) {
        audio_out_effect->eq->updata = 1;
    }
    local_irq_enable();

    return true;
}
#endif /*AUDIO_OUT_EQ_USE_SPEC_NUM*/

#if AUIDO_OUT_EFFECT_FOUR_CH

// 拆成两个双路的数据放入four_ch_buf中
static int audio_out_four_effect_data_before(s16 *data, u32 len)
{
    if (len > sizeof(audio_out_effect->four_ch_buf)) {
        len = sizeof(audio_out_effect->four_ch_buf);
    }
    s16 *buf_0 = audio_out_effect->four_ch_buf[0];
    s16 *buf_1 = audio_out_effect->four_ch_buf[1];
    for (int i = 0; i < len / 2; i += 4) {
        *buf_0++ = *data++;
        *buf_0++ = *data++;
        *buf_1++ = *data++;
        *buf_1++ = *data++;
    }
    return len / 2;
}
// 合并两个双路的数据
static int audio_out_four_effect_data_after(s16 *data_0, s16 *data_1, u32 len)
{
    s16 *buf_out = audio_out_effect->four_ch_buf_out;
    s16 *buf_0 = data_0;
    s16 *buf_1 = data_1;
    for (int i = 0; i < len / 2; i += 2) {
        *buf_out++ = *buf_0++;
        *buf_out++ = *buf_0++;
        *buf_out++ = *buf_1++;
        *buf_out++ = *buf_1++;
    }
    return len * 2;
}
// 挤出剩余数据
static int audio_out_four_effect_out(void *priv)
{
#if TCFG_AUDIO_OUT_EQ_ENABLE && AUDIO_OUT_EQ_SUPPORT_ASYNC
    if (audio_out_effect->four_ch_eq_work) {
        audio_eq_flush_out();
        if (audio_out_effect->four_ch_eq_work) {
            return false;
        }
    }
#endif
    if (audio_out_effect->four_ch_points) {
        /* printf("points:%d, off:%d \n", audio_out_effect->four_ch_points, audio_out_effect->four_ch_offset); */
        int wlen = mix_output_handler(priv, &audio_out_effect->four_ch_buf_out[audio_out_effect->four_ch_offset], audio_out_effect->four_ch_points * 2);
        audio_out_effect->four_ch_offset += wlen / 2;
        audio_out_effect->four_ch_points -= wlen / 2;
        if (audio_out_effect->four_ch_points) {
            return false;
        }
    }
    return true;
}

#if AUIDO_OUT_EFFECT_FOUR_CH
static int audio_out_eq_output_four(void *priv, s16 *data, u32 len)
{
#if AUDIO_OUT_EQ_SUPPORT_ASYNC
#if TCFG_AUDIO_OUT_DRC_ENABLE
    if (audio_out_effect->four_ch_drc) {
        audio_drc_run(audio_out_effect->four_ch_drc, data, len);
    }
#endif
    audio_out_four_effect_data_after(audio_out_effect->four_ch_buf[0], data, len);
    audio_out_effect->four_ch_eq_work = 0;
    audio_out_effect->four_ch_offset = 0;
    audio_out_effect->four_ch_points = len;
    audio_out_four_effect_out(priv);
#endif
    return len;
}
#endif
#endif /*AUIDO_OUT_EFFECT_FOUR_CH*/

#if TCFG_AUDIO_OUT_EQ_ENABLE
static int audio_out_eq_output(void *priv, s16 *data, u32 len)
{
#if AUDIO_OUT_EQ_SUPPORT_ASYNC
    if (!audio_out_effect->eq_async_remain) {
#if TCFG_AUDIO_OUT_DRC_ENABLE
        if (audio_out_effect->drc) {
            audio_drc_run(audio_out_effect->drc, data, len);
        }
#endif
#if AUIDO_OUT_EFFECT_FOUR_CH
        if (audio_out_effect->four_ch_eq) {
            memcpy(&audio_out_effect->four_ch_buf[0], data, len);
            int wlen = audio_eq_run(audio_out_effect->four_ch_eq, audio_out_effect->four_ch_buf[1], len);
            if (wlen != len) {
                r_printf("\n\n eq err wl:%d, l:%d \n\n", wlen, len);
            }
            return len;
        }
#if TCFG_AUDIO_OUT_DRC_ENABLE
        if (audio_out_effect->four_ch_drc) {
            audio_drc_run(audio_out_effect->four_ch_drc, audio_out_effect->four_ch_buf[1], len);
        }
#endif
        audio_out_four_effect_data_after(audio_out_effect->four_ch_buf[0], audio_out_effect->four_ch_buf[1], len);
        audio_out_effect->four_ch_eq_work = 0;
        audio_out_effect->four_ch_offset = 0;
        audio_out_effect->four_ch_points = len;
        audio_out_four_effect_out(priv);
        return len;
#endif /*AUIDO_OUT_EFFECT_FOUR_CH*/
    }
    int wlen = mix_output_handler(priv, data, len);
    if (wlen == len) {
        audio_out_effect->eq_async_remain = 0;
    } else {
        audio_out_effect->eq_async_remain = 1;
    }
    return wlen;
#endif
    return len;
}
#endif

static void audio_out_effect_close(void)
{
    if (!audio_out_effect) {
        return ;
    }
#if TCFG_AUDIO_OUT_EQ_ENABLE
    if (audio_out_effect->eq) {
        audio_eq_close(audio_out_effect->eq);
        local_irq_disable();
        free(audio_out_effect->eq);
        audio_out_effect->eq = NULL;
        local_irq_enable();

#if AUIDO_OUT_EFFECT_FOUR_CH
        if (audio_out_effect->four_ch_eq) {
            audio_eq_close(audio_out_effect->four_ch_eq);
            local_irq_disable();
            free(audio_out_effect->four_ch_eq);
            audio_out_effect->four_ch_eq = NULL;
            local_irq_enable();
        }
#endif /*AUIDO_OUT_EFFECT_FOUR_CH*/

#if AUIDO_OUT_EFFECT_MUL_CH
        for (int idx = 0; idx < AUIDO_OUT_EFFECT_MUL_CH; idx++) {
            if (audio_out_effect->mul_eq[idx]) {
                audio_eq_close(audio_out_effect->mul_eq[idx]);
                local_irq_disable();
                free(audio_out_effect->mul_eq[idx]);
                audio_out_effect->mul_eq[idx] = NULL;
                local_irq_enable();
            }
        }
#endif /*AUIDO_OUT_EFFECT_MUL_CH*/
    }
#endif
#if TCFG_AUDIO_OUT_DRC_ENABLE
    if (audio_out_effect->drc) {
        audio_drc_close(audio_out_effect->drc);
        local_irq_disable();
        free(audio_out_effect->drc);
        audio_out_effect->drc = NULL;
        local_irq_enable();
#if AUIDO_OUT_EFFECT_FOUR_CH
        if (audio_out_effect->four_ch_drc) {
            audio_drc_close(audio_out_effect->four_ch_drc);
            local_irq_disable();
            free(audio_out_effect->four_ch_drc);
            audio_out_effect->four_ch_drc = NULL;
            local_irq_enable();
        }
#endif /*AUIDO_OUT_EFFECT_FOUR_CH*/
#if AUIDO_OUT_EFFECT_FOUR_CH
        for (int idx = 0; idx < AUIDO_OUT_EFFECT_MUL_CH; idx++) {
            if (audio_out_effect->mul_drc[idx]) {
                audio_drc_close(audio_out_effect->mul_drc[idx]);
                local_irq_disable();
                free(audio_out_effect->mul_drc[idx]);
                audio_out_effect->mul_drc[idx] = NULL;
                local_irq_enable();
            }
        }
#endif /*AUIDO_OUT_EFFECT_FOUR_CH*/
    }
#endif

    local_irq_disable();
    free(audio_out_effect);
    audio_out_effect = NULL;
    local_irq_enable();
}

static int audio_out_effect_open(void *priv, u16 sample_rate, u8 ch_num)
{
    audio_out_effect_close();

    struct audio_out_effect_hdl *hdl = zalloc(sizeof(struct audio_out_effect_hdl));
    if (!hdl) {
        return false;
    }
#if TCFG_AUDIO_OUT_EQ_ENABLE
    hdl->eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (hdl->eq) {
        hdl->eq->eq_ch = (struct hw_eq_ch *)((int)hdl->eq + sizeof(struct audio_eq));
        struct audio_eq_param eq_param = {0};
        eq_param.channels = ch_num;
#if AUDIO_OUT_EQ_USE_SPEC_NUM
        eq_param.max_nsection = AUDIO_OUT_EQ_USE_SPEC_NUM;
        eq_param.cb = audio_out_eq_get_filter_info;
#else
        eq_param.online_en = 1;
        eq_param.mode_en = 1;
        eq_param.remain_en = 1;
        eq_param.max_nsection = EQ_SECTION_MAX;
        eq_param.cb = eq_get_filter_info;
#endif
#if AUDIO_OUT_EQ_SUPPORT_ASYNC
        eq_param.no_wait = 1;//异步
#endif
        eq_param.eq_name = 0;
        audio_eq_open(hdl->eq, &eq_param);
        audio_eq_set_samplerate(hdl->eq, sample_rate);
        audio_eq_set_output_handle(hdl->eq, audio_out_eq_output, priv);
        audio_eq_start(hdl->eq);

#if AUIDO_OUT_EFFECT_FOUR_CH
        hdl->four_ch_eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
        if (hdl->four_ch_eq) {
            hdl->four_ch_eq->eq_ch = (struct hw_eq_ch *)((int)hdl->four_ch_eq + sizeof(struct audio_eq));
#if AUDIO_OUT_EQ_USE_SPEC_NUM
            // null
#else
#if TCFG_EQ_DIVIDE_ENABLE
            eq_param.cb = eq_get_filter_info2;
#endif
#endif
            audio_eq_open(hdl->four_ch_eq, &eq_param);
            audio_eq_set_samplerate(hdl->four_ch_eq, sample_rate);
            audio_eq_set_output_handle(hdl->four_ch_eq, audio_out_eq_output_four, priv);
            audio_eq_start(hdl->four_ch_eq);
        }
#endif /*AUIDO_OUT_EFFECT_FOUR_CH*/

#if AUIDO_OUT_EFFECT_MUL_CH
        for (int idx = 0; idx < AUIDO_OUT_EFFECT_MUL_CH; idx++) {
            hdl->mul_eq[idx] = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
            if (hdl->mul_eq[idx]) {
                hdl->mul_eq[idx]->eq_ch = (struct hw_eq_ch *)((int)hdl->mul_eq[idx] + sizeof(struct audio_eq));
#if AUDIO_OUT_EQ_USE_SPEC_NUM
                // null
#else
#if TCFG_EQ_DIVIDE_ENABLE
                eq_param.cb = eq_get_filter_info2;
#endif
#endif
                audio_eq_open(hdl->mul_eq[idx], &eq_param);
                audio_eq_set_samplerate(hdl->mul_eq[idx], sample_rate);
                audio_eq_set_output_handle(hdl->mul_eq[idx], NULL, priv);
                audio_eq_start(hdl->mul_eq[idx]);
            }
        }
#endif /*AUIDO_OUT_EFFECT_MUL_CH*/

    }
#endif

#if TCFG_AUDIO_OUT_DRC_ENABLE
    hdl->drc = malloc(sizeof(struct audio_drc));
    if (hdl->drc) {
        struct audio_drc_param drc_param = {0};
        drc_param.channels = ch_num;
        drc_param.online_en = 1;
        drc_param.remain_en = 1;
        drc_param.cb = drc_get_filter_info;
        drc_param.stero_div = 0;
        drc_param.drc_name = 0;
        audio_drc_open(hdl->drc, &drc_param);
        audio_drc_set_samplerate(hdl->drc, sample_rate);
        audio_drc_set_output_handle(hdl->drc, NULL, NULL);
        audio_drc_start(hdl->drc);

#if AUIDO_OUT_EFFECT_FOUR_CH
        hdl->four_ch_drc = malloc(sizeof(struct audio_drc));
        if (hdl->four_ch_drc) {
#if TCFG_EQ_DIVIDE_ENABLE
            drc_param.cb = drc_get_filter_info2;
#endif
            audio_drc_open(hdl->four_ch_drc, &drc_param);
            audio_drc_set_samplerate(hdl->four_ch_drc, sample_rate);
            audio_drc_set_output_handle(hdl->four_ch_drc, NULL, NULL);
            audio_drc_start(hdl->four_ch_drc);
        }
#endif /*AUIDO_OUT_EFFECT_FOUR_CH*/

#if AUIDO_OUT_EFFECT_MUL_CH
        for (int idx = 0; idx < AUIDO_OUT_EFFECT_MUL_CH; idx++) {
            hdl->mul_drc[idx] = malloc(sizeof(struct audio_drc));
            if (hdl->mul_drc[idx]) {
#if TCFG_EQ_DIVIDE_ENABLE
                drc_param.cb = drc_get_filter_info2;
#endif
                audio_drc_open(hdl->mul_drc[idx], &drc_param);
                audio_drc_set_samplerate(hdl->mul_drc[idx], sample_rate);
                audio_drc_set_output_handle(hdl->mul_drc[idx], NULL, NULL);
                audio_drc_start(hdl->mul_drc[idx]);
            }
        }
#endif /*AUIDO_OUT_EFFECT_MUL_CH*/

    }
#endif
    audio_out_effect = hdl;
    return true;
}

static int mix_output_effect_handler(struct audio_mixer *mixer, s16 *data, u16 len)
{
    if (!audio_out_effect) {
        return mix_output_handler(mixer, data, len);
    }
#if AUIDO_OUT_EFFECT_FOUR_CH
    if (audio_out_four_effect_out(mixer) == false) {
        return 0;
    }
    int tlen = audio_out_four_effect_data_before(data, len);
    /* printf("l:%d, tl:%d \n", len, tlen); */
#endif /*AUIDO_OUT_EFFECT_FOUR_CH*/

#if AUDIO_OUT_EQ_SUPPORT_ASYNC
    if (audio_out_effect->eq) {
#if AUIDO_OUT_EFFECT_FOUR_CH
        audio_out_effect->four_ch_eq_work = 1;
        audio_eq_run(audio_out_effect->eq, audio_out_effect->four_ch_buf[0], tlen);
        return tlen * 2;
#else
        return audio_eq_run(audio_out_effect->eq, data, len);
#endif
    }
#endif /*AUDIO_OUT_EQ_SUPPORT_ASYNC*/

#if AUIDO_OUT_EFFECT_FOUR_CH
#if TCFG_AUDIO_OUT_EQ_ENABLE
    if (audio_out_effect->eq) {
        audio_eq_run(audio_out_effect->eq, audio_out_effect->four_ch_buf[0], tlen);
        if (audio_out_effect->four_ch_eq) {
            audio_eq_run(audio_out_effect->four_ch_eq, audio_out_effect->four_ch_buf[1], tlen);
        }
    }
#endif
#if TCFG_AUDIO_OUT_DRC_ENABLE
    if (audio_out_effect->drc) {
        audio_drc_run(audio_out_effect->drc, audio_out_effect->four_ch_buf[0], tlen);
        if (audio_out_effect->four_ch_drc) {
            audio_drc_run(audio_out_effect->four_ch_drc, audio_out_effect->four_ch_buf[1], tlen);
        }
    }
#endif
    audio_out_four_effect_data_after(audio_out_effect->four_ch_buf[0], audio_out_effect->four_ch_buf[1], tlen);
    audio_out_effect->four_ch_eq_work = 0;
    audio_out_effect->four_ch_offset = 0;
    audio_out_effect->four_ch_points = tlen;
    audio_out_four_effect_out(mixer);
    return tlen * 2;

#else /*AUIDO_OUT_EFFECT_FOUR_CH*/

    if (!audio_out_effect->remain) {
#if TCFG_AUDIO_OUT_EQ_ENABLE
        if (audio_out_effect->eq) {
            audio_eq_run(audio_out_effect->eq, data, len);
        }
#endif
#if TCFG_AUDIO_OUT_DRC_ENABLE
        if (audio_out_effect->drc) {
            audio_drc_run(audio_out_effect->drc, data, len);
        }
#endif
    }
    int wlen = mix_output_handler(mixer, data, len);
    if (wlen == len) {
        audio_out_effect->remain = 0;
    } else {
        audio_out_effect->remain = 1;
    }
    return wlen;
#endif /*AUIDO_OUT_EFFECT_FOUR_CH*/
}

static int mix_output_effect_ext_handler(struct audio_mixer *mixer, s16 *data, u16 len, u8 idx)
{
#if AUIDO_OUT_EFFECT_MUL_CH
#if TCFG_AUDIO_OUT_EQ_ENABLE
    if (idx < AUIDO_OUT_EFFECT_MUL_CH) {
        if (audio_out_effect->mul_eq[idx]) {
            audio_eq_run(audio_out_effect->mul_eq[idx], data, len);
        }
    }
#endif
#if TCFG_AUDIO_OUT_DRC_ENABLE
    if (idx < AUIDO_OUT_EFFECT_MUL_CH) {
        if (audio_out_effect->mul_drc[idx]) {
            audio_drc_run(audio_out_effect->mul_drc[idx], data, len);
        }
    }
#endif
#endif

    return mix_output_ext_handler(mixer, data, len, idx);
}

#endif /*AUDIO_OUT_EFFECT_ENABLE*/

static void mixer_event_handler(struct audio_mixer *mixer, int event)
{
    switch (event) {
    case MIXER_EVENT_CH_OPEN:
        //r_printf("----MIXER_EVENT_CH_OPEN\n");
        if ((audio_mixer_get_ch_num(mixer) == 1) || (!app_audio_output_state_get())) {
            audio_output_start(audio_mixer_get_sample_rate(mixer), 1);
#if AUDIO_OUT_EFFECT_ENABLE
            u8 ch_num = audio_output_channel_num();
            audio_out_effect_open(mixer, audio_mixer_get_sample_rate(mixer), ch_num);
#endif
        } else {
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
            /* reset_reverb_src_out(audio_mixer_get_sample_rate(mixer)); */
#endif
        }
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
        if (bt_user_priv_var.emitter_or_receiver == BT_EMITTER_EN) {
            audio_sbc_enc_reset_buf(1);
        }
#endif
        break;
    case MIXER_EVENT_CH_CLOSE:
        //r_printf("----MIXER_EVENT_CH_CLOSE\n");
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
        if (bt_user_priv_var.emitter_or_receiver == BT_EMITTER_EN) {
            audio_sbc_enc_reset_buf(0);
        }
#endif
        if (audio_mixer_get_ch_num(mixer) == 0) {
#if AUDIO_OUT_EFFECT_ENABLE
            audio_out_effect_close();
#endif
            audio_output_stop();
        } else {
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
            reset_reverb_src_out(audio_output_rate(audio_mixer_get_sample_rate(mixer)));
#endif
            // 还有其他解码，让出点时间给解码执行
            audio_resume_all_decoder_run(0, 2, 2);
        }
        break;
    }

}

static int mix_probe_handler(struct audio_mixer *mixer)
{
    return 0;
}

static int mix_output_handler(struct audio_mixer *mixer, s16 *data, u16 len)
{
#if ((TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR) && (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1)))
#if (defined (TCFG_MIXER_EXT_ENABLE) && (TCFG_MIXER_EXT_ENABLE))
    return mix_output_dual_to_quad(mixer, data, len);
#endif//TCFG_MIXER_EXT_ENABLE
#endif

    int ret = audio_output_data(data, len);
    /*audio output完整输出，表示后级buf有足够空间，可以继续解码*/
    if (ret == len) {
        audio_resume_all_decoder();
    }

#if (defined(TCFG_MIXERCH_REC_EN) && (TCFG_MIXERCH_REC_EN))
    record_player_pcm2file_write_pcm_ex(data, ret);
#endif

    return ret;
}


static int mix_output_ext_handler(struct audio_mixer *mixer, s16 *data, u16 len, u8 idx)
{
#if ((TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR) && (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1)))
#if (defined (TCFG_MIXER_EXT_ENABLE) && (TCFG_MIXER_EXT_ENABLE))
    mix_output_ext_dual_to_quad_handler(mixer, data, len, idx);
#endif//TCFG_MIXER_EXT_ENABLE
#endif
    ///不关注返回值
    return 0;
}

static const struct audio_mix_handler mix_handler  = {
    .mix_probe  = mix_probe_handler,
#if AUDIO_OUT_EFFECT_ENABLE
    .mix_output = mix_output_effect_handler,
    .mix_output_ext = mix_output_effect_ext_handler,
#else
    .mix_output = mix_output_handler,
    .mix_output_ext = mix_output_ext_handler,
#endif
};

static void bt_audio_mixer_ch_event_handler(void *priv, int event)
{
    switch (event) {
    case MIXER_EVENT_CH_OPEN:
#if AUDIO_CODEC_SUPPORT_SYNC
        if (audio_sync) {
            audio_output_sync_start();
        }
#endif
        break;
    case MIXER_EVENT_CH_CLOSE:
    case MIXER_EVENT_CH_RESET:
#if AUDIO_CODEC_SUPPORT_SYNC
        if (audio_sync) {
            audio_output_sync_stop();
        }
#endif
        break;
    default:
        break;
    }
}

static int get_rtp_header_len(u8 new_frame, u8 *buf, int len)
{
    int ext, csrc;
    int byte_len;
    int header_len = 0;
    u8 *data = buf;

    csrc = buf[0] & 0x0f;
    ext  = buf[0] & 0x10;

    byte_len = 12 + 4 * csrc;
    buf += byte_len;
    if (ext) {
        ext = (RB16(buf + 2) + 1) << 2;
    }

    if (new_frame) {
        header_len = byte_len + ext + 1;
    } else {
        header_len = byte_len + ext;
    }
    if (a2dp_dec->coding_type == AUDIO_CODING_SBC) {
        while (data[header_len] != 0x9c) {
            if (++header_len > len) {
                return len;
            }
        }
    }

    return header_len;
}

void __a2dp_drop_frame(void *p)
{
    int len;
    u8 *frame;
    int num = a2dp_media_get_packet_num();
    if (num > 1) {
        for (int i = 0; i < (num - 1); i++) {
            len = a2dp_media_get_packet(&frame);
            if (len <= 0) {
                break;
            }
            /* printf("a2dp_drop_frame: %d\n", len); */
            log_c('V');
            a2dp_media_free_packet(frame);
        }

    }
}

static void __a2dp_clean_frame_by_number(struct a2dp_dec_hdl *dec, u16 num)
{
    u16 end_seqn = dec->seqn + num;
    if (end_seqn == 0) {
        end_seqn++;
    }
    /*__a2dp_drop_frame(NULL);*/
    /*dec->drop_seqn = end_seqn;*/
    a2dp_media_clear_packet_before_seqn(end_seqn);
}


static void a2dp_drop_frame_start()
{
    /* printf("a2dp_drop_frame_start %x %x\n", a2dp_dec, a2dp_dec->timer); */
    if (a2dp_dec && (a2dp_dec->timer == 0)) {
        a2dp_dec->timer = sys_timer_add(NULL, __a2dp_drop_frame, 100);
    }
}

static void a2dp_drop_frame_stop()
{
    printf("a2dp_drop_frame_stop %x %x\n", a2dp_dec, a2dp_dec->timer);
    if (a2dp_dec && a2dp_dec->timer) {
        sys_timer_del(a2dp_dec->timer);
        a2dp_dec->timer = 0;
    }
}

void a2dp_dec_reset_output_channel(u8 ch_type)
{
    if (a2dp_dec) {
        a2dp_dec->tws_ch_type = ch_type;
    }
}

static void a2dp_dec_set_output_channel(struct a2dp_dec_hdl *dec)
{
    int state;
    enum audio_channel ch_type = AUDIO_CH_DIFF;
    u8 channel_num = audio_output_channel_num();
    if (channel_num == 2) {
        ch_type = AUDIO_CH_LR;
    }
#if TCFG_USER_TWS_ENABLE
    /* if (ch_type != AUDIO_CH_LR) { */
    if (!dec->tws_ch_type) {
        state = tws_api_get_tws_state();
        if (state & TWS_STA_SIBLING_CONNECTED) {
            if (channel_num == 1) {
                ch_type = tws_api_get_local_channel() == 'L' ? AUDIO_CH_L : AUDIO_CH_R;
            } else if (channel_num == 2) {
                ch_type = tws_api_get_local_channel() == 'L' ? AUDIO_CH_DUAL_L : AUDIO_CH_DUAL_R;
            }
        }
    }
#endif

    if (ch_type != dec->ch_type) {
        printf("set_channel: %d\n", ch_type);
        int ret = audio_decoder_set_output_channel(&dec->decoder, ch_type);
        dec->ch = AUDIO_CH_IS_MONO(ch_type) ? 1 : 2;
        if (ret == 0) {
            dec->ch_type = ch_type;
#if TCFG_BT_MUSIC_EQ_ENABLE
            if (a2dp_eq) {
                audio_eq_set_channel(a2dp_eq, dec->ch);
            }
#endif
        }
    }
}

static int a2dp_dec_get_frame(struct audio_decoder *decoder, u8 **frame)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);
    u8 *packet = NULL;
    int len = 0;

    len = a2dp_media_get_packet(&packet);
    if (len < 0) {
        putchar('X');
    }

    if (len > 0) {
        if (dec->coding_type == AUDIO_CODING_AAC && dec->seqn != RB16(packet + 2)) {
            dec->header_len = get_rtp_header_len(0, packet, len);
        } else {
            dec->header_len = get_rtp_header_len(1, packet, len);
        }
        dec->seqn = RB16(packet + 2);

        if (dec->header_len >= len) {
            a2dp_media_free_packet(packet);
            return -EFAULT;
        }

        *frame = packet + dec->header_len;
        len -= dec->header_len;
    }

    return len;
    /*return a2dp_media_get_packet(frame);*/
}

static void a2dp_dec_put_frame(struct audio_decoder *decoder, u8 *frame)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);

    if (frame) {
        a2dp_media_free_packet((void *)(frame - dec->header_len));
    }
}

static int a2dp_dec_fetch_frame(struct audio_decoder *decoder, u8 **frame)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);
    u8 *packet = NULL;
    int len = 0;
    int f_cnt = 0;

__find_data:
    packet = a2dp_media_fetch_packet(&len, NULL);
    if (packet) {
        dec->header_len = get_rtp_header_len(1, packet, len);
        *frame = packet + dec->header_len;
        len -= dec->header_len;
    } else {
        if (f_cnt++ < 10) {
            os_time_dly(1);
            log_i("wait bt data\n");
            goto __find_data;
        }
    }


    return len;
}


static const struct audio_dec_input a2dp_input = {
    .coding_type = AUDIO_CODING_SBC,
    .data_type   = AUDIO_INPUT_FRAME,
    .ops = {
        .frame = {
            .fget = a2dp_dec_get_frame,
            .fput = a2dp_dec_put_frame,
            .ffetch = a2dp_dec_fetch_frame,
        }
    }
};

#if BT_DEC_SYNC_EN
static int a2dp_src_output_handler(void *priv, void *buf, int len);

static int a2dp_audio_stream_sync(struct a2dp_dec_hdl *dec, int data_size)
{
    if (!dec->hw_src) {
        return 0;
    }

    int sr = dec->audio_new_rate + dec->rate_offset + dec->adjust_step;

    if (data_size < dec->bottom_size) {
        /* putchar('<'); */
        dec->adjust_step += BT_DEC_RATE_INC_STEP;
        if (dec->adjust_step < 0) {
            dec->adjust_step += BT_DEC_RATE_INC_STEP * 2;
        }
    } else if (data_size > dec->top_size) {
        /* putchar('>'); */
        dec->adjust_step -= BT_DEC_RATE_DEC_STEP;
        if (dec->adjust_step > 0) {
            dec->adjust_step -= BT_DEC_RATE_DEC_STEP * 2;
        }
    } else {
        /* putchar('='); */
        if (dec->adjust_step > 0) {
            dec->adjust_step -= (dec->adjust_step * BT_DEC_RATE_INC_STEP) / BT_DEC_RATE_MAX_STEP;
            if (dec->adjust_step > 0) {
                dec->adjust_step --;
            }
        } else if (dec->adjust_step < 0) {
            dec->adjust_step += ((0 - dec->adjust_step) * BT_DEC_RATE_DEC_STEP) / BT_DEC_RATE_MAX_STEP;
            if (dec->adjust_step < 0) {
                dec->adjust_step ++;
            }
        }
    }

    if (dec->adjust_step < -BT_DEC_RATE_MAX_STEP) {
        dec->adjust_step = -BT_DEC_RATE_MAX_STEP;
    } else if (dec->adjust_step > BT_DEC_RATE_MAX_STEP) {
        dec->adjust_step = BT_DEC_RATE_MAX_STEP;
    }

    if (sr != (dec->audio_new_rate + dec->rate_offset + dec->adjust_step)) {
        /* printf("a2dp sr[%d] [%d] \n",dec->sample_rate,dec->audio_new_rate + dec->rate_offset + dec->adjust_step); */
        audio_hw_src_set_rate(dec->hw_src, dec->sample_rate, dec->audio_new_rate + dec->rate_offset + dec->adjust_step);
    }

    return 0;
}

static int a2dp_audio_sync_init(struct a2dp_dec_hdl *dec)
{
    u8 ch = dec->ch;
    // printf("total:%d, size:%d \n", linein_sample_total(dec->linein), linein_sample_size(dec->linein));
    int a2dp_total_size = a2dp_media_get_total_buffer_size();
    dec->sync_start = 0;
    dec->begin_size = a2dp_total_size * 60 / 100;
    dec->top_size = a2dp_total_size * 50 / 100;
    dec->bottom_size = a2dp_total_size * 30 / 100;

    u16 out_sr = dec->src_out_sr;//audio_output_rate(dec->sample_rate);
    printf("out_sr:%d, dsr:%d, dch:%d \n", out_sr, dec->sample_rate, ch);
    dec->audio_new_rate = out_sr;

    dec->hw_src = zalloc(sizeof(struct audio_src_handle));
    if (!dec->hw_src) {
        return -ENODEV;
    }
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
    u8 ch_num = ch;
#else
    u8 ch_num = 4;
#endif
#else
    u8 ch_num = ch;
#endif

    audio_hw_src_open(dec->hw_src, ch_num, SRC_TYPE_AUDIO_SYNC);

    audio_hw_src_set_rate(dec->hw_src, dec->sample_rate, dec->audio_new_rate);

    audio_src_set_output_handler(dec->hw_src, dec, a2dp_src_output_handler);
    return 0;
}
#endif


#if AUDIO_CODEC_SUPPORT_SYNC
static int a2dp_dec_rx_info_check(struct rt_stream_info *info)
{
    int len = 0;
    u8 fetch_cnt = 0;
    info->remain_len = a2dp_media_get_remain_buffer_size();

    if (a2dp_media_get_packet_num() < 1) {
        if (!a2dp_media_channel_exist()) {
            return 0;
        }
    }

    while (fetch_cnt++ < 3) {
        info->baddr = (void *)a2dp_media_fetch_packet(&len, NULL);
        if (info->baddr) {
            info->seqn = RB16(info->baddr + 2);
            if (a2dp_dec->sync_step) {
                if ((u16)(info->seqn - a2dp_dec->seqn) > 1) {
                    log_e("rx seqn error : %d, %d\n", a2dp_dec->seqn, info->seqn);
                    return -EFAULT;
                }
            }
            a2dp_dec->seqn = info->seqn;
            return 0;
        }
        if (!a2dp_media_channel_exist()) {
            return 0;
        }

        if (fetch_cnt > 1 && a2dp_dec->sync_step && audio_output_dev_is_working()) {
            log_w("a2dp rx no packet, tws time : %dms\n", a2dp_media_get_remain_play_time(1));
            return -EFAULT;
        }
        os_time_dly(2);
    }

    /* putchar('x'); */
    return -EINVAL;
}

static int a2dp_dec_stop_and_restart(struct audio_decoder *decoder, u8 lite_reset)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);

#if AUDIO_CODEC_SUPPORT_SYNC
    if (audio_sync) {
        if (!lite_reset) {
            __a2dp_clean_frame_by_number(dec, dec->coding_type == AUDIO_CODING_AAC ? 20 : 20);
        }
#if TCFG_BT_MUSIC_EQ_ENABLE
#if A2DP_EQ_SUPPORT_ASYNC
        if (a2dp_eq) {
            audio_eq_async_data_clear(a2dp_eq);
            dec->remain = 0;
            dec->eq_remain = 0;
        }
#endif
#endif
        audio_mixer_ch_reset(&dec->mix_ch);
        audio_wireless_sync_stop(audio_sync);
        audio_decoder_reset(decoder);
        dec->sync_step = 0;
        if (audio_mixer_get_active_ch_num(&mixer) > 1) {
            return 0;
        }
#if TCFG_USER_TWS_ENABLE
        int state = tws_api_get_tws_state();
        if (!lite_reset && (state & TWS_STA_SIBLING_CONNECTED)) {
            app_audio_output_reset(300);
        } else
#endif
        {
            app_audio_output_reset(0);
        }
        /*__a2dp_drop_frame(NULL);*/
    }
#endif
    return 0;
}

#if (TCFG_USER_TWS_ENABLE && AUDIO_CODEC_SUPPORT_SYNC)

#define TWS_FUNC_ID_A2DP_DEC \
	((int)(('A' + '2' + 'D' + 'P') << (2 * 8)) | \
	 (int)(('D' + 'E' + 'C') << (1 * 8)) | \
	 (int)(('S' + 'Y' + 'N' + 'C') << (0 * 8)))

static u16 a2dp_align_timeout = 0;
static void tws_a2dp_dec_align(int time)
{
    if (a2dp_align_timeout) {
        sys_hi_timeout_del(a2dp_align_timeout);
        a2dp_align_timeout = 0;
    }

    if (!a2dp_dec) {
        if (!a2dp_align_timeout) {
            a2dp_align_timeout = sys_hi_timeout_add((void *)time, (void(*)(void *))tws_a2dp_dec_align, 30);
        }
        return;
    }

    local_irq_disable();
    a2dp_dec->tws_confirm = 1;
    a2dp_dec->tws_together_time = time;
    local_irq_enable();
}

TWS_SYNC_CALL_REGISTER(tone_player_align) = {
    .uuid = TWS_FUNC_ID_A2DP_DEC,
    .func = tws_a2dp_dec_align,
};
#endif

#define A2DP_DELAY_TIME         250

#define bt_time_before(t1, t2) \
         (((t1 < t2) && ((t2 - t1) & 0x7ffffff) < 0xffff) || \
          ((t1 > t2) && ((t1 - t2) & 0x7ffffff) > 0xffff))
#define bt_time_to_msecs(clk)   (((clk) * 625) / 1000)

static int a2dp_dec_prepare_to_start(struct audio_decoder *decoder, int msecs, int rx_remain)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);

#if TCFG_USER_TWS_ENABLE
    if (!dec->sync_step) {
        if (!msecs) {
            return -EAGAIN;
        }
        int state = tws_api_get_tws_state();
        int distance_time = A2DP_DELAY_TIME - msecs;
        if (distance_time <= 0) {
            if ((state & TWS_STA_SIBLING_CONNECTED) && !audio_output_dev_is_working()) {
                log_w("a2dp tws dec align, distance : %dms, confirm : %d\n", distance_time, dec->tws_confirm);
            } else {
                log_d("reach to time : %dms\n", msecs);
            }
            dec->sync_step = 2;
            return 0;
        }

        local_irq_disable();
        if (dec->tws_confirm &&
            (bt_time_before(dec->tws_together_time, bt_tws_future_slot_time(0)) ||
             bt_time_to_msecs(__builtin_abs((int)dec->tws_together_time - (int)bt_tws_future_slot_time(0))) > distance_time)) {
            dec->tws_confirm = 0;
        }
        local_irq_enable();

        if (state & TWS_STA_SIBLING_CONNECTED) {
            if (!audio_output_dev_is_working()) {
                if (tws_api_get_role() == TWS_ROLE_SLAVE) {
#if A2DP_RX_AND_AUDIO_DELAY
                    tws_api_sync_call_by_uuid(TWS_FUNC_ID_A2DP_DEC, bt_tws_future_slot_time(distance_time + 10), 0);
#else
                    tws_api_sync_call_by_uuid(TWS_FUNC_ID_A2DP_DEC, bt_tws_future_slot_time(distance_time + 30), 0);
#endif
                    /*printf("confirm delay time : %dms\n", distance_time);*/
                }
                dec->sync_step = 1;
                dec->wait_time = jiffies + msecs_to_jiffies(distance_time);
                audio_decoder_wakeup_modify(2);
                return -EAGAIN;
            }
        }
        if (msecs < A2DP_DELAY_TIME && rx_remain > 768) {
            return -EAGAIN;
        }
        dec->sync_step = 2;
    } else if (dec->sync_step == 1) {
        if (!msecs) {
            dec->sync_step = 0;
            return -EAGAIN;
        }

        if (dec->tws_confirm) {
            audio_sync_set_tws_together(audio_sync, 1, dec->tws_together_time);
            printf("a2dp tws together time : %d\n", dec->tws_together_time);
            dec->tws_confirm = 0;
            dec->sync_step = 2;
            audio_decoder_wakeup_modify(AUDIO_DECODE_TASK_WAKEUP_TIME);
            return 0;
        }

        if (time_after(jiffies, dec->wait_time)) {
            log_w("a2dp wait tws confirm timeout\n");
            dec->sync_step = 2;
            audio_decoder_wakeup_modify(AUDIO_DECODE_TASK_WAKEUP_TIME);
            return 0;
        }
        return -EAGAIN;
    }
#else
    if (!dec->sync_step) {
        if (msecs < A2DP_DELAY_TIME && rx_remain > 768) {
            return -EAGAIN;
        }
        dec->sync_step = 2;
    }
#endif

    return 0;
}

static int a2dp_audio_delay_time(struct audio_decoder *decoder)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);
    int msecs = 0;
#if TCFG_BT_MUSIC_EQ_ENABLE
#if A2DP_EQ_SUPPORT_ASYNC
    if (a2dp_eq) {
        msecs += (((audio_eq_data_len(a2dp_eq) >> 1) / dec->ch * 1000000) / dec->sample_rate) / 1000;
    }
#endif
#endif
    msecs += ((audio_mixer_ch_data_len(&dec->mix_ch) >> 1) / dec->ch * 1000000) / dec->sample_rate / 1000;

    msecs += audio_output_buf_time();

    return msecs;
}

static int a2dp_dec_rx_delay_monitor(struct audio_decoder *decoder, struct rt_stream_info *info)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);
    int msecs = 0;
    int err = 0;

#if TCFG_USER_TWS_ENABLE
    msecs = a2dp_media_get_remain_play_time(1);
#else
    msecs = a2dp_media_get_remain_play_time(0);
#endif

    err = a2dp_dec_prepare_to_start(decoder, msecs, info->remain_len);
    if (err) {
        return err;
    }

#if A2DP_RX_AND_AUDIO_DELAY
    msecs += a2dp_audio_delay_time(decoder);
#endif

    info->rx_delay = RX_DELAY_NULL;
#if A2DP_RX_AND_AUDIO_DELAY
    if (msecs < A2DP_DELAY_TIME) {
        info->rx_delay = RX_DELAY_DOWN;
    } else if (msecs >= A2DP_DELAY_TIME) {
        info->rx_delay = RX_DELAY_UP;
    }
#else
    if (msecs < A2DP_DELAY_TIME - 10) {
        info->rx_delay = RX_DELAY_DOWN;
    } else if (msecs >= A2DP_DELAY_TIME + 10) {
        info->rx_delay = RX_DELAY_UP;
    }
#endif

    if (info->remain_len < 768) {
        info->rx_delay = RX_DELAY_UP;
    }

    /*printf("%dms\n", msecs);*/
    return 0;
}
#endif /*AUDIO_CODEC_SUPPORT_SYNC*/


#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))

// 延迟一下，过滤掉从其他模式返回后由于快速对齐导致的杂音
// 从其他模式返回后可能不是从同一个seqn开始，差异比较大
#define A2DP_DEC_DELAY_BT_TIME		(500)

extern u32 audio_bt_time_read(u16 *bt_phase, s32 *bt_bitoff);

static u32 a2dp_bt_tmr = 0;
void a2dp_dec_output_set_start_bt_time(u32 bt_tmr)
{
#if A2DP_DEC_DELAY_BT_TIME
    bt_tmr += A2DP_DEC_DELAY_BT_TIME;
#endif
    if (bt_tmr <= a2dp_bt_tmr) {
        y_printf("a2dp_dec bt:%d, cbt:%d \n", bt_tmr, a2dp_bt_tmr);
        return ;
    }
    a2dp_bt_tmr = bt_tmr;
    y_printf("a2dp_dec_output_set_start_bt_time:%d \n", bt_tmr);
}
int a2dp_dec_output_run_by_start_bt_time(struct audio_decoder *decoder, s16 *data, int len)
{
    if (a2dp_bt_tmr == 0) {
        return -1;
    }
    u16 phase = 0;
    s32 bitoff = 0;
    u32 cur_tmr = audio_bt_time_read(&phase, &bitoff);
    int offset = (a2dp_bt_tmr - cur_tmr) * 625 + phase - bitoff;
    /* y_printf("a2dp_dec_output_run_by_start_bt_time, cur:%d, bt:%d, offset:%d \n", cur_tmr, a2dp_bt_tmr, offset); */
    if (offset <= 0) {
        a2dp_bt_tmr = 0;
        return -2;
    } else if (offset >= 1000 * 1000) {
        a2dp_bt_tmr = 0;
        return -3;
    }
    if (offset >= 10 * 1000) {
        memset(data, 0, len);
        return 0;
    }
    int tlen = offset * decoder->fmt.sample_rate / (1000 * 1000) * decoder->fmt.channel * 2;
    if (tlen <= len) {
        memset(data, 0, tlen);
        a2dp_bt_tmr = 0;
        return 0;
    }
    memset(data, 0, len);
    return 0;
}
#endif
static int a2dp_dec_probe_handler(struct audio_decoder *decoder)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);
    int err = 0;

#if AUDIO_CODEC_SUPPORT_SYNC
    if (audio_sync) {
        if (dec->preempt_state == DEC_PREEMTED_BY_OUTSIDE) {
            audio_decoder_pause(decoder);
            return -EINVAL;
        }

        if (dec->preempt_state == DEC_RESUME_BY_OUTSIDE) {
            audio_wireless_sync_reset(audio_sync);
            a2dp_dec_stop_and_restart(decoder, 1);
            dec->preempt_state = DEC_RUN_BY_ITSELF;
            return -EAGAIN;
        }
        struct rt_stream_info rts_info = {0};
        err = a2dp_dec_rx_info_check(&rts_info);
        if (err) {
            if (err == -EFAULT) {
                audio_wireless_sync_suspend(audio_sync);
                a2dp_dec_stop_and_restart(decoder, 0);
            } else {
                audio_decoder_suspend(decoder, 0);
            }
            if (dec->sync_step == 1) {
                dec->sync_step = 0;
            }
            return -EAGAIN;
        }

        err = a2dp_dec_rx_delay_monitor(decoder, &rts_info);
        if (err) {
            audio_decoder_suspend(decoder, 0);
            return -EAGAIN;
        }

        err = audio_wireless_sync_probe(audio_sync, &rts_info);
        if (err) {
            /* printf("AE:%d\n", err); */
            if (err == SYNC_ERR_STREAM_RESET) {
                a2dp_dec_stop_and_restart(decoder, 0);
            }

            return -EAGAIN;
        }

#if TCFG_USER_TWS_ENABLE
        if (tws_network_audio_was_started()) {
            /*a2dp播放中副机加入，声音复位500ms*/
            tws_network_local_audio_start();
            app_audio_output_reset(500);
        }

#endif
        if (dec->preempt_state == DEC_PREEMTED_BY_PRIORITY) {
            dec->preempt_state = DEC_RUN_BY_ITSELF;
#if TCFG_USER_TWS_ENABLE
            int state = tws_api_get_tws_state();
            if (state & TWS_STA_SIBLING_CONNECTED) {
                app_audio_output_reset(500);
            }
#endif
        }
    }
#endif

#if BT_DEC_SYNC_EN
    if (!dec->sync_start) {
        if (a2dp_media_get_total_data_len() < dec->begin_size) {
            audio_decoder_suspend(decoder, 0);
            return -EAGAIN;
        }
        dec->sync_start = 1;
        audio_mixer_ch_pause(&dec->mix_ch, 0);
    }
    if (a2dp_media_get_packet_num() < 1) {
        u32 cur_tmr = timer_get_ms();
        u32 offset = cur_tmr - dec->read_time;
        if (cur_tmr < dec->read_time) {
            offset = ((u32) - 1) - dec->read_time + cur_tmr;
        }
        if (offset >= 15) {
            dec->sync_start = 0;
            audio_mixer_ch_pause(&dec->mix_ch, 1);
            r_printf("a2dp to \n");
        }
        audio_decoder_suspend(decoder, 0);
        return -EAGAIN;
    }
    dec->read_time = timer_get_ms();
    a2dp_audio_stream_sync(dec, a2dp_media_get_total_data_len());
#endif /*BT_DEC_SYNC_EN*/

    a2dp_dec_set_output_channel(dec);
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) && (A2DP_SRC_EN == 0))
    reset_reverb_src_out(dec->sample_rate);
#endif

    return err;
}

static int a2dp_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    int wlen = 0;
    int rlen = len;
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);
#if TEST_POINTS
    {
        static u32 points = 0;//
        points += len / 2;
        if (points >= 16384) {
            points -= 16384;
            JL_PORTC->DIR &= ~BIT(3);
            JL_PORTC->OUT ^= BIT(3);
        }
        return len;
    }
#endif
    if (!dec->remain) {
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        a2dp_dec_output_run_by_start_bt_time(decoder, data, len);
#endif
        if (audio_sync) {
            audio_wireless_sync_after_dec(audio_sync, data, len);
        }

#if ((defined(DEC_OUT_OTHER_DATA) && (DEC_OUT_OTHER_DATA)))
        other_audio_dec_output(decoder, data, len, dec->ch, dec->decoder.fmt.sample_rate);
#endif

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
        if (dec->user_hdl) {
            u8 ch_num = dec->ch;
            user_audio_process_handler_run(dec->user_hdl, data, len, ch_num);
        }
#endif


    }


#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)

#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
#else
    u8 *eq[2] = {NULL, NULL};
#if TCFG_BT_MUSIC_EQ_ENABLE
    eq[0] =	(void *)a2dp_eq;
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
    eq[1] =	(void *)a2dp_eq2;
#endif
#endif
    u8 *drc[2] = {NULL, NULL};
#if TCFG_BT_MUSIC_DRC_ENABLE
    drc[0] = (void *)a2dp_drc;
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
    drc[1] = (void *)a2dp_drc2;
#endif
#endif

    void *hw_src = NULL;
#if A2DP_SRC_EN
    hw_src = dec->hw_src;
#endif
#if TCFG_BT_MUSIC_EQ_ENABLE
#if A2DP_EQ_SUPPORT_ASYNC
    wlen = audio_dualeq_to_quadeq_output_async(eq, drc, &dec->mix_ch, data, len);
#else
    wlen = audio_dualeq_to_quadeq_output_sync(hw_src, eq, drc, &dec->mix_ch, data, len, A2DP_DEC_ID, dec->quad_data, NULL);
#endif
#else
    wlen = audio_dual_to_quad_output_src(hw_src, &dec->mix_ch, data, len, A2DP_DEC_ID, dec->quad_data, NULL);
#endif
    len -= wlen;
    if (len == 0) {
        dec->remain = 0;
    } else {
        dec->remain = 1;
    }

    return wlen;
#endif
#endif


#if A2DP_EQ_SUPPORT_ASYNC
#if TCFG_BT_MUSIC_EQ_ENABLE
    if (a2dp_eq) {
        int eqlen = audio_eq_run(a2dp_eq, data, len);
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
#if TCFG_BT_MUSIC_EQ_ENABLE
        if (a2dp_eq) {
            audio_eq_run(a2dp_eq, data, len);
        }
#endif
#if TCFG_BT_MUSIC_DRC_ENABLE
        if (a2dp_drc) {
            audio_drc_run(a2dp_drc, data, len);
        }
#endif

    }


#if A2DP_SRC_EN
    if (dec->hw_src) {
        wlen = audio_src_resample_write(dec->hw_src, data, len);
    } else
#endif
    {
        wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
    }

    int remain_len = len - wlen;

    if (remain_len == 0) {
        dec->remain = 0;
    } else {
        dec->remain = 1;
    }

    return wlen;
}

static int a2dp_dec_post_handler(struct audio_decoder *decoder)
{
    int limit_dec[1];
    limit_dec[0] = (int)decoder;
    audio_decoder_resume_off_limits(&decode_task, 1, limit_dec);
    return 0;
}

static int a2dp_dec_stop_handler(struct audio_decoder *decoder)
{
#if AUDIO_CODEC_SUPPORT_SYNC
    audio_sync_close();
#endif
    return 0;
}

static const struct audio_dec_handler a2dp_dec_handler = {
    .dec_probe  = a2dp_dec_probe_handler,
    .dec_output = a2dp_dec_output_handler,
    .dec_post   = a2dp_dec_post_handler,
    .dec_stop   = a2dp_dec_stop_handler,
};

int a2dp_dec_close();

static void a2dp_dec_release()
{
    audio_decoder_task_del_wait(&decode_task, &a2dp_dec->wait);
    a2dp_drop_frame_stop();

    if (a2dp_dec->coding_type == AUDIO_CODING_SBC) {
        clock_remove(DEC_SBC_CLK);
    } else if (a2dp_dec->coding_type == AUDIO_CODING_AAC) {
        clock_remove(DEC_AAC_CLK);
    }

    local_irq_disable();
    if (a2dp_dec->quad_data) {
        free(a2dp_dec->quad_data);
        a2dp_dec->quad_data = NULL;
    }
    free(a2dp_dec);
    a2dp_dec = NULL;
    local_irq_enable();


}

static void a2dp_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        puts("AUDIO_DEC_EVENT_END\n");
        a2dp_dec_close();
        break;
    }
}

#if A2DP_SRC_EN
static int a2dp_src_output_handler(void *priv, void *buf, int len)
{
    s16 *data = buf;
    struct a2dp_dec_hdl *dec = (struct a2dp_dec_hdl *)priv;
    if (!dec || !dec->start) {
        return 0;
    }

    return audio_mixer_ch_write(&dec->mix_ch, data, len);
}
#endif


int a2dp_dec_start()
{
    int err;
    struct audio_fmt *fmt;
    struct a2dp_dec_hdl *dec = a2dp_dec;

    if (!a2dp_dec) {
        return -EINVAL;
    }

    puts("a2dp_dec_start: in\n");

    err = audio_decoder_open(&dec->decoder, &a2dp_input, &decode_task);
    if (err) {
        goto __err1;
    }
    dec->ch_type = AUDIO_CH_MAX;

    audio_decoder_set_handler(&dec->decoder, &a2dp_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, a2dp_dec_event_handler, 0);

    if (a2dp_dec->coding_type != a2dp_input.coding_type) {
        struct audio_fmt f = {0};
        f.coding_type = a2dp_dec->coding_type;
        err = audio_decoder_set_fmt(&dec->decoder, &f);
        if (err) {
            goto __err2;
        }
    }

    err = audio_decoder_get_fmt(&dec->decoder, &fmt);
    if (err) {
        goto __err2;
    }

    dec->sample_rate = fmt->sample_rate;

#if (A2DP_SRC_EN == 0)
    set_source_sample_rate(fmt->sample_rate);
#endif
    a2dp_dec_set_output_channel(dec);

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
    app_audio_output_mode_set(DAC_OUTPUT_FRONT_LR_REAR_LR);
    if (app_audio_output_mode_get() == DAC_OUTPUT_FRONT_LR_REAR_LR) {
        app_audio_output_channel_set(4);
    }
#endif

    audio_mixer_ch_open(&dec->mix_ch, &mixer);
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, audio_output_rate(fmt->sample_rate));
#if (defined (TCFG_MIXER_EXT_ENABLE) && (TCFG_MIXER_EXT_ENABLE))
    audio_mixer_ch_set_ext_out_mask(&dec->mix_ch, BIT(0));
#endif//TCFG_MIXER_EXT_ENABLE

    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

    printf("dec->ch:%d, fmt->channel:%d,fmt sample_rate%d \n", dec->ch, fmt->channel, fmt->sample_rate);

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
    dec->quad_data = NULL;
#else
    dec->quad_data = (s16 *)malloc(DUAL_TO_QUAD_POINTS * 2 * 2); //四通道buf
#endif
#endif


    a2dp_eq_drc_open(dec, fmt);

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    struct user_audio_digital_parm  vol_parm = {0};
#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
    vol_parm.en  = a2dp_dec_param.d_vol_en;
    vol_parm.vol_max = ((a2dp_dec_param.d_vol_max != NULL) ? a2dp_dec_param.d_vol_max() : 0);
    vol_parm.vol = ((a2dp_dec_param.d_vol_cur != NULL) ? a2dp_dec_param.d_vol_cur() : 0);
    vol_parm.fade_step = a2dp_dec_param.d_vol_fade_step;
#endif
    dec->user_hdl = user_audio_process_open((void *)&vol_parm, (void *)&a2dp_dec_param, user_audio_process_handler);
#endif


#if A2DP_SRC_EN
    dec->src_out_sr = audio_output_rate(fmt->sample_rate);
    printf("fmt->sample_rate:%d dec->src_out_sr:%d \n\n", fmt->sample_rate, dec->src_out_sr);
#if BT_DEC_SYNC_EN
    a2dp_audio_sync_init(dec);
#else
    if (fmt->sample_rate != dec->src_out_sr) {
        u8 ch_num = dec->ch;//fmt->channel;
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
        ch_num = dec->ch;//fmt->channel;
#else
        ch_num = 4;
#endif
#endif
        dec->hw_src = audio_hw_resample_open(dec, a2dp_src_output_handler, ch_num, fmt->sample_rate, dec->src_out_sr);;
    }
#endif
#endif

#if AUDIO_CODEC_SUPPORT_SYNC
#if A2DP_SRC_EN
    audio_sync = audio_sync_a2dp_open(dec, dec->src_out_sr, dec->src_out_sr, a2dp_dec->coding_type);
#else
    audio_sync = audio_sync_a2dp_open(dec, fmt->sample_rate, audio_output_rate(fmt->sample_rate), a2dp_dec->coding_type);
#endif
#endif
    a2dp_drop_frame_stop();
    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err3;
    }


    dec->start = 1;
    clock_set_cur();


    return 0;

__err3:

#if A2DP_SRC_EN
    if (dec->hw_src) {
        audio_hw_resample_close(dec->hw_src);
        dec->hw_src = NULL;
    }

#endif
    set_source_sample_rate(0);
    audio_mixer_ch_close(&dec->mix_ch);

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    if (dec->user_hdl) {
        user_audio_process_close(dec->user_hdl);
        dec->user_hdl = NULL;
    }
#endif
__err2:
    audio_decoder_close(&dec->decoder);
    a2dp_eq_drc_close(dec);
__err1:
    a2dp_dec_release();

    return err;
}


static int __a2dp_audio_res_close(void)
{
    if (a2dp_dec->start == 0) {
        printf("a2dp_dec->start == 0");
        return 0;
    }


    a2dp_dec->start = 0;
    audio_decoder_close(&a2dp_dec->decoder);
#if AUDIO_CODEC_SUPPORT_SYNC
    audio_sync_close();
    a2dp_dec->preempt_state = DEC_PREEMTED_BY_PRIORITY;
#endif
    set_source_sample_rate(0);
#if A2DP_SRC_EN
    if (a2dp_dec->hw_src) {
        audio_hw_resample_close(a2dp_dec->hw_src);
        a2dp_dec->hw_src = NULL;
    }
#endif

    audio_mixer_ch_close(&a2dp_dec->mix_ch);

    a2dp_eq_drc_close(a2dp_dec);


#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    if (a2dp_dec->user_hdl) {
        user_audio_process_close(a2dp_dec->user_hdl);
        a2dp_dec->user_hdl = NULL;
    }
#endif



    app_audio_state_exit(APP_AUDIO_STATE_MUSIC);

#if 0//(defined(TCFG_IIS_ENABLE) && (TCFG_IIS_ENABLE)&&(TCFG_IIS_OUTPUT_EN))
    audio_link_switch_sr(TCFG_IIS_OUTPUT_PORT, TCFG_IIS_OUTPUT_SR);
    audio_link_sync_en(0);
#endif

    return 0;
}

static int a2dp_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

    printf("a2dp_wait_res_handler: %d\n", event);

    if (event == AUDIO_RES_GET) {
        err = a2dp_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        if (a2dp_dec->start) {
            __a2dp_audio_res_close();
            a2dp_drop_frame_start();
        }
    }
    return err;
}

void audio_overlay_load_code(u32 type);
#define A2DP_CODEC_SBC			0x00
#define A2DP_CODEC_MPEG12		0x01
#define A2DP_CODEC_MPEG24		0x02
u8 is_a2dp_dec_open()
{
    if (a2dp_dec) {
        return 1;
    }
    return 0;
}
int a2dp_dec_open(int media_type)
{
    struct a2dp_dec_hdl *dec;

    if (a2dp_dec) {
        return 0;
    }

    printf("a2dp_dec_open: %d\n", media_type);

    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }
    a2dp_dec = dec;
    switch (media_type) {
    case A2DP_CODEC_SBC:
        printf("a2dp_media_type:SBC");
        dec->coding_type = AUDIO_CODING_SBC;
        clock_add(DEC_SBC_CLK);
        break;
    case A2DP_CODEC_MPEG24:
        printf("a2dp_media_type:AAC");
        dec->coding_type = AUDIO_CODING_AAC;
        clock_add(DEC_AAC_CLK);
        break;
    default:
        printf("a2dp_media_type unsupoport:%d", media_type);
        free(dec);
        return -EINVAL;
    }
    dec->wait.priority = 1;
#if (defined(DEC_MIX_ENABLE) && DEC_MIX_ENABLE)
    dec->wait.preemption = 0;
    dec->wait.protect = 1;
#else
    dec->wait.preemption = 1;
#endif//DEC_MIX_ENABLE

    dec->wait.handler = a2dp_wait_res_handler;
    audio_decoder_task_add_wait(&decode_task, &dec->wait);

    if (a2dp_dec && (a2dp_dec->start == 0)) {
        a2dp_drop_frame_start();
    }

    return 0;
}

int a2dp_dec_close()
{
    if (!a2dp_dec) {
        return 0;
    }
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    a2dp_bt_tmr = 0;
#endif

    if (a2dp_dec->start) {
        __a2dp_audio_res_close();
    }
    a2dp_dec_release();/*free a2dp_dec*/

    puts("a2dp_dec_close: exit\n");
    clock_set_cur();
    return 1;
}

static int esco_dec_get_frame(struct audio_decoder *decoder, u8 **frame)
{
    int len = 0;
    u32 hash = 0;
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);

__again:
    if (dec->frame) {
        int len = dec->frame_len - dec->offset;
        if (len > dec->esco_len - dec->data_len) {
            len = dec->esco_len - dec->data_len;
        }
        memcpy((u8 *)dec->data + dec->data_len, dec->frame + dec->offset, len);
        dec->offset   += len;
        dec->data_len += len;
        if (dec->offset == dec->frame_len) {
            lmp_private_free_esco_packet(dec->frame);
            dec->frame = NULL;
        }
    }
    if (dec->data_len < dec->esco_len) {
        dec->frame = lmp_private_get_esco_packet(&len, &hash);
        /* printf("hash1=%d,%d ",hash,dec->frame ); */
        if (len <= 0) {
            printf("rlen=%d ", len);
            return -EIO;
        }
        if (len == 0) {
            return -ETIMEDOUT;
        }

        dec->frame_get = 1;
        dec->offset = 0;
        dec->frame_len = len;
        dec->hash = hash;
        goto __again;
    }
    *frame = (u8 *)dec->data;
    return dec->esco_len;
}

static void esco_dec_put_frame(struct audio_decoder *decoder, u8 *frame)
{
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);

    dec->data_len = 0;

    /*lmp_private_free_esco_packet((void *)frame);*/
}

static const struct audio_dec_input esco_input = {
    .coding_type = AUDIO_CODING_MSBC,
    .data_type   = AUDIO_INPUT_FRAME,
    .ops = {
        .frame = {
            .fget = esco_dec_get_frame,
            .fput = esco_dec_put_frame,
        }
    }
};

#if BT_DEC_SYNC_EN
static int esco_src_output_handler(void *priv, void *buf, int len);

static int esco_audio_stream_sync(struct esco_dec_hdl *dec, int data_size)
{
    if (!dec->hw_src) {
        return 0;
    }

    int sr = dec->audio_new_rate + dec->rate_offset + dec->adjust_step;

    if (data_size < dec->bottom_size) {
        /* putchar('<'); */
        dec->adjust_step += BT_DEC_RATE_INC_STEP;
        if (dec->adjust_step < 0) {
            dec->adjust_step += BT_DEC_RATE_INC_STEP * 2;
        }
    } else if (data_size > dec->top_size) {
        /* putchar('>'); */
        dec->adjust_step -= BT_DEC_RATE_DEC_STEP;
        if (dec->adjust_step > 0) {
            dec->adjust_step -= BT_DEC_RATE_DEC_STEP * 2;
        }
    } else {
        /* putchar('='); */
        if (dec->adjust_step > 0) {
            dec->adjust_step -= (dec->adjust_step * BT_DEC_RATE_INC_STEP) / BT_DEC_RATE_MAX_STEP;
            if (dec->adjust_step > 0) {
                dec->adjust_step --;
            }
        } else if (dec->adjust_step < 0) {
            dec->adjust_step += ((0 - dec->adjust_step) * BT_DEC_RATE_DEC_STEP) / BT_DEC_RATE_MAX_STEP;
            if (dec->adjust_step < 0) {
                dec->adjust_step ++;
            }
        }
    }

    if (dec->adjust_step < -BT_DEC_RATE_MAX_STEP) {
        dec->adjust_step = -BT_DEC_RATE_MAX_STEP;
    } else if (dec->adjust_step > BT_DEC_RATE_MAX_STEP) {
        dec->adjust_step = BT_DEC_RATE_MAX_STEP;
    }

    if (sr != (dec->audio_new_rate + dec->rate_offset + dec->adjust_step)) {
        /* printf("esco sr[%d] [%d] \n",dec->sample_rate,dec->audio_new_rate + dec->rate_offset + dec->adjust_step); */
        audio_hw_src_set_rate(dec->hw_src, dec->sample_rate, dec->audio_new_rate + dec->rate_offset + dec->adjust_step);
    }

    return 0;
}

static int esco_audio_sync_init(struct esco_dec_hdl *dec)
{
    u8 ch = dec->decoder.fmt.channel;//dec->ch;
    // printf("total:%d, size:%d \n", linein_sample_total(dec->linein), linein_sample_size(dec->linein));
    int esco_total_size = 60 * 50; //a2dp_media_get_total_buffer_size();
    dec->sync_start = 0;
    dec->begin_size = esco_total_size * 60 / 100;
    dec->top_size = esco_total_size * 20 / 100;
    dec->bottom_size = esco_total_size * 6 / 100;

    u16 out_sr = dec->src_out_sr;//audio_output_rate(dec->sample_rate);
    printf("out_sr:%d, dsr:%d, dch:%d \n", out_sr, dec->sample_rate, ch);
    dec->audio_new_rate = out_sr;

    dec->hw_src = zalloc(sizeof(struct audio_src_handle));
    if (!dec->hw_src) {
        return -ENODEV;
    }
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
    u8 ch_num = ch;
#else
    u8 ch_num = 4;
#endif
#else
    u8 ch_num = ch;
#endif

    audio_hw_src_open(dec->hw_src, ch_num, SRC_TYPE_AUDIO_SYNC);

    audio_hw_src_set_rate(dec->hw_src, dec->sample_rate, dec->audio_new_rate);

    audio_src_set_output_handler(dec->hw_src, dec, esco_src_output_handler);
    return 0;
}
#endif

#if AUDIO_CODEC_SUPPORT_SYNC
static void esco_dump_rt_stream_info(struct audio_decoder *decoder, struct rt_stream_info *info)
{
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);
    int len = 0;
    u32 hash = 0;
    u8 fetch_cnt = 0;
    info->remain_len = 512;
    info->data_len = lmp_private_get_esco_data_len();

    if (dec->frame_get) {
        if (dec->frame) {
            lmp_private_free_esco_packet(dec->frame);
            dec->data_len = 0;
            dec->frame = NULL;
        }
    } else {
        if (dec->frame) {
            info->baddr = dec->frame;
            info->seqn = dec->hash;
            info->len = dec->frame_len;
            info->frame_num = 1;
            return;
        }
    }

    if (!dec->frame) {
        info->baddr = (void *)lmp_private_get_esco_packet(&len, &hash);
        if (info->baddr) {
            info->seqn = hash;
            info->frame_num = 1;
            /*printf("frame num : %d\n", info->frame_num);*/
            info->len = len;
            dec->offset = 0;
            dec->frame = info->baddr;
            dec->frame_len = len;
            return;
        } else {
            os_time_dly(2);
        }
    }
}

static int esco_undec_current_packet(struct audio_decoder *decoder)
{
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);

    if (dec->frame) {
        lmp_private_free_esco_packet(dec->frame);
        dec->data_len = 0;
        dec->frame = NULL;
        dec->frame_get = 0;
    }

    return 0;
}

static int esco_dec_stop_and_restart(struct audio_decoder *decoder)
{
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);

    if (audio_sync) {
        dec->frame_get = 0;
        audio_mixer_ch_reset(&dec->mix_ch);
        audio_wireless_sync_stop(audio_sync);
        app_audio_output_reset(300);
    }
    return 0;
}
#endif /*AUDIO_CODEC_SUPPORT_SYNC*/

static int esco_find_packet(struct audio_packet_buffer *pkt)
{
    u32 hash = 0xffffffff;
    int read_len = 0;
    pkt->baddr = (u32)lmp_private_get_esco_packet(&read_len, &hash);
    pkt->slot_time = hash;
    /* printf("hash0=%d,%d ",hash,pkt->baddr ); */
    if (pkt->baddr && read_len) {
        return 0;
    }
    if (read_len < 0) {

        return -ENOMEM;
    }
    return ENOMEM;
}

#if AUDIO_CODEC_SUPPORT_SYNC
static int esco_dec_rx_delay_monitor(struct audio_decoder *decoder, struct rt_stream_info *info)
{
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);

    if (!dec->sync_step) {
        if (info->data_len <= 0) {
            return -EAGAIN;
        }
#if TCFG_USER_TWS_ENABLE
        int state = tws_api_get_tws_state();
        if (state & TWS_STA_SIBLING_CONNECTED) {
            if (tws_api_get_role() == TWS_ROLE_SLAVE) {
                /*app_audio_output_reset(150);*/
            }
        }
#endif
        dec->sync_step = 2;
    }

    info->rx_delay = RX_DELAY_NULL;
    if (info->data_len <= 120) {
        info->rx_delay = RX_DELAY_DOWN;
    } else if (info->data_len > 240) {
        info->rx_delay = RX_DELAY_UP;
    }

    if (info->remain_len < 256) {
        info->rx_delay = RX_DELAY_UP;
    }

    /*printf("%d - %d\n", info->data_len, info->remain_len);*/
    return 0;
}
#endif /*AUDIO_CODEC_SUPPORT_SYNC*/

static int esco_dec_probe_handler(struct audio_decoder *decoder)
{
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);
    int err = 0;
    int hash = 0, len = 0;
    struct audio_packet_buffer undec_pkt;
#if BT_DEC_SYNC_EN
    if (!dec->sync_start) {
        if (lmp_private_get_esco_data_len() < dec->begin_size) {
            audio_decoder_suspend(decoder, 0);
            return -EAGAIN;
        }
        dec->sync_start = 1;
        audio_mixer_ch_pause(&dec->mix_ch, 0);
    }
#endif /*BT_DEC_SYNC_EN*/

    int find_packet = esco_find_packet(&undec_pkt);
    if (find_packet < 0) {
        /* os_time_dly(2); */
        audio_decoder_suspend(&dec->decoder, 0);
        return -EAGAIN;
    } else if (find_packet) {
        return EINVAL;
    }
    if (!dec->enc_start) {
        audio_decoder_suspend(decoder, 0);
        return -EAGAIN;
    }

#if BT_DEC_SYNC_EN
    if (find_packet != 0) {
        u32 cur_tmr = timer_get_ms();
        u32 offset = cur_tmr - dec->read_time;
        if (cur_tmr < dec->read_time) {
            offset = ((u32) - 1) - dec->read_time + cur_tmr;
        }
        if (offset >= 15) {
            dec->sync_start = 0;
            audio_mixer_ch_pause(&dec->mix_ch, 1);
            r_printf("esco to \n");
        }
        audio_decoder_suspend(decoder, 0);
        return -EAGAIN;
    }
    dec->read_time = timer_get_ms();
    esco_audio_stream_sync(dec, lmp_private_get_esco_data_len());
#endif /*BT_DEC_SYNC_EN*/

#if AUDIO_CODEC_SUPPORT_SYNC
    if (audio_sync) {
        struct rt_stream_info rts_info = {0};
        esco_dump_rt_stream_info(decoder, &rts_info);
        err = esco_dec_rx_delay_monitor(decoder, &rts_info);
        if (err) {
            audio_decoder_suspend(decoder, 0);
            return -EAGAIN;
        }

        err = audio_wireless_sync_probe(audio_sync, &rts_info);
        if (err) {
            if (err == SYNC_ERR_STREAM_RESET) {
                esco_dec_stop_and_restart(decoder);
            }
            return -EAGAIN;
        }
    }
#endif/*AUDIO_CODEC_SUPPORT_SYNC*/
    return err;
}

#if TCFG_PHONE_EQ_ENABLE
extern const int phone_eq_filt_16000[];
extern const int phone_eq_filt_08000[];
static int eq_phone_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    int *coeff = NULL;
    if (sr == 16000) {
        coeff = (void *)phone_eq_filt_16000;
    } else if (sr == 8000) {
        coeff = (void *)phone_eq_filt_08000;
    } else {
        return -1;
    }
    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = 0;
    info->nsection = 3;
    /* info->soft_sec = __this->soft_sec; */
    return 0;
}

static int esco_eq_output(void *priv, void *data, u32 len)
{
    return len;
}
#endif/*TCFG_PHONE_EQ_ENABLE*/

#if TCFG_ESCO_PLC
void esco_plc_run(s16 *data, u16 len, u8 repair)
{
    u16 repair_point, tmp_point;
    s16 *p_in, *p_out;
    p_in    = data;
    p_out   = data;
    tmp_point = len / 2;

#if 0	//debug
    static u16 repair_cnt = 0;
    if (repair) {
        repair_cnt++;
        y_printf("[E%d]", repair_cnt);
    } else {
        repair_cnt = 0;
    }
    //printf("[%d]",point);
#endif/*debug*/

    while (tmp_point) {
        repair_point = (tmp_point > PLC_FRAME_LEN) ? PLC_FRAME_LEN : tmp_point;
        tmp_point = tmp_point - repair_point;
        PLC_run(p_in, p_out, repair_point, repair);
        p_in  += repair_point;
        p_out += repair_point;
    }
}
#endif


static inline void audio_pcm_mono_to_four(s16 *four_pcm, s16 *mono_pcm, int points)
{
    s16 *mono = mono_pcm;
    int i = 0;
    u8 j = 0;

    for (i = 0; i < points; i++, mono++) {
        *four_pcm++ = *mono;
        *four_pcm++ = *mono;
        *four_pcm++ = *mono;
        *four_pcm++ = *mono;
    }
}
#define MONO_TO_DUAL_POINTS 30

#if ESCO_SRC_EN
static int esco_src_output_handler(void *priv, void *buf, int len)
{
    int wlen = 0;
    int rlen = len;
    s16 *data = buf;
    struct esco_dec_hdl *dec = (struct esco_dec_hdl *)priv;
    if (!dec || !dec->start) {
        return 0;
    }

    /* memset(buf, 0, len); */

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
    s16 xch_data[MONO_TO_DUAL_POINTS * 2];
#else
    s16 xch_data[MONO_TO_DUAL_POINTS * 4];
#endif
#else
    s16 xch_data[MONO_TO_DUAL_POINTS * 2];
#endif
    s16 point_num = 0;
    u8 mono_to_xchannel = 0;//单声道转换成x声道
    s16 *mono_data = (s16 *)data;
    u16 remain_points = (len >> 1);
    u16 convert_len = 0;

    /*
     *如果dac输出非双声道，因为esco解码出来时单声道
     *所以这里要根据dac通道确定是否做声道转换
     */
#if 0
    if (audio_output_channel_num() == 2) {
        mono_to_xchannel = 2;
    }
    if (audio_output_channel_num() == 4) {
        mono_to_xchannel = 4;
    }
#else
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
    if (audio_output_channel_num() == 2) {
        mono_to_xchannel = 2;
    }
#else
    mono_to_xchannel = 4;
#endif
#else
    if (audio_output_channel_num() == 2) {
        mono_to_xchannel = 2;
    }
#endif
#endif

    if (mono_to_xchannel) {
        do {
            point_num = MONO_TO_DUAL_POINTS;
            if (point_num >= remain_points) {
                point_num = remain_points;
            }
            convert_len = point_num * mono_to_xchannel * 2;
            if (mono_to_xchannel == 4) {
                audio_pcm_mono_to_four(xch_data, mono_data, point_num);
            } else {
                audio_pcm_mono_to_dual(xch_data, mono_data, point_num);
            }
            int tmp_len = audio_mixer_ch_write(&dec->mix_ch, xch_data, convert_len);
            wlen += tmp_len;
            remain_points -= (tmp_len / 2 / mono_to_xchannel);
            //printf("<len:%d,convert_len:%d,tmp_len:%d,wlen:%d,remain_points:%d>",len,convert_len,tmp_len,wlen,remain_points);
            if (tmp_len < convert_len) {
                break;
            }
            mono_data += point_num;
        } while (remain_points);
        if (mono_to_xchannel == 4) {
            wlen = wlen >> 1;
        }
    } else {
        wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
    }

    return mono_to_xchannel ? (wlen >> 1) : wlen;
}
#endif

/* int sosmem[2]; */
static int esco_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    int wlen = 0;
    int wsync_len = 0;
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);

    if (dec->remain == 0) {
        if (audio_sync) {
#if ((AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC) && (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR))
#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
            audio_wireless_sync_after_dec(audio_sync, data, len * audio_output_channel_num());
#else
            audio_wireless_sync_after_dec(audio_sync, data, len * 4);
#endif
#else
            audio_wireless_sync_after_dec(audio_sync, data, len * audio_output_channel_num());
#endif
        }

        if (dec->tws_mute_en) {
            memset(data, 0, len);
        }

#if TCFG_ESCO_PLC
        if (esco_plc && priv) {
            esco_plc_run(data, len, *(u8 *)priv);
        }
#endif/*TCFG_ESCO_PLC*/

#if TCFG_PHONE_EQ_ENABLE
        if (esco_eq) {
            audio_eq_run(esco_eq, data, len);
        }
#endif

#if TCFG_CALL_USE_DIGITAL_VOLUME
        audio_digital_vol_run(data, len);
#endif/*TCFG_CALL_USE_DIGITAL_VOLUME*/

#if TCFG_ESCO_LIMITER
        limiter_noiseGate_run(limiter_noiseGate_buf, data, data, len / 2);
#endif/*TCFG_ESCO_LIMITER*/

#if ((AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM) && (FM_AEC_REF_TYPE == 0))
        wsync_len = local_fm_sync_by_emitter(fm_sync, data, len, fm_emitter_cbuf_data_len());
        audio_aec_refbuf(local_fm_sync_output_addr(fm_sync), wsync_len);
        audio_pcm_mono_to_dual(dec->ext_buf, local_fm_sync_output_addr(fm_sync), wsync_len >> 1);
        dec->ext_buf_offset = 0;
        dec->ext_buf_remain = wsync_len;

#endif

#if ((defined(DEC_OUT_OTHER_DATA) && (DEC_OUT_OTHER_DATA)))
        other_audio_dec_output(decoder, data, len, 1, dec->decoder.fmt.sample_rate);
#endif

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
        if (dec->user_hdl) {
            u8 ch_num = 1;
            user_audio_process_handler_run(dec->user_hdl, data, len, ch_num);
        }
#endif


    }

#if ((AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM) && (FM_AEC_REF_TYPE == 0))
    /*
    u16 *tbuf2k = dec->sw_src_buf2k;
    int rdlen = 0;
    if (sw_src_api) {
        rdlen = sw_src_api->run(sw_src_buf, data, len >> 1,  tbuf2k);
        int i = 0;
        for (i = rdlen; i >= 0; i--) {
            tbuf2k[2 * i] = tbuf2k[i];
            tbuf2k[2 * i + 1] = tbuf2k[i];
        }
    }
    wlen = audio_output_data(tbuf2k, rdlen << 2);
    */
    wlen = audio_mixer_ch_write(&dec->mix_ch, &dec->ext_buf[dec->ext_buf_offset], dec->ext_buf_remain << 1/* len */);
    dec->ext_buf_offset += (wlen >> 1);
    dec->ext_buf_remain -= (wlen >> 1);
    if (dec->ext_buf_remain) {
        dec->remain = 1;
    } else {
        dec->remain = 0;
        return len;
    }

    return wlen >> 1;
#endif

#if ESCO_SRC_EN

    char err = 0;
    int rlen = len;

    do {
        if (dec->hw_src) {
            wlen = audio_src_resample_write(dec->hw_src, data, rlen);
        } else {
            /* wlen = audio_mixer_ch_write(&dec->mix_ch, data, rlen); */
            wlen = esco_src_output_handler(dec, data, rlen);
        }
        if (!wlen) {
            //src库有修改，此处重试用法可去掉
            /* err++; */
            /* if (err < 4) { */
            /* continue; */
            /* } */
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

#else /* ESCO_SRC_EN */

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)

#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
    s16 xch_data[MONO_TO_DUAL_POINTS * 2];
#else
    s16 xch_data[MONO_TO_DUAL_POINTS * 4];
#endif
#else
    s16 xch_data[MONO_TO_DUAL_POINTS * 2];
#endif
    s16 point_num = 0;
    u8 mono_to_xchannel = 0;//单声道转换成x声道
    s16 *mono_data = (s16 *)data;
    u16 remain_points = (len >> 1);
    u16 convert_len = 0;

    /*
     *如果dac输出非双声道，因为esco解码出来时单声道
     *所以这里要根据dac通道确定是否做声道转换
     */
#if 0
    if (audio_output_channel_num() == 2) {
        mono_to_xchannel = 2;
    }
    if (audio_output_channel_num() == 4) {
        mono_to_xchannel = 4;
    }
#else
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)

#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
    if (audio_output_channel_num() == 2) {
        mono_to_xchannel = 2;
    }

#else
    mono_to_xchannel = 4;
#endif
#else
    if (audio_output_channel_num() == 2) {
        mono_to_xchannel = 2;
    }
#endif
#endif

    if (mono_to_xchannel) {
        do {
            point_num = MONO_TO_DUAL_POINTS;
            if (point_num >= remain_points) {
                point_num = remain_points;
            }
            convert_len = point_num * mono_to_xchannel * 2;
            if (mono_to_xchannel == 4) {
                audio_pcm_mono_to_four(xch_data, mono_data, point_num);
            } else {
                audio_pcm_mono_to_dual(xch_data, mono_data, point_num);
            }
            int tmp_len = audio_mixer_ch_write(&dec->mix_ch, xch_data, convert_len);
            wlen += tmp_len;
            remain_points -= (tmp_len / 2 / mono_to_xchannel);
            //printf("<len:%d,convert_len:%d,tmp_len:%d,wlen:%d,remain_points:%d>",len,convert_len,tmp_len,wlen,remain_points);
            if (tmp_len < convert_len) {
                break;
            }
            mono_data += point_num;
        } while (remain_points);
        if (mono_to_xchannel == 4) {
            wlen = wlen >> 1;
        }
    } else {
        wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
    }

    if (len != (mono_to_xchannel ? (wlen >> 1) : wlen)) {
        dec->remain = 1;
    } else {
        dec->remain = 0;
    }
    return mono_to_xchannel ? (wlen >> 1) : wlen;
#endif /* ESCO_SRC_EN */
}

static int esco_dec_post_handler(struct audio_decoder *decoder)
{
    int limit_dec[1];
    limit_dec[0] = (int)decoder;
    audio_decoder_resume_off_limits(&decode_task, 1, limit_dec);
    return 0;
}

static int esco_dec_stop_handler(struct audio_decoder *decoder)
{
#if AUDIO_CODEC_SUPPORT_SYNC
    audio_sync_close();
#endif
    return 0;
}

static const struct audio_dec_handler esco_dec_handler = {
    .dec_probe  = esco_dec_probe_handler,
    .dec_output = esco_dec_output_handler,
    .dec_post   = esco_dec_post_handler,
    .dec_stop   = esco_dec_stop_handler,
};

void esco_dec_release()
{
    audio_decoder_task_del_wait(&decode_task, &esco_dec->wait);

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    if (esco_dec->sw_src_buf2k) {
        free(esco_dec->sw_src_buf2k);
        esco_dec->sw_src_buf2k = NULL;
    }
    if (sw_src_api) {
        sw_src_api = NULL;
    }
    if (sw_src_buf) {
        free(sw_src_buf);
        sw_src_buf = NULL;
    }
    if (esco_dec->ext_buf) {
        free(esco_dec->ext_buf);
        esco_dec->ext_buf = NULL;
    }
#endif

    if (esco_dec->coding_type == AUDIO_CODING_MSBC) {
        clock_remove(DEC_MSBC_CLK);
    } else if (esco_dec->coding_type == AUDIO_CODING_CVSD) {
        clock_remove(DEC_CVSD_CLK);
    }

    free(esco_dec);
    esco_dec = NULL;
}

void esco_dec_close();

static void esco_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        puts("AUDIO_DEC_EVENT_END\n");
        esco_dec_close();
#if 0//(defined(TCFG_IIS_ENABLE) && (TCFG_IIS_ENABLE)&&(TCFG_IIS_OUTPUT_EN))
        audio_link_switch_sr(TCFG_IIS_OUTPUT_PORT, TCFG_IIS_OUTPUT_SR);
#endif
        break;
    }
}

static int __esco_audio_res_close(void)
{
    /*
     *先关闭aec，里面有复用到enc的buff，再关闭enc，
     *如果没有buf复用，则没有先后顺序要求。
     */
    if (!esco_dec->start) {
        return 0;
    }
    set_source_sample_rate(0);
    esco_dec->start = 0;
    esco_dec->enc_start = 0;
    audio_mixer_ch_pause(&esco_dec->mix_ch, 1);
    audio_aec_close();
#if ESCO_SRC_EN
    audio_resume_all_decoder_run(2, 2, 2);
#endif
    esco_enc_close();
    audio_decoder_close(&esco_dec->decoder);
#if AUDIO_CODEC_SUPPORT_SYNC
    audio_sync_close();
#if ((AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM) && (FM_AEC_REF_TYPE == 0))
    local_fm_sync_close(fm_sync);
#endif
    esco_dec->sync_step = 0;
    esco_dec->preempt_state = DEC_PREEMTED_BY_PRIORITY;
#endif
    audio_mixer_ch_close(&esco_dec->mix_ch);
#if ESCO_SRC_EN
    audio_resume_all_mix();
    if (esco_dec->hw_src) {
        audio_hw_resample_close(esco_dec->hw_src);
        esco_dec->hw_src = NULL;
    }
#endif
#if TCFG_PHONE_EQ_ENABLE
    if (esco_eq) {
        audio_eq_close(esco_eq);
        free(esco_eq);
        esco_eq = NULL;
    }
#endif

#if TCFG_ESCO_PLC
    if (esco_plc) {
        free(esco_plc);
        //free_mux(esco_plc);
        esco_plc = NULL;
    }
#endif/*TCFG_ESCO_PLC*/

#if TCFG_ESCO_LIMITER
    if (limiter_noiseGate_buf) {
        free(limiter_noiseGate_buf);
        limiter_noiseGate_buf = NULL;
    }
#endif /*TCFG_ESCO_LIMITER*/

    /*恢复mix_buf的长度*/
#if (ESCO_SRC_EN == 0)
    audio_mixer_set_output_buf(&mixer, mix_buff, sizeof(mix_buff));
#endif
    app_audio_state_exit(APP_AUDIO_STATE_CALL);
    esco_dec->start = 0;

    return 0;
}
u16 source_sr;
void set_source_sample_rate(u16 sample_rate)
{
    source_sr = sample_rate;
}

u16 get_source_sample_rate()
{
    if (bt_audio_is_running()) {
        return source_sr;
    }
    return 0;
}

int esco_dec_start()
{
    int err;
    struct audio_fmt f;
    struct esco_dec_hdl *dec = esco_dec;

    if (!esco_dec) {
        return -EINVAL;
    }

    err = audio_decoder_open(&dec->decoder, &esco_input, &decode_task);
    if (err) {
        goto __err1;
    }

    audio_decoder_set_handler(&dec->decoder, &esco_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, esco_dec_event_handler, 0);

    if (dec->coding_type == AUDIO_CODING_MSBC) {
        f.coding_type = AUDIO_CODING_MSBC;
        f.sample_rate = 16000;
        f.channel = 1;
    } else if (dec->coding_type == AUDIO_CODING_CVSD) {
        f.coding_type = AUDIO_CODING_CVSD;
        f.sample_rate = 8000;
        f.channel = 1;
    }
#if (ESCO_SRC_EN == 0)
    set_source_sample_rate(f.sample_rate);
#endif
    err = audio_decoder_set_fmt(&dec->decoder, &f);
    if (err) {
        goto __err2;
    }
    dec->sample_rate = f.sample_rate;

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
    app_audio_output_mode_set(TCFG_AUDIO_DAC_CONNECT_MODE);
    if (app_audio_output_mode_get() == TCFG_AUDIO_DAC_CONNECT_MODE) {
        app_audio_output_channel_set(4);
    }
#endif
    /*
     *(1)esco_dec输出是120或者240，所以通话的时候，修改mix_buff的长度，提高效率
     *(2)其他大部分时候解码输出是512的倍数，通话结束，恢复mix_buff的长度，提高效率
     */
#if (ESCO_SRC_EN == 0)
    audio_mixer_set_output_buf(&mixer, mix_buff, sizeof(mix_buff) / 240 * 240);
#endif
    audio_mixer_ch_open(&dec->mix_ch, &mixer);

#if (defined (TCFG_MIXER_EXT_ENABLE) && (TCFG_MIXER_EXT_ENABLE))
    audio_mixer_ch_set_ext_out_mask(&dec->mix_ch, BIT(0));
#endif//TCFG_MIXER_EXT_ENABLE

#if ((defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE)) && AUDIO_CODEC_SUPPORT_SYNC)
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, app_audio_output_samplerate_select(f.sample_rate, 1));
#else
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, audio_output_rate(f.sample_rate));
#endif
    printf("esco s_rate%d,out rate%d \n ", f.sample_rate, audio_output_rate(f.sample_rate));
    audio_output_set_start_volume(APP_AUDIO_STATE_CALL);

    printf("max_vol:%d,call_vol:%d", app_var.aec_dac_gain, app_audio_get_volume(APP_AUDIO_STATE_CALL));
#if TCFG_PHONE_EQ_ENABLE
    esco_eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (esco_eq) {
        esco_eq->eq_ch = (struct hw_eq_ch *)((int)esco_eq + sizeof(struct audio_eq));
        struct audio_eq_param esco_eq_param = {0};
        esco_eq_param.channels = f.channel;
        esco_eq_param.online_en = 1; // 支持在线调试
        esco_eq_param.mode_en = 0;
        esco_eq_param.remain_en = 0;
        esco_eq_param.max_nsection = 3;
        esco_eq_param.cb = eq_phone_get_filter_info;
        audio_eq_open(esco_eq, &esco_eq_param);
        audio_eq_set_samplerate(esco_eq, f.sample_rate);
        audio_eq_set_output_handle(esco_eq, esco_eq_output, dec);
        audio_eq_start(esco_eq);
    }
#endif

#if TCFG_ESCO_PLC
    esco_plc = malloc(PLC_query()); /*buf_size:1040*/
    //esco_plc = zalloc_mux(PLC_query());
    printf("PLC_buf:%x,size:%d\n", esco_plc, PLC_query());
    if (esco_plc) {
        err = PLC_init(esco_plc);
        if (err) {
            printf("PLC_init err:%d", err);
            free(esco_plc);
            esco_plc = NULL;
        }
    }
#endif

#if TCFG_ESCO_LIMITER
    limiter_noiseGate_buf = malloc(need_limiter_noiseGate_buf(1));
    printf("Limiter_noisegate_buf size:%d\n", need_limiter_noiseGate_buf(1));
    if (limiter_noiseGate_buf) {
        //限幅器启动因子 int32(exp(-0.65/(16000 * 0.005))*2^30)   16000为采样率  0.005 为启动时间(s)
        int limiter_attfactor = 1065053018;
        //限幅器释放因子 int32(exp(-0.15/(16000 * 0.1))*2^30)     16000为采样率  0.1   为释放时间(s)
        int limiter_relfactor = 1073641165;
        //限幅器阈值(mdb)
        //int limiter_threshold = CONST_LIMITER_THR;

        //噪声门限启动因子 int32(exp(-1/(16000 * 0.1))*2^30)       16000为采样率  0.1   为释放时间(s)
        int noiseGate_attfactor = 1073070945;
        //噪声门限释放因子 int32(exp(-1/(16000 * 0.005))*2^30)     16000为采样率  0.005 为启动时间(s)
        int noiseGate_relfactor = 1060403589;
        //噪声门限(mdb)
        //int noiseGate_threshold = -25000;
        //低于噪声门限阈值的增益 (0~1)*2^30
        //int noise
        //Gate_low_thr_gain = 0 << 30;

        if (dec->sample_rate == 8000) {
            limiter_attfactor = 1056434522;
            limiter_relfactor =  1073540516;
            noiseGate_attfactor = 1072400485;
            noiseGate_relfactor =  1047231044;
        }

        limiter_noiseGate_init(limiter_noiseGate_buf,
                               limiter_attfactor,
                               limiter_relfactor,
                               noiseGate_attfactor,
                               noiseGate_relfactor,
                               LIMITER_THR,
                               LIMITER_NOISE_GATE,
                               LIMITER_NOISE_GAIN,
                               dec->sample_rate, 1);
    }
#endif /* TCFG_ESCO_LIMITER */

#if ESCO_SRC_EN
    dec->src_out_sr = audio_output_rate(f.sample_rate);
    printf("f.sample_rate:%d dec->src_out_sr:%d \n\n", f.sample_rate, dec->src_out_sr);
#if BT_DEC_SYNC_EN
    esco_audio_sync_init(dec);
#else
    if (f.sample_rate != dec->src_out_sr) {
        dec->hw_src = audio_hw_resample_open(dec, esco_src_output_handler, f.channel, f.sample_rate, dec->src_out_sr);;
    }
#endif
#endif

    lmp_private_esco_suspend_resume(2);
    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err3;
    }

    dec->start = 1;
    dec->frame_get = 0;

    err = audio_aec_init(f.sample_rate);
    if (err) {
        printf("audio_aec_init failed:%d", err);
        //goto __err3;
    }
    err = esco_enc_open(dec->coding_type, dec->esco_len);
    if (err) {
        printf("audio_enc_open failed:%d", err);
        //goto __err3;
    }
    dec->enc_start = 1;
#if AUDIO_CODEC_SUPPORT_SYNC
#if ((AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM) && (FM_AEC_REF_TYPE == 0))
    fm_sync = local_fm_sync_open(f.sample_rate, f.sample_rate, f.channel, fm_emitter_cbuf_len());
#endif
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
    audio_sync = audio_sync_esco_open(dec, f.sample_rate, app_audio_output_samplerate_select(f.sample_rate, 1));
#else

#if ESCO_SRC_EN
    audio_sync = audio_sync_esco_open(dec, dec->src_out_sr, dec->src_out_sr);
#else
    audio_sync = audio_sync_esco_open(dec, f.sample_rate, audio_output_rate(f.sample_rate));
#endif

#endif
#endif


#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    sw_src_api = get_rs16_context();
    u32 t_buf_size = sw_src_api->need_buf();
    sw_src_buf = malloc(t_buf_size);
    RS_PARA_STRUCT rs_para_obj;
    rs_para_obj.nch = 1;

#if ((AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM) && (FM_AEC_REF_TYPE == 0))
    rs_para_obj.new_insample = (int)(f.sample_rate * 1.566f); //25056;// 1.566*16000;       f.sample_rate;//audio_output_rate(f.sample_rate);
    rs_para_obj.new_outsample = 65250;// 1.566*41666.66..   audio_output_rate(f.sample_rate);//f.sample_rate;
#else
    rs_para_obj.new_outsample = (int)(f.sample_rate * 1.566f); //25056;// 1.566*16000;       f.sample_rate;//audio_output_rate(f.sample_rate);
    rs_para_obj.new_insample = 65250;// 1.566*41666.66..   audio_output_rate(f.sample_rate);//f.sample_rate;
#endif
    printf("sw src,in = %d,out = %d\n", rs_para_obj.new_insample, rs_para_obj.new_outsample);
    sw_src_api->open(sw_src_buf, &rs_para_obj);
#endif
    clock_set_cur();

    return 0;

__err3:
#if ESCO_SRC_EN
    if (dec->hw_src) {
        audio_hw_resample_close(esco_dec->hw_src);
        dec->hw_src = NULL;
    }
#endif
    audio_mixer_ch_close(&dec->mix_ch);
__err2:
    set_source_sample_rate(0);
    audio_decoder_close(&dec->decoder);
__err1:
    esco_dec_release();
    return err;
}

static int esco_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    log_d("esco_wait_res_handler %d\n", event);
#if (defined(AUDIO_OUTPUT_AUTOMUTE) && (AUDIO_OUTPUT_AUTOMUTE == ENABLE))
    u8 i = 0;
    u8 all_ch = 0;
    for (i = 0; i < audio_output_channel_num(); i++) {
        all_ch |= BIT(i);
    }
#endif
    if (event == AUDIO_RES_GET) {
#if (defined(AUDIO_OUTPUT_AUTOMUTE) && (AUDIO_OUTPUT_AUTOMUTE == ENABLE))
        audio_automute_skip(automute, 1);
        app_audio_output_ch_mute(all_ch, 0);
#endif
        err = esco_dec_start();
    } else if (event == AUDIO_RES_PUT) {
#if (defined(AUDIO_OUTPUT_AUTOMUTE) && (AUDIO_OUTPUT_AUTOMUTE == ENABLE))
        audio_automute_skip(automute, 0);
        app_audio_output_ch_mute(all_ch, 1);
#endif
        if (esco_dec->start) {
            lmp_private_esco_suspend_resume(1);
            __esco_audio_res_close();
        }
    }

    return err;
}

int esco_dec_open(void *param, u8 mute)
{
    int err;
    struct esco_dec_hdl *dec;
    u32 esco_param = *(u32 *)param;
    int esco_len = esco_param >> 16;
    int codec_type = esco_param & 0x000000ff;

    printf("esco_dec_open, type=%d,len=%d\n", codec_type, esco_len);

    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
#if FM_AEC_REF_TYPE == 1
    dec->sw_src_buf2k = zalloc(1024 + 512);
    if (!dec->sw_src_buf2k) {
        printf("sw_src_buf2k malloc fail\n");
        return -ENOMEM;
    }
#else
    dec->ext_buf = malloc(512);
    if (!dec->ext_buf) {
        return -ENOMEM;
    }
#endif
#endif

    esco_dec = dec;

    dec->esco_len = esco_len;
    if (codec_type == 3) {
        dec->coding_type = AUDIO_CODING_MSBC;
        clock_add(DEC_MSBC_CLK);
    } else if (codec_type == 2) {
        dec->coding_type = AUDIO_CODING_CVSD;
        clock_add(DEC_CVSD_CLK);
    }

    dec->tws_mute_en = mute;

    dec->wait.priority = 2;
#if (defined(DEC_MIX_ENABLE) && DEC_MIX_ENABLE)
    dec->wait.preemption = 0;
    dec->wait.protect = 1;
#else
    dec->wait.preemption = 1;
#endif//DEC_MIX_ENABLE

    dec->wait.handler = esco_wait_res_handler;
    err = audio_decoder_task_add_wait(&decode_task, &dec->wait);
    if (esco_dec->start == 0) {
        lmp_private_esco_suspend_resume(1);
    }

    return err;
}


void esco_dec_close()
{
    if (!esco_dec) {
        return;
    }

    __esco_audio_res_close();


    esco_dec_release();
    puts("esco_dec_close: exit\n");
    clock_set_cur();
}


//////////////////////////////////////////////////////////////////////////////
u8 bt_audio_is_running(void)
{
    return (a2dp_dec || esco_dec);
}
u8 bt_media_is_running(void)
{
    return a2dp_dec != NULL;
}
u8 bt_phone_dec_is_running()
{
    return esco_dec != NULL;
}

extern s16 read_capless_DTB(void);

//////////////////////////////////////////////////////////////////////////////
const int config_sbc_modules              = 1;
#ifdef CONFIG_CPU_BR23
const int config_sbchw_modules            = 1;
#else
const int config_sbchw_modules            = 0;
#endif
const int config_msbc_modules             = 1;
const int config_pcm_modules              = 1;
const int config_mty_modules              = 0;
const int config_cvsd_modules             = 1;

#if (defined(TCFG_DEC_G729_ENABLE) && (TCFG_DEC_G729_ENABLE))
const int config_g729_modules             = 1;
#else
const int config_g729_modules             = 0;
#endif

#if (defined(TCFG_DEC_WAV_ENABLE) && (TCFG_DEC_WAV_ENABLE))
const int config_wav_modules              = 1;
#else
const int config_wav_modules              = 0;
#endif

#if (defined(TCFG_DEC_WMA_ENABLE) && (TCFG_DEC_WMA_ENABLE))
const int config_wma_modules              = 1;
#else
const int config_wma_modules              = 0;
#endif

#if ((defined(TCFG_DEC_WMA_ENABLE) && (TCFG_DEC_WMA_ENABLE)) && TCFG_DEC2TWS_ENABLE)
const int config_wmapick_modules          = 1;
#else
const int config_wmapick_modules          = 0;
#endif


#if (defined(TCFG_DEC_MP3_ENABLE) && (TCFG_DEC_MP3_ENABLE))
const int config_mp3_modules              = 1;
#else
const int config_mp3_modules              = 0;
#endif

#if ((defined(TCFG_DEC_MP3_ENABLE) && (TCFG_DEC_MP3_ENABLE)) && TCFG_DEC2TWS_ENABLE)
const int config_mp3pick_modules          = 1;
#else
const int config_mp3pick_modules          = 0;
#endif


#if (defined(TCFG_DEC_M4A_ENABLE) && (TCFG_DEC_M4A_ENABLE))
const int config_m4a_modules              = 1;
#else
const int config_m4a_modules              = 0;
#endif

#if TCFG_BT_SUPPORT_AAC
const int config_aac_modules              = 1;
const int config_aachw_modules            = 1;
#else
const int config_aac_modules              = 0;
const int config_aachw_modules            = 0;
#endif

#if (defined(TCFG_DEC_AMR_ENABLE) && (TCFG_DEC_AMR_ENABLE))
const int config_amr_modules              = 1;
#else
const int config_amr_modules              = 0;
#endif

#if (defined(TCFG_DEC_APE_ENABLE) && (TCFG_DEC_APE_ENABLE))
const int config_ape_modules              = 1;
#else
const int config_ape_modules              = 0;
#endif


#if (defined(TCFG_DEC_DTS_ENABLE) && (TCFG_DEC_DTS_ENABLE))
const int config_dts_modules              = 1;
#else
const int config_dts_modules              = 0;
#endif

#if (defined(TCFG_DEC_FLAC_ENABLE) && (TCFG_DEC_FLAC_ENABLE))
const int config_flac_modules             = 1;
#else
const int config_flac_modules             = 0;
#endif

#if (defined(TCFG_DEC_G726_ENABLE) && (TCFG_DEC_G726_ENABLE))
const int config_g726_modules              = 1;
#else
const int config_g726_modules              = 0;
#endif

#if (defined(TCFG_DEC_MIDI_ENABLE) && (TCFG_DEC_MIDI_ENABLE))
const int config_midi_modules              = 1;
#else
const int config_midi_modules              = 0;
#endif




#if (defined(AUDIO_OUTPUT_AUTOMUTE) && (AUDIO_OUTPUT_AUTOMUTE == ENABLE))
/*
 *automute计算相关信息dump,debug使用
 */
#if 0
void audio_automute_info(u8 ch, int energy, u32 mute_count, u32 unmute_count)
{
    printf(" [ch%d:%d-%d-%d] ", ch, energy, mute_count, unmute_count);
}
#endif

void audio_automute_event_handler(u8 event, u8 channel)
{

    u8 i = 0;
    u8 all_ch = 0;
    for (i = 0; i < audio_output_channel_num(); i++) {
        all_ch |= BIT(i);
    }

    switch (event) {
    case AUDIO_EVENT_AUTO_MUTE:
        if (channel == automute->channels) {
            automute->mute_channel |= all_ch;
        } else {
            if (automute->mute_channel & BIT(channel)) {
            } else {
                printf(">>>>>>>>>>>>>>>>>>>> Mute %d\n", channel);
                app_audio_output_ch_mute(BIT(channel), 1);
            }
            automute->mute_channel |= BIT(channel);
        }
        //y_printf("audio auto_mute:%d-%d\n", channel, automute->mute_channel);
        break;
    case AUDIO_EVENT_AUTO_UNMUTE:
        if (channel == automute->channels) {
            automute->mute_channel = 0;
        } else {
            if (automute->mute_channel & BIT(channel)) {
                printf(">>>>>>>>>>>>>>>>>>>> unMute %d\n", channel);
                app_audio_output_ch_mute(BIT(channel), 0);
            } else {
            }
            automute->mute_channel &= ~BIT(channel);
        }
        //y_printf("audio auto_unmute:%d-%d\n", channel, automute->mute_channel);
        break;
    }

    if (automute->mute_channel == all_ch) {
        if (!automute->mute) {
            app_audio_output_ch_mute(all_ch, 1);
            printf(">>>>>>>>>>>>>>>>>>>>>>>>>>> Mute\n");
        }
        automute->mute = 1;
    } else if (automute->mute_channel == 0) {
        if (automute->mute) {
            app_audio_output_ch_mute(all_ch, 0);
            printf(">>>>>>>>>>>>>>>>>>>>>>>>>>> uMute\n");
        }
        automute->mute = 0;
    }
}


/***********************************************************************
 *	                    automute 参数说明
 *
 *  mute_energy :计算能量阈值，每次计算能量低于这个值则判断为需要 mute
 *  unmute_energy :计算能量阈值，每次计算能量高于这个值则判断为需要 unmute
 *  filt_points	:每次计算多少个采样点
 *  filt_mute_number   :连续多少采样点计算能量均低于 mute_energy 开始 mute
 *  filt_unmute_number :连续多少采样点计算能量均高于 unmute_energy 开始 unmute
 *                      number = time(s) * samplerate * channals * 2
 *  channels	:通道数
 *  handler		:回调函数，第一个参数表示 mute 或者 unmute；第二个参数
 				 如果等于 channels 则表示全部通道，如果小于 channels
 				 则表示对应的通道号。
 *************************** -HB ****************************************/
void audio_dec_automute_start(void)
{
    if (automute != NULL) {
        printf("automute is aleady start!\n");
        return;
    }
    clock_add(AUTOMUTE_CLK);

    automute = zalloc(sizeof(*automute));
    if (automute) {
        automute->mute_energy = 10;
        automute->unmute_energy = 15;
        automute->filt_points = 100;
        automute->filt_mute_number   = 40000;
        automute->filt_unmute_number = 2000;

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
        automute->channels =  4;
#else
        automute->channels =  audio_output_channel_num();
#endif

        automute->handler = audio_automute_event_handler;
        audio_automute_open(automute);
    } else {
        printf("automute zalloc fail!\n");
    }
}

void audio_dec_automute_stop(void)
{
    if (automute) {
        audio_automute_close(automute);
        free(automute);
        automute = NULL;
    }
    clock_remove(AUTOMUTE_CLK);
}

#endif


void app_audio_irq_handler_hook(int source)
{
    switch (source) {
    case 0:
        audio_resume_all_decoder();
        break;
    case 1:
        break;
    default:
        break;
    }
}


static u8 audio_dec_inited = 0;
int audio_dec_init()
{
    int err;

    printf("audio_dec_init\n");
    if (config_sbc_modules) {
        sbc_decoder_init();
    }

    if (config_sbchw_modules) {
        platform_device_sbc_init();
    }

    if (config_mty_modules) {
        mty_decoder_init();
    }
    if (config_mp3_modules) {
        mp3_decoder_init();
    }
    if (config_mp3pick_modules) {
        mp3pick_decoder_init();
    }

    if (config_wma_modules) {
        wma_decoder_init();
    }
    if (config_wmapick_modules) {
        wmapick_decoder_init();
    }

    if (config_wav_modules) {
        wav_decoder_init();
    }

    if (config_flac_modules) {
        flac_decoder_init();
    }

    if (config_ape_modules) {
        ape_decoder_init();
    }

    if (config_m4a_modules) {
        m4a_decoder_init();
    }

    if (config_amr_modules) {
        amr_decoder_init();
    }

    if (config_dts_modules) {
        dts_decoder_init();
    }
    if (config_msbc_modules) {
        msbc_decoder_init();
    }
    if (config_cvsd_modules) {
        cvsd_decoder_init();
    }
    if (config_pcm_modules) {
        pcm_decoder_enable();
    }
    if (config_g729_modules) {
        g729_decoder_init();
    }

    if (config_aac_modules) {
        aac_decoder_init();
    }

    if (config_g726_modules) {
        g726_decoder_init();
    }

    if (config_midi_modules) {
        midi_decoder_init();
    }


    err = audio_decoder_task_create(&decode_task, "audio_dec");

#if (defined(TCFG_DEC2TWS_TASK_ENABLE) && (TCFG_DEC2TWS_TASK_ENABLE))
    err = audio_decoder_task_create(&decode_task_local, "local_dec");
#endif

#if (defined(TCFG_SPDIF_ENABLE) && (TCFG_SPDIF_ENABLE))
    extern void spdif_init(void);
    spdif_init();
#endif
    app_audio_output_init();
#if AUDIO_CODEC_SUPPORT_SYNC
    /*音频同步DA端buffer设置*/
    app_audio_output_sync_buff_init(dac_sync_buff, sizeof(dac_sync_buff));
#endif

    /*硬件SRC模块滤波器buffer设置，可根据最大使用数量设置整体buffer*/
    audio_src_base_filt_init(audio_src_hw_filt, sizeof(audio_src_hw_filt));

    clock_add(DEC_MIX_CLK);
    audio_mixer_open(&mixer);
    audio_mixer_set_handler(&mixer, &mix_handler);
    audio_mixer_set_event_handler(&mixer, mixer_event_handler);
    /*初始化mix_buf的长度*/
    audio_mixer_set_output_buf(&mixer, mix_buff, sizeof(mix_buff));

#if (defined (TCFG_MIXER_EXT_ENABLE) && (TCFG_MIXER_EXT_ENABLE))
    int mixer_ext_buf[1];
    mixer_ext_buf[0] = (int)mix_buff_filt;
    audio_mixer_set_ext_output_buf(&mixer, mixer_ext_buf, 1);
#endif//TCFG_MIXER_EXT_ENABLE

    app_audio_volume_init();
    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

#if (defined(AUDIO_OUTPUT_AUTOMUTE) && (AUDIO_OUTPUT_AUTOMUTE == ENABLE))
    audio_dec_automute_start();
#endif

#if (defined(TCFG_IIS_ENABLE) && (TCFG_IIS_ENABLE))
    audio_link_init();
#if AUDIO_OUTPUT_INCLUDE_IIS
    audio_link_open(TCFG_IIS_OUTPUT_PORT, ALINK_DIR_TX);
#endif
#endif
    audio_dec_inited = 1;
    return err;
}

static u8 audio_dec_init_complete()
{
    if (!audio_dec_inited) {
        return 0;
    }

    return 1;
}
REGISTER_LP_TARGET(audio_dec_init_lp_target) = {
    .name = "audio_dec_init",
    .is_idle = audio_dec_init_complete,
};


#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
/* s16 quad_data[DUAL_TO_QUAD_POINTS * 2]; */

/* #ifndef CONFIG_EQ_SUPPORT_ASYNC */
/* s16 dual2_data[DUAL_TO_QUAD_POINTS]; */
/* #endif */


/*
 *
 *四声道中，rl rr合成后输出
 *quad_data: 四声道数据
 *points :四声道点数
 * */
void rl_rr_mix_out(s16 *quad_data, int points)
{
    for (int i = 0; i < (points >> 2); i++) {
        s32 dual_pcm = (quad_data[2 + 4 * i] + quad_data[3 + 4 * i]) >> 1;
        quad_data[2 + 4 * i] = dual_pcm;
        quad_data[3 + 4 * i] = dual_pcm;
    }
}

/*
 *
 *四声道中，fl fr合成后输出
 *quad_data: 四声道数据
 *points :四声道点数
 * */
void fl_fr_mix_out(s16 *quad_data, int points)
{
    for (int i = 0; i < (points >> 2); i++) {
        s32 dual_pcm = (quad_data[0 + 4 * i] + quad_data[1 + 4 * i]) >> 1;
        quad_data[0 + 4 * i] = dual_pcm;
        quad_data[1 + 4 * i] = dual_pcm;
    }
}

/* static inline void audio_pcm_dual_to_quad(s16 *quad_pcm, s16 *dual_pcm, int points, void *dvol_hdl) */
/* { */
/* s16 *dual = dual_pcm; */
/* dual_to_quad_fun(quad_pcm, dual, dual, points, dvol_hdl); */
/* } */

int audio_dual_to_quad_output_src(void *src, struct audio_mixer_ch *ch, s16 *data, int len, u32 id, s16 *quad_data, void *user_hdl)
{
    int wlen = 0;
    s16 *dual_data = (s16 *)data;
    s16 point_num = 0;
    u8 dual_to_quad = 1;
    u16 remain_points = (len >> 1);
    /* s16 *quad_data = (s16 *)malloc(DUAL_TO_QUAD_POINTS * 2 * 2); */
    /* if (!quad_data){ */
    /* log_e("%s malloc err\n", __FUNCTION__); */
    /* return len; */
    /* } */
    do {
        if (dual_to_quad) {
            point_num = DUAL_TO_QUAD_POINTS;
            if (point_num > remain_points) {
                point_num = remain_points;
            }
            if (point_num < 0) {
                ASSERT(0);
            }
            /* audio_pcm_dual_to_quad(quad_data, dual_data, point_num); */
            dual_to_quad_fun(quad_data, dual_data, dual_data, point_num, user_hdl);

            int  tmp_len;
            if (src) {
                tmp_len = audio_src_resample_write(src, quad_data, point_num << 2);
            } else {
                tmp_len = audio_mixer_ch_write(ch, quad_data, point_num << 2);
            }
            wlen += tmp_len;
            remain_points -= (tmp_len >> 2);
            /* if (!tmp_len){ */
            /* break; */
            /* } */
            if (tmp_len < (point_num << 2)) {
                break;
            }
            dual_data += point_num;
        }
    } while (remain_points);
    /* if (quad_data){ */
    /* free(quad_data); */
    /* } */

    return (wlen >> 1);
}


#if TCFG_EQ_ENABLE

u32 audio_pcm_dual_to_quadeq_async(struct audio_eq *_eq[2], struct audio_drc *_drc[2], s16 *quad_pcm, s16 *dual_pcm,  int points)
{
    u32 j = 0;
    int eqlen0 = 0;
    int eqlen1 = 0;
    s16 *dual = dual_pcm;

    int dual_len = points * 2;
    int quad_len = DUAL_TO_QUAD_POINTS * 2 * 2;
    if (dual_len > (quad_len / 2)) {
        ASSERT(0);
    }

    if (_eq[0]) {
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
        s16 *dual_FL_FR = (s16 *)0xaa;
#else
        s16 *dual_FL_FR = (s16 *)NULL;
#endif
        eqlen0 = audio_eq_run_inOut(_eq[0], dual, dual_FL_FR, dual_len);//eq0异步
    }

    return eqlen0;
}
//四路异步输出
int audio_dualeq_to_quadeq_output_async(struct audio_eq *eq[2], struct audio_drc *drc[2], struct audio_mixer_ch *ch, s16 *data, int len)
{
    int wlen = 0;

    s16 *dual_data = (s16 *)data;
    s16 point_num = 0;
    u8 dual_to_quad = 1;
    u16 remain_points = (len >> 1);
    u32 cnt = 0;
    do {
        if (dual_to_quad) {
            point_num = DUAL_TO_QUAD_POINTS;
            if (point_num > remain_points) {
                point_num = remain_points;
            }
            int tmp_len = audio_pcm_dual_to_quadeq_async(eq, drc, NULL, dual_data, point_num);
            if (!tmp_len) {
                break;
            }
            wlen += tmp_len;
            remain_points -= (tmp_len >> 1);
            dual_data += point_num;
        }
    } while (remain_points);
    return (wlen);
}
//同步做eq drc
static inline void audio_pcm_dual_to_quadeq(struct audio_eq *_eq[2], struct audio_drc *_drc[2], s16 *quad_pcm, s16 *dual_pcm,  int points, void *user_hdl)
{
    s16 *dual = dual_pcm;
    u32 j = 0;
    int dual_len = points * 2;
    s16 *dual_FL_FR = (s16 *)&quad_pcm[DUAL_TO_QUAD_POINTS];

    u32 tlen = DUAL_TO_QUAD_POINTS * sizeof(s16);
    s16 *dual_RL_RR = (s16 *)malloc(tlen);
    if (dual_len > tlen) {
        ASSERT(0);
    }

    if (_eq[0]) {
        audio_eq_run_inOut(_eq[0], dual, dual_FL_FR, dual_len);
    } else {
        dual_FL_FR = dual;
    }

#if (TCFG_DRC_ENABLE == 1)
    if (_eq[0] && _drc[0]) {
        audio_drc_run(_drc[0], dual_FL_FR, dual_len);
    }
#endif

#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
    if (_eq[1]) {
        audio_eq_run_inOut(_eq[1], dual, dual_RL_RR, dual_len);
    } else {
        dual_RL_RR = dual;
    }
#if (TCFG_DRC_ENABLE == 1)
    if (_eq[1] && _drc[1]) {
        audio_drc_run(_drc[1], dual_RL_RR, dual_len);
    }
#endif
#else
    dual_RL_RR = dual_FL_FR;
#endif

    dual_to_quad_fun(quad_pcm, dual_FL_FR, dual_RL_RR, points, user_hdl);
    if (dual_RL_RR) {
        free(dual_RL_RR);
        dual_RL_RR = NULL;
    }
}
//四路同步输出
int audio_dualeq_to_quadeq_output_sync(void *src, struct audio_eq *eq[2], struct audio_drc *drc[2], struct audio_mixer_ch *ch, s16 *data, int len, u32 id, s16 *quad_data, void *user_hdl)
{
    int wlen = 0;
    s16 *dual_data = (s16 *)data;
    s16 point_num = 0;
    u8 dual_to_quad = 1;
    u16 remain_points = (len >> 1);
    /* u32 cnt = 0; */
    /* s16 *quad_data = (s16 *)malloc(DUAL_TO_QUAD_POINTS * 2 * 2); */
    /* if (!quad_data){ */
    /* log_e("%s malloc err\n", __FUNCTION__); */
    /* return len; */
    /* } */

    do {
        if (dual_to_quad) {
            point_num = DUAL_TO_QUAD_POINTS;
            if (point_num > remain_points) {
                point_num = remain_points;
            }

            audio_pcm_dual_to_quadeq(eq, drc, quad_data, dual_data, point_num, user_hdl);
            int tmp_len;
            if (src) {
                tmp_len = audio_src_resample_write(src, quad_data, point_num << 2);
            } else {
                tmp_len = audio_mixer_ch_write(ch, quad_data, point_num << 2);
            }
            wlen += tmp_len;
            remain_points -= (tmp_len >> 2);
            /* cnt++; */
            if (tmp_len < (point_num << 2)) {
                /* printf("tmp_len %d, %d, %d, %d\n", tmp_len, wlen, cnt, remain_points); */
                break;
            }
            dual_data += point_num;
        }
    } while (remain_points);
    /* if (quad_data){ */
    /* free(quad_data); */
    /* } */

    return (wlen >> 1);
}
#endif

//双声道转四声道
void dual_to_quad_fun(s16 *_quad_pcm, s16 *_dual_FL_FR, s16 *_dual_RL_RR, int points, void *user_hdl)
{
    s16 *quad_pcm = _quad_pcm;
    s16 *dual_FL_FR = _dual_FL_FR;
    s16 *dual_RL_RR = _dual_RL_RR;

    int len = points >> 1;   //循环次数
    int value;
    __asm__ volatile(
        "1:											\n\t"
        "rep %4	{								\n\t"   //循环控制
        "%3 = h[%1 ++= 2](s)	\n\t"   //取dual_FL_FR的值并偏移到下一个值的地址
        "h[%0 ++= 2] = %3			\n\t"		//保存值到quad_pcm并偏移到下一个值的地址
        "%3 = h[%1 ++= 2](s)	\n\t"	  //取dual_FL_FR的值并偏移到下一个值的地址
        "h[%0 ++= 2] = %3			\n\t"		//保存值到quad_pcm并偏移到下一个值的地址
        "%3 = h[%2 ++= 2](s)	\n\t"		//取dual_RL_RR的值并偏移到下一个值的地址
        "h[%0 ++= 2] = %3			\n\t"		//保存值到quad_pcm并偏移到下一个值的地址
        "%3 = h[%2 ++= 2](s)	\n\t"		//取dual_RL_RR的值并偏移到下一个值的地址
        "h[%0 ++= 2] = %3			\n\t"		//保存值到quad_pcm并偏移到下一个值的地址
        "}											\n\t"
        "if(%4 != 0) goto 1b		\n\t"		//循环控制
        :
        "=&r"(quad_pcm),
        "=&r"(dual_FL_FR),
        "=&r"(dual_RL_RR),
        "=&r"(value),
        "=&r"(len)
        :
        "0"(quad_pcm),
        "1"(dual_FL_FR),
        "2"(dual_RL_RR),
        "3"(value),
        "4"(len)
        :
    );
    u32 quad_data_len = points << 2;
#if (defined(RL_RR_MIX_ENABLE) && (RL_RR_MIX_ENABLE != 0))
    rl_rr_mix_out(_quad_pcm, quad_data_len >> 1);
#endif

    /* #if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0)) */
    /* if (user_hdl){ */
    /* u8 ch_num = 4; */
    /* user_audio_process_handler_run(user_hdl, _quad_pcm, quad_data_len, ch_num); */
    /* } */
    /* #endif */
}
#endif

/* #include "audio_linein.c" */
/* #include "audio_fm.c" */
/* #include "audio_file_dec.c" */
/* #include "audio_spdif.c" */
/* #include "audio_pc.c" */

/* #include "audio_local_tws.c" */
#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
/*
 *蓝牙播歌数字音量调节
 * */
void a2dp_user_digital_volume_set(u8 vol)
{
    if (a2dp_dec && a2dp_dec->user_hdl && a2dp_dec->user_hdl->dvol_hdl) {
        user_audio_digital_volume_set(a2dp_dec->user_hdl->dvol_hdl, vol);
    }
}

u8 a2dp_user_audio_digital_volume_get()
{
    if (!a2dp_dec) {
        return 0;
    }
    if (!a2dp_dec->user_hdl) {
        return 0;
    }
    if (!a2dp_dec->user_hdl->dvol_hdl) {
        return 0;
    }
    return user_audio_digital_volume_get(a2dp_dec->user_hdl->dvol_hdl);
}

/*
 *user_vol_max:音量级数
 *user_vol_tab:自定义音量表,自定义表长user_vol_max+1
 *注意：如需自定义音量表，须在volume_set前调用 ,否则会在下次volume_set时生效
 */
void a2dp_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max)
{
    if (a2dp_dec && a2dp_dec->user_hdl && a2dp_dec->user_hdl->dvol_hdl) {
        user_audio_digital_set_volume_tab(a2dp_dec->user_hdl->dvol_hdl, user_vol_tab, user_vol_max);
    }
}



#endif


#if ((defined(DEC_OUT_OTHER_DATA) && (DEC_OUT_OTHER_DATA)))

struct other_dec_hdl {
    struct audio_src_handle *hw_src;
    u8 in_ch_num;
};
struct other_dec_hdl other_dec;

void stero_to_mono(s16 *data, u32 len, s16 *L_data, s16 *R_data)
{
    u32 points = (len >> 1);
    for (int i = 0, j = 0; i < (points); i += 2, j++) {
        L_data[j] = data[i];
        R_data[j] = data[i + 1];
    }

}

void write_iis(s16 *data, u32 len, u8 in_ch_num)
{
#if ((defined(TCFG_IIS_ENABLE) && (TCFG_IIS_ENABLE)))
    if (in_ch_num == 1) {
        if (TCFG_IIS_OUTPUT_CH_NUM) {
            s16 xch_data[MONO_TO_DUAL_POINTS * 2];
            u16 point_num = 0;
            u16 mono_to_xchannel = 2;
            u16 convert_len = 0;
            u16 remain_points = (len >> 1);
            do {
                point_num = MONO_TO_DUAL_POINTS;
                if (point_num >= remain_points) {
                    point_num = remain_points;
                }
                convert_len = point_num * mono_to_xchannel * 2;
                audio_pcm_mono_to_dual(xch_data, data, point_num);
                audio_link_write_stereodata(xch_data, convert_len, TCFG_IIS_OUTPUT_PORT);
                remain_points -= (convert_len / 2 / mono_to_xchannel);
            } while (remain_points);
        } else {
            u32 wlen = audio_link_write_monodata(data, data, data, data, len, TCFG_IIS_OUTPUT_PORT);
        }
    } else if (in_ch_num == 2) {

        if (TCFG_IIS_OUTPUT_CH_NUM) {
            audio_link_write_stereodata(data, len, TCFG_IIS_OUTPUT_PORT);
        } else {
            s16 *L_data = data;
            s16 *R_data = malloc(len >> 1);
            u32 ch_en = TCFG_IIS_OUTPUT_DATAPORT_SEL;
            stero_to_mono(data, len, L_data, R_data);
            u32 wlen = audio_link_write_monodata(((ch_en & BIT(0)) ? L_data : NULL), ((ch_en & BIT(1)) ? R_data : NULL),
                                                 ((ch_en & BIT(2)) ? L_data : NULL), ((ch_en & BIT(3)) ? R_data : NULL), len >> 1, TCFG_IIS_OUTPUT_PORT);
            if ((len - wlen)) {
                log_e("JUST TEST0\n");
                /* ASSERT(0);	 */
            }
            if (R_data) {
                free(R_data);
            }
        }
    } else {
        log_e("ch err!!!\n");
    }
#endif
}

static int other_dec_src_output_handler(void *priv, s16 *data, int len)
{
    //write iis buf

    write_iis(data, len, other_dec.in_ch_num);
    return len;
}


void other_audio_dec_output(struct audio_decoder *decoder, s16 *data, u32 len, u8 in_ch_num, u16 in_sample_rate)
{
    static u16 in_sample_rate_bk = 0;
    u16 src_out_sr = TCFG_IIS_OUTPUT_SR;
    other_dec.in_ch_num = in_ch_num;
    /* log_i("other_dec.in_ch_num %d\n", other_dec.in_ch_num); */
    if (in_sample_rate != src_out_sr) {
        if (!other_dec.hw_src) {
            other_dec.hw_src = audio_hw_resample_open(&other_dec, other_dec_src_output_handler,
                               other_dec.in_ch_num, in_sample_rate, src_out_sr);
            /* log_i("in_sample_rate %d, src_out_sr %d\n", in_sample_rate, src_out_sr); */
            in_sample_rate_bk = in_sample_rate; //初始备份
        }

        if (other_dec.hw_src) {
            if (in_sample_rate_bk != in_sample_rate) {
                /* log_i("in_sample_rate %d, in_sample_rate_bk %d\n", in_sample_rate, in_sample_rate_bk); */
                /* audio_link_clear_stereodata(TCFG_IIS_OUTPUT_PORT); */
                audio_hw_src_set_rate(other_dec.hw_src, in_sample_rate, src_out_sr);
            }
            int wlen = audio_src_resample_write(other_dec.hw_src, data, len);
            if ((len - wlen)) {
                log_e("JUST TEST1");
                /* ASSERT(0);	 */
            }
        } else {
            log_e("other src err\n");
        }
        in_sample_rate_bk = in_sample_rate;
    } else {
        //write iis buf
        write_iis(data, len, other_dec.in_ch_num);
    }
}

void other_aduio_dec_output_close()
{
    if (other_dec.hw_src) {
        audio_hw_resample_close(other_dec.hw_src);
        other_dec.hw_src = NULL;
    }

}

#endif


#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
int a2dp_eq_output2(void *priv, s16 *data, u32 len)
{
    return len;
}

#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
int a2dp_eq_get_source(void *priv, s16 *buf, len)
{
    struct a2dp_dec_hdl *dec = priv;

    if (!dec->eq_remain) {
#if TCFG_BT_MUSIC_EQ_ENABLE
        audio_eq_run_inOut(a2dp_eq2, buf, dec->rr_rl_source, len);//eq1同步  sys60 ->eq sync 47M
#endif
    }
    return true;
}
#endif
int a2dp_eq_output_async(void *priv, s16 *data, u32 len)
{
    int wlen = 0;
    int total_wlen = 0;
    int rlen = len;
    s16 *dual_FL_FR = (s16 *)data;
    s16 *dual_RL_RR = NULL;
    struct a2dp_dec_hdl *dec = priv;
    s16 *quad_pcm = dec->quad_data;

#if A2DP_EQ_SUPPORT_ASYNC
    if (!dec->eq_remain) {

#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
        //eq1
        dual_RL_RR = dec->rr_rl_source;
#else
        dual_RL_RR = dual_FL_FR;
#endif

#if TCFG_BT_MUSIC_DRC_ENABLE

#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
        if (a2dp_drc2) {
            audio_drc_run(a2dp_drc2, dual_RL_RR, len);
        }
#endif

        if (a2dp_drc) {
            audio_drc_run(a2dp_drc, dual_FL_FR, len);
        }
#endif

        dec->quad_pcm_point_offset = 0;
        u32 points = len >> 1;
        dual_to_quad_fun(quad_pcm, dual_FL_FR, dual_RL_RR, points, NULL);
    }

    len = len << 1;
    //sys60  mix_write  16M 256点 4channel
    do {
#if A2DP_SRC_EN
        if (dec->hw_src) {
            wlen = audio_src_resample_write(dec->hw_src, &dec->quad_data[dec->quad_pcm_point_offset], len);
        } else
#endif
        {
            wlen = audio_mixer_ch_write(&dec->mix_ch, &dec->quad_data[dec->quad_pcm_point_offset], len);
        }
        total_wlen += wlen;
        if (!wlen) {
            break;
        }
        dec->quad_pcm_point_offset += (wlen >> 1);
        len -= wlen;
    } while (len);

    len = len >> 1;
    if (!len /* && wlen */) {
        dec->eq_remain = 0;
    } else {
        dec->eq_remain = 1;
    }

#endif
    return rlen - len;
}
#endif

void a2dp_eq_drc_open(struct a2dp_dec_hdl *dec, struct audio_fmt *fmt)
{
    if (!dec) {
        return;
    }

#if TCFG_BT_MUSIC_EQ_ENABLE
    a2dp_eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (a2dp_eq) {
        a2dp_eq->eq_ch = (struct hw_eq_ch *)((int)a2dp_eq + sizeof(struct audio_eq));
        struct audio_eq_param a2dp_eq_param = {0};
        a2dp_eq_param.channels = dec->ch;//fmt->channel;
        a2dp_eq_param.online_en = 1;
        a2dp_eq_param.mode_en = 1;
        a2dp_eq_param.remain_en = 1;
        a2dp_eq_param.max_nsection = EQ_SECTION_MAX;
        a2dp_eq_param.cb = eq_get_filter_info;
        a2dp_eq_param.eq_switch = 0;
#if A2DP_EQ_SUPPORT_ASYNC
        a2dp_eq_param.no_wait = 1;//异步
#endif
        a2dp_eq_param.eq_name = 0;
        audio_eq_open(a2dp_eq, &a2dp_eq_param);
        audio_eq_set_samplerate(a2dp_eq, fmt->sample_rate);
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)

#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
        audio_eq_set_output_handle(a2dp_eq, a2dp_eq_output, dec);
#else
        /* #if A2DP_EQ_SUPPORT_ASYNC */
        audio_eq_set_output_handle(a2dp_eq, a2dp_eq_output_async, dec);
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
        audio_eq_set_output_source_handle(a2dp_eq, a2dp_eq_get_source, dec);
        dec->rr_rl_source = (s16 *)&dec->quad_data[DUAL_TO_QUAD_POINTS];
#endif
        /* #else */
        /* audio_eq_set_output_handle(a2dp_eq, a2dp_eq_output2, dec); */
        /* #endif */
#endif

#else

#if A2DP_EQ_SUPPORT_32BIT
        audio_eq_set_info(a2dp_eq, a2dp_eq_param.channels, 1);
#endif

        audio_eq_set_output_handle(a2dp_eq, a2dp_eq_output, dec);
#endif
        audio_eq_start(a2dp_eq);
    }

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)

#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
#else
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
    a2dp_eq2 = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (a2dp_eq2) {
        a2dp_eq2->eq_ch = (struct hw_eq_ch *)((int)a2dp_eq2 + sizeof(struct audio_eq));
        struct audio_eq_param a2dp_eq_param = {0};
        a2dp_eq_param.channels = dec->ch;//fmt->channel;
        a2dp_eq_param.online_en = 1;
        a2dp_eq_param.mode_en = 1;
        a2dp_eq_param.remain_en = 1;
        a2dp_eq_param.max_nsection = EQ_SECTION_MAX;
        a2dp_eq_param.cb = eq_get_filter_info2;
        a2dp_eq_param.eq_switch = 1;
        a2dp_eq_param.no_wait = 0;//同步
        a2dp_eq_param.eq_name = 1;
        audio_eq_open(a2dp_eq2, &a2dp_eq_param);
        audio_eq_set_samplerate(a2dp_eq2, fmt->sample_rate);
        audio_eq_set_output_handle(a2dp_eq2, a2dp_eq_output2, dec);
        audio_eq_start(a2dp_eq2);
    }
#endif
#endif
#endif



#endif

#if TCFG_BT_MUSIC_DRC_ENABLE
    a2dp_drc = malloc(sizeof(struct audio_drc));
    if (a2dp_drc) {
        struct audio_drc_param drc_param = {0};
        drc_param.channels = dec->ch;//fmt->channel;
        drc_param.online_en = 1;
        drc_param.remain_en = 1;
        drc_param.cb = drc_get_filter_info;
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
        drc_param.stero_div = 1;
#else
        drc_param.stero_div = 0;
#endif
#else
        drc_param.stero_div = 0;
#endif
        drc_param.drc_name = 0;
        audio_drc_open(a2dp_drc, &drc_param);
        audio_drc_set_samplerate(a2dp_drc, fmt->sample_rate);
#if A2DP_EQ_SUPPORT_32BIT
        audio_drc_set_32bit_mode(a2dp_drc, 1);
#endif
        audio_drc_set_output_handle(a2dp_drc, NULL, NULL);
        audio_drc_start(a2dp_drc);
    }

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
    a2dp_drc2 = malloc(sizeof(struct audio_drc));
    if (a2dp_drc2) {
        struct audio_drc_param drc_param = {0};
        drc_param.channels = dec->ch;//fmt->channel;
        drc_param.online_en = 1;
        drc_param.remain_en = 1;
        drc_param.cb = drc_get_filter_info2;
        drc_param.stero_div = 1;
        drc_param.drc_name = 1;
        audio_drc_open(a2dp_drc2, &drc_param);
        audio_drc_set_samplerate(a2dp_drc2, fmt->sample_rate);
        audio_drc_set_output_handle(a2dp_drc2, NULL, NULL);
        audio_drc_start(a2dp_drc2);
    }
#endif
#endif

#endif


}

void a2dp_eq_drc_close(struct a2dp_dec_hdl *dec)
{
    if (!dec) {
        return;
    }

#if TCFG_BT_MUSIC_EQ_ENABLE
    if (a2dp_eq) {
        audio_eq_close(a2dp_eq);
        free(a2dp_eq);
        a2dp_eq = NULL;
    }

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
    if (a2dp_eq2) {
        audio_eq_close(a2dp_eq2);
        free(a2dp_eq2);
        a2dp_eq2 = NULL;
    }

#endif
#endif

#endif

#if TCFG_BT_MUSIC_DRC_ENABLE
    if (a2dp_drc) {
        audio_drc_close(a2dp_drc);
        free(a2dp_drc);
        a2dp_drc = NULL;
    }

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#if (defined(TCFG_EQ_DIVIDE_ENABLE) && (TCFG_EQ_DIVIDE_ENABLE !=0 ))
    if (a2dp_drc2) {
        audio_drc_close(a2dp_drc2);
        free(a2dp_drc2);
        a2dp_drc2 = NULL;
    }
#endif
#endif
#endif

#if A2DP_EQ_SUPPORT_32BIT
    if (dec->eq_out_buf) {
        free(dec->eq_out_buf);
        dec->eq_out_buf = NULL;
    }
#endif

}

static void voice_cut_handler(void *data, int len, u8 ch_num)
{
    if (ch_num == 2) {
        s16 *L_R = data;
        u32 points = len >> 1;
        int L_Rin = 0;
        s16 L_Rout = 0;
        for (int i = 0; i < points; i += 2) {
            L_Rin = L_R[i] - L_R[i + 1];
            L_Rout = data_sat_s16(L_Rin);
            L_R[i] = L_Rout;
            L_R[i + 1] = L_Rout;
        }
    }
}

void *user_audio_process_handler(void *priv, void *data, int len, u8 ch_num)
{
    struct dec_board_param *dec_par = (struct dec_board_param *)priv;
    if (dec_par->voice_cut_en) {
        voice_cut_handler(data, len, ch_num);
    }
    return NULL;
}


