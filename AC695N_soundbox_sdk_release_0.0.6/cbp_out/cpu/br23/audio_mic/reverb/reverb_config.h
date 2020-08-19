#ifndef __REVERB_CONFIG_H__
#define __REVERB_CONFIG_H__

#include "system/includes.h"
#include "reverb/reverb_api.h"
#include "audio_mic/mic_stream.h"
#include "pitchshifter/pitchshifter_api.h"
#include "asm/noisegate.h"
#include "audio_mic/digital_vol.h"
#include "effects_config.h"
#include "audio_eq.h"

struct __reverb_parm 
{
	u32	 effect_config;
	u32	 effect_run;
	u16	 sample_rate;

};

extern const struct __reverb_parm 					reverb_parm_default; 
extern const struct __mic_stream_parm 				r_mic_stream_parm_default;
extern const REVERBN_PARM_SET 						r_reverb_parm_default; 
extern const PITCH_SHIFT_PARM	 					r_picth_parm_default; 
extern const NOISEGATE_PARM 						r_noisegate_parm_default;
extern const struct __dvol_parm 					r_dvol_default_parm;
extern const SHOUT_WHEAT_PARM_SET 					r_shout_wheat_default; 
extern const LOW_SOUND_PARM_SET 					r_low_sound_default; 
extern const HIGH_SOUND_PARM_SET 					r_high_sound_default; 
extern const struct audio_eq_param 					r_eq_default_parm; 

#endif// __REVERB_CONFIG_H__

