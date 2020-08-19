#ifndef __DIGITAL_VOL_H__
#define __DIGITAL_VOL_H__

#include "generic/typedef.h"

struct __dvol_parm
{
    u8 toggle;					/*数字音量开关*/
    u8 fade;					/*淡入淡出标志*/
    u8 ch_num;					/*数据源声道数*/
    u8 vol_max;					/*淡入淡出最大音量*/
    u16 fade_step;				/*淡入淡出的步进*/
	u8	vol_max_level;
    u16 *vol_tab;	        	/*自定义音量表*/
};


typedef struct __dvol dvol;

dvol *digital_vol_creat(struct __dvol_parm *parm, u8 cur_vol);
void digital_vol_set(dvol *d_vol, u8 vol);
u8 digital_vol_get(dvol *d_vol);
void digital_vol_process(struct __dvol *d_vol, s16 *data, u32 points);
void digital_vol_destroy(dvol **hdl);

#endif// __DIGITAL_VOL_H__

