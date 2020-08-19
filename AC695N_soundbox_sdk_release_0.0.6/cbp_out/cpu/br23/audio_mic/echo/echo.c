#include "echo.h"
#include "echo_config.h"
#include "reverb/reverb_api.h"
#include "audio_digital_vol.h"
#include "audio_splicing.h"

#define LOG_TAG     "[APP-REVERB]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

struct __echo
{
	u32						 effect_config;
	u16						 sample_rate;
	mic_stream 				*mic;
	dvol					*d_vol;
	ECHO_API_STRUCT 		*e_api;
	ECHO_PARM_SET 			*e_set;
	PITCHSHIFT_API_STRUCT 	*p_api;
	PITCH_SHIFT_PARM        *p_set;
	NOISEGATE_API_STRUCT    *n_api;
};

static struct __echo *p_echo = NULL;
#define __this  p_echo
#define E_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

static u32 echo_effect_run(void *priv, void *in, void *out, u32 inlen, u32 outlen)
{
	struct __echo *echo = (struct __echo *)priv;
	if(echo == NULL)
		return 0;		
	inlen >>= 1;
	///声音门限处理
	if(echo->n_api)
		noiseGate_run(echo->n_api->ptr, in, out, inlen);
	//数字音量设置
	if(echo->d_vol)
		digital_vol_process(echo->d_vol, in, inlen);
	///变声处理
	if(echo->p_api)
		echo->p_api->func_api->run(echo->p_api->ptr, in, out, inlen);
	///混响处理,内部会转成双声道
	if(echo->e_api)
	{
		echo->e_api->func_api->run(echo->e_api->ptr, in, in, inlen);
		pcm_single_to_dual(out, in, inlen*2);
	}
	else
	{
		pcm_single_to_dual(out, in, inlen*2);
	}

	return outlen;		
}

static void echo_destroy(struct __echo **hdl)
{
	if(hdl == NULL || *hdl == NULL)
		return ;

	struct __echo *echo = *hdl;

	if(echo->mic)
		mic_stream_destroy(&echo->mic);	

	if(echo->e_api)
		free(echo->e_api);		
	if(echo->p_api)
		free(echo->p_api);		
	if(echo->n_api)
		free(echo->n_api);		
	if(echo->d_vol)
		digital_vol_destroy(&echo->d_vol);

	local_irq_disable();
	free(echo);
	*hdl = NULL;
	local_irq_enable();
}

