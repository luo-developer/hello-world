#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_online_cfg.h"
#include "audio_drc.h"
#include "common/Resample_api.h"
#include "audio_reverb.h"
#include "clock_cfg.h"
#include "audio_config.h"
#include "audio_pitch.h"
#include "filtparm_api.h"
#include "audio_eq.h"
#include "audio_dec.h"
#include "audio_digital_vol.h"
#include "audio_enc.h"
#include "audio_dec_board_cfg.h"
#include "effects_config.h"
#define LOG_TAG     "[APP-REVERB]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

#define REVREB_EN     1
#define REVERB_RUN_POINT_NUM		160	//固定160个点
#define REVERBN_MODE                0
#define ECHO_MODE                   1

#define HOWLING_ENABLE    			TCFG_REVERB_HOWLING_EN //啸叫抑制
#define PITCH_ENABLE                TCFG_REVERB_PITCH_EN //变音
#define REVERB_EQ_ENABLE            TCFG_REVERB_EQ_EN //高低音
#define REVERB_DODGE_ENABLE         TCFG_REVERB_DODGE_EN//闪避 
#define REVERB_MONO2STERO            0
#define NOISEGATE_ENABLE            1
#define REVERB_MODE_SEL             REVERBN_MODE  
/* #define REVERB_MODE_SEL             ECHO_MODE   */

#define REVERB_PARM_FADE_EN     	1

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
#undef NOISEGATE_ENABLE
#define NOISEGATE_ENABLE            0
#undef REVERB_MODE_SEL
#define REVERB_MODE_SEL             ECHO_MODE 
#endif

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
extern u32 audio_output_rate(int input_rate);
extern u32 audio_output_channel_num(void);
extern int audio_output_set_start_volume(u8 state);

extern struct audio_src_handle *audio_hw_resample_open(void *priv, int (*output_handler)(void *, void *, int),
        u8 channel, u16 input_sample_rate, u16 output_sample_rate);
extern void audio_hw_resample_close(struct audio_src_handle *hdl);

extern struct audio_adc_hdl adc_hdl;

extern struct audio_decoder_task decode_task;
extern struct audio_mixer mixer;

extern void reverb_dodge_open(void *reverb, u8 dodge_en);
extern void reverb_dodge_close();
extern void reverb_dodge_run(s16 *data, u32 len, u8 ch_num);
extern int audio_three_adc_open(void);
extern void audio_three_adc_close();
extern void three_adc_mic_enable(u8 mark);
extern void set_mic_cbuf_hdl(cbuffer_t *mic_cbuf);
extern void three_adc_mic_set_gain(u8 level);


static u8 audio_reverb_dec_get_volume_defualt(u8 state)
{
	return app_audio_get_volume(APP_AUDIO_CURRENT_STATE);	
}

REGISTER_DEC_BOARD_PARAM(reverb_dec_param) = 
{
	.name = "reverb_dec_param",
	.d_vol_en = 1,		
	.d_vol_max = (void *)app_audio_get_max_volume,		
	.d_vol_cur = (void *)audio_reverb_dec_get_volume_defualt,
	.d_vol_fade_step = 2,		
};


//*******************混响接口h***********************//
REVERBN_API_STRUCT *open_reverb(REVERBN_PARM_SET *reverb_seting, u16 sample_rate)
{
    REVERBN_API_STRUCT *reverb_api_obj;

    int buf_lent;

    reverb_api_obj = zalloc(sizeof(REVERBN_API_STRUCT));
    if (!reverb_api_obj) {
        return NULL;
    }
    reverb_api_obj->func_api = get_reverbn_func_api();

    //初始化混响参数
    if (reverb_seting) {
        memcpy(&reverb_api_obj->parm, reverb_seting, sizeof(REVERBN_PARM_SET));
    } else {
        reverb_api_obj->parm.dry = REVERB_dry;	//[0:200]%
        reverb_api_obj->parm.wet = REVERB_wet;	  //[0:300]%
        reverb_api_obj->parm.delay = REVERB_delay;	//[0-100]ms
        reverb_api_obj->parm.rot60= REVERB_rot60;	//[0:15000]ms  //反射系数 值越大 发射越厉害 衰减慢
        reverb_api_obj->parm.Erwet = REVERB_Erwet;	//[5:250]%
        reverb_api_obj->parm.Erfactor = REVERB_Erfactor;	//[50:250]%
        reverb_api_obj->parm.Ewidth = REVERB_Ewidth;	//[-100:100]%
        reverb_api_obj->parm.Ertolate  = REVERB_Ertolate;	//[0:100]%
        reverb_api_obj->parm.predelay  = REVERB_predelay;	//[0:20]ms
        reverb_api_obj->parm.width  = REVERB_width;	//[0:100]%  //参数无效、可以通过EQ调节
        reverb_api_obj->parm.diffusion  = REVERB_diffusion;	//[0:100]%
        reverb_api_obj->parm.dampinglpf  = REVERB_dampinglpf;	//[0:18k]
        reverb_api_obj->parm.basslpf = REVERB_basslpf;	//[0:1.1k]//参数无效、可以通过EQ调节
        reverb_api_obj->parm.bassB = REVERB_bassB;	//[0:80]%//参数无效、可以通过EQ调节
        reverb_api_obj->parm.inputlpf  = REVERB_inputlpf;	//[0:18k]//参数无效、可以通过EQ调节
        reverb_api_obj->parm.outputlpf  = REVERB_outputlpf;	//[0:18k]//参数无效、可以通过EQ调节

    }
    if (sample_rate) {
        /* reverb_api_obj->parm.sr = sample_rate; */
    }

    //申请混响空间，初始
    buf_lent = reverb_api_obj->func_api->need_buf(reverb_api_obj->ptr, &reverb_api_obj->parm,sample_rate);
    reverb_api_obj->ptr = zalloc(buf_lent);
    if (!reverb_api_obj->ptr) {
        free(reverb_api_obj);
        return NULL;
    }

    reverb_api_obj->func_api->open(reverb_api_obj->ptr, &reverb_api_obj->parm,sample_rate);
    return reverb_api_obj;
}


void  close_reverb(REVERBN_API_STRUCT *reverb_api_obj)
{
    if (reverb_api_obj) {
        if (reverb_api_obj->ptr) {
            free(reverb_api_obj->ptr);
            reverb_api_obj->ptr = NULL;
        }
        free(reverb_api_obj);
    }
}

static u8 update_reverb_parm_cpy(REVERBN_API_STRUCT *reverb_api_obj, REVERBN_PARM_SET *reverb_seting)
{
    REVERBN_PARM_SET *parm = &reverb_api_obj->parm;
    memcpy(reverb_seting,parm,sizeof(REVERBN_PARM_SET));
    printf("\n\n\n----------------------");
    return 0;
}

u8 update_reverb_parm_fader(REVERBN_API_STRUCT *reverb_api_obj, REVERBN_PARM_SET *reverb_seting)
{
    u8 ret = 0;
    u8 update = 0;
    if (reverb_api_obj){
        REVERBN_PARM_SET *parm = &reverb_api_obj->parm;
		if(parm->wet != reverb_seting->wet){
            update = 1;
            if(parm->wet>reverb_seting->wet){
                parm->wet--;
            }else{
                parm->wet++;
            }
        }		
#if 0
		if(parm->dry != reverb_seting->dry){
            update = 1;
            if(parm->dry>reverb_seting->dry){
                parm->dry--;
            }else{
                parm->dry++;
            }
        }
        if(parm->delay != reverb_seting->delay){
            update = 1;
            if(parm->delay>reverb_seting->delay){
                parm->delay--;
            }else{
                parm->delay++;
            }
        }       
	   	if(parm->rot60 != reverb_seting->rot60){
            update = 1;
            if(parm->rot60>reverb_seting->rot60){
                parm->rot60--;
            }else{
                parm->rot60++;
            }
        }	   	
		if(parm->Erwet != reverb_seting->Erwet){
            update = 1;
            if(parm->Erwet>reverb_seting->Erwet){
                parm->Erwet--;
            }else{
                parm->Erwet++;
            }
        }	
		if(parm->Erfactor != reverb_seting->Erfactor){
            update = 1;
            if(parm->Erfactor>reverb_seting->Erfactor){
                parm->Erfactor--;
            }else{
                parm->Erfactor++;
            }
        }		
		if(parm->Ewidth != reverb_seting->Ewidth){
            update = 1;
            if(parm->Ewidth>reverb_seting->Ewidth){
                parm->Ewidth--;
            }else{
                parm->Ewidth++;
            }
        }		
		if(parm->Ertolate != reverb_seting->Ertolate){
            update = 1;
            if(parm->Ertolate>reverb_seting->Ertolate){
                parm->Ertolate--;
            }else{
                parm->Ertolate++;
            }
        }		
		if(parm->predelay != reverb_seting->predelay){
            update = 1;
            if(parm->predelay>reverb_seting->predelay){
                parm->predelay--;
            }else{
                parm->predelay++;
            }
        }

		if(parm->width != reverb_seting->width){
            update = 1;
			parm->width = reverb_seting->width;
		}
		if(parm->diffusion != reverb_seting->diffusion){
            update = 1;
			parm->diffusion = reverb_seting->diffusion;
		}
		if(parm->dampinglpf != reverb_seting->dampinglpf){
            update = 1;
			parm->dampinglpf = reverb_seting->dampinglpf;
		}
		if(parm->basslpf != reverb_seting->basslpf){
            update = 1;
			parm->basslpf = reverb_seting->basslpf;
		}
		if(parm->bassB != reverb_seting->bassB){
            update = 1;
			parm->bassB = reverb_seting->bassB;
		}
		if(parm->inputlpf != reverb_seting->inputlpf){
            update = 1;
			parm->inputlpf = reverb_seting->inputlpf;
		}
		if(parm->outputlpf != reverb_seting->outputlpf){
            update = 1;
			parm->outputlpf = reverb_seting->outputlpf;
		}
#endif

        if(update){
            reverb_api_obj->func_api->init(reverb_api_obj->ptr, parm);
        }
    }
    return ret;
}
void update_reverb_parm(REVERBN_API_STRUCT *reverb_api_obj, REVERBN_PARM_SET *reverb_seting)
{
    reverb_api_obj->func_api->init(reverb_api_obj->ptr, reverb_seting);
}



