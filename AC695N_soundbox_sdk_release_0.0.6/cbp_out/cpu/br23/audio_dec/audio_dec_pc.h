
#ifndef _AUDIO_DEC_PC_H_
#define _AUDIO_DEC_PC_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "media/audio_decoder.h"

int uac_vol_switch(int vol);

int uac_dec_restart(int magic);
int uac_dec_push_restart(void);
void uac_dec_resume(void);
int uac_dec_no_out_sound(void);

u8 usb_audio_mix_out_ext0_filt_switch_state(void);
void usb_audio_mix_out_ext0_filt_switch(void);

#endif /* TCFG_APP_PC_EN */

