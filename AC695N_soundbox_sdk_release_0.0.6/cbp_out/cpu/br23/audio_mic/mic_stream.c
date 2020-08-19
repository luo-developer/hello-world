#include "mic_stream.h"
#include "app_config.h"
#include "system/includes.h"
#include "audio_splicing.h"
#include "audio_config.h"
#include "asm/dac.h"
#include "audio_enc.h"

#define MIC_STREAM_TASK_NAME				"mic_stream"

struct __mic_stream
{
	struct __mic_stream_parm		*parm;
	struct __mic_stream_io 			out;
	u8								*temp_buf;
	u8								*adc_buf;
	cbuffer_t 						adc_cbuf;
	OS_SEM 							sem;
	volatile u8						busy:		1;
	volatile u8						release:	1;
	volatile u8						revert:		6;
};

#define MIC_SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

extern struct audio_dac_hdl dac_hdl;
extern int audio_three_adc_open(void);
extern void audio_three_adc_close();
extern void three_adc_mic_enable(u8 mark);

void mic_stream_adc_resume(void *priv)
{
	struct __mic_stream *stream = (struct __mic_stream *)priv;
	if(stream != NULL && (stream->release == 0))	
	{
		os_sem_set(&stream->sem, 0);
		os_sem_post(&stream->sem);
	}
}

static int mic_stream_effects_run(struct __mic_stream *stream)
{
	u32 tmp_len, wlen;
	s16 *read_buf = (s16 *)(stream->temp_buf + stream->parm->point_unit*2*3);
	s16 *dual_buf = (s16 *)(stream->temp_buf + stream->parm->point_unit*2*2);
	s16 *qual_buf = (s16 *)stream->temp_buf;

	u8 dac_chls = audio_dac_get_channel(&dac_hdl);

	int res = os_sem_pend(&stream->sem, 0);
	if(res)
	{
		return -1;		
	}
	if(stream->release)
	{
		return -1;		
	}

	wlen = cbuf_read(&stream->adc_cbuf, read_buf, stream->parm->point_unit*2);
	if(wlen)
	{
		if(stream->out.func)		
		{
			wlen = stream->out.func(stream->out.priv, read_buf, dual_buf, stream->parm->point_unit*2, stream->parm->point_unit*2*2);
			if(wlen != stream->parm->point_unit*2*2)
			{
				putchar('E');
				return 0;	
			}

			if(dac_chls == 1)
			{
				pcm_dual_to_single(dual_buf, dual_buf, stream->parm->point_unit*2*2);
				tmp_len = stream->parm->point_unit*2;		
				wlen = audio_dac_mix_write(&dac_hdl, dual_buf, tmp_len);
			}
			if(dac_chls == 2)
			{
				tmp_len = stream->parm->point_unit*2*2;
				wlen = audio_dac_mix_write(&dac_hdl, dual_buf, tmp_len);
			}
			else if(dac_chls == 4)
			{
				pcm_dual_to_qual(qual_buf, dual_buf, stream->parm->point_unit*2*2);
				tmp_len = stream->parm->point_unit*2*4;
				wlen = audio_dac_mix_write(&dac_hdl, qual_buf, tmp_len);
			}
		}
		else
		{
			if(dac_chls == 1)
			{
				tmp_len = stream->parm->point_unit*2;		
				wlen = audio_dac_mix_write(&dac_hdl, read_buf, tmp_len);
			}
			if(dac_chls == 2)
			{
				pcm_single_to_dual(dual_buf, read_buf, stream->parm->point_unit*2);
				tmp_len = stream->parm->point_unit*2*2;
				wlen = audio_dac_mix_write(&dac_hdl, dual_buf, tmp_len);
			}
			else if(dac_chls == 4)
			{
				pcm_single_to_qual(qual_buf, read_buf, stream->parm->point_unit*2);
				tmp_len = stream->parm->point_unit*2*4;
				wlen = audio_dac_mix_write(&dac_hdl, qual_buf, tmp_len);
			}
		}
		if(wlen < tmp_len)
		{
			putchar('D');		
		}
	}
	else
	{
		putchar('R');		
	}
	return 0;
}