ECHO_API_STRUCT *open_echo(ECHO_PARM_SET *echo_seting, u16 sample_rate)
{
	ECHO_API_STRUCT *echo_api_obj;

    int buf_lent;

    echo_api_obj = zalloc(sizeof(ECHO_API_STRUCT));
    if (!echo_api_obj) {
        return NULL;
    }
    echo_api_obj->func_api = get_echo_func_api();

    //初始化混响参数
    if (echo_seting) {
        memcpy(&echo_api_obj->echo_parm_obj, echo_seting, sizeof(ECHO_PARM_SET));
	} else {
		echo_api_obj->echo_parm_obj.delay = ECHO_delay;	//回声延时时间 0- 300 
		echo_api_obj->echo_parm_obj.decayval = ECHO_decayval;	//衰减系数 0-70%
		echo_api_obj->echo_parm_obj.direct_sound_enable = ECHO_direct_sound_enable;	//直达声使能 0:1
		echo_api_obj->echo_parm_obj.filt_enable = ECHO_filt_enable;	//发散滤波器使能 0:1
	}

	echo_api_obj->echo_fix_parm.wetgain = 2048;	//湿声增益：[0:4096]
	echo_api_obj->echo_fix_parm.drygain = 0;	//干声增益: [0:4096]
#if (defined(TCFG_REVERB_SAMPLERATE_DEFUALT))
	echo_api_obj->echo_fix_parm.sr = TCFG_REVERB_SAMPLERATE_DEFUALT; //配置输入的采样率，影响need_buf 大小
#else
	echo_api_obj->echo_fix_parm.sr = 16000; //配置输入的采样率，影响need_buf 大小
#endif//TCFG_REVERB_SAMPLERATE_DEFUALT

	echo_api_obj->echo_fix_parm.max_ms = 200;	//所需要的最大延时，影响 need_buf 大小


    if (sample_rate) {
        echo_api_obj->echo_fix_parm.sr = sample_rate;
    }

    //申请混响空间，初始
    buf_lent = echo_api_obj->func_api->need_buf(echo_api_obj->ptr, &echo_api_obj->echo_fix_parm);
    echo_api_obj->ptr = zalloc(buf_lent);
    if (!echo_api_obj->ptr) {
        free(echo_api_obj);
        return NULL;
    }

    echo_api_obj->func_api->open(echo_api_obj->ptr, &echo_api_obj->echo_parm_obj,&echo_api_obj->echo_fix_parm);
    return echo_api_obj; 	
}

void  close_echo(ECHO_API_STRUCT *echo_api_obj)
{
    if (echo_api_obj) {
        if (echo_api_obj->ptr) {
            free(echo_api_obj->ptr);
            echo_api_obj->ptr = NULL;
        }
        free(echo_api_obj);
    }
}

u8 update_echo_parm_tager(ECHO_API_STRUCT *echo_api_obj, ECHO_PARM_SET *echo_seting)
{
    ECHO_PARM_SET *parm = &echo_api_obj->echo_parm_obj;
    memcpy(echo_seting,parm,sizeof(ECHO_PARM_SET));
    printf("\n\n\n----------------------");
    printf("空间  delay (0-300)ms:                  | %d\n", echo_seting->delay);
    printf("衰减  decayval (0-70%):                 | %d\n", echo_seting->decayval);
    printf("  direct_sound_enable(0-4096):                   | %d\n", echo_seting->direct_sound_enable);
    printf("  filt_enable(0-4096):                   | %d\n", echo_seting->filt_enable);
    printf("----------------------\n\n\n");
    return 0;
}
u8 update_echo_parm_fader(ECHO_API_STRUCT *echo_api_obj, ECHO_PARM_SET *echo_seting)
{
    u8 ret = 0;
    u8 update = 0;
    if (echo_api_obj){
        ECHO_PARM_SET *parm = &echo_api_obj->echo_parm_obj;
        if(parm->delay != echo_seting->delay){
            update = 1;
            if(parm->delay>echo_seting->delay){
                parm->delay--;
            }else{
                parm->delay++;
            }
        }
        if(parm->decayval != echo_seting->decayval){
            update = 1;
            if(parm->decayval>echo_seting->decayval){
                parm->decayval--;
            }else{
                parm->decayval++;
            }
        }
        if(update){
            echo_api_obj->func_api->init(echo_api_obj->ptr, parm);
        }
    }
    return ret;
}
void update_echo_parm(ECHO_API_STRUCT *echo_api_obj, ECHO_PARM_SET *echo_seting)
{
    echo_api_obj->func_api->init(echo_api_obj->ptr, echo_seting);
}


//*******************啸叫抑制***********************//
#if HOWLING_ENABLE
HOWLING_API_STRUCT *open_howling(HOWLING_PARM_SET *howl_para, u16 sample_rate, u8 channel)
{
    HOWLING_API_STRUCT *howling_hdl = zalloc(sizeof(HOWLING_API_STRUCT));
    if (!howling_hdl) {
        return NULL;
    }
    howling_hdl->ptr = zalloc(get_howling_buf());
    if (!howling_hdl->ptr) {
        free(howling_hdl);
        return NULL;
    }

    if (howl_para) {
        memcpy(&howling_hdl->parm, howl_para, sizeof(HOWLING_PARM_SET));
    } else {
        howling_hdl->parm.threshold = 13;
        howling_hdl->parm.depth  = 20;
        howling_hdl->parm.bandwidth = 20;
        howling_hdl->parm.attack_time = 10;
        howling_hdl->parm.release_time = 5;
        howling_hdl->parm.noise_threshold = -25000;
        howling_hdl->parm.low_th_gain = 0;

#if (defined(TCFG_REVERB_SAMPLERATE_DEFUALT))
        howling_hdl->parm.sample_rate = TCFG_REVERB_SAMPLERATE_DEFUALT;
#else
        howling_hdl->parm.sample_rate = 16000;
#endif//TCFG_REVERB_SAMPLERATE_DEFUALT

        howling_hdl->parm.channel = 1;
    }
    if (sample_rate) {
        howling_hdl->parm.sample_rate = sample_rate;
    }
    if (channel) {
        howling_hdl->parm.channel = channel;
    }

    howling_init(howling_hdl->ptr,
                 howling_hdl->parm.threshold,
                 howling_hdl->parm.depth,
                 howling_hdl->parm.bandwidth,
                 howling_hdl->parm.attack_time,
                 howling_hdl->parm.release_time,
                 howling_hdl->parm.noise_threshold,
                 howling_hdl->parm.low_th_gain,
                 howling_hdl->parm.sample_rate,
                 howling_hdl->parm.channel);
    return howling_hdl;
}

void close_howling(HOWLING_API_STRUCT *holing_hdl)
{
    if (holing_hdl) {
        if (holing_hdl->ptr) {
            free(holing_hdl->ptr);
            holing_hdl->ptr = NULL;
        }
        free(holing_hdl);
    }
}
#endif

#if REVERB_MONO2STERO
REVERB_MONO2STERO_API_STRUCT *init_reverb_mono2stero(void)
{
    int buf_lent;
    REVERB_MONO2STERO_API_STRUCT *mono2stero_hdl;
    mono2stero_hdl = zalloc(sizeof(RMONO2STEREO_FUNC_API));
    if(!mono2stero_hdl){
        return NULL;
    }
    mono2stero_hdl->func_api = get_rm2s_func_api();
    buf_lent = mono2stero_hdl->func_api->need_buf();
    mono2stero_hdl->ptr = malloc(buf_lent);
    if(!mono2stero_hdl->ptr){
        free(mono2stero_hdl);
        return NULL;
    }
    return mono2stero_hdl;
}
void close_reverb_mono2stero(REVERB_MONO2STERO_API_STRUCT *hdl)
{
    if(hdl){
       if(hdl->ptr){
            free(hdl->ptr);
       }
       free(hdl);
       hdl = NULL;
    }
}
#endif // REVERB_MONO2STERO

/******************************************************************/
//*******************噪声抑制***********************//
#if NOISEGATE_ENABLE

NOISEGATE_API_STRUCT *open_noisegate(NOISEGATE_PARM *gate_parm, u16 sample_rate, u8 channel)
{
	printf("\n--func=%s\n", __FUNCTION__);
	NOISEGATE_API_STRUCT* noisegate_hdl = zalloc(sizeof(NOISEGATE_API_STRUCT));
	if(!noisegate_hdl){	
		return NULL;	
	}
	noisegate_hdl->ptr = zalloc(noiseGate_buf());
	if(!noisegate_hdl->ptr){
		free(noisegate_hdl);
		return NULL;	
	}
	if(gate_parm){
		memcpy(&noisegate_hdl->parm,gate_parm,sizeof(NOISEGATE_PARM));		
	}else{
		noisegate_hdl->parm.attackTime = NOISEGATE_attacktime;//启动时间ms
		noisegate_hdl->parm.releaseTime = NOISEGATE_releasetime;	//释放时间
		noisegate_hdl->parm.threshold = NOISEGAT_threshold;//阈值mdb
		noisegate_hdl->parm.low_th_gain = NOISEGATE_low_th_gain;//低于阈值的增益，输入值为（0，1）*2^30
#if(defined(TCFG_REVERB_SAMPLERATE_DEFUALT))
		noisegate_hdl->parm.sampleRate = TCFG_REVERB_SAMPLERATE_DEFUALT;
#else
		noisegate_hdl->parm.sampleRate = 16000;
#endif
		noisegate_hdl->parm.channel = 1;
	}
	if(sample_rate){
		noisegate_hdl->parm.sampleRate = sample_rate;
	}
	if(channel){
		noisegate_hdl->parm.channel = channel;
	}
	printf("\n\n noisegate parm: attackTime[%d],releaseTime[%d],threshold[%d],low_th_gain[%d]\n\n",	
			noisegate_hdl->parm.attackTime,
			noisegate_hdl->parm.releaseTime,
			noisegate_hdl->parm.threshold,
			noisegate_hdl->parm.low_th_gain
			);
 	noiseGate_init(noisegate_hdl->ptr,
			noisegate_hdl->parm.attackTime,
			noisegate_hdl->parm.releaseTime,
			noisegate_hdl->parm.threshold,
			noisegate_hdl->parm.low_th_gain,
			noisegate_hdl->parm.sampleRate,
			noisegate_hdl->parm.channel);		
	return noisegate_hdl;	
}

void close_noisegete(NOISEGATE_API_STRUCT *gate_hdl)
{
	if(gate_hdl){	
		if(gate_hdl->ptr){	
			free(gate_hdl->ptr);
			gate_hdl->ptr = NULL;	
		}
		free(gate_hdl);
	}
}
void updat_noisegate_parm(NOISEGATE_API_STRUCT *gate_hdl,int attackTime,int releaseTime,int threshold,int low_th_gain)
{
	NOISEGATE_API_STRUCT* noisegate_hdl =gate_hdl;
	noisegate_hdl->parm.attackTime = attackTime;/*1~1500ms*/
	noisegate_hdl->parm.releaseTime = releaseTime;/*1~300ms*/
	noisegate_hdl->parm.threshold = threshold;
	noisegate_hdl->parm.low_th_gain = low_th_gain;
	noiseGate_init(noisegate_hdl->ptr,
			noisegate_hdl->parm.attackTime,
			noisegate_hdl->parm.releaseTime,
			noisegate_hdl->parm.threshold,
			noisegate_hdl->parm.low_th_gain,
			noisegate_hdl->parm.sampleRate,
			noisegate_hdl->parm.channel);		
}

#endif

/******************************************************************/
//************************* MIC+reverb  to DAC API *****************************//
#define ADC_BUF_NUM        	2
#define ADC_CH_NUM         	1
#define ADC_IRQ_POINTS     	256
#define ADC_BUFS_SIZE      	(ADC_BUF_NUM *ADC_CH_NUM* ADC_IRQ_POINTS)
#define PCM_BUF_RUN_MS		50
#define PCM_BUF_LEN  		(44100*PCM_BUF_RUN_MS/1000*2) //

#define PCM_SRC_SIZE_CNT	2	// 多少次判断一下size

#define PCM_RATE_MAX_STEP		80
#define PCM_RATE_INC_STEP       5
#define PCM_RATE_DEC_STEP       5

