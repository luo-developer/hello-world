#include "reverb.h"
#include "reverb_config.h"
#include "reverb_debug.h"
#include "reverb/reverb_api.h"
#include "audio_digital_vol.h"
#include "audio_splicing.h"
#include "effects_config.h"
#include "audio_effect/filtparm_api.h"
#include "audio_effect/audio_eq.h"
#include "clock_cfg.h"

#define LOG_TAG     "[APP-REVERB]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

typedef struct _BFILT_API_STRUCT_
{
	SHOUT_WHEAT_PARM_SET 	shout_wheat; 
	LOW_SOUND_PARM_SET 		low_sound;
	HIGH_SOUND_PARM_SET 	high_sound;
	u8	 					enable: 1;
	u8	 					revert: 7;
    int 					outval[3][5]; //开3个2阶滤波器的空间，给硬件eq存系数的
	unsigned int			*ptr;         //运算buf指针
	BFILT_FUNC_API			*func_api;    //函数指针
}BFILT_API_STRUCT;

struct __reverb_fade
{
	int wet;             //0-300%
};

struct __reverb
{
	OS_MUTEX				 mutex;
	struct __reverb_parm     parm;
	struct __reverb_fade	 p_fade;
	mic_stream 				*mic;
	dvol					*d_vol;
	REVERBN_API_STRUCT 		*r_api; 
	PITCHSHIFT_API_STRUCT 	*p_api;
	PITCH_SHIFT_PARM        *p_set;
	NOISEGATE_API_STRUCT    *n_api;
	BFILT_API_STRUCT 		*filt;
    struct audio_eq 		*eq;

};

static struct __reverb *p_reverb = NULL;
#define __this  p_reverb
#define R_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

static int reverb_eq_output(void *priv, s16 *data, u32 len)
{
    return len;
}

