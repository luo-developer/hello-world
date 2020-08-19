#ifndef __REVERV_DEBUG_H__
#define __REVERV_DEBUG_H__

#include "system/includes.h"
#include "reverb/reverb_api.h"
#include "audio_mic/mic_stream.h"
#include "pitchshifter/pitchshifter_api.h"
#include "asm/noisegate.h"
#include "effects_config.h"

void reverb_parm_printf(REVERBN_PARM_SET *parm);
void pitch_parm_printf(PITCH_PARM_SET2 *parm);
void noisegate_parm_printf(NOISE_PARM_SET *parm);

#endif// __REVERV_DEBUG_H__