#if (defined(TCFG_EFFECTS_ENABLE) && (TCFG_EFFECTS_ENABLE != 0))
#define REVERB_EQ_SECTION  (EQ_SECTION_MAX + 3)
#else
#define REVERB_EQ_SECTION  (3)
#endif
enum {
    REVERB_STATUS_STOP = 0,
    REVERB_STATUS_START,
    REVERB_STATUS_PAUSE,
};
struct s_reverb_hdl {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;

#ifndef THREE_ADC_ENABLE
    s16 adc_buf[ADC_BUFS_SIZE];
#endif
    int mic_gain;
    u16 mic_sr;
    u16 src_out_sr;
    u16 src_out_sr_n;
    int begin_size;
    int top_size;
    int bottom_size;
    int audio_new_rate;
	int rate_offset;
	s16 adjust_step;
    u8 sync_start;
	u8 data_cnt;
	u32 data_size;
    struct audio_src_handle *src_sync;

    u8 pcm_buf[PCM_BUF_LEN];
    cbuffer_t pcm_cbuf;

    u8  run_buf[REVERB_RUN_POINT_NUM * 2 * 2];
    u16 run_r;
    u16 run_len;

    /* u8 stop; */
    /* u8 dec_start; */
    /* u8 dec_pause; */

    u32 status : 2;
    u32 out_ch_num : 4;
    u32 source_ch_num : 2;
    u8 first_start;
    struct audio_decoder decoder;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;

#if(REVERB_MODE_SEL == REVERBN_MODE)
    REVERBN_API_STRUCT *p_reverb_obj;
    REVERBN_API_STRUCT p_reverb_obj_target;
#endif

#if(REVERB_MODE_SEL == ECHO_MODE)
    ECHO_API_STRUCT *p_echo_obj;
    ECHO_API_STRUCT p_echo_obj_target;
#endif	
    u8 first_tone;
#if NOISEGATE_ENABLE
	NOISEGATE_API_STRUCT *p_noisegate_obj;
#endif
#if HOWLING_ENABLE
    HOWLING_API_STRUCT *p_howling_obj;
#endif
#if PITCH_ENABLE
    s_pitch_hdl *pitch_hdl;
    u8 pitch_en;
#endif
#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)
    struct audio_eq *p_eq;
    BFILT_FUNC_API *filt_ops;
    void *filt_buf;
    int outval[3][5];             //开3个2阶滤波器的空间，给硬件eq存系数的
    u8 filt_en;
	SHOUT_WHEAT_PARM_SET shout_wheat; 
	LOW_SOUND_PARM_SET low_sound;
	HIGH_SOUND_PARM_SET high_sound;
#endif

#if REVERB_MONO2STERO
    REVERB_MONO2STERO_API_STRUCT *p_mono2stereo;
#endif // REVERB_MONO2STERO
#if (defined(REVERB_DODGE_ENABLE) && REVERB_DODGE_ENABLE != 0)
    int dodge_threshold_in;//启动闪避阈值
    int dodge_threshold_out;//退出闪避阈值
    u8 fade_tar;//闪避目标值
    u8 dodge_en;//闪避使能
    u8 dodge_filter_cnt;
    u8 ctrl[3];
    u8 vol_bk[3];
    struct pcm_energy energy;
#endif

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
    s16 *quad_data;
#endif

    struct user_audio_parm *user_hdl;
};

void mic_2_dac_rl(u8 r_en, u8 l_en);

static struct s_reverb_hdl *reverb_hdl = NULL;
static u8 pcm_dec_maigc = 0;

static void adc_output_to_buf(void *priv, s16 *data, int len)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    int wlen = cbuf_write(&reverb_hdl->pcm_cbuf, data, len);
    if (!wlen) {
        putchar('C');
    }
    audio_decoder_resume(&reverb_hdl->decoder);
}

static int reverb_src_output_handler(void *priv, void *buf, int len)
{
    int wlen = 0;
    int rlen = len;
    struct s_reverb_hdl *dec = (struct s_reverb_hdl *) priv;
    s16 *data = (s16 *)buf;
    /* return len;		 */
    do {
        wlen = audio_mixer_ch_write(&dec->mix_ch, data, rlen);
        if (!wlen) {
            break;
        }
        data += wlen / 2 ;
        rlen -= wlen;
    } while (rlen);
    /* printf("src,l:%d, wl:%d \n", len, len-rlen); */
    return (len - rlen);
}

void pcm_dec_relaese()
{
    audio_decoder_task_del_wait(&decode_task, &reverb_hdl->wait);
}
static void pcm_dec_close(void)
{
    audio_decoder_close(&reverb_hdl->decoder);
    if (reverb_hdl->src_sync) {
        audio_hw_resample_close(reverb_hdl->src_sync);
        reverb_hdl->src_sync = NULL;
    }
    audio_mixer_ch_close(&reverb_hdl->mix_ch);
    app_audio_state_exit(APP_AUDIO_STATE_MUSIC);
}

void stop_reverb_mic2dac(void)
{
    if (!reverb_hdl) {
        return;
    }
    printf("\n--func=%s\n", __FUNCTION__);

    reverb_hdl->status = REVERB_STATUS_STOP;

#ifndef THREE_ADC_ENABLE
    audio_adc_del_output_handler(&adc_hdl, &reverb_hdl->adc_output);
    audio_adc_mic_close(&reverb_hdl->mic_ch);
#else
    three_adc_mic_enable(0);
    audio_three_adc_close();
#endif
    pcm_dec_close();

#if(REVERB_MODE_SEL == REVERBN_MODE)
    close_reverb(reverb_hdl->p_reverb_obj);
#endif
#if(REVERB_MODE_SEL == ECHO_MODE)
    close_echo(reverb_hdl->p_echo_obj);
#endif
#if NOISEGATE_ENABLE
	close_noisegete(reverb_hdl->p_noisegate_obj);
#endif
#if HOWLING_ENABLE
    close_howling(reverb_hdl->p_howling_obj);
#endif
#if PITCH_ENABLE
    close_pitch(reverb_hdl->pitch_hdl);
#endif

#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)
    if (reverb_hdl->p_eq) {
        audio_eq_close(reverb_hdl->p_eq);
        free(reverb_hdl->p_eq);
        reverb_hdl->p_eq = NULL;
    }
    reverb_hdl->filt_en = 0;
#endif

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    if (reverb_hdl->user_hdl) {
        user_audio_process_close(reverb_hdl->user_hdl);
        reverb_hdl->user_hdl = NULL;
    }
#endif


#if (defined(REVERB_DODGE_ENABLE) && REVERB_DODGE_ENABLE != 0)
    reverb_dodge_close();
#endif



    pcm_dec_relaese();

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
    if (reverb_hdl->quad_data) {
        free(reverb_hdl->quad_data);
        reverb_hdl->quad_data = NULL;
    }
#endif

    free(reverb_hdl);
    reverb_hdl = NULL;


#if HOWLING_ENABLE
    clock_remove(REVERB_HOWLING_CLK);
#endif
#if PITCH_ENABLE
    clock_remove(REVERB_PITCH_CLK);
#endif

    clock_remove_set(REVERB_CLK);

}

void restart_reverb_mic2dac()
{
    if (reverb_hdl) {
        return;
    }

}

/* static inline void audio_pcm_mono_to_dual(s16 *dual_pcm, s16 *mono_pcm, int points) */
/* { */
/* s16 *mono = mono_pcm; */
/* int i = 0; */
/* u8 j = 0; */

/* for (i = 0; i < points; i++, mono++) { */
/* *dual_pcm++ = *mono; */
/* *dual_pcm++ = *mono; */
/* } */
/* } */

static int pcm_fread(struct audio_decoder *decoder, void *buf, u32 len)
{
    int rlen = 0;
    struct s_reverb_hdl *dec = container_of(decoder, struct s_reverb_hdl, decoder);
    // 固定输出单声道
    if (dec->source_ch_num == 2) {
        rlen = cbuf_read(&dec->pcm_cbuf, (void *)((int)buf + (len / 2)), len / 2);
        audio_pcm_mono_to_dual(buf, (void *)((int)buf + (len / 2)), rlen / 2);
        rlen <<= 1;
    } else {
        rlen = cbuf_read(&dec->pcm_cbuf, buf, len);
    }
    if (rlen == 0) {
        if (dec->first_start < 10) { //打开时mic未出数填0，避免挡住mixer混合，造成DAC没数据推
            dec->first_start++;
            memset(buf, 0, len);
            rlen = len;
            return rlen;
        }
        return -1;
    } else {
        dec->first_start = 100;
    }
    /* printf("fread len %d %d\n",len,rlen); */
    return rlen;
}
static const struct audio_dec_input pcm_input = {
    .coding_type = AUDIO_CODING_PCM,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
#if VFS_ENABLE == 0
#undef fread
#undef fseek
#undef flen
#endif
            .fread = pcm_fread,
            /* .fseek = file_fseek, */
            /* .flen  = linein_flen, */
        }
    }
};

static int pcm_output(struct s_reverb_hdl *dec, s16 *data, u16 len)
{
    char err = 0;
    int wlen = 0;
    int rlen = len;
    /* return rlen;//for test */

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)

#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
#else
    if (dec->src_sync) {
        if (dec->src_out_sr_n != dec->src_out_sr) {
            dec->src_out_sr = dec->src_out_sr_n;
            dec->audio_new_rate = dec->src_out_sr;
            audio_hw_src_set_rate(dec->src_sync, dec->mic_sr, dec->src_out_sr_n);
            printf(" set reverb src[%d] [%d] \n", dec->mic_sr, dec->src_out_sr_n);
        }
    }
    wlen = audio_dual_to_quad_output_src(dec->src_sync, &dec->mix_ch, data, len, REVERB_DEC_ID, dec->quad_data, NULL);
    return wlen;
