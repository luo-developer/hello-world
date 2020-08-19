#include "audio_mix_splicing.h"

#if (defined (TCFG_MIXER_EXT_ENABLE) && (TCFG_MIXER_EXT_ENABLE))

struct __d2q
{
	u16 remain_points;
	u16 max_points;
	s16 outbuf[MIX_OUTPUT_DUAL_TO_QUAD_POINTS*2];	
	u8  output_type;	
};
static struct __d2q d2q =
{
	.max_points = MIX_OUTPUT_DUAL_TO_QUAD_POINTS*2,
	.remain_points = 0,
	.output_type = MIX_OUTPUT_DUAL_TO_QUAD_TYPE,
};

extern struct audio_decoder_task decode_task;
extern u32 audio_output_data(s16 *data, u16 len);

static inline int mix_output_dual_to_quad_copy_only(struct audio_mixer *mixer, s16 *data, u16 len)
{
	int wlen = 0;
	int ret = 0;
	u16 offset = 0;
	u16 dual_points = 0;
	if(d2q.remain_points)
	{
        offset = d2q.max_points - d2q.remain_points;
		ret = audio_output_data(d2q.outbuf+offset, (d2q.remain_points << 1));
		d2q.remain_points -= (ret>>1);
		return 0;		
	}
	dual_points = len>>1;
	if(dual_points >= MIX_OUTPUT_DUAL_TO_QUAD_POINTS)
	{
		dual_points = MIX_OUTPUT_DUAL_TO_QUAD_POINTS;
	}
	wlen = dual_points<<1;

	d2q.remain_points = dual_points<<1;
	pcm_dual_to_qual(d2q.outbuf, data, wlen);	

	offset = d2q.max_points - d2q.remain_points;
	ret = audio_output_data(d2q.outbuf+offset, (d2q.remain_points << 1));
	d2q.remain_points -= (ret>>1);
	if(d2q.remain_points == 0)
	{
		///一次就输出完， 可以启动继续解码
        audio_decoder_resume_all(&decode_task);
	}

	return wlen;
}

static inline int mix_output_dual_fill_main(struct audio_mixer *mixer, s16 *data, u16 len)
{
	int wlen = 0;
	int ret = 0;
	u16 offset = 0;
	u16 dual_points = 0;
	///检查数据是否还有未输出完整的数据， 没有输出完，mix output消耗0输出， 等剩余数据输出完，再继续
	if(d2q.remain_points)
	{
        offset = d2q.max_points - d2q.remain_points;
		ret = audio_output_data(d2q.outbuf+offset, (d2q.remain_points << 1));
		d2q.remain_points -= (ret>>1);
		if(d2q.remain_points == 0)
		{
			///一次就输出完， 可以启动继续解码
			audio_decoder_resume_all(&decode_task);
		}
		return 0;		
	}
	dual_points = len>>1;
	if(dual_points >= MIX_OUTPUT_DUAL_TO_QUAD_POINTS)
	{
		dual_points = MIX_OUTPUT_DUAL_TO_QUAD_POINTS;
	}
	wlen = dual_points<<1;
	d2q.remain_points = dual_points<<1;
	if(d2q.output_type == MIX_OUTPUT_DUAL_TO_QUAD_TYPE_MAINFLFR_EXT0RLRR)
	{
		pcm_fill_flfr_2_qual(d2q.outbuf, data, wlen);
	}
	else if(d2q.output_type == MIX_OUTPUT_DUAL_TO_QUAD_TYPE_MAINRLRR_EXT0FLFR)
	{
		pcm_fill_rlrr_2_qual(d2q.outbuf, data, wlen);
	}
	return wlen;
}

static inline int mix_output_dual_fill_ext0(struct audio_mixer *mixer, s16 *data, u16 len)
{
	int wlen = 0;
	if(d2q.output_type == MIX_OUTPUT_DUAL_TO_QUAD_TYPE_MAINFLFR_EXT0RLRR
			|| d2q.output_type == MIX_OUTPUT_DUAL_TO_QUAD_TYPE_MAINRLRR_EXT0FLFR)
	{
		int ret = 0;
		u16 offset = 0;
		u16 dual_points = 0;
		dual_points = len>>1;
		if(dual_points >= MIX_OUTPUT_DUAL_TO_QUAD_POINTS)
		{
			dual_points = MIX_OUTPUT_DUAL_TO_QUAD_POINTS;
		}
		wlen = dual_points<<1;
		if(d2q.remain_points != (dual_points<<1))
		{
			ASSERT(0, "%s,%d, points err!!\n", __FUNCTION__, __LINE__);			
		}
		/* d2q.remain_points = dual_points<<1; */
		if(d2q.output_type == MIX_OUTPUT_DUAL_TO_QUAD_TYPE_MAINFLFR_EXT0RLRR)
		{

#if (defined(RL_RR_MIX_ENABLE) && (RL_RR_MIX_ENABLE != 0))
			pcm_dual_mix_to_dual(data, data,  wlen);
#endif//RL_RR_MIX_ENABLE

			pcm_fill_rlrr_2_qual(d2q.outbuf, data, wlen);

#if ((defined (USB_MIC_DATA_FROM_MIX_ENABLE)) && USB_MIC_DATA_FROM_MIX_ENABLE)
			extern int usb_audio_mic_write(void *data, u16 len);
			usb_audio_mic_write(data, wlen);
#endif//USB_MIC_DATA_FROM_MIX_ENABLE

		}
		else if(d2q.output_type == MIX_OUTPUT_DUAL_TO_QUAD_TYPE_MAINRLRR_EXT0FLFR)
		{
#if (defined(RL_RR_MIX_ENABLE) && (RL_RR_MIX_ENABLE != 0))
			pcm_dual_mix_to_dual(data, data,  wlen);
#endif//RL_RR_MIX_ENABLE

			pcm_fill_flfr_2_qual(d2q.outbuf, data, wlen);
		}
		//数据组装好， 马上启动一下解码运转mix输出, 目的马上检测到有remain， 可以及时输出
		audio_decoder_resume_all(&decode_task);
	}
	return wlen;
}


int mix_output_dual_to_quad(struct audio_mixer *mixer, s16 *data, u16 len)
{
	int wlen = 0;
	switch(d2q.output_type)	
	{
		case MIX_OUTPUT_DUAL_TO_QUAD_TYPE_COPY_ONLY:
			wlen = mix_output_dual_to_quad_copy_only(mixer, data, len);
			break;
		case MIX_OUTPUT_DUAL_TO_QUAD_TYPE_MAINFLFR_EXT0RLRR:
		case MIX_OUTPUT_DUAL_TO_QUAD_TYPE_MAINRLRR_EXT0FLFR:
			wlen = mix_output_dual_fill_main(mixer, data, len);
			break;
		default:
			ASSERT(0, "%s,%d, type err = %d!!\n", __FUNCTION__, __LINE__, d2q.output_type);			
			break;
	}
	return wlen;
}

int mix_output_ext_dual_to_quad_handler(struct audio_mixer *mixer, s16 *data, u16 len, u8 idx)
{
	if(idx == 0)		
	{
		mix_output_dual_fill_ext0(mixer, data, len);	
	}
	return 0;
}

#endif//TCFG_MIXER_EXT_ENABLE

