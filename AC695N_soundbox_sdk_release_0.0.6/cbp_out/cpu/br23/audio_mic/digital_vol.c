#include "digital_vol.h"
#include "os/os_type.h"
#include "os/os_api.h"

#define ASM_ENABLE						1

#if (ASM_ENABLE == 0)
#define  L_sat(b,a)       __asm__ volatile("%0=sat16(%1)(s)":"=&r"(b) : "r"(a));
#define  L_sat32(b,a,n)       __asm__ volatile("%0=%1>>%2(s)":"=&r"(b) : "r"(a),"r"(n));
#endif

struct __dvol {
	struct __dvol_parm parm;
	volatile u8  vol;			/*淡入淡出当前音量*/
	volatile s16 vol_fade;		/*淡入淡出对应的起始音量*/
	volatile s16 vol_target;	/*淡入淡出对应的目标音量*/
	OS_MUTEX mutex;
};

#define DVOL_MAX		31
const u16 dvol_table_default[DVOL_MAX + 1] = {
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

struct __dvol *digital_vol_creat(struct __dvol_parm *parm, u8 cur_vol)
{
	struct __dvol *d_vol = (struct __dvol *)zalloc(sizeof(struct __dvol));
    if (!d_vol) {
        return NULL;
    }

	if(parm)
	{
		memcpy(&d_vol->parm, parm, sizeof(struct __dvol_parm));
		if(d_vol->parm.vol_tab == NULL)
		{
			d_vol->parm.vol_max = DVOL_MAX; 
			d_vol->parm.vol_max_level = DVOL_MAX; 
			d_vol->parm.vol_tab = dvol_table_default;
		}
	}
	else
	{
		d_vol->parm.toggle = 1;
		d_vol->parm.fade = 1;
		d_vol->parm.ch_num = 2;//这里默认是双声道， 使用注意
		d_vol->parm.fade_step = 2;
		d_vol->parm.vol_max = DVOL_MAX; 
		d_vol->parm.vol_max_level = DVOL_MAX; 
		d_vol->parm.vol_tab = dvol_table_default;
	}

	if(cur_vol > d_vol->parm.vol_max)
	{
		cur_vol = d_vol->parm.vol_max;
	}

	d_vol->vol = cur_vol;
	d_vol->vol_target = d_vol->parm.vol_tab[d_vol->vol*d_vol->parm.vol_max_level/d_vol->parm.vol_max];
	if(d_vol->parm.fade)
	{
		d_vol->vol_fade = 0;
	}
	else
	{
		d_vol->vol_fade = d_vol->vol_target;
	}
    os_mutex_create(&d_vol->mutex);

	return d_vol;
}

void digital_vol_set(struct __dvol *d_vol, u8 vol)
{
	if(d_vol)		
	{
		if (d_vol->parm.toggle == 0) 
		{
			return ;
		}
		if(vol > d_vol->parm.vol_max)
		{
			vol = d_vol->parm.vol_max;
		}

		os_mutex_pend(&d_vol->mutex, 0);
		d_vol->vol = vol;
		u8 vol_level = d_vol->vol*d_vol->parm.vol_max_level/d_vol->parm.vol_max;
		d_vol->vol_target = d_vol->parm.vol_tab[vol_level];
		printf("vol = %d %d, d_vol->vol_target = %d\n", vol, vol_level, d_vol->vol_target);
		os_mutex_post(&d_vol->mutex);
	}
}

u8 digital_vol_get(struct __dvol *d_vol)
{
	if(d_vol)		
	{
		return d_vol->vol;	
	}
	return 0;
}

#if ASM_ENABLE
#define  audio_vol_mix(data,len, ch_num,volumeNOW,volumeDest,step, fade_en){  \
	int i, j;              \
	int fade = 0;             \
	if (volumeNOW != volumeDest)  \
	{                         \
		fade = 1;               \
	}            \
	if(fade_en == 0)\
	{\
		fade = 0;\
		volumeNOW = volumeDest;\
	}\
	if (ch_num == 2)  \
	{                         \
		len = len >> 1;            \
	}                              \
	else if (ch_num == 3)         \
	{                                \
		len = (len*5462)>>14;             /*等效除3，因为5462向上取整得到的*/\
	}      \
	else if(ch_num == 4)               \
	{                \
		len = len >> 2; \
	}        \
	if (fade)            \
	{                           \
		short *in_ptr = data;              \
		for (i = 0; i < len; i++)               \
		{                                      \
			if (volumeNOW < volumeDest)            \
			{                                         \
				volumeNOW = volumeNOW + step;           \
				if (volumeNOW > volumeDest)              \
				{                                 \
					volumeNOW = volumeDest;          \
				}                                 \
			}         \
			else if (volumeNOW > volumeDest)    \
			{     \
				volumeNOW = volumeNOW - step; \
				if (volumeNOW < volumeDest)         \
				{      \
					volumeNOW = volumeDest; \
				} \
			}  \
			{                \
				int tmp;     \
				int reptime = ch_num;  \
				__asm__ volatile(  \
					" 1 : \n\t"         \
					" rep %0 {  \n\t"  \
					"   %1 = h[%2](s) \n\t" \
					"   %1 =%1* %3  \n\t "\
					"   %1 =%1 >>>14 \n\t"\
					"   h[%2++=2]= %1 \n\t"\
					" }\n\t" \
					" if(%0!=0 )goto 1b \n\t" \
					: "=&r"(reptime),      \
					"=&r"(tmp),        \
					"=&r"(in_ptr)  \
					:"r"(volumeNOW),  \
					"0"(reptime),\
					"2"(in_ptr)\
					: "cc", "memory");  \
			} \
		} \
	}  \
	else  \
	{  \
		for (i = 0; i < ch_num; i++) \
		{  \
			short *in_ptr = &data[i]; \
			{  \
				int tmp;  \
				int chnumv=ch_num*2;\
				int reptime = len;\
				__asm__ volatile(\
					" 1 : \n\t"\
					" rep %0 {  \n\t"\
					"   %1 = h[%2](s) \n\t"\
					"   %1 = %1 *%3  \n\t "\
					"   %1=  %1 >>>14 \n\t"\
					"   h[%2++=%4]= %1 \n\t"\
					" }\n\t"\
					" if(%0!=0 )goto 1b \n\t"\
					: "=&r"(reptime),\
					"=&r"(tmp),\
					"=&r"(in_ptr) \
					:"r"(volumeNOW),  \
					"r"(chnumv),\
					"0"(reptime),\
					"2"(in_ptr)\
					: "cc", "memory");\
			}  \
		}\
	}\
}
#else
#define  audio_vol_mix(data,len, ch_num,volumeNOW,volumeDest,step,fade_en){  \
	int i, j;              \
	int fade = 0;             \
	if (volumeNOW != volumeDest)  \
	{                         \
		fade = 1;               \
	}            \
	if(fade_en == 0)\
	{\
		fade = 0;\
		volumeNOW = volumeDest;\
	}\
	if (ch_num == 2)  \
	{                         \
		len = len >> 1;            \
	}                              \
	else if (ch_num == 3)         \
	{                                \
		len = (len*5462)>>14;             /*等效除3，因为5462向上取整得到的*/\
	}      \
	else if(ch_num == 4)               \
	{                \
		len = len >> 2; \
	}        \
	if (fade)            \
	{                           \
		short *in_ptr = data;              \
		for (i = 0; i < len; i++)               \
		{                                      \
			if (volumeNOW < volumeDest)            \
			{                                         \
				volumeNOW = volumeNOW + step;           \
				if (volumeNOW > volumeDest)              \
				{                                 \
					volumeNOW = volumeDest;          \
				}                                 \
			}         \
			else if (volumeNOW > volumeDest)    \
			{     \
				volumeNOW = volumeNOW - step; \
				if (volumeNOW < volumeDest)         \
				{      \
					volumeNOW = volumeDest; \
				} \
			}  \
			for (j = 0; j < ch_num; j++)  \
			{          \
				int tmp = (*in_ptr*volumeNOW) >> 14;  \
				L_sat(tmp, tmp);  \
				*in_ptr = tmp; \
				in_ptr++; \
			} \
		} \
	}  \
	else  \
	{  \
		for (i = 0; i < ch_num; i++) \
		{  \
			short *in_ptr = &data[i]; \
			for (j = 0; j < len; j++)\
			{\
				int tmp= (*in_ptr*volumeNOW) >> 14;  \
				L_sat(tmp, tmp);  \
				*in_ptr = tmp;\
				in_ptr += ch_num;\
			}\
		}\
	}\
}
#endif

void digital_vol_process(struct __dvol *d_vol, s16 *data, u32 points)
{
	if(d_vol == NULL)		
		return ;
	if(d_vol->parm.toggle == 0)
		return ;

    s32 valuetemp;
    s16 *buf = data;
    u32 len = points; 

	os_mutex_pend(&d_vol->mutex, 0);
    audio_vol_mix(
					buf, 
					len, 
					d_vol->parm.ch_num, 
					d_vol->vol_fade, 
					d_vol->vol_target, 
					d_vol->parm.fade_step, 
					d_vol->parm.fade
				);
	os_mutex_post(&d_vol->mutex);
}

void digital_vol_destroy(struct __dvol **hdl)
{
	if(hdl == NULL || *hdl == NULL)		
		return ;
	struct __dvol *d_vol = *hdl;
	if(d_vol)
	{
		free(d_vol);		
		*hdl = NULL;
	}
}