#endif
#endif

    do {
        if (dec->src_sync) {
            if (dec->src_out_sr_n != dec->src_out_sr) {
                dec->src_out_sr = dec->src_out_sr_n;
                dec->audio_new_rate = dec->src_out_sr;
                audio_hw_src_set_rate(dec->src_sync, dec->mic_sr, dec->src_out_sr_n);
                printf(" set reverb src[%d] [%d] \n", dec->mic_sr, dec->src_out_sr_n);
            }
            wlen = audio_src_resample_write(dec->src_sync, data, rlen);
        } else {
            wlen = audio_mixer_ch_write(&dec->mix_ch, data, rlen);
        }
        if (!wlen) {
			//src库有修改，此处重试用法可去掉
            /* err++; */
            /* if (err < 2) { */
                /* continue; */
            /* } */
            break;
        }
        /* err = 0; */
        /* data += wlen / 2; */
        rlen -= wlen;
    } while (0);
    /* putchar('A'); */
    return len - rlen;
}
static int pcm_dec_output(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    struct s_reverb_hdl *dec = container_of(decoder, struct s_reverb_hdl, decoder);
    int wlen = 0;
    if (dec->run_len >= (REVERB_RUN_POINT_NUM * 2)) {
        wlen = pcm_output(dec, &dec->run_buf[dec->run_r], dec->run_len - dec->run_r);
        dec->run_r += wlen;
        /* printf("wl:%d, r:%d, \n", wlen, dec->run_r); */
        if (dec->run_r < dec->run_len) {
            // 没有输出完
            return 0;
        }
        // 输出完毕
        dec->run_len = 0;
    }

    wlen = len;
    if (wlen > (REVERB_RUN_POINT_NUM * 2) - dec->run_len) {
        wlen = (REVERB_RUN_POINT_NUM * 2) - dec->run_len;
    }
    // 保存在后半部中
    memcpy(&dec->run_buf[dec->run_len + (REVERB_RUN_POINT_NUM * 2)], data, wlen);
    dec->run_len += wlen;
    if (dec->run_len >= (REVERB_RUN_POINT_NUM * 2)) {

#if (defined(TCFG_EFFECTS_ENABLE) && (TCFG_EFFECTS_ENABLE != 0))
		effects_app_run_check(dec);
#endif
			
#if HOWLING_ENABLE
        if (dec->p_howling_obj) {
            howling_run(dec->p_howling_obj->ptr, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM);
        }
#endif
#if NOISEGATE_ENABLE
		if(dec->p_noisegate_obj){
			noiseGate_run(dec->p_noisegate_obj->ptr,&dec->run_buf[REVERB_RUN_POINT_NUM*2],&dec->run_buf[REVERB_RUN_POINT_NUM*2],REVERB_RUN_POINT_NUM);	
		}
#endif
#if PITCH_ENABLE
#if REVREB_EN
        if (dec->pitch_hdl) {
            if (dec->pitch_en) {
                picth_run(dec->pitch_hdl, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM * 2, 1);
            }
        }
#endif
#endif
#if REVREB_EN
#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)
        audio_eq_run(dec->p_eq, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM * 2);
#endif
#endif

#if (defined(REVERB_DODGE_ENABLE) && REVERB_DODGE_ENABLE != 0)
        reverb_dodge_run(&dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM * 2, 1);
#endif


        
#if(REVERB_MODE_SEL == ECHO_MODE) 
        if (dec->p_echo_obj) {
#if REVERB_PARM_FADE_EN
            update_echo_parm_fader(dec->p_echo_obj, &dec->p_echo_obj_target.echo_parm_obj);
#endif
#if REVREB_EN
            dec->p_echo_obj->func_api->run(dec->p_echo_obj->ptr, &dec->run_buf[REVERB_RUN_POINT_NUM * 2],&dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM);
#endif
        }
#endif
#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
        if (dec->user_hdl) {
            u8 ch_num = 1;
            user_audio_process_handler_run(dec->user_hdl, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM * 2, ch_num);
        }
#endif

        dec->run_len <<= 1;
        if (dec->out_ch_num == 1) {
            // 单声道，播放后半部
            dec->run_r = (REVERB_RUN_POINT_NUM * 2);
        } else {
			// 双声道，扩充
			dec->run_r = 0;

#if(REVERB_MODE_SEL == REVERBN_MODE)
			if (dec->p_reverb_obj) {
#if REVERB_PARM_FADE_EN
            update_reverb_parm_fader(dec->p_reverb_obj, &dec->p_reverb_obj_target.parm);
#endif
#if REVREB_EN
				dec->p_reverb_obj->func_api->run(dec->p_reverb_obj->ptr, &dec->run_buf[REVERB_RUN_POINT_NUM * 2],dec->run_buf, REVERB_RUN_POINT_NUM);
#else
				audio_pcm_mono_to_dual(dec->run_buf, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM);
#endif
			}else{

				audio_pcm_mono_to_dual(dec->run_buf, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM);
			}
#else
#if REVERB_MONO2STERO
			dec->p_mono2stereo->func_api->run(dec->p_mono2stereo->ptr,&dec->run_buf[REVERB_RUN_POINT_NUM * 2],dec->run_buf,REVERB_RUN_POINT_NUM);
#else
			audio_pcm_mono_to_dual(dec->run_buf, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM);
#endif // REVERB_MONO2STERO
#endif

		}
	}
	/* printf("wl:%d, rl:%d \n", wlen, dec->run_len); */
    return wlen;
}
static int pcm_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    int wlen = 0;
    int rlen = len;
    do {
        wlen = pcm_dec_output(decoder, data, rlen, priv);
        if (!wlen) {
            break;
        }
        data += wlen / 2 ;
        rlen -= wlen;
    } while (rlen);
    /* printf("dec,l:%d, wl:%d \n", len, len-rlen); */
    return (len - rlen);
}

void audio_reverb_set_src_by_dac_sync(int in_rate, int out_rate)
{
	struct s_reverb_hdl *dec = reverb_hdl;
    if (dec && dec->src_sync && (dec->status != REVERB_STATUS_STOP)) {
		if (!in_rate || !out_rate) {
			dec->rate_offset = 0;
		} else {
			int offset = in_rate - out_rate;
			dec->rate_offset = offset * dec->audio_new_rate / out_rate;
			if ((dec->audio_new_rate + dec->rate_offset + PCM_RATE_MAX_STEP) > 65535) {
				dec->rate_offset = 65535 - PCM_RATE_MAX_STEP - dec->audio_new_rate;
			} else if ((dec->audio_new_rate + dec->rate_offset - PCM_RATE_MAX_STEP) < 4000) {
				dec->rate_offset = 4000 + PCM_RATE_MAX_STEP - dec->audio_new_rate;
			}
		}
		dec->data_size = 0;
		dec->data_cnt = 0;
		/* dec->adjust_step = 0; */
		/* dec->adjust_step /= 2; */
		/* printf("i:%d, o:%d, step:%d, offset:%d, osr:%d \n", in_rate, out_rate, dec->adjust_step, dec->rate_offset, dec->audio_new_rate + dec->rate_offset + dec->adjust_step); */
        audio_hw_src_set_rate(dec->src_sync, dec->mic_sr, dec->audio_new_rate + dec->rate_offset + dec->adjust_step);
    }
}

static int pcm_dec_stream_sync(struct s_reverb_hdl *dec, int data_size)
{
    if (!dec->src_sync) {
        return 0;
    }
	if (dec->data_cnt < PCM_SRC_SIZE_CNT) {
		dec->data_size += data_size;
		dec->data_cnt ++;
		if (dec->data_cnt < PCM_SRC_SIZE_CNT) {
			return 0;
		}
	}
	data_size = dec->data_size / PCM_SRC_SIZE_CNT;
	dec->data_size = 0;
	dec->data_cnt = 0;

	int sr = dec->audio_new_rate + dec->rate_offset + dec->adjust_step;

    if (data_size < dec->bottom_size) {
		/* putchar('<'); */
		dec->adjust_step += PCM_RATE_INC_STEP;
		if (dec->adjust_step < 0) {
			dec->adjust_step += PCM_RATE_INC_STEP * 2;
		}
    } else if (data_size > dec->top_size) {
		/* putchar('>'); */
		dec->adjust_step -= PCM_RATE_DEC_STEP;
		if (dec->adjust_step > 0) {
			dec->adjust_step -= PCM_RATE_DEC_STEP * 2;
		}
	} else {
		/* putchar('='); */
		if (dec->adjust_step > 0) {
			dec->adjust_step -= (dec->adjust_step * PCM_RATE_INC_STEP) / PCM_RATE_MAX_STEP;
			if (dec->adjust_step > 0) {
				dec->adjust_step --;
			}
		} else if (dec->adjust_step < 0) {
			dec->adjust_step += ((0-dec->adjust_step) * PCM_RATE_DEC_STEP) / PCM_RATE_MAX_STEP;
			if (dec->adjust_step < 0) {
				dec->adjust_step ++;
			}
		}
    }

    if (dec->adjust_step < -PCM_RATE_MAX_STEP) {
        dec->adjust_step = -PCM_RATE_MAX_STEP;
	} else if (dec->adjust_step > PCM_RATE_MAX_STEP) {
        dec->adjust_step = PCM_RATE_MAX_STEP;
    }

    if (sr != (dec->audio_new_rate + dec->rate_offset + dec->adjust_step)) {
		/* printf(" set reverb sr[%d] [%d] \n",dec->mic_sr,dec->audio_new_rate + dec->rate_offset + dec->adjust_step); */
        audio_hw_src_set_rate(dec->src_sync, dec->mic_sr, dec->audio_new_rate + dec->rate_offset + dec->adjust_step);
    }
    return 0;
}

static int pcm_dec_probe_handler(struct audio_decoder *decoder)
{
    struct s_reverb_hdl *dec = container_of(decoder, struct s_reverb_hdl, decoder);

    if (!dec->sync_start) {
        if (cbuf_get_data_len(&dec->pcm_cbuf) > dec->begin_size) {
            dec->sync_start = 1;
            return 0;
        } else {
            audio_decoder_suspend(&dec->decoder, 0);
            return -EINVAL;
        }
    }

    pcm_dec_stream_sync(dec, cbuf_get_data_len(&dec->pcm_cbuf));
    return 0;
}

static const struct audio_dec_handler pcm_dec_handler = {
    .dec_probe = pcm_dec_probe_handler,
    .dec_output = pcm_dec_output_handler,
    /* .dec_post   = linein_dec_post_handler, */
};
static void pcm_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        if ((u8)argv[1] != (u8)(pcm_dec_maigc - 1)) {
            log_i("maigc err, %s\n", __FUNCTION__);
            break;
        }
        /* pcm_dec_close(); */
        break;
    }
}

static int pcm_dec_sync_init(struct s_reverb_hdl *dec)
{
    dec->sync_start = 0;
    dec->begin_size = dec->pcm_cbuf.total_len * 60 / 100;
    dec->top_size = dec->pcm_cbuf.total_len * 50 / 100;
    dec->bottom_size = dec->pcm_cbuf.total_len * 30 / 100;

    u16 out_sr = dec->src_out_sr;
    printf("out_sr:%d, dsr:%d, dch:%d \n", out_sr, dec->mic_sr, dec->out_ch_num);
    dec->audio_new_rate = out_sr;

    if (dec->audio_new_rate == dec->src_out_sr) {
		// 当和蓝牙叠加时，蓝牙同步会改变采样率
        /* return 0; */
    }

    dec->src_sync = zalloc(sizeof(struct audio_src_handle));
    if (!dec->src_sync) {
        return -ENODEV;
    }
    u8 ch_num = dec->out_ch_num;
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)

#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
	ch_num = dec->out_ch_num;
#else
    ch_num = 4;
#endif
#endif
    audio_hw_src_open(dec->src_sync, ch_num, SRC_TYPE_AUDIO_SYNC);

    audio_hw_src_set_rate(dec->src_sync, dec->mic_sr, dec->audio_new_rate);

    audio_src_set_output_handler(dec->src_sync, dec, reverb_src_output_handler);
    return 0;
}

static int pcm_dec_start(void)
{
    int err;
    struct audio_fmt f;
    struct s_reverb_hdl *dec = reverb_hdl;

    printf("\n--func=%s\n", __FUNCTION__);
    err = audio_decoder_open(&dec->decoder, &pcm_input, &decode_task);
    if (err) {
        goto __err1;
    }
    dec->out_ch_num = audio_output_channel_num();//AUDIO_CH_MAX;

    audio_decoder_set_handler(&dec->decoder, &pcm_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, pcm_dec_event_handler, pcm_dec_maigc++);


    f.coding_type = AUDIO_CODING_PCM;
    f.sample_rate = dec->mic_sr;
    f.channel = 1;//dec->channel;

    err = audio_decoder_set_fmt(&dec->decoder, &f);
    if (err) {
        goto __err2;
    }

    audio_mixer_ch_open(&dec->mix_ch, &mixer);
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, audio_output_rate(f.sample_rate));

#if (defined (TCFG_MIXER_EXT_ENABLE) && (TCFG_MIXER_EXT_ENABLE))
	audio_mixer_ch_set_ext_out_mask(&dec->mix_ch, BIT(0));
