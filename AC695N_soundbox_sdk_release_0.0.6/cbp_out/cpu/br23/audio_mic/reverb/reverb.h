#ifndef __REVERB_H__
#define __REVERB_H__

#include "system/includes.h"

enum{
	REVERB_EQ_MODE_SHOUT_WHEAT = 0x0,
	REVERB_EQ_MODE_LOW_SOUND,
	REVERB_EQ_MODE_HIGH_SOUND,
};

bool reverb_start(void);
void reverb_stop(void);
u8 reverb_get_status(void);
void reverb_set_dvol(u8 vol);
u8 reverb_get_dvol(void);
void reverb_set_wet(int wet);
int reverb_get_wet(void);
void reverb_set_function_mask(u32 mask);
u32 reverb_get_function_mask(void);
void reverb_cal_coef(u8 filtN, int gainN, u8 sw);
void reverb_eq_mode_set(u8 type, u32 gainN);

#endif// __REVERB_H__
