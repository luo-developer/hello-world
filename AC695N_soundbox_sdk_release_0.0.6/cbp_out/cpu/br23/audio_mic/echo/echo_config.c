#include "echo_config.h"

#define EHCO_EFFECT_CONFIG 	  	  0	\
								| BIT(MIC_EFFECT_CONFIG_ECHO) \
								| BIT(MIC_EFFECT_CONFIG_PITCH) \
								| BIT(MIC_EFFECT_CONFIG_EQ) \
								| BIT(MIC_EFFECT_CONFIG_NOISEGATE) 

#define ECHO_SAMPLERATE			(44100L)

const u32 ehco_fuction_mask = EHCO_EFFECT_CONFIG;

const struct __mic_stream_parm e_mic_stream_parm_default = 
{
	.sample_rate 		= ECHO_SAMPLERATE,//采样率
	.point_unit  		= 128,//一次处理数据的数据单元， 单位点
	.dac_delay			= 6,//dac硬件混响延时， 单位ms
};

const EF_REVERB_FIX_PARM e_echo_fix_parm_default = 
{
	.wetgain = 2048,			////湿声增益：[0:4096]
	.drygain = 0,				////干声增益: [0:4096]
	.sr = ECHO_SAMPLERATE,		////采样率
	.max_ms = 200,				////所需要的最大延时，影响 need_buf 大小

};

const ECHO_PARM_SET e_ehco_parm_default = 
{
	.delay = 300,				//回声的延时时间 0-300ms	
	.decayval = 50,				// 0-70%		
	.direct_sound_enable = 1,	//直达声使能  0/1		
	.filt_enable = 1,			//发散滤波器使能
};

const  PITCH_SHIFT_PARM e_picth_parm_default = 
{
	.sr = ECHO_SAMPLERATE,		
	.shiftv = 100,
	.effect_v = EFFECT_PITCH_SHIFT,
	.formant_shift = 100,
};

const NOISEGATE_PARM e_noisegate_parm_default = 
{
	.attackTime = 300,
	.releaseTime = 5,
	.threshold = -45000,
	.low_th_gain = 0, 
	.sampleRate = ECHO_SAMPLERATE,
	.channel = 1,
};

static const u16 e_dvol_table[] = {
    0	, //0
    93	, //1
    111	, //2
    132	, //3
    158	, //4
    189	, //5
    226	, //6
    270	, //7
    323	, //8
    386	, //9
    462	, //10
    552	, //11
    660	, //12
    789	, //13
    943	, //14
    1127, //15
    1347, //16
    1610, //17
    1925, //18
    2301, //19
    2751, //20
    3288, //21
    3930, //22
    4698, //23
    5616, //24
    6713, //25
    8025, //26
    9592, //27
    11466,//28
    15200,//29
    16000,//30
    16384 //31
};

const struct __dvol_parm e_dvol_default_parm = 
{
	.toggle = 1,	
	.fade = 1,	
	.ch_num = 1,	
	.vol_max = ARRAY_SIZE(e_dvol_table) - 1,	
	.fade_step = 2,	
	.vol_max_level = ARRAY_SIZE(e_dvol_table) - 1,	
	.vol_tab = (void *)e_dvol_table,	
};