#endif//TCFG_MIXER_EXT_ENABLE

    dec->src_out_sr = audio_output_rate(f.sample_rate);
    dec->src_out_sr_n = dec->src_out_sr;
    pcm_dec_sync_init(dec);

    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

    /* audio_adc_mic_start(&dec->mic_ch); */
    printf("\n\n audio decoder start \n");
    audio_decoder_set_run_max(&dec->decoder, 20);

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)

#if (defined(DUAL_TO_QUAD_AFTER_MIX) && (DUAL_TO_QUAD_AFTER_MIX == 1))
	dec->quad_data = NULL;
#else
    dec->quad_data = (s16 *)malloc(DUAL_TO_QUAD_POINTS * 2 * 2); //四通道buf
#endif
#endif
    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err3;
    }
    dec->status = REVERB_STATUS_START;
    printf("\n\n audio mic start  1 \n");
    return 0;
__err3:
    if (dec->src_sync) {
        audio_hw_resample_close(dec->src_sync);
        dec->src_sync = NULL;
    }
#ifndef THREE_ADC_ENABLE
    audio_adc_mic_close(&dec->mic_ch);
#else
    three_adc_mic_enable(0);
#endif
__err2:
    audio_decoder_close(&dec->decoder);
__err1:
    audio_decoder_task_del_wait(&decode_task, &dec->wait);
    return err;
}



static int pcmdec_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    if (!reverb_hdl) {
        log_e("reverb_hdl err \n");
        return -1;
    }
    log_i("pcmdec_wait_res_handler, event:%d;status:%d; \n", event, reverb_hdl->status);
    /* printf("\n--func=%s\n", __FUNCTION__); */
    if (event == AUDIO_RES_GET) {
        if (reverb_hdl->status == REVERB_STATUS_STOP) {
            err = pcm_dec_start();
        } else if (reverb_hdl->status == REVERB_STATUS_PAUSE) {
            reverb_hdl->status = REVERB_STATUS_START;
        }
    } else if (event == AUDIO_RES_PUT) {
        if (reverb_hdl->status == REVERB_STATUS_START) {
            /* reverb_hdl->status = REVERB_STATUS_PAUSE; */
        }
    }

    return err;
}
static u8 reset_mark = 0;
void reverb_pause(void)
{
    printf("\n--func=%s\n", __FUNCTION__);
    if (reverb_hdl) {
        stop_reverb_mic2dac();
        reset_mark = 1;
    }
}
void reverb_resume(void)
{
    printf("\n--func=%s\n", __FUNCTION__);
    if (reset_mark) {
        if (!reverb_hdl) {
            start_reverb_mic2dac(NULL);
            /* os_time_dly(10);// */
        }
        reset_mark = 0;
    }

}
void reverb_restart(void)
{
    if (reverb_hdl->status) {
        pcmdec_wait_res_handler(NULL, 0);
    }
    /* err = audio_decoder_task_add_wait(&decode_task, &reverb->wait);	 */
}
#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)
u8 *get_reverb_high_and_low_sound_tab()
{
	if (!reverb_hdl){
		return NULL;	
	}
	//高低音系数表地址
	return reverb_hdl->outval;
}

u8  get_reverb_eq_section_num()
{
	if (REVERB_EQ_SECTION > 8){
		printf("REVERB_EQ_SECTION %d\n", REVERB_EQ_SECTION);	
	}
	return REVERB_EQ_SECTION;
}


__attribute__((weak))int reverb_eq_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    //log_i("reverb_eq_get_filter_info \n");
    int *coeff = NULL;
    coeff = reverb_hdl->outval;
    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = 0;

    info->nsection = get_reverb_eq_section_num();
    return 0;
}

int reverb_eq_get_filter_info(int sr, struct audio_eq_filter_info *info);
#endif
static int reverb_eq_output(void *priv, s16 *data, u32 len)
{
    return len;
}

static void reverb_dec_resume(void *priv)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    audio_decoder_resume(&reverb_hdl->decoder);
}

void start_reverb_mic2dac(struct audio_fmt *fmt)
{
    struct s_reverb_hdl *reverb = NULL;
    int err;
    if (reverb_hdl) {
        stop_reverb_mic2dac();
    }
    reverb = zalloc(sizeof(struct s_reverb_hdl));
    printf("reverb hdl:%d", sizeof(struct s_reverb_hdl));
    ASSERT(reverb);

    struct audio_fmt f = {0};
    if (fmt) {
        f.sample_rate = fmt->sample_rate;
    }
    if (f.sample_rate == 0) {
#if (defined(TCFG_REVERB_SAMPLERATE_DEFUALT))
        f.sample_rate = TCFG_REVERB_SAMPLERATE_DEFUALT;
#else
        f.sample_rate = 16000;
#endif//TCFG_REVERB_SAMPLERATE_DEFUALT
    }
    f.channel = 1;

    reverb->source_ch_num = f.channel;

    reverb->mic_sr = f.sample_rate;
    reverb->mic_gain = 6;

#if HOWLING_ENABLE
    clock_add(REVERB_HOWLING_CLK);
#endif
#if PITCH_ENABLE
    clock_add(REVERB_PITCH_CLK);
#endif

    clock_add_set(REVERB_CLK);
#if(REVERB_MODE_SEL == REVERBN_MODE)
	reverb->p_reverb_obj = open_reverb(NULL, f.sample_rate);
#if REVERB_PARM_FADE_EN
	update_reverb_parm_cpy(reverb->p_reverb_obj, &reverb->p_reverb_obj_target.parm);
#endif

#endif

#if(REVERB_MODE_SEL == ECHO_MODE)
#if REVERB_MONO2STERO
	reverb->p_mono2stereo = init_reverb_mono2stero();
	if(reverb->p_mono2stereo){
		reverb->p_mono2stereo->func_api->open(reverb->p_mono2stereo->ptr,30,1);
	}

#endif // REVERB_MONO2STERO
	reverb->p_echo_obj = open_echo(NULL, f.sample_rate);
#if REVERB_PARM_FADE_EN
	update_echo_parm_fader(reverb->p_echo_obj, &reverb->p_echo_obj_target.echo_parm_obj);
#endif

#endif

#if NOISEGATE_ENABLE
	reverb->p_noisegate_obj = open_noisegate(NULL,f.sample_rate,1);
#endif	

#if HOWLING_ENABLE
	reverb->p_howling_obj = open_howling(NULL, f.sample_rate, 1);;
#endif
#if PITCH_ENABLE
	reverb->pitch_hdl = open_pitch(NULL);
	reverb->pitch_en = 1;
#endif

#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)
    reverb->filt_ops = get_bfiltTAB_func_api();
    reverb->filt_buf = zalloc(reverb->filt_ops->needbuf());
    reverb->filt_ops->open(reverb->filt_buf);

	reverb->shout_wheat.center_frequency = SHOUT_WHEAT_center_freq;
	reverb->shout_wheat.bandwidth = SHOUT_WHEAT_bandwidth;
	reverb->shout_wheat.occupy = SHOUT_WHEAT_OCCUPY;
	
	reverb->low_sound.cutoff_frequency = LOW_SOUND_cutoff_freq;
	reverb->low_sound.lowest_gain = LOW_SOUND_lowest_gain;
	reverb->low_sound.highest_gain = LOW_SOUND_highest_gain;//1000抬一个db

	reverb->high_sound.cutoff_frequency = HIGH_SOUND_cutoff_freq;
	reverb->high_sound.lowest_gain = HIGH_SOUND_lowest_gain;
	reverb->high_sound.highest_gain = HIGH_SOUND_highest_gain;//1000抬一个db
printf("vvvvvvvvvvvvvvvvvvvvv reverb->high_sound.lowest_gain  %d\n", reverb->high_sound.lowest_gain );
	// 运算buf, 中心频率，带宽设置， 滤波器类型，采样率，滤波器index
    reverb->filt_ops->init(reverb->filt_buf, reverb->shout_wheat.center_frequency, reverb->shout_wheat.bandwidth, TYPE_BANDPASS, f.sample_rate, 0); //喊麦滤波器
    reverb->filt_ops->init(reverb->filt_buf, reverb->low_sound.cutoff_frequency, 1024, TYPE_LOWPASS, f.sample_rate, 1);//低音滤波器
    reverb->filt_ops->init(reverb->filt_buf, reverb->high_sound.cutoff_frequency, 1024, TYPE_HIGHPASS, f.sample_rate, 2);//高音滤波器
	
	reverb->filt_ops->cal_coef(reverb->filt_buf, reverb->outval[0], 0, 0);
    reverb->filt_ops->cal_coef(reverb->filt_buf, reverb->outval[1], 0, 1);
    reverb->filt_ops->cal_coef(reverb->filt_buf, reverb->outval[2], 0, 2);

    reverb->p_eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (reverb->p_eq) {
        reverb->p_eq->eq_ch = (struct hw_eq_ch *)((int)reverb->p_eq + sizeof(struct audio_eq));
        struct audio_eq_param reverb_eq_param = {0};
        reverb_eq_param.channels = f.channel;
        reverb_eq_param.online_en = 1;
        reverb_eq_param.mode_en = 1;
        reverb_eq_param.remain_en = 1;
        reverb_eq_param.max_nsection = REVERB_EQ_SECTION;
        reverb_eq_param.cb = reverb_eq_get_filter_info;
        reverb_eq_param.eq_name = 3;
        audio_eq_open(reverb->p_eq, &reverb_eq_param);
        audio_eq_set_samplerate(reverb->p_eq, f.sample_rate);
        audio_eq_set_output_handle(reverb->p_eq, reverb_eq_output, reverb);
        audio_eq_start(reverb->p_eq);
    }
    reverb->filt_en = 1;
#endif

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    struct user_audio_digital_parm  vol_parm = {0};
#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
	vol_parm.en  = reverb_dec_param.d_vol_en;
	vol_parm.vol_max = ((reverb_dec_param.d_vol_max != NULL) ? reverb_dec_param.d_vol_max() : 0);
	vol_parm.vol = ((reverb_dec_param.d_vol_cur != NULL) ? reverb_dec_param.d_vol_cur() : 0);
    vol_parm.fade_step = reverb_dec_param.d_vol_fade_step;
#endif
    reverb->user_hdl = user_audio_process_open((void *)&vol_parm, NULL, NULL);
#endif


    cbuf_init(&reverb->pcm_cbuf, reverb->pcm_buf, sizeof(reverb->pcm_buf));
#ifndef THREE_ADC_ENABLE
    audio_adc_mic_open(&reverb->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
    audio_adc_mic_set_sample_rate(&reverb->mic_ch, f.sample_rate);
    audio_adc_mic_set_gain(&reverb->mic_ch, reverb->mic_gain);
    audio_adc_mic_set_buffs(&reverb->mic_ch, reverb->adc_buf, ADC_IRQ_POINTS * 2, ADC_BUF_NUM);
    reverb->adc_output.handler = adc_output_to_buf;
    audio_adc_add_output_handler(&adc_hdl, &reverb->adc_output);
    audio_adc_mic_start(&reverb->mic_ch);
#else

    if (audio_three_adc_open() == 0) {
        printf("mic adc open success \n");
        set_mic_cbuf_hdl(&reverb->pcm_cbuf);
		set_mic_resume_hdl(reverb_dec_resume, reverb);
        three_adc_mic_enable(1);
    }
#endif




    reverb->wait.priority = 0;
    reverb->wait.preemption = 0;
    reverb->wait.protect = 1;
    reverb->wait.handler = pcmdec_wait_res_handler;

    /* mic_2_dac_rl(1,1); */
    reverb_hdl = reverb;

#if (defined(REVERB_DODGE_ENABLE) && REVERB_DODGE_ENABLE != 0)
    reverb_dodge_open(reverb, 0);
#endif

    err = audio_decoder_task_add_wait(&decode_task, &reverb->wait);
    if (err == 0) {
        return ;
    }
    printf("audio decoder task add wait err \n");

#ifndef THREE_ADC_ENABLE
    audio_adc_mic_close(&reverb->mic_ch);
    audio_adc_del_output_handler(&adc_hdl, &reverb->adc_output);
#else
    three_adc_mic_enable(0);
    audio_three_adc_close();
#endif

#if(REVERB_MODE_SEL == REVERBN_MODE)
    close_reverb(reverb->p_reverb_obj);
#endif

#if(REVERB_MODE_SEL == ECHO_MODE)
    close_echo(reverb->p_echo_obj);
#endif
#if NOISEGATE_ENABLE
	close_noisegete(reverb_hdl->p_noisegate_obj);
#endif
#if HOWLING_ENABLE
    close_howling(reverb->p_howling_obj);
#endif
#if PITCH_ENABLE
    close_pitch(reverb->pitch_hdl);
#endif

#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)
    if (reverb->p_eq) {
        audio_eq_close(reverb->p_eq);
        free(reverb->p_eq);
        reverb->p_eq = NULL;
    }
    reverb->filt_en = 0;
#endif

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    if (reverb->user_hdl) {
        user_audio_process_close(reverb->user_hdl);
        reverb->user_hdl = NULL;
    }

#endif


#if (defined(REVERB_DODGE_ENABLE) && REVERB_DODGE_ENABLE != 0)
    reverb_dodge_close();
#endif

#if HOWLING_ENABLE
    clock_remove(REVERB_HOWLING_CLK);
#endif
#if PITCH_ENABLE
    clock_remove(REVERB_PITCH_CLK);
#endif

    clock_remove_set(REVERB_CLK);

}

int reverb_if_working(void)
{
    if (reverb_hdl && (reverb_hdl->status == REVERB_STATUS_START)) {
        return 1;
    }
    return 0;
}

void set_mic_gain_up(u8 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    //0-14
    reverb_hdl->mic_gain += value;
    if (reverb_hdl->mic_gain > 14) {
        reverb_hdl->mic_gain = 14;
    }
#ifndef THREE_ADC_ENABLE
    audio_adc_mic_set_gain(&reverb_hdl->mic_ch, reverb_hdl->mic_gain);
#else
    three_adc_mic_set_gain(reverb_hdl->mic_gain);
#endif
}

void set_mic_gain_down(u8 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    //o-14
    reverb_hdl->mic_gain -= value;
    if (reverb_hdl->mic_gain < 0) {
        reverb_hdl->mic_gain = 0;
    }
#ifndef THREE_ADC_ENABLE
    audio_adc_mic_set_gain(&reverb_hdl->mic_ch, reverb_hdl->mic_gain);
#else
    three_adc_mic_set_gain(reverb_hdl->mic_gain);
#endif
}

u8 get_mic_gain(void)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return 0;
    }
    return reverb_hdl->mic_gain;
}

