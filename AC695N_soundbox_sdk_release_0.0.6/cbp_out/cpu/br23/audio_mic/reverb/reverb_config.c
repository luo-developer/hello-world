#include "reverb_config.h"
#include "app_config.h"

#define REVERV_EFFECT_CONFIG 	  0	\
								| BIT(MIC_EFFECT_CONFIG_REVERB) \
								| BIT(MIC_EFFECT_CONFIG_PITCH) \
								| BIT(MIC_EFFECT_CONFIG_EQ) \
								| BIT(MIC_EFFECT_CONFIG_NOISEGATE) \
								| BIT(MIC_EFFECT_CONFIG_DODGE) \
								| BIT(MIC_EFFECT_CONFIG_DVOL) \
								| BIT(MIC_EFFECT_CONFIG_FILT) \


#define REVERB_SAMPLERATE			(44100L)

const struct __reverb_parm reverb_parm_default = 
{
	.effect_config = REVERV_EFFECT_CONFIG,///混响通路支持哪些功能
	.effect_run = REVERV_EFFECT_CONFIG,///混响打开之时， 默认打开的功能
	.sample_rate = REVERB_SAMPLERATE,
};

const struct __mic_stream_parm r_mic_stream_parm_default = 
{
	.sample_rate 		= REVERB_SAMPLERATE,//采样率
	.point_unit  		= 128,//一次处理数据的数据单元， 单位点
	.dac_delay			= 6,//dac硬件混响延时， 单位ms
};

const REVERBN_PARM_SET r_reverb_parm_default = 
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

const  PITCH_SHIFT_PARM r_picth_parm_default = 
{
	.sr = REVERB_SAMPLERATE,		
	.shiftv = 100,
	.effect_v = EFFECT_PITCH_SHIFT,
	.formant_shift = 100,
};

const NOISEGATE_PARM r_noisegate_parm_default = 
{
	.attackTime = 300,
	.releaseTime = 5,
	.threshold = -45000,
	.low_th_gain = 0, 
	.sampleRate = REVERB_SAMPLERATE,
	.channel = 1,
};

static const u16 r_dvol_table[] = {
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

const struct __dvol_parm r_dvol_default_parm = 
{
	.toggle = 1,	
	.fade = 1,	
	.ch_num = 1,	
	.vol_max = 10,//ARRAY_SIZE(r_dvol_table) - 1,	
	.fade_step = 2,	
	.vol_max_level = ARRAY_SIZE(r_dvol_table) - 1,	
	.vol_tab = (void *)r_dvol_table,	
};


// 喊麦滤波器:
const SHOUT_WHEAT_PARM_SET r_shout_wheat_default = 
{	
	.center_frequency = 1500,//中心频率: 800
	.bandwidth = 4000,//带宽:   4000
	.occupy = 80,//占比:   100%  
};
// 低音：
const LOW_SOUND_PARM_SET r_low_sound_default = 
{
	.cutoff_frequency = 700,//截止频率：600
	.highest_gain = -12000,//最高增益：0
	.lowest_gain = 1000,//最低增益:  -12000

};
// 高音：
const HIGH_SOUND_PARM_SET r_high_sound_default = 
{
	.cutoff_frequency = 2000,//:截止频率:  1800
	.highest_gain = -12000,//最高增益：0
	.lowest_gain = 1000,// 最低增益：-12000
};

const struct audio_eq_param r_eq_default_parm = 
{
    .channels  		= 1,
    .online_en 		= 1,
    .mode_en   		= 1,
    .remain_en 		= 1,
    .no_wait   		= 0,
    .eq_switch 		= 0,
	.eq_name 		= 3,
    .max_nsection 	= EQ_SECTION_MAX + 3,///后三段是做高低音及喊麦的
	.cb 			= reverb_eq_get_filter_info,
};