static void reverb_fade_run(struct __reverb *reverb)
{
	if(reverb)	
	{
		u8 update = 0;
		if(reverb->r_api->parm.wet != reverb->p_fade.wet)	
		{
			update = 1;
			if(reverb->r_api->parm.wet > reverb->p_fade.wet)	
			{
				reverb->r_api->parm.wet --;
			}
			else
			{
				reverb->r_api->parm.wet ++;
			}
		}
		if(update)
		{
			reverb->r_api->func_api->init(reverb->r_api->ptr,&reverb->r_api->parm);
		}
	}
}
static u32 reverb_effect_run(void *priv, void *in, void *out, u32 inlen, u32 outlen)
{
	struct __reverb *reverb = (struct __reverb *)priv;
	if(reverb == NULL)
		return 0;		
	os_mutex_pend(&__this->mutex, 0);

	inlen >>= 1;
	///声音门限处理
	if(reverb->n_api && (reverb->parm.effect_run & BIT(MIC_EFFECT_CONFIG_NOISEGATE)))
		noiseGate_run(reverb->n_api->ptr, in, out, inlen);
	//数字音量设置
	if(reverb->d_vol && (reverb->parm.effect_run & BIT(MIC_EFFECT_CONFIG_DVOL)))
		digital_vol_process(reverb->d_vol, in, inlen);
	///变声处理
	if(reverb->p_api && (reverb->parm.effect_run & BIT(MIC_EFFECT_CONFIG_PITCH)))
		reverb->p_api->func_api->run(reverb->p_api->ptr, in, out, inlen);
	///混响处理,内部会转成双声道
	if(reverb->r_api && (reverb->parm.effect_run & BIT(MIC_EFFECT_CONFIG_REVERB)))
		reverb->r_api->func_api->run(reverb->r_api->ptr, in, out, inlen);
	else
		pcm_single_to_dual(out, in, inlen*2);

	///效果切换
	effects_app_run_check(reverb);
	///淡入淡出
	reverb_fade_run(reverb);

	os_mutex_post(&__this->mutex);
	return outlen;		
}
static void reverb_destroy(struct __reverb **hdl)
{
	if(hdl == NULL || *hdl == NULL)
		return ;
	struct __reverb *reverb = *hdl;
	if(reverb->mic)
		mic_stream_destroy(&reverb->mic);	

	if(reverb->r_api)
		free(reverb->r_api);		
	if(reverb->p_api)
		free(reverb->p_api);		
	if(reverb->n_api)
		free(reverb->n_api);		
	if(reverb->d_vol)
		digital_vol_destroy(&reverb->d_vol);

	local_irq_disable();
	free(reverb);
	*hdl = NULL;
	local_irq_enable();

    clock_remove_set(REVERB_CLK);
}
static bool reverb_creat(struct __reverb *reverb)
{
	if(reverb->parm.effect_config & BIT(MIC_EFFECT_CONFIG_REVERB))
	{
		u8 *buf;
		u32 offset = 0;
		u32 buf_size = 0;
		REVERBN_FUNC_API *r_func = get_reverbn_func_api();	
		buf_size = R_ALIN(sizeof(REVERBN_API_STRUCT), 4)
			+ R_ALIN(r_func->need_buf(NULL, NULL, 0), 4);
		offset = 0;
		buf = zalloc(buf_size);
		if(buf == NULL)
		{
			reverb_destroy(&reverb);
			return false;		
		}
		reverb->r_api = (REVERBN_API_STRUCT *)(buf + offset);
		offset += R_ALIN(sizeof(REVERBN_API_STRUCT), 4);
		reverb->r_api->ptr = (int *)(buf + offset);
		offset += R_ALIN(r_func->need_buf(NULL, NULL, 0), 4);

		memcpy(&reverb->r_api->parm, &r_reverb_parm_default, sizeof(REVERBN_PARM_SET));

		reverb->p_fade.wet = reverb->r_api->parm.wet;//设置fade 目标
		reverb->r_api->parm.wet = 0;///初始化wet为0

		reverb->r_api->func_api = r_func;
		reverb->r_api->func_api->open(
				reverb->r_api->ptr,
				&reverb->r_api->parm,
				reverb->parm.sample_rate);
	}
	return true;
}
static bool pitch_creat(struct __reverb *reverb)
{
	if(reverb->parm.effect_config & BIT(MIC_EFFECT_CONFIG_PITCH))
	{
		u8 *buf;
		u32 offset = 0;
		u32 buf_size = 0;
		PITCHSHIFT_FUNC_API *p_func = get_pitchshift_func_api();	
		buf_size = R_ALIN(sizeof(PITCHSHIFT_API_STRUCT), 4)
					+ R_ALIN(p_func->need_buf(NULL, NULL), 4);
		offset = 0;
		buf = zalloc(buf_size);
		if(buf == NULL)
		{
			reverb_destroy(&reverb);
			return false;		
		}
		reverb->p_api = (PITCHSHIFT_API_STRUCT *)(buf + offset);
		offset += R_ALIN(sizeof(PITCHSHIFT_API_STRUCT), 4);
		reverb->p_api->ptr = (void *)(buf + offset);
		offset += R_ALIN(p_func->need_buf(NULL, NULL), 4);

		memcpy(&reverb->p_api->parm, &r_picth_parm_default, sizeof(PITCH_SHIFT_PARM));
		reverb->p_api->func_api = p_func;
		reverb->p_api->func_api->open(reverb->p_api->ptr, &reverb->p_api->parm);
	}
	return true;
}
static bool noisegate_creat(struct __reverb *reverb)
{
	if(reverb->parm.effect_config & BIT(MIC_EFFECT_CONFIG_NOISEGATE))
	{
		u8 *buf;
		u32 offset = 0;
		u32 buf_size = 0;
		buf_size = R_ALIN(sizeof(NOISEGATE_API_STRUCT), 4)
					+ R_ALIN(noiseGate_buf(), 4);
		offset = 0;
		buf = zalloc(buf_size);
		if(buf == NULL)
		{
			reverb_destroy(&reverb);
			return false;		
		}
		reverb->n_api = (int *)(buf + offset);
		offset += R_ALIN(sizeof(NOISEGATE_API_STRUCT), 4);
		reverb->n_api->ptr = (int *)(buf + offset);
		offset += R_ALIN(noiseGate_buf(), 4);

		memcpy(&reverb->n_api->parm, &r_noisegate_parm_default, sizeof(NOISEGATE_PARM));
		noiseGate_init(reverb->n_api->ptr,
				reverb->n_api->parm.attackTime,
				reverb->n_api->parm.releaseTime,
				reverb->n_api->parm.threshold,
				reverb->n_api->parm.low_th_gain,
				reverb->n_api->parm.sampleRate,
				reverb->n_api->parm.channel);		
	}
	return true;
}
static bool dvol_creat(struct __reverb *reverb)
{
	if(reverb->parm.effect_config & BIT(MIC_EFFECT_CONFIG_DVOL))
	{
		struct __dvol_parm *dvol_parm = (struct __dvol_parm *)&r_dvol_default_parm;
		reverb->d_vol = digital_vol_creat(dvol_parm, dvol_parm->vol_max);
		if(reverb->d_vol == NULL)
		{
			reverb_destroy(&reverb);
			return false;
		}
	}
	return true;	
}
static bool filt_creat(struct __reverb *reverb)
{
	if(reverb->parm.effect_config & BIT(MIC_EFFECT_CONFIG_FILT))
	{
		u8 *buf;
		u32 offset = 0;
		u32 buf_size = 0;
		BFILT_FUNC_API *filt_api = get_bfiltTAB_func_api();		
		buf_size = R_ALIN(sizeof(BFILT_API_STRUCT), 4)
					+ R_ALIN(filt_api->needbuf(), 4);
		offset = 0;
		buf = zalloc(buf_size);
		if(buf == NULL)
		{
			reverb_destroy(&reverb);
			return false;		
		}
		reverb->filt = (BFILT_API_STRUCT *)(buf + offset);
		offset += R_ALIN(sizeof(BFILT_API_STRUCT), 4);

		reverb->filt->ptr = (buf + offset);
		offset += R_ALIN(filt_api->needbuf(), 4);
		
		memcpy(&reverb->filt->shout_wheat, &r_shout_wheat_default, sizeof(SHOUT_WHEAT_PARM_SET));
		memcpy(&reverb->filt->low_sound, &r_low_sound_default, sizeof(LOW_SOUND_PARM_SET));
		memcpy(&reverb->filt->high_sound, &r_high_sound_default, sizeof(HIGH_SOUND_PARM_SET));
		reverb->filt->func_api = filt_api;
		// 运算buf, 中心频率，带宽设置， 滤波器类型，采样率，滤波器index
		reverb->filt->func_api->init(
				reverb->filt->ptr, 
				reverb->filt->shout_wheat.center_frequency, 
				reverb->filt->shout_wheat.bandwidth, 
				TYPE_BANDPASS, 
				reverb->parm.sample_rate, 
				0); 
		reverb->filt->func_api->init(
				reverb->filt->ptr, 
				reverb->filt->low_sound.cutoff_frequency, 
				1024, 
				TYPE_LOWPASS, 
				reverb->parm.sample_rate, 
				1);
		reverb->filt->func_api->init(
				reverb->filt->ptr, 
				reverb->filt->high_sound.cutoff_frequency, 
				1024, 
				TYPE_HIGHPASS, 
				reverb->parm.sample_rate,
			   	2);

		reverb->filt->func_api->cal_coef(reverb->filt->ptr, reverb->filt->outval[0], 0, 0);
		reverb->filt->func_api->cal_coef(reverb->filt->ptr, reverb->filt->outval[1], 0, 1);
		reverb->filt->func_api->cal_coef(reverb->filt->ptr, reverb->filt->outval[2], 0, 2);
	}
	return true;
}
static bool eq_creat(struct __reverb *reverb)
{
	if(reverb->parm.effect_config & BIT(MIC_EFFECT_CONFIG_EQ))
	{
		u8 *buf;
		u32 offset = 0;
		u32 buf_size = 0;
		buf_size = R_ALIN(sizeof(struct audio_eq), 4)
					+ R_ALIN(sizeof(struct hw_eq_ch), 4);
		offset = 0;
		buf = zalloc(buf_size);
		if(buf == NULL)
		{
			reverb_destroy(&reverb);
			return false;		
		}
		reverb->eq = (struct audio_eq *)(buf + offset);
		offset += R_ALIN(sizeof(struct audio_eq), 4);

		reverb->eq->eq_ch = (struct hw_eq_ch *)(buf + offset);
		offset += R_ALIN(sizeof(struct hw_eq_ch), 4);

        audio_eq_open(reverb->eq, (struct audio_eq_param *)&r_eq_default_parm);
        audio_eq_set_samplerate(reverb->eq, reverb->parm.sample_rate);
        audio_eq_set_output_handle(reverb->eq, reverb_eq_output, reverb);
        audio_eq_start(reverb->eq);
	}
	return true;	
}
static bool stream_creat(struct __reverb *reverb)
{
	struct __mic_stream_parm *mic_parm = (struct __mic_stream_parm *)&r_mic_stream_parm_default;
	reverb->mic = mic_stream_creat(mic_parm);
	if(reverb->mic == NULL)
	{
		reverb_destroy(&reverb);
		return false;
	}
	mic_stream_set_output(reverb->mic, (void*)reverb, reverb_effect_run);
	mic_stream_start(reverb->mic);
	return true;
}
bool reverb_start(void)
{
	bool ret = false;
	struct __reverb *reverb = (struct __reverb *)zalloc(sizeof(struct __reverb));
	if(reverb == NULL)
		return false;
	
    clock_add_set(REVERB_CLK);
    os_mutex_create(&reverb->mutex);

	memcpy(&reverb->parm, &reverb_parm_default, sizeof(struct __reverb_parm));
	///reverb 初始化
	ret = reverb_creat(reverb);
	if(ret == false)
		return ret;
	///pitch 初始化
	ret = pitch_creat(reverb);
	if(ret == false)
		return ret;
	///声音门限初始化
	ret = noisegate_creat(reverb);
	if(ret == false)
		return ret;
	///初始化数字音量
	ret = dvol_creat(reverb);
	if(ret == false)
		return ret;
	///滤波器初始化
	ret = filt_creat(reverb);
	if(ret == false)
		return ret;
	///eq初始化
	ret = eq_creat(reverb);
	if(ret == false)
		return ret;
	///mic 数据流初始化
	ret = stream_creat(reverb);
	if(ret == false)
		return ret;

	reverb->filt->enable = 1;

    local_irq_disable();
	__this = reverb;
	local_irq_enable();
	printf("--------------------------reverb start ok\n");
	return true;
}