void set_mic_gain(u8 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    //0-14
    reverb_hdl->mic_gain = value;
    if (reverb_hdl->mic_gain > 14) {
        reverb_hdl->mic_gain = 14;
    }
#ifndef THREE_ADC_ENABLE
    audio_adc_mic_set_gain(&reverb_hdl->mic_ch, reverb_hdl->mic_gain);
#else
 	three_adc_mic_set_gain(reverb_hdl->mic_gain);
#endif
}




void reset_reverb_src_out(u16 s_rate)
{
	if (reverb_hdl && reverb_hdl->src_sync) {
		if(reverb_hdl->src_out_sr_n != s_rate){
			printf("reset reverb srcout[%d]",s_rate);
			reverb_hdl->src_out_sr_n = s_rate;
		}
	}
}
/**************************************************************************/
#if(REVERB_MODE_SEL == REVERBN_MODE)
void set_reverb_deepval(u16 value)
{
  
}
void set_reverb_deepval_up(u16 value)
{
 
}

/*
 *混响深度减少
 * */
void set_reverb_deepval_down(u16 value)
{
 
}

/*
 *混响深度增加
 * */

void set_reverb_decayval_up(u16 value)
{

}

void set_reverb_decayval_down(u16 value)
{
 
}

u16 get_reverb_wetgain(void)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return 0;
    }
    return reverb_hdl->p_reverb_obj->parm.wet;
}

void set_reverb_wetgain(u32 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
#if REVERB_PARM_FADE_EN
	reverb_hdl->p_reverb_obj_target.parm.wet = value;
	if (reverb_hdl->p_reverb_obj_target.parm.wet > 300) {
		reverb_hdl->p_reverb_obj_target.parm.wet = 300;
	}
	y_printf("wet:%d", reverb_hdl->p_reverb_obj_target.parm.wet);
	return;
#endif	
    if (reverb_hdl->p_reverb_obj) {
        reverb_hdl->p_reverb_obj->parm.wet = value;
        if (reverb_hdl->p_reverb_obj->parm.wet > 300) {
            reverb_hdl->p_reverb_obj->parm.wet = 300;
        }
        y_printf("wet:%d", reverb_hdl->p_reverb_obj->parm.wet);
        reverb_hdl->p_reverb_obj->func_api->init(reverb_hdl->p_reverb_obj->ptr, &reverb_hdl->p_reverb_obj->parm);
    }
}
#endif

#if(REVERB_MODE_SEL == ECHO_MODE)
void set_reverb_deepval(u16 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
 
}

void set_reverb_deepval_up(u16 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
 
}

/*
 *混响深度减少
 * */
void set_reverb_deepval_down(u16 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }

}

/*
 *混响深度增加
 * */

void set_reverb_decayval_up(u16 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    if (reverb_hdl->p_echo_obj) {
        reverb_hdl->p_echo_obj->echo_parm_obj.decayval += value;
        if (reverb_hdl->p_echo_obj->echo_parm_obj.decayval > 4096) {
            reverb_hdl->p_echo_obj->echo_parm_obj.decayval = 4096;
        }
        printf("decayval:%d", reverb_hdl->p_echo_obj->echo_parm_obj.decayval);
        reverb_hdl->p_echo_obj->func_api->init(reverb_hdl->p_echo_obj->ptr, &reverb_hdl->p_echo_obj->echo_parm_obj);
    }
}

void set_reverb_decayval_down(u16 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    if (reverb_hdl->p_echo_obj) {
        if (reverb_hdl->p_echo_obj->echo_parm_obj.decayval > value) {
            reverb_hdl->p_echo_obj->echo_parm_obj.decayval -= value;
        } else {
            reverb_hdl->p_echo_obj->echo_parm_obj.decayval = 0;
        }

        printf("decayval:%d", reverb_hdl->p_echo_obj->echo_parm_obj.decayval);
        reverb_hdl->p_echo_obj->func_api->init(reverb_hdl->p_echo_obj->ptr, &reverb_hdl->p_echo_obj->echo_parm_obj);
    }
}

u16 get_reverb_wetgain(void)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return 0;
    }
    return 0;
    /* return reverb_hdl->p_echo_obj->parm.wetgain; */
}

void set_reverb_wetgain(u32 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
}
#endif


void set_pitch_para(u32 shiftv,u32 sr,u8 effect,u32 formant_shift)
{
#if PITCH_ENABLE
 	PITCH_SHIFT_PARM *p_pitch_parm = get_pitch_parm();

		p_pitch_parm->shiftv = shiftv;


		p_pitch_parm->sr = sr;

		p_pitch_parm->effect_v = effect;

		p_pitch_parm->formant_shift = formant_shift;
	/* printf("\n\n shiftv[%d],sr[%d],effect[%d],formant_shift[%d] \n\n",shiftv,sr,effect,formant_shift); */
	printf("\n\n\nshiftv[%d],sr[%d],effect[%d],formant_shift[%d] \n\n",p_pitch_parm->shiftv,p_pitch_parm->sr,p_pitch_parm->effect_v,p_pitch_parm->formant_shift);
	if(reverb_hdl&&reverb_hdl->pitch_hdl){
		reverb_hdl->pitch_en = 1;
		update_pict_parm(reverb_hdl->pitch_hdl);
	}
#endif

}

void set_pitch_onoff(u8 onoff)
{
#if PITCH_ENABLE
	if(reverb_hdl)
	{
		reverb_hdl->pitch_en = onoff;
	}
#endif//PITCH_ENABLE

}



static u32 temp_formantshift = 8192;
void adjust_formantshift()
{

}

u32 get_formantshift()
{
    return 	temp_formantshift;
}

/*
 *变音，调用一次切换一次
 * */
void switch_pitch_mode(void)
{
    static u8 mark = 0;
    u16 sample_rate;

#if (defined(TCFG_REVERB_SAMPLERATE_DEFUALT))
    sample_rate = TCFG_REVERB_SAMPLERATE_DEFUALT;
#else
    sample_rate = 16000;
#endif//TCFG_REVERB_SAMPLERATE_DEFUALT

__here:
    mark++;

#if PITCH_ENABLE
    if (reverb_hdl && reverb_hdl->pitch_hdl) {
        printf("mark %d\n", mark);
        switch (mark) {
        case 1://哇哇音
            log_i("========kids sound\n");
            set_pitch_para(56, sample_rate, EFFECT_PITCH_SHIFT, 0);
            break;
        case 2://女变男
            log_i("========girl to boy sound\n");
            set_pitch_para(136, sample_rate, EFFECT_PITCH_SHIFT, 0);
            break;
        case 3://男变女
            log_i("========boy to girl 0 sound\n");
            set_pitch_para(56, sample_rate, EFFECT_VOICECHANGE_KIN0, 150);
            break;
		case 4://男变女
            log_i("========boy to girl 1 sound\n");
            set_pitch_para(56, sample_rate, EFFECT_VOICECHANGE_KIN1, 150);
            break;
		case 5://男变女
            log_i("========boy to girl 2 sound\n");
            set_pitch_para(56, sample_rate, EFFECT_VOICECHANGE_KIN2, 150);
            break;

        case 6://魔音、怪兽音
            log_i("========magic sound\n");
            set_pitch_para(196, sample_rate, EFFECT_PITCH_SHIFT, 100);
            break;
        case 7://电音
            log_i("========electric sound\n");
            set_pitch_para(100, sample_rate, EFFECT_AUTOTUNE, D_MAJOR);
            break;
        default:
            mark = 0;
            break;
        }
    }
#endif
}


void shout_wheat_cal_coef(int sw)
{
#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)
	if (reverb_hdl && reverb_hdl->filt_buf) {
		reverb_hdl->filt_ops->init(reverb_hdl->filt_buf, reverb_hdl->shout_wheat.center_frequency, reverb_hdl->shout_wheat.bandwidth, TYPE_BANDPASS, reverb_hdl->mic_sr, 0); //喊麦滤波器
		if (sw){
			reverb_hdl->filt_ops->cal_coef(reverb_hdl->filt_buf, reverb_hdl->outval[0], reverb_hdl->shout_wheat.occupy, 0);
printf("shout_wheat_cal_coef on\n");
		}else{
			reverb_hdl->filt_ops->cal_coef(reverb_hdl->filt_buf, reverb_hdl->outval[0], 0, 0);
printf("shout_wheat_cal_coef off\n");
		}
	}
#endif
}

