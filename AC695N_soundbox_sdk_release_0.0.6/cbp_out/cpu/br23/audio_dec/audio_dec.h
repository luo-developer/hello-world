
#ifndef _AUDIO_DEC_H_
#define _AUDIO_DEC_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "asm/audio_src.h"
#include "audio_digital_vol.h"

#ifndef RB16
#define RB16(b)    (u16)(((u8 *)b)[0] << 8 | (((u8 *)b))[1])
#endif

extern struct audio_decoder_task decode_task;
extern struct audio_mixer mixer;

u32 audio_output_rate(int input_rate);
u32 audio_output_channel_num(void);
int audio_output_set_start_volume(u8 state);

int audio_output_start(u32 sample_rate, u8 reset_rate);
void audio_output_stop(void);

struct audio_src_handle *audio_hw_resample_open(void *priv, int (*output_handler)(void *, void *, int),
        u8 channel, u16 input_sample_rate, u16 output_sample_rate);
void audio_hw_resample_close(struct audio_src_handle *hdl);

void audio_resume_all_decoder(void);
void audio_resume_all_decoder_run(int limit_mix_num, int cnt, int time_out);
void audio_resume_all_mix(void);

void __a2dp_drop_frame(void *p);

int a2dp_dec_start();
u8 is_a2dp_dec_open();
int a2dp_dec_open(int media_type);
int a2dp_dec_close();

void esco_plc_run(s16 *data, u16 len, u8 repair);
void esco_dec_release();
int esco_dec_start();
int esco_dec_open(void *param, u8 mute);
void esco_dec_close();

//////////////////////////////////////////////////////////////////////////////
u8 bt_audio_is_running(void);
u8 bt_media_is_running(void);
u8 bt_phone_dec_is_running();

//////////////////////////////////////////////////////////////////////////////
void audio_dec_bt_emitter_check_empty_en(u8 en);

//////////////////////////////////////////////////////////////////////////////
void audio_automute_event_handler(u8 event, u8 channel);
void audio_dec_automute_start(void);
void audio_dec_automute_stop(void);

int audio_dec_init();

void set_source_sample_rate(u16 sample_rate);
u16 get_source_sample_rate();

int a2dp_eq_output2(void *priv, s16 *data, u32 len);
int a2dp_eq_output_async(void *priv, s16 *data, u32 len);

// int audio_dual_to_quad_output(struct audio_mixer_ch *ch, s16 *data, int len);
u32 audio_pcm_dual_to_quadeq_async(struct audio_eq *_eq[2], struct audio_drc *_drc[2], s16 *quad_pcm, s16 *dual_pcm,  int points);

// int audio_dualeq_to_quadeq_output(struct audio_eq *eq[2], struct audio_drc *drc[2], struct audio_mixer_ch *ch, s16 *data, int len);
int audio_dualeq_to_quadeq_output_sync(void *src, struct audio_eq *eq[2], struct audio_drc *drc[2], struct audio_mixer_ch *ch, s16 *data, int len, u32 id, s16 *quad_data, void *user_hdl);

// void dual_to_quad_fun(s16 *_quad_pcm, s16 *_dual_FL_FR, s16 *_dual_RL_RR, int points);
void dual_to_quad_fun(s16 *_quad_pcm, s16 *_dual_FL_FR, s16 *_dual_RL_RR, int points, void *dvol_hdl);
int audio_dual_to_quad_output_src(void *src, struct audio_mixer_ch *ch, s16 *data, int len, u32 id, s16 *quad_data, void *user_hdl);
int audio_dualeq_to_quadeq_output_async(struct audio_eq *eq[2], struct audio_drc *drc[2], struct audio_mixer_ch *ch, s16 *data, int len);

#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
void a2dp_user_digital_volume_set(u8 vol);
u8 a2dp_user_audio_digital_volume_get();
void a2dp_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max);

void linein_user_digital_volume_set(u8 vol);
u8 linein_user_audio_digital_volume_get();
void linein_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max);

void fm_user_digital_volume_set(u8 vol);
u8 fm_user_audio_digital_volume_get();
void fm_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max);

void file_user_digital_volume_set(u8 vol);
u8 file_user_audio_digital_volume_get();
void file_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max);

void pc_user_digital_volume_set(u8 vol);
u8 pc_user_audio_digital_volume_get();
void pc_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max);

void spdif_user_digital_volume_set(u8 vol);
u8 spdif_user_audio_digital_volume_get();
void spdif_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max);


void reverb_user_digital_volume_set(u8 vol);
u8 reverb_user_audio_digital_volume_get();
void reverb_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max);
#endif
void reverb_set_dodge_threshold(int threshold_in, int threshold_out, u8 fade_tar, u8 dodge_en);

#if ((defined(DEC_OUT_OTHER_DATA) && (DEC_OUT_OTHER_DATA)))
extern void other_audio_dec_output(struct audio_decoder *decoder, s16 *data, u32 len, u8 in_ch_num, u16 in_sample_rate);
#endif

void *user_audio_process_handler(void *priv, void *data, int len, u8 ch_num);
//////////////////////////////////////////////////////////////////////////////
static inline void audio_pcm_mono_to_dual(s16 *dual_pcm, s16 *mono_pcm, int points)
{
    s16 *mono = mono_pcm;
    int i = 0;
    u8 j = 0;

    for (i = 0; i < points; i++, mono++) {
        *dual_pcm++ = *mono;
        *dual_pcm++ = *mono;
    }
}

#define DUAL_TO_QUAD_POINTS  512

#define A2DP_DEC_ID      BIT(0)
#define ESCO_DEC_ID      BIT(1)
#define FILE_DEC_ID      BIT(2)
#define AUX_DEC_ID       BIT(3)
#define FM_DEC_ID        BIT(4)
#define REVERB_DEC_ID    BIT(5)
#define PC_DEC_ID        BIT(6)
#define SPDIF_DEC_ID     BIT(7)
#define TONE_DEC_ID      BIT(8)

//////////////////////////////////////////////////////////////////////////////
#include "audio_dec_file.h"
#include "audio_dec_fm.h"
#include "audio_dec_linein.h"
#include "audio_dec_pc.h"
#include "audio_dec_local_tws.h"
#include "audio_dec_spdif.h"

#endif

