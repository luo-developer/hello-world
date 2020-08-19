#ifndef __ECHO_CONFIG_H__
#define __ECHO_CONFIG_H__

#include "system/includes.h"
#include "reverb/reverb_api.h"
#include "audio_mic/mic_stream.h"
#include "pitchshifter/pitchshifter_api.h"
#include "asm/noisegate.h"
#include "audio_mic/digital_vol.h"


extern const u32 									ehco_fuction_mask;
extern const struct __mic_stream_parm 				e_mic_stream_parm_default;
extern const EF_REVERB_FIX_PARM 					e_echo_fix_parm_default;
extern const ECHO_PARM_SET 							e_ehco_parm_default; 
extern const PITCH_SHIFT_PARM	 					e_picth_parm_default; 
extern const NOISEGATE_PARM 						e_noisegate_parm_default;
extern const struct __dvol_parm 					e_dvol_default_parm;

#endif// __REVERB_CONFIG_H__