void low_sound_cal_coef(int gainN)
{
#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)
	if (reverb_hdl && reverb_hdl->filt_buf) {
		reverb_hdl->filt_ops->init(reverb_hdl->filt_buf, reverb_hdl->low_sound.cutoff_frequency, 1024, TYPE_LOWPASS, reverb_hdl->mic_sr, 1); //低音滤波器
		gainN =reverb_hdl->low_sound.lowest_gain + gainN*(reverb_hdl->low_sound.highest_gain - reverb_hdl->low_sound.lowest_gain)/10;
#ifdef EFFECTS_DEBUG
		printf("gainN %d\n", gainN);
		printf("reverb_hdl->low_sound.lowest_gain %d\n", reverb_hdl->low_sound.lowest_gain);
		printf("reverb_hdl->low_sound.highest_gain %d\n", reverb_hdl->low_sound.highest_gain);
#endif
		if ((gainN >= reverb_hdl->low_sound.lowest_gain) && (gainN <= reverb_hdl->low_sound.highest_gain)) {
			reverb_hdl->filt_ops->cal_coef(reverb_hdl->filt_buf, reverb_hdl->outval[1], gainN, 1);
		}
	}
#endif

}

void high_sound_cal_coef(int gainN)
{
#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)
	if (reverb_hdl && reverb_hdl->filt_buf) {
		reverb_hdl->filt_ops->init(reverb_hdl->filt_buf, reverb_hdl->high_sound.cutoff_frequency, 1024, TYPE_HIGHPASS, reverb_hdl->mic_sr, 2); //高音滤波器
		gainN =reverb_hdl->high_sound.lowest_gain + gainN*(reverb_hdl->high_sound.highest_gain - reverb_hdl->high_sound.lowest_gain)/10;
#ifdef EFFECTS_DEBUG  
		printf("gainN %d\n", gainN);
		printf("reverb_hdl->high_sound.lowest_gain %d\n", reverb_hdl->high_sound.lowest_gain);
		printf("reverb_hdl->high_sound.highest_gain %d\n", reverb_hdl->high_sound.highest_gain);
#endif
		if ((gainN >= reverb_hdl->high_sound.lowest_gain) && (gainN <= reverb_hdl->high_sound.highest_gain)) {
			reverb_hdl->filt_ops->cal_coef(reverb_hdl->filt_buf, reverb_hdl->outval[2], gainN, 2);
		}
	}
#endif
}

/*
 *混响高低音调节
 *filtN为0 与 sw 组合：控制喊麦开关
 *filtN为1或者2时，改变gainN值 调节高低音值
 * */
void reverb_eq_cal_coef(u8 filtN, int gainN, u8 sw)
{
	if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
		return;
	}
#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)

	if (filtN == 0){
		shout_wheat_cal_coef(sw);
	}else if (filtN == 1){
		low_sound_cal_coef(gainN);
	}else if (filtN == 2){
		high_sound_cal_coef(gainN);
	}
	reverb_eq_mode_set_updata(filtN);
#endif
}

void reverb_eq_set(u8 type, u32 gainN)
{
	if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
		return;
	}
#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)
	if (reverb_hdl && reverb_hdl->filt_buf){
		printf("filN %d, gainN %d\n", type, gainN);
		if (reverb_hdl->filt_en){
			if (type == 0){
				shout_wheat_cal_coef(gainN);
			}else if (type == 1){
				low_sound_cal_coef(gainN);
			}else if (type == 2){
				high_sound_cal_coef(gainN);
			}
		}
		reverb_eq_mode_set_updata(type);
	}
#endif
}



#if (defined(REVERB_DODGE_ENABLE) && REVERB_DODGE_ENABLE != 0)
/*
 *threshold_in:闪避启动阈值
 *threshold_out:闪避结束阈值
 *fade_tar : 闪避音量目标值
 *dodge_en:闪避使能
 * */
void reverb_set_dodge_threshold(int threshold_in, int threshold_out, u8 fade_tar, u8 dodge_en)
{
    if (reverb_hdl) {
        reverb_hdl->dodge_threshold_in = threshold_in;
        reverb_hdl->dodge_threshold_out = threshold_out;
        reverb_hdl->fade_tar = fade_tar;
        reverb_hdl->dodge_en = dodge_en;
        reverb_hdl->dodge_filter_cnt = 200;
    }
}


void pcm_energy_analysis_init(struct pcm_energy *energy, u32 points);
u32 pcm_energy_analysis(struct pcm_energy *energy, void *buf, int len, u8 channel_num, u8 channel);
void reverb_dodge_open(void *_reverb, u8 dodge_en)
{
    struct s_reverb_hdl *reverb = _reverb;
    if (!reverb) {
        printf("reverb NULL\n");
        return;
    }
    u32 filt_points = 100;                //单条通道每次计算多少个采样点
    int dodge_threshold_in = 30; //人声能量大于 100触发闪避 */
    int dodge_threshold_out = 10; //底噪能量 < 10, 人声能量小于10 推出闪避
    u8  fade_tar = 5;             //默认闪避目标音量
    reverb->ctrl[0]     = 0;
    reverb->ctrl[1]     = 0;
    reverb->ctrl[2]     = 0;
    reverb_set_dodge_threshold(dodge_threshold_in, dodge_threshold_out, fade_tar, dodge_en);
    pcm_energy_analysis_init(&reverb->energy, filt_points);
}

void reverb_dodge_open_api(u8 dodge_en)
{
	if (!reverb_hdl){
		printf("reverb NULL\n");
		return;
	}
	u32 filt_points = 100;                //单条通道每次计算多少个采样点
	int dodge_threshold_in = 1000; //人声能量大于 100触发闪避 */
	int dodge_threshold_out = 100; //底噪能量 < 10, 人声能量小于10 推出闪避
	u8  fade_tar = 8;             //默认闪避目标音量

    if(dodge_en){
        reverb_hdl->ctrl[0]     = 0;
	    reverb_hdl->ctrl[1]     = 0;
        reverb_hdl->ctrl[2]     = 0;
	    reverb_set_dodge_threshold(dodge_threshold_in, dodge_threshold_out, fade_tar, dodge_en);
    }
    else
    {
#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
        if (reverb_hdl->ctrl[0] == 1){
            /* if (a2dp_user_audio_digital_volume_get() == reverb_hdl->fade_tar){ */
                a2dp_user_digital_volume_set(reverb_hdl->vol_bk[0]);//还原a2dp 音量

            /* } */
            reverb_hdl->ctrl[0] = 0;
        }

        if (reverb_hdl->ctrl[1] == 1){
            /* if (linein_user_audio_digital_volume_get() == reverb_hdl->fade_tar){ */
                linein_user_digital_volume_set(reverb_hdl->vol_bk[1]);//还原linein数字音量
                reverb_hdl->ctrl[1] = 0;
            /* } */
        }

#if TCFG_APP_PC_EN
        if (reverb_hdl->ctrl[2] == 1){
                pc_user_digital_volume_set(reverb_hdl->vol_bk[2]);
                reverb_hdl->ctrl[2] = 0;
        }
#endif
#endif
	    reverb_set_dodge_threshold(0, 0, 0, 0);
    }
}

void reverb_dodge_close()
{
    if (!reverb_hdl) {
        return;
    }
    reverb_hdl->ctrl[0] = 0;
    reverb_hdl->ctrl[1] = 0;
    reverb_hdl->ctrl[2] = 0;
    reverb_set_dodge_threshold(0, 0, 0, 0);
}
void reverb_dodge_run(s16 *data, u32 len, u8 ch_num)
{
    static u32 filter_cnt = 0;
    if (reverb_hdl->dodge_en) {
        int energy = pcm_energy_analysis(&reverb_hdl->energy, data, len, ch_num, 0);
        /* printf("energy %d ", energy); */
        if (energy >= reverb_hdl->dodge_threshold_in) {
#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
            filter_cnt = 0;
            if (!reverb_hdl->ctrl[0]) {
                reverb_hdl->vol_bk[0] = a2dp_user_audio_digital_volume_get();
                if (reverb_hdl->vol_bk[0] != reverb_hdl->fade_tar) {
                    a2dp_user_digital_volume_set(reverb_hdl->fade_tar);
                }
                reverb_hdl->ctrl[0] = 1;
            }

            if (!reverb_hdl->ctrl[1]) {
                reverb_hdl->vol_bk[1] = linein_user_audio_digital_volume_get();
                if (reverb_hdl->vol_bk[1] != reverb_hdl->fade_tar) {
                    linein_user_digital_volume_set(reverb_hdl->fade_tar);
                }
                reverb_hdl->ctrl[1] = 1;
            }
#if TCFG_APP_PC_EN
            if (!reverb_hdl->ctrl[2]) {
                reverb_hdl->vol_bk[2] = pc_user_audio_digital_volume_get();
                if (reverb_hdl->vol_bk[2] != reverb_hdl->fade_tar) {
                    pc_user_digital_volume_set(reverb_hdl->fade_tar);
                }
                reverb_hdl->ctrl[2] = 1;
            }
#endif
#endif
        } else {
            if (energy < (reverb_hdl->dodge_threshold_out)) {
#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
                if(filter_cnt<reverb_hdl->dodge_filter_cnt){
                    filter_cnt++;
                    return;
                }
                if (reverb_hdl->ctrl[0] == 1) {
                    /* if (a2dp_user_audio_digital_volume_get() == reverb_hdl->fade_tar){ */
                    a2dp_user_digital_volume_set(reverb_hdl->vol_bk[0]);//还原a2dp 音量
                    /* } */
                    reverb_hdl->ctrl[0] = 0;
                }

                if (reverb_hdl->ctrl[1] == 1) {
                    /* if (linein_user_audio_digital_volume_get() == reverb_hdl->fade_tar){ */
                    linein_user_digital_volume_set(reverb_hdl->vol_bk[1]);//还原linein数字音量
                    reverb_hdl->ctrl[1] = 0;
                }
#if TCFG_APP_PC_EN
                if (reverb_hdl->ctrl[2] == 1) {
                    pc_user_digital_volume_set(reverb_hdl->vol_bk[2]);//还原linein数字音量
                    reverb_hdl->ctrl[2] = 0;
                    /* } */
                }
#endif

#endif
            }
        }
    }
}
#endif

#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
/*
 *混响音量调节
 * */
void reverb_user_digital_volume_set(u8 vol)
{
    if (reverb_hdl && reverb_hdl->user_hdl && reverb_hdl->user_hdl->dvol_hdl) {
        user_audio_digital_volume_set(reverb_hdl->user_hdl->dvol_hdl, vol);
    }
}

u8 reverb_user_audio_digital_volume_get()
{
    if (!reverb_hdl) {
        return 0;
    }
    if (!reverb_hdl->user_hdl) {
        return 0;
    }
    if (!reverb_hdl->user_hdl->dvol_hdl) {
        return 0;
    }
    return user_audio_digital_volume_get(reverb_hdl->user_hdl->dvol_hdl);
}

/*
 *user_vol_max:音量级数
 *user_vol_tab:自定义音量表,自定义表长user_vol_max+1
 *注意：如需自定义音量表，须在volume_set前调用 ,否则会在下次volume_set时生效
 */
void reverb_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max)
{
    if (reverb_hdl && reverb_hdl->user_hdl && reverb_hdl->user_hdl->dvol_hdl) {
        user_audio_digital_set_volume_tab(reverb_hdl->user_hdl->dvol_hdl, user_vol_tab, user_vol_max);
    }
}