bool echo_start(void)
{
	u8 *buf;
	u32 offset = 0;
	u32 buf_size = 0;

	struct __echo *echo = (struct __echo *)zalloc(sizeof(struct __echo));
	if(echo == NULL)
		return false;

	echo->sample_rate = e_mic_stream_parm_default.sample_rate;
	echo->effect_config = (u32)ehco_fuction_mask;

	///echo 初始化
	
	if(echo->effect_config & BIT(MIC_EFFECT_CONFIG_ECHO))
	{
		ECHO_FUNC_API *e_func = get_echo_func_api();	
		buf_size = E_ALIN(sizeof(ECHO_API_STRUCT), 4)
			+ E_ALIN(sizeof(ECHO_PARM_SET), 4)
			+ E_ALIN(e_func->need_buf(NULL, &e_echo_fix_parm_default), 4);
		offset = 0;
		buf = zalloc(buf_size);
		if(buf == NULL)
		{
			echo_destroy(&echo);
			return false;		
		}
		echo->e_api = (ECHO_API_STRUCT *)(buf + offset);
		offset += E_ALIN(sizeof(ECHO_API_STRUCT), 4);
		echo->e_set = (ECHO_PARM_SET *)(buf + offset); 
		offset += E_ALIN(sizeof(ECHO_PARM_SET), 4);
		echo->e_api->ptr = (int *)(buf + offset);
		offset += E_ALIN(e_func->need_buf(NULL, &e_echo_fix_parm_default), 4);

		memcpy(&echo->e_api->echo_parm_obj, &e_ehco_parm_default, sizeof(ECHO_PARM_SET));
		echo->e_api->func_api = e_func;
		echo->e_api->func_api->open(
				echo->e_api->ptr,
				&echo->e_api->echo_parm_obj,
				&e_echo_fix_parm_default
				);
	}
	///pitch 初始化
	if(echo->effect_config & BIT(MIC_EFFECT_CONFIG_PITCH))
	{
		PITCHSHIFT_FUNC_API *p_func = get_pitchshift_func_api();	
		buf_size = E_ALIN(sizeof(PITCHSHIFT_API_STRUCT), 4)
					+ E_ALIN(p_func->need_buf(NULL, NULL), 4);
		offset = 0;
		buf = zalloc(buf_size);
		if(buf == NULL)
		{
			echo_destroy(&echo);
			return false;		
		}
		echo->p_api = (PITCHSHIFT_API_STRUCT *)(buf + offset);
		offset += E_ALIN(sizeof(PITCHSHIFT_API_STRUCT), 4);
		echo->p_api->ptr = (void *)(buf + offset);
		offset += E_ALIN(p_func->need_buf(NULL, NULL), 4);

		memcpy(&echo->p_api->parm, &e_picth_parm_default, sizeof(PITCH_SHIFT_PARM));
		echo->p_api->func_api = p_func;
		echo->p_api->func_api->open(echo->p_api->ptr, &echo->p_api->parm);
	}
	///声音门限初始化
	if(echo->effect_config & BIT(MIC_EFFECT_CONFIG_NOISEGATE))
	{
		buf_size = E_ALIN(sizeof(NOISEGATE_API_STRUCT), 4)
					+ E_ALIN(noiseGate_buf(), 4);
		offset = 0;
		buf = zalloc(buf_size);
		if(buf == NULL)
		{
			echo_destroy(&echo);
			return false;		
		}
		echo->n_api = (int *)(buf + offset);
		offset += E_ALIN(sizeof(NOISEGATE_API_STRUCT), 4);
		echo->n_api->ptr = (int *)(buf + offset);
		offset += E_ALIN(noiseGate_buf(), 4);

		memcpy(&echo->n_api->parm, &e_noisegate_parm_default, sizeof(NOISEGATE_PARM));
		noiseGate_init(echo->n_api->ptr,
				echo->n_api->parm.attackTime,
				echo->n_api->parm.releaseTime,
				echo->n_api->parm.threshold,
				echo->n_api->parm.low_th_gain,
				echo->n_api->parm.sampleRate,
				echo->n_api->parm.channel);		
	}
	///初始化数字音量
	if(echo->effect_config & BIT(MIC_EFFECT_CONFIG_DVOL))
	{
		struct __dvol_parm *dvol_parm = (struct __dvol_parm *)&e_dvol_default_parm;
		echo->d_vol = digital_vol_creat(dvol_parm, dvol_parm->vol_max);
		if(echo->d_vol == NULL)
		{
			echo_destroy(&echo);
			return false;
		}
	}

	///mic 数据流初始化
	struct __mic_stream_parm *mic_parm = (struct __mic_stream_parm *)&e_mic_stream_parm_default;
	echo->mic = mic_stream_creat(mic_parm);
	if(echo->mic == NULL)
	{
		echo_destroy(&echo);
		return false;
	}
	mic_stream_set_output(echo->mic, (void*)echo, echo_effect_run);
	mic_stream_start(echo->mic);

    local_irq_disable();
	__this = echo;
	local_irq_enable();

	printf("--------------------------echo start ok\n");

	return true;
}

void echo_stop(void)
{
	echo_destroy(&__this);
}

u8 echo_get_status(void)
{
	return (__this ? 1 : 0);
}

void echo_set_dvol(u8 vol)
{
	if(__this)	
	{
		digital_vol_set(__this->d_vol, vol);	
	}
}

u8 echo_get_dvol(void)
{
	if(__this)	
	{
		return digital_vol_get(__this->d_vol);	
	}
	return 0;
}