static void mic_stream_task_deal(void *p)
{
	int res = 0;
	struct __mic_stream *stream = (struct __mic_stream *)p;
	stream->busy = 1;
	while(1)
	{
		res = mic_stream_effects_run(stream);
		if(res)
		{
			///等待删除线程
			stream->busy = 0;
			while(1)		
			{
				os_time_dly(10000);		
			}
		}
	}
}

struct __mic_stream *mic_stream_creat(struct __mic_stream_parm *parm)
{
	int err = 0;
	struct __mic_stream_parm *p = parm;
	if(parm == NULL)
	{
		printf("%s parm err\n", __FUNCTION__);
		return NULL;	
	}
	printf("p->dac_delay = %d\n",p->dac_delay);
	printf("p->point_unit = %d\n",p->point_unit);
	printf("p->sample_rate = %d\n",p->sample_rate);

	u32 offset = 0;
	u32 buf_size = MIC_SIZEOF_ALIN(sizeof(struct __mic_stream), 4)
					+ MIC_SIZEOF_ALIN((p->point_unit * 4 * 2), 4)
					+ MIC_SIZEOF_ALIN((p->point_unit * 2 * 3), 4);

	u8 *buf = zalloc(buf_size);
	if(buf == NULL)
	{
		return NULL;		
	}

	struct __mic_stream *stream = (struct __mic_stream *)buf;
	offset += MIC_SIZEOF_ALIN(sizeof(struct __mic_stream), 4);

	stream->temp_buf = (u8*)buf + offset;
	offset += MIC_SIZEOF_ALIN((p->point_unit * 4 * 2), 4);

	stream->adc_buf = (u8*)buf + offset;
	offset += MIC_SIZEOF_ALIN((p->point_unit * 2 * 3), 4);

	stream->parm = p;

	os_sem_create(&stream->sem, 0);
	cbuf_init(&stream->adc_cbuf, stream->adc_buf, MIC_SIZEOF_ALIN((p->point_unit * 2 * 3), 4));

	audio_dac_mix_ch_open(&dac_hdl, p->dac_delay);//初始化dac混合通道

	err = task_create(mic_stream_task_deal, (void *)stream, MIC_STREAM_TASK_NAME);
	if (err != OS_NO_ERR) {
		printf("%s creat fail %x\n",__FUNCTION__,  err);
		free(stream);
		return NULL;
	}


	printf("mic stream creat ok\n");

	return stream;
}

void mic_stream_set_output(struct __mic_stream  *stream, void *priv, u32 (*func)(void *priv, void *in, void *out, u32 inlen, u32 outlen))
{
	if(stream)	
	{
		stream->out.priv = priv;
		stream->out.func = func;
	}
}

bool mic_stream_start(struct __mic_stream  *stream)
{
	if(stream)		
	{
		if (audio_three_adc_open() == 0) 
		{
			printf("%s, mic adc open success \n", __FUNCTION__);
			set_mic_cbuf_hdl(&stream->adc_cbuf);
			set_mic_resume_hdl(mic_stream_adc_resume, stream);
			three_adc_mic_enable(1);
			printf("mic_stream_start ok\n");
			return true;		
		}
	}
	return false;
}

void mic_stream_destroy(struct __mic_stream **hdl)
{
	int err = 0;
	if((hdl == NULL) || (*hdl == NULL))
	{
		return ;		
	}

	struct __mic_stream *stream = *hdl;

    three_adc_mic_enable(0);
    audio_three_adc_close();

	stream->release = 1;

	os_sem_set(&stream->sem, 0);
	os_sem_post(&stream->sem);

	while(stream->busy)
	{
		os_time_dly(5);		
	}

	printf("%s wait busy ok!!!\n", __FUNCTION__);

	err = task_kill(MIC_STREAM_TASK_NAME);
	os_sem_del(&stream->sem, 0);
	audio_dac_mix_ch_close(&dac_hdl);
    local_irq_disable();
	free(*hdl);
	*hdl = NULL;
	/* p_stream = NULL; */
	local_irq_enable();
}