#endif



void mic_2_dac_rl(u8 r_en, u8 l_en)
{
    if (r_en) {
        SFR(JL_ANA->ADA_CON4, 19, 1, 1);    //MIC_2_R
    } else {
        SFR(JL_ANA->ADA_CON4, 19, 1, 0);    //MIC_2_R
    }
    if (l_en) {
        SFR(JL_ANA->ADA_CON4, 18, 1, 1);    //MIC_2_L
    } else {
        SFR(JL_ANA->ADA_CON4, 18, 1, 0);    //MIC_2_L
    }
    printf("\n--func=%s\n", __FUNCTION__);
}

#endif
#if 0
void update_reverb_parm_debug(u8 type,u8 dir,s16 set_data)
{
    s32 data;
    s32 step;
    s32 max_val;
    s32 min_val;

    switch (type)
    {
    case 0:
        /* code */
        data = reverb_hdl->p_reverb_obj->parm.deepval;
        max_val = 4096;
        min_val = 0;
        step = (max_val-min_val)/20;
        if(dir==1){
            data += step;
            if(data>max_val){
                data = max_val;
            }
        }else if(dir==0){
            data -= step;
            if(data<min_val){
                data = min_val;
            }
        }
        reverb_hdl->p_reverb_obj->parm.deepval = data;
        break;
    case 1:
        /* code */
        data = reverb_hdl->p_reverb_obj->parm.decayval;
        max_val = 4096;
        min_val = 0;
        step = (max_val-min_val)/20;
        if(dir==1){
            data += step;
            if(data>max_val){
                data = max_val;
            }
        }else if(dir==0){
            data -= step;
            if(data<min_val){
                data = min_val;
            }
        }
        reverb_hdl->p_reverb_obj->parm.decayval = data;
        break;
    case 2:
        data = reverb_hdl->p_reverb_obj->parm.diffusion_decay;
        max_val = 127;
        min_val = 110;
        step = 1;//(max_val-min_val)/20;
        if(dir==1){
            data += step;
            if(data>max_val){
                data = max_val;
            }
        }else if(dir==0){
            data -= step;
            if(data<min_val){
                data = min_val;
            }
        }
        reverb_hdl->p_reverb_obj->parm.diffusion_decay = data;        /* code */
        break;

    case 3:
        data = reverb_hdl->p_reverb_obj->parm.diffusion_feedback;
        max_val = -100;
        min_val = -120;
        step = (max_val-min_val)/20;
        if(dir==1){
            data += step;
            if(data>max_val){
                data = max_val;
            }
        }else if(dir==0){
            data -= step;
            if(data<min_val){
                data = min_val;
            }
        }
        reverb_hdl->p_reverb_obj->parm.diffusion_feedback = data;

        break;
    case 4:
        data = reverb_hdl->p_reverb_obj->parm.wetgain;
        max_val = 8192;
        min_val = 0;
        step = (max_val-min_val)/20;
        if(dir==1){
            data += step;
            if(data>max_val){
                data = max_val;
            }
        }else if(dir==0){
            data -= step;
            if(data<min_val){
                data = min_val;
            }
        }
        reverb_hdl->p_reverb_obj->parm.wetgain = data;
        break;
    case 5:
        data = reverb_hdl->p_reverb_obj->parm.drygain;
        max_val = 4096;
        min_val = 0;
        step = (max_val-min_val)/20;
        if(dir==1){
            data += step;
            if(data>max_val){
                data = max_val;
            }
        }else if(dir==0){
            data -= step;
            if(data<min_val){
                data = min_val;
            }
        }else{
            ;
        }
        reverb_hdl->p_reverb_obj->parm.drygain = data;
        break;

    default:
        break;
    }

    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    if (reverb_hdl->p_reverb_obj)
    {
        printf("\n\n\n----------------------");
        printf("空间  deepval (0-4096):                  | %d\n", reverb_hdl->p_reverb_obj->parm.deepval);
        printf("衰减  decayval (0-4096):                 | %d\n", reverb_hdl->p_reverb_obj->parm.decayval);
        /* printf("发散衰减  diffusion_decay(110-127):      | %d\n", reverb_hdl->p_reverb_obj->parm.diffusion_decay); */
        /* printf("发散反馈  diffusion_feedback(-100--120): | %d\n", reverb_hdl->p_reverb_obj->parm.diffusion_feedback); */
        printf("湿声  wetgain(0-4096):                   | %d\n", reverb_hdl->p_reverb_obj->parm.wetgain);
        printf("干声  drygain(0-4096):                   | %d\n", reverb_hdl->p_reverb_obj->parm.drygain);
        printf("----------------------\n\n\n");
        reverb_hdl->p_reverb_obj->func_api->init(reverb_hdl->p_reverb_obj->ptr, &reverb_hdl->p_reverb_obj->parm);
    }
}
#endif


#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
#if(REVERB_MODE_SEL == REVERBN_MODE)
extern u8 get_effects_online();
static void update_reverb_parm_tager_from_effects(struct s_reverb_hdl *reverb, REVERBN_PARM_SET *parm)
{
    REVERBN_PARM_SET tmp_parm;
    memcpy(&tmp_parm, parm, sizeof(REVERBN_PARM_SET));
    if(get_effects_online())
    {
        memcpy(&reverb->p_reverb_obj_target.parm, &tmp_parm, sizeof(REVERBN_PARM_SET));
        tmp_parm.wet = reverb->p_reverb_obj->parm.wet;///获取当前的增益, wet通过过淡入方式达到target值
        memcpy(&reverb->p_reverb_obj->parm, &tmp_parm, sizeof(REVERBN_PARM_SET));
        reverb->p_reverb_obj->func_api->init(reverb->p_reverb_obj->ptr, &tmp_parm);
    }
    else
    {
        tmp_parm.wet = reverb->p_reverb_obj->parm.wet;///使用当前参数，调旋钮， 始终不更新此参数， 
        memcpy(&reverb->p_reverb_obj_target.parm, &tmp_parm, sizeof(REVERBN_PARM_SET));
        memcpy(&reverb->p_reverb_obj->parm, &tmp_parm, sizeof(REVERBN_PARM_SET));
        reverb->p_reverb_obj->func_api->init(reverb->p_reverb_obj->ptr, &tmp_parm);
    }
}
#endif
#endif


#if 0
void effects_run_check(void *_reverb, u8 update, void *parm, u16 cur_mode)
{
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
	struct s_reverb_hdl *reverb = (struct s_reverb_hdl *)_reverb;
	{
		switch(update){
			case MAGIC_FLAG_reverb:	
#ifdef EFFECTS_DEBUG
				{
					REVERBN_PARM_SET *parmx = (void *)parm;
					{

						log_info("parm.dry        :%d\n", parmx->dry);
						log_info("parm.wet        :%d\n", parmx->wet);
						log_info("parm.delay      :%d\n", parmx->delay);
						log_info("parm.rot60      :%d\n", parmx->rot60);
						log_info("parm.Erwet 	  :%d\n", parmx->Erwet);
						log_info("parm.Erfactor   :%d\n", parmx->Erfactor);
						log_info("parm.Ewidth     :%d\n", parmx->Ewidth);
						log_info("parm.Ertolate   :%d\n", parmx->Ertolate);
						log_info("parm.predelay   :%d\n", parmx->predelay);
						log_info("parm.width      :%d\n", parmx->width);
						log_info("parm.diffusion  :%d\n", parmx->diffusion);
						log_info("parm.dampinglpf :%d\n", parmx->dampinglpf);
						log_info("parm.basslpf    :%d\n", parmx->basslpf);
						log_info("parm.bassB      :%d\n", parmx->bassB);
						log_info("parm.inputlpf   :%d\n", parmx->inputlpf);
						log_info("parm.outputlpf  :%d\n", parmx->outputlpf);
					}

				}
#endif
#if(REVERB_MODE_SEL == REVERBN_MODE)
#if REVERB_PARM_FADE_EN
				update_reverb_parm_tager_from_effects(reverb, (REVERBN_PARM_SET *)parm);
#endif
#endif
				break;
#if PITCH_ENABLE
			case MAGIC_FLAG_pitch2:	
				{
					PITCH_PARM_SET2 *p_parm = (PITCH_PARM_SET2 *)parm;
					u32 effect_v =  p_parm->effect_v;
					u32 shiftv = p_parm->pitch;
					u32 formant = p_parm->formant_shift;

					if(effect_v == EFFECT_FUNC_NULL)
					{
						log_i("effect_v = EFFECT_FUNC_NULL, close pitch\n");
						set_pitch_onoff(0);
						break;		
					}
					else
					{
						log_i("pitch config run\n");
					}

					set_pitch_para(shiftv, reverb->src_out_sr, effect_v/* EFFECT_VOICECHANGE_KIN0 */, formant);
#ifdef EFFECTS_DEBUG
						log_i("effect_v %d\n", effect_v);
						log_i("shiftv %d\n", shiftv);
						log_i("formant %d\n", formant);
#endif

				}
				break;
#endif

#if(REVERB_MODE_SEL == ECHO_MODE)
			case MAGIC_FLAG_echo:	

#ifdef EFFECTS_DEBUG
				{
					ECHO_PARM_SET  *e_parm = (ECHO_PARM_SET  *)parm;
					printf("e_parm.delay  :%d\n",       e_parm->delay);
					printf("e_parm.decayval  :%d\n",    e_parm->decayval);
					printf("e_parm.dir_s_en:%d\n",      e_parm->direct_sound_enable);
					printf("e_parm.filt_enable  :%d\n", e_parm->filt_enable);
				}
#endif

				update_echo_parm_tager(reverb->p_echo_obj ,parm);
				break;
#endif

#if NOISEGATE_ENABLE
			case MAGIC_FLAG_noise:	
				{
					NOISE_PARM_SET *n_parm = (NOISE_PARM_SET *)parm;
					int attackTime = n_parm->attacktime;
					int releaseTime = n_parm->releasetime;
					int threshold = n_parm->threadhold;
					int gain = n_parm->gain;
#ifdef EFFECTS_DEBUG
					log_i("attackTime %d\n", attackTime);
					log_i("releaseTime %d\n", releaseTime);
					log_i("threshold %d\n", threshold);
					log_i("gain %d\n", gain);
#endif
					updat_noisegate_parm(reverb->p_noisegate_obj, attackTime, releaseTime, threshold, gain);
				}
#endif
				break;
#if (defined(REVERB_EQ_ENABLE) && REVERB_EQ_ENABLE != 0)
		     case MAGIC_FLAG_shout_wheat:
				{
					memcpy(&reverb->shout_wheat, parm, sizeof(SHOUT_WHEAT_PARM_SET));
				}
				break;
			 case MAGIC_FLAG_low_sound:
				{
					memcpy(&reverb->low_sound, parm, sizeof(LOW_SOUND_PARM_SET));
				}	
				break;

			 case MAGIC_FLAG_high_sound:
				{
					memcpy(&reverb->high_sound, parm, sizeof(HIGH_SOUND_PARM_SET));
				}	
				break;
#endif
			 case MAGIC_FLAG_mic_gain:
				{
					u8 value = (u8)parm;
#ifdef EFFECTS_DEBUG
					log_i("value %d\n", value);
#endif
					set_mic_gain(value);
				}	
				break;



			default:
				break;

		}
	}
#endif
}
#endif
