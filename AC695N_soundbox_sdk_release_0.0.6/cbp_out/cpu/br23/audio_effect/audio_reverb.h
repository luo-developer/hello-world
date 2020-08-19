
#ifndef _AUDIO_REVERB_API_H_
#define _AUDIO_REVERB_API_H_
#include "pitchshifter/pitchshifter_api.h"
#include "mono2stereo/reverb_mono2stero_api.h"
#include "reverb/reverb_api.h"
#include "asm/howling_api.h"
#include "asm/noisegate.h"

#include "audio_config.h"

#define   REVERB_PARM_SET     REVERBN_PARM_SET
#define   REVERB_FUNC_API     REVERBN_FUNC_API
#define   REVERB_API_STRUCT   REVERBN_API_STRUCT

ECHO_API_STRUCT *open_echo(ECHO_PARM_SET *echo_seting, u16 sample_rate);
void  close_echo(ECHO_API_STRUCT *echo_api_obj);
void update_echo_parm(ECHO_API_STRUCT *echo_api_obj, ECHO_PARM_SET *echo_seting);
u8 update_echo_parm_fader(ECHO_API_STRUCT *echo_api_obj, ECHO_PARM_SET *echo_seting);
u8 update_echo_parm_tager(ECHO_API_STRUCT *echo_api_obj, ECHO_PARM_SET *echo_seting);

REVERBN_API_STRUCT *open_reverb(REVERBN_PARM_SET *reverb_seting, u16 sample_rate);
void  close_reverb(REVERBN_API_STRUCT *reverb_api_obj);
void update_reverb_parm(REVERBN_API_STRUCT *reverb_api_obj, REVERBN_PARM_SET *reverb_seting);
u8 update_reverb_parm_fader(REVERBN_API_STRUCT *reverb_api_obj, REVERBN_PARM_SET *reverb_seting);
u8 update_reverb_parm_tager(REVERBN_API_STRUCT *reverb_api_obj, REVERBN_PARM_SET *reverb_seting);

HOWLING_API_STRUCT *open_howling(HOWLING_PARM_SET *howl_para, u16 sample_rate, u8 channel);
void close_howling(HOWLING_API_STRUCT *holing_hdl);

void start_reverb_mic2dac(struct audio_fmt *fmt);
int reverb_if_working(void);
void stop_reverb_mic2dac(void);
void set_mic_gain_up(u8 value);
void set_mic_gain_down(u8 value);
void set_mic_gain(u8 value);
u8 get_mic_gain(void);
void set_reverb_deepval(u16 value);
void set_reverb_deepval_up(u16 value);
void set_reverb_deepval_down(u16 value);
void reset_reverb_src_out(u16 s_rate);
void reverb_pause(void);
void reverb_resume(void);
void set_reverb_decayval_up(u16 value);
void set_reverb_decayval_down(u16 value);
u16 get_reverb_wetgain(void);
void set_reverb_wetgain(u32 value);
void set_pitch_para(u32 shiftv,u32 sr,u8 effect,u32 formant_shift);
void set_pitch_onoff(u8 onoff);
void switch_pitch_mode(void);
void reverb_eq_cal_coef(u8 filtN, int gainN, u8 sw);
void reverb_eq_set(u8 type, u32 gainN);

void update_reverb_parm_debug(u8 type,u8 dir,s16 data);

#endif

