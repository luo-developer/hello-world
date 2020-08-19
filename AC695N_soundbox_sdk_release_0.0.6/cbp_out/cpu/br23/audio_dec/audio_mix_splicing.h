#ifndef __AUDIO_MIX_SPLICING_H__
#define __AUDIO_MIX_SPLICING_H__

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "asm/audio_src.h"
#include "audio_digital_vol.h"
#include "audio_splicing.h"
#include "app_config.h"

int mix_output_dual_to_quad(struct audio_mixer *mixer, s16 *data, u16 len);
int mix_output_ext_dual_to_quad_handler(struct audio_mixer *mixer, s16 *data, u16 len, u8 idx);

#endif//__AUDIO_MIX_SPLICING_H__


