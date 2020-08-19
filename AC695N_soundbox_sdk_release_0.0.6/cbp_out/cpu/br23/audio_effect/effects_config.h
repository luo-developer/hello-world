#ifndef _EFFECTS_CFG_h__
#define _EFFECTS_CFG_h__ 

/*effetcs type*/
typedef enum {
    EFFECTS_TYPE_FILE = 0x01,
    EFFECTS_TYPE_ONLINE,
    EFFECTS_TYPE_MODE_TAB,
} EFFECTS_TYPE;

// 0x09 查询是否有密码
// 0x0A 密码是否正确
// 0x0B 查询文件大小
// 0x0C 读取文件内容

/*effects online cmd*/
typedef enum {
    EFFECTS_ONLINE_CMD_GETVER = 0x5,

    EFFECTS_ONLINE_CMD_PASSWORD = 0x9,
    EFFECTS_ONLINE_CMD_VERIFY_PASSWORD = 0xA,
    EFFECTS_ONLINE_CMD_FILE_SIZE = 0xB,
    EFFECTS_ONLINE_CMD_FILE = 0xC,
	EFFECTS_EQ_ONLINE_CMD_GET_SECTION_NUM = 0xD,//工具查询 小机需要的eq段数
	EFFECTS_EQ_ONLINE_CMD_CHANGE_MODE = 0xE,//切换变声模式

	EFFECTS_ONLINE_CMD_MODE_COUNT = 0x100,//模式个数
	EFFECTS_ONLINE_CMD_MODE_NAME = 0x101,//模式的名字
	EFFECTS_ONLINE_CMD_MODE_GROUP_COUNT = 0x102,//模式下组的个数,4个字节
	EFFECTS_ONLINE_CMD_MODE_GROUP_RANGE = 0x103,//模式下组的id内容
	EFFECTS_ONLINE_CMD_EQ_GROUP_COUNT = 0x104,//eq组的id个数
	EFFECTS_ONLINE_CMD_EQ_GROUP_RANGE = 0x105,//eq组的id内容
	EFFECTS_ONLINE_CMD_MODE_SEQ_NUMBER = 0x106,//mode的编号

	EFFECTS_CMD_REVERB = 0x1001,
	EFFECTS_CMD_PITCH1= 0x1002,//无用
	EFFECTS_CMD_PITCH2 = 0x1003,
	EFFECTS_CMD_ECHO  = 0x1004,
	EFFECTS_CMD_NOISE = 0x1005,
	//高音、低音、喊麦
	EFFECTS_CMD_HIGH_SOUND = 0x1006,
	EFFECTS_CMD_LOW_SOUND = 0x1007,
	EFFECTS_CMD_SHOUT_WHEAT = 0x1008,
	//混响eq
	EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG = 0x1009,
    EFFECTS_EQ_ONLINE_CMD_PARAMETER_TOTAL_GAIN = 0x100A,//无用
    EFFECTS_CMD_MIC_ANALOG_GAIN = 0x100B,
	//add xx


	EFFECTS_CMD_MAX,
} EQ_ONLINE_CMD;

enum {
    MAGIC_FLAG_reverb_eq = 0,
    MAGIC_FLAG_reverb = 1,
    MAGIC_FLAG_pitch1 = 2,
    MAGIC_FLAG_pitch2 = 3,
    MAGIC_FLAG_echo = 4,
    MAGIC_FLAG_noise = 5,

    MAGIC_FLAG_shout_wheat = 6,
    MAGIC_FLAG_low_sound = 7,
    MAGIC_FLAG_high_sound = 8,

	MAGIC_FLAG_eq_seg  = 9,
	MAGIC_FLAG_eq_global_gain  = 10,
	MAGIC_FLAG_mic_gain,
};

typedef struct {
    int gain;
}  EFFECTS_MIC_GAIN_PARM;

typedef struct PITCH_PARM_SET2{
	u32 effect_v;//变声模式：变调/变声0/变声1/变声2/机器音
	u32 pitch;		//变音时 pitch 对饮shiftv, effectv 填 EFFECT_VOICECHANGE_KIN0 
	u32 formant_shift;//变音对应formant
} PITCH_PARM_SET2;


typedef struct NOISE_PARM{
	int attacktime;// 0 - 15000ms
	int releasetime;//  0 - 300ms
	int threadhold;//    -92 - 0 db   （传下时转化为mdb,(thr * 1000)）
	int gain;//   0 - 1               (传下来时扩大30bit，(int)(gain * （1 <<30）))
} NOISE_PARM_SET;