void reverb_stop(void)
{
	reverb_destroy(&__this);
}

u8 reverb_get_status(void)
{
	return (__this ? 1 : 0);
}

void reverb_set_dvol(u8 vol)
{
	if(__this == NULL)
		return ;
	digital_vol_set(__this->d_vol, vol);	
}

u8 reverb_get_dvol(void)
{
	if(__this)	
	{
		return digital_vol_get(__this->d_vol);	
	}
	return 0;
}

void reverb_set_wet(int wet)
{
	if(__this == NULL)
		return ;
	os_mutex_pend(&__this->mutex, 0);
	__this->p_fade.wet = wet;
	os_mutex_post(&__this->mutex);
}

int reverb_get_wet(void)
{
	if(__this)
	{
		return __this->p_fade.wet;	
	}
	return 0;
}

void reverb_set_function_mask(u32 mask)
{
	if(__this == NULL)
		return ;
	os_mutex_pend(&__this->mutex, 0);
	__this->parm.effect_run = mask;	
	os_mutex_post(&__this->mutex);
}

u32 reverb_get_function_mask(void)
{
	if(__this)
	{
		return __this->parm.effect_run;	
	}
	return 0;
}

static void reverb_shout_wheat_cal_coef(int sw)
{
	if(__this == NULL)
		return ;
	os_mutex_pend(&__this->mutex, 0);
	BFILT_API_STRUCT *filt = __this->filt;
	filt->func_api->init(
			filt->ptr, 
			filt->shout_wheat.center_frequency, 
			filt->shout_wheat.bandwidth, 
			TYPE_BANDPASS, 
			__this->parm.sample_rate, 
			0); 
	if (sw){
		filt->func_api->cal_coef(filt->ptr, filt->outval[0], filt->shout_wheat.occupy, 0);
		log_i("shout_wheat_cal_coef on\n");
	}else{
		filt->func_api->cal_coef(filt->ptr, filt->outval[0], 0, 0);
		log_i("shout_wheat_cal_coef off\n");
	}
	os_mutex_post(&__this->mutex);
}

