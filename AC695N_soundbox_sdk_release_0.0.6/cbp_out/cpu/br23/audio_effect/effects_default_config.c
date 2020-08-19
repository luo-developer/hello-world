#include "effects_default_config.h"


const REVERBN_PARM_SET reverb_defult_config = 
{
	.dry = 100,						//[0:200]%
	.wet = 80,	  					//[0:300]%
	.delay = 70,					//[0-100]ms
	.rot60= 1400,					//[0:15000]ms  //反射系数 值越大 发射越厉害 衰减慢
	.Erwet = 60,					//[5:250]%
	.Erfactor =180,					//[50:250]%
	.Ewidth = 100,					//[-100:100]%
	.Ertolate  = 100,				//[0:100]%
	.predelay  = 0,					//[0:20]ms
	//以下参数无效、可以通过EQ调节
	.width  = 100,					//[0:100]%
	.diffusion  = 70,				//[0:100]%
	.dampinglpf  = 15000,			//[0:18k]
	.basslpf = 500,					//[0:1.1k]
	.bassB = 10,					//[0:80]%
	.inputlpf  = 18000,				//[0:18k]
	.outputlpf  = 18000,			//[0:18k]
};

const PITCH_PARM_SET2 pitch2_default_config = 
{
	.effect_v 				= EFFECT_FUNC_NULL,//变声模式：变调/变声0/变声1/变声2/机器音
	.pitch	 				= 0,		//变音时 pitch 对饮shiftv, effectv 填 EFFECT_VOICECHANGE_KIN0 
	.formant_shift 			= 0,//变音对应formant
};

const PITCH_PARM_SET2 electric_pitch2_default_config = 
{
	.effect_v 				= EFFECT_AUTOTUNE,//变声模式：变调/变声0/变声1/变声2/机器音
	.pitch	 				= 100,		//变音时 pitch 对饮shiftv, effectv 填 EFFECT_VOICECHANGE_KIN0 
	.formant_shift 			= A_MAJOR,//变音对应formant
};

const NOISE_PARM_SET noise_gain_default_config = 
{
	.attacktime 			= 300,// 0 - 15000ms
	.releasetime 			= 5,//  0 - 300ms
	.threadhold 			= -45000,//    -92 - 0 db   （传下时转化为mdb,(thr * 1000)）
	.gain 					= 0,//   0 - 1               (传下来时扩大30bit，(int)(gain * （1 <<30）))
};


const ECHO_PARM_SET echo_default_config = 
{
	.delay 					= 300,					//回声延时时间 0- 300 
	.decayval 				= 50,					//衰减系数 0-70%
	.direct_sound_enable 	= 1,		//直达声使能 0:1
	.filt_enable 			= 1,				//发散滤波器使能 0:1
};

const SHOUT_WHEAT_PARM_SET shout_wheat_default_config = 
{
	.center_frequency 		= 1500,//中心频率: 800
	.bandwidth 				= 4000,//带宽:   4000
	.occupy 				= 80,//占比:   100%  
		
};

const LOW_SOUND_PARM_SET low_sound_default_config = 
{
	.cutoff_frequency 		= 700, //截止频率：600
	.highest_gain 			= -12000,//最高增益：0
	.lowest_gain 			= 1000,//最低增益:  -12000
};

const HIGH_SOUND_PARM_SET high_sound_default_config = 
{
	.cutoff_frequency 		= 2000,//:截止频率:  1800
	.highest_gain 			= -12000,//最高增益：0
	.lowest_gain 			= 1000,// 最低增益：-12000
};



const EFFECTS_MIC_GAIN_PARM mic_gain_defualt_config = 
{
	.gain = 0,	
};