// 高音:
typedef struct HIGH_SOUND{
	int cutoff_frequency;//:截止频率:  1800
	int highest_gain;//最高增益：0
	int lowest_gain;// 最低增益：-12000
} HIGH_SOUND_PARM_SET;

// 低音：
typedef struct LOW_SOUND{
	int cutoff_frequency;//截止频率：600
	int highest_gain;//最高增益：0
	int lowest_gain;//最低增益:  -12000
} LOW_SOUND_PARM_SET;

// 喊麦滤波器:
typedef struct SHOUT_WHEAT{
	int center_frequency;//中心频率: 800
	int bandwidth;//带宽:   4000
	int occupy;//占比:   100%  
} SHOUT_WHEAT_PARM_SET;





u16 get_efects_mode_index();
u16 get_effects_mode();
u8 get_effects_electric_mode_max(void);
void set_effects_mode(u16 mode);
void set_effects_electric_mode(u16 mode);
void effects_run_check(void *_reverb, u8 update, void *parm, u16 cur_mode);
void effects_app_run_check(void *reverb);
u8 get_effects_online();
int reverb_eq_get_filter_info(int sr, struct audio_eq_filter_info *info);


/*混响音效默认系数*/ 
#define REVERB_dry       (100)
#define REVERB_wet       (80)
#define REVERB_delay     (70)
#define REVERB_rot60     (2000)
#define REVERB_Erwet     (60)
#define REVERB_Erfactor      (180)
#define REVERB_Ewidth        (100)
#define REVERB_Ertolate      (100)
#define REVERB_predelay      (0)
#define REVERB_width         (100)
#define REVERB_diffusion     (70)
#define REVERB_dampinglpf    (15000)
#define REVERB_basslpf       (500)
#define REVERB_bassB         (10)
#define REVERB_inputlpf      (18000)
#define REVERB_outputlpf     (18000)

#define PICTH_SHIFTV         (100)
#define PICTH_FORMANT_SHIFT  (100)

#define ECHO_delay                   (300)
#define ECHO_decayval                (50)
#define ECHO_direct_sound_enable     (1)
#define ECHO_filt_enable             (1)

#define NOISEGATE_attacktime   (300)
#define NOISEGATE_releasetime  (5)
#define NOISEGAT_threshold     (-45000)
#define NOISEGATE_low_th_gain  (0)

#define SHOUT_WHEAT_center_freq    (1500)
#define SHOUT_WHEAT_bandwidth      (4000) 
#define SHOUT_WHEAT_OCCUPY         (80)

#define LOW_SOUND_cutoff_freq      (700)
#define LOW_SOUND_lowest_gain      (-12000)
#define LOW_SOUND_highest_gain     (1000)

#define HIGH_SOUND_cutoff_freq      (2000)
#define HIGH_SOUND_lowest_gain      (-12000)
#define HIGH_SOUND_highest_gain     (1000)

//一共有多少个模式
#define mode_num  (8)


enum
{
	EFFECTS_MODE_REVERB = 0x0,		
	EFFECTS_MODE_ELECTRIC,		
	EFFECTS_MODE_BOY_TO_GIRL,		
	EFFECTS_MODE_GIRL_TO_BOY,		
	EFFECTS_MODE_KIDS,		
	EFFECTS_MODE_MAGIC,		
	EFFECTS_MODE_BOOM,		
	EFFECTS_MODE_SHOUTING_WHEAT,		
	EFFECTS_MODE_MAX,
};


//每个模式的名字,使用utf8 编码格式填充,固定16个字节，不足16填 '\0'‘
#define mode0_name "纯混响" 
#define mode1_name "电音"
#define mode2_name "男声变女声"
#define mode3_name "女声变男声"
#define mode4_name "娃娃音"
#define mode5_name "魔音"
#define mode6_name "爆音"
#define mode7_name "喊麦"

//每个模式编号，可以用于文件存储数据模式识别
#define mode0_seq  0x2000
#define mode1_seq  0x2001
#define mode2_seq  0x2002
#define mode3_seq  0x2003
#define mode4_seq  0x2004
#define mode5_seq  0x2005
#define mode6_seq  0x2006
#define mode7_seq  0x2007


#endif 