static void reverb_low_sound_cal_coef(int gainN)
{
	if(__this == NULL)
		return ;
	os_mutex_pend(&__this->mutex, 0);
	BFILT_API_STRUCT *filt = __this->filt;
	filt->func_api->init(
			filt->ptr, 
			filt->low_sound.cutoff_frequency, 
			1024, 
			TYPE_LOWPASS, 
			__this->parm.sample_rate, 
			1);
	gainN = filt->low_sound.lowest_gain 
		+ gainN*(filt->low_sound.highest_gain - filt->low_sound.lowest_gain)/10;
	log_i("low sound gainN %d\n", gainN);
	log_i("lowest_gain %d\n", filt->low_sound.lowest_gain);
	log_i("highest_gain %d\n", filt->low_sound.highest_gain);
	if ((gainN >= filt->low_sound.lowest_gain) && (gainN <= filt->low_sound.highest_gain)) {
		filt->func_api->cal_coef(filt->ptr, filt->outval[1], gainN, 1);
	}

	os_mutex_post(&__this->mutex);
}

static void reverb_high_sound_cal_coef(int gainN)
{
	if(__this == NULL)
		return ;
	os_mutex_pend(&__this->mutex, 0);
	BFILT_API_STRUCT *filt = __this->filt;
	filt->func_api->init(
			filt->ptr, 
			filt->high_sound.cutoff_frequency, 
			1024, 
			TYPE_HIGHPASS, 
			__this->parm.sample_rate,
			2);
	gainN = filt->high_sound.lowest_gain + gainN*(filt->high_sound.highest_gain - filt->high_sound.lowest_gain)/10;
	log_i("high gainN %d\n", gainN);
	log_i("lowest_gain %d\n", filt->high_sound.lowest_gain);
	log_i("highest_gain %d\n", filt->high_sound.highest_gain);
	if ((gainN >= filt->high_sound.lowest_gain) && (gainN <= filt->high_sound.highest_gain)) {
		filt->func_api->cal_coef(filt->ptr, filt->outval[2], gainN, 2);
	}
	os_mutex_post(&__this->mutex);
}

/*
 *混响高低音调节
 *filtN为0 与 sw 组合：控制喊麦开关
 *filtN为1或者2时，改变gainN值 调节高低音值
 * */
void reverb_cal_coef(u8 filtN, int gainN, u8 sw)
{
	if(__this == NULL)
		return ;
	if (filtN == 0){
		reverb_shout_wheat_cal_coef(sw);
	}else if (filtN == 1){
		reverb_low_sound_cal_coef(gainN);
	}else if (filtN == 2){
		reverb_high_sound_cal_coef(gainN);
	}
	reverb_eq_mode_set_updata(filtN);
}


void reverb_eq_mode_set(u8 type, u32 gainN)
{
	if(__this == NULL)
		return ;
	log_i("filN %d, gainN %d\n", type, gainN);
	if (__this->filt->enable){
		if (type == REVERB_EQ_MODE_SHOUT_WHEAT){
			reverb_shout_wheat_cal_coef(gainN);
		}else if (type == REVERB_EQ_MODE_LOW_SOUND){
			reverb_low_sound_cal_coef(gainN);
		}else if (type == REVERB_EQ_MODE_HIGH_SOUND){
			reverb_high_sound_cal_coef(gainN);
		}
	}
	reverb_eq_mode_set_updata(type);
}

void effects_run_check(void *hdl, u8 update, void *parm, u16 cur_mode)
{
	struct __reverb *reverb = (struct __reverb *)hdl;
	switch(update)
	{
		case MAGIC_FLAG_reverb:	
			if(reverb->r_api == NULL)
				break;
			REVERBN_PARM_SET *p1 = (REVERBN_PARM_SET *)parm;
			reverb_parm_printf(p1);
			if(get_effects_online())
			{
				///在线工具更新参数， 全部更改
				memcpy(&reverb->r_api->parm, p1, sizeof(REVERBN_PARM_SET));	
				reverb->p_fade.wet = p1->wet;///设置wet fade 目标值, 通过fade更新
				reverb->r_api->func_api->init(reverb->r_api->ptr,&reverb->r_api->parm);
			}
			else
			{
				REVERBN_PARM_SET tmp;
				memcpy(&tmp, p1, sizeof(REVERBN_PARM_SET));
				tmp.wet = reverb->r_api->parm.wet;///读取旧值,暂时不更新
				reverb->p_fade.wet = p1->wet;///设置wet fade 目标值, 通过fade更新
				memcpy(&reverb->r_api->parm, &tmp, sizeof(REVERBN_PARM_SET));	
				reverb->r_api->func_api->init(reverb->r_api->ptr,&reverb->r_api->parm);
			}
			break;

		case MAGIC_FLAG_pitch2:	
			if(reverb->p_api == NULL)
				break;
			PITCH_PARM_SET2 *p2 = (PITCH_PARM_SET2 *)parm;
			pitch_parm_printf(p2);
			if(p2->effect_v == EFFECT_FUNC_NULL)
			{
				reverb->parm.effect_run &= ~BIT(MIC_EFFECT_CONFIG_PITCH);		
			}
			else
			{
				reverb->parm.effect_run |= BIT(MIC_EFFECT_CONFIG_PITCH);		
			}
			reverb->p_api->parm.effect_v = p2->effect_v;
			reverb->p_api->parm.shiftv = p2->pitch;
			reverb->p_api->parm.formant_shift = p2->formant_shift;
			reverb->p_api->func_api->init(reverb->p_api->ptr, &reverb->p_api->parm);
			break;
		case MAGIC_FLAG_noise:	
			if(reverb->n_api == NULL)
				break;
			NOISE_PARM_SET *p3 = (NOISE_PARM_SET *)parm;
			noisegate_parm_printf(p3);
			reverb->n_api->parm.attackTime 		= p3->attacktime;
			reverb->n_api->parm.releaseTime 	= p3->releasetime;
			reverb->n_api->parm.threshold 		= p3->threadhold;
			reverb->n_api->parm.low_th_gain 	= p3->gain;
			noiseGate_init(reverb->n_api->ptr,
					reverb->n_api->parm.attackTime,
					reverb->n_api->parm.releaseTime,
					reverb->n_api->parm.threshold,
					reverb->n_api->parm.low_th_gain,
					reverb->n_api->parm.sampleRate,
					reverb->n_api->parm.channel);		
			break;
		case MAGIC_FLAG_shout_wheat:
			/* memcpy(&reverb->shout_wheat, parm, sizeof(SHOUT_WHEAT_PARM_SET)); */
			break;
		case MAGIC_FLAG_low_sound:
			/* memcpy(&reverb->low_sound, parm, sizeof(LOW_SOUND_PARM_SET)); */
			break;
		case MAGIC_FLAG_high_sound:
			/* memcpy(&reverb->high_sound, parm, sizeof(HIGH_SOUND_PARM_SET)); */
			break;
		case MAGIC_FLAG_mic_gain:
			log_info("value %d\n", (u8)parm);
			/* set_mic_gain((u8)parm); */
			break;
		default:
			break;
	}
}





