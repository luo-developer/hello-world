#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_online_cfg.h"

#include "audio_eq.h"
#include "reverb/reverb_api.h"

#include "effects_config.h"
#include "effects_default_config.h"

#define LOG_TAG     "[APP-EFFECTS]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"


#if (defined(TCFG_EFFECTS_ENABLE) && (TCFG_EFFECTS_ENABLE == 1))
const u8 audio_effects_sdk_name[16] 		= "AC695X";
const u8 audio_effects_ver[4] 			    = {0, 4, 3, 0};

#define EFFECTS_FILE_NAME 			        SDFILE_RES_ROOT_PATH"effects_cfg.bin"
const u8 audio_effects_password[16] 		= "000";

#define TCFG_EFFECTS_FILE_ENABLE		1

//每个模式下，有多少组（有多少参数结构体）
#define  mode0_group_num 6
#define  mode1_group_num 6
#define  mode2_group_num 7
#define  mode3_group_num 7
#define  mode4_group_num 7
#define  mode5_group_num 7
#define  mode6_group_num 6
#define  mode7_group_num 7

/*每个模式下拥有哪些 功能(group)*/
static u16 mode0_groups[mode0_group_num] = {
							 EFFECTS_CMD_REVERB, 	 
	                         EFFECTS_CMD_NOISE, 	 
							 EFFECTS_CMD_HIGH_SOUND,
							 EFFECTS_CMD_LOW_SOUND,
							 EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG, 
							 EFFECTS_CMD_MIC_ANALOG_GAIN
};
static u16 mode1_groups[mode1_group_num] = {
							 EFFECTS_CMD_REVERB, 	 
	                         EFFECTS_CMD_NOISE, 	 
							 EFFECTS_CMD_HIGH_SOUND,
							 EFFECTS_CMD_LOW_SOUND,
							 EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG, 
							 EFFECTS_CMD_MIC_ANALOG_GAIN
};

static u16 mode2_groups[mode2_group_num] = {
							 EFFECTS_CMD_REVERB, 	 
							 EFFECTS_CMD_PITCH2,
	                         EFFECTS_CMD_NOISE, 	 
							 EFFECTS_CMD_HIGH_SOUND,
							 EFFECTS_CMD_LOW_SOUND,
							 EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG, 
							 EFFECTS_CMD_MIC_ANALOG_GAIN
};

static u16 mode3_groups[mode3_group_num] = {
							 EFFECTS_CMD_REVERB, 	 
							 EFFECTS_CMD_PITCH2,
	                         EFFECTS_CMD_NOISE, 	 
							 EFFECTS_CMD_HIGH_SOUND,
							 EFFECTS_CMD_LOW_SOUND,
							 EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG, 
							 EFFECTS_CMD_MIC_ANALOG_GAIN
};

static u16 mode4_groups[mode4_group_num] = {
							 EFFECTS_CMD_REVERB, 	 
							 EFFECTS_CMD_PITCH2,
							 EFFECTS_CMD_NOISE, 	 
							 EFFECTS_CMD_HIGH_SOUND,
							 EFFECTS_CMD_LOW_SOUND,
							 EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG, 
							 EFFECTS_CMD_MIC_ANALOG_GAIN
};

static u16 mode5_groups[mode5_group_num] = {
							 EFFECTS_CMD_REVERB, 	 
							 EFFECTS_CMD_PITCH2,
							 EFFECTS_CMD_NOISE, 	 
							 EFFECTS_CMD_HIGH_SOUND,
							 EFFECTS_CMD_LOW_SOUND,
							 EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG, 
							 EFFECTS_CMD_MIC_ANALOG_GAIN
};
static u16 mode6_groups[mode6_group_num] = {
							 EFFECTS_CMD_REVERB, 	 
							 EFFECTS_CMD_NOISE, 	 
							 EFFECTS_CMD_HIGH_SOUND,
							 EFFECTS_CMD_LOW_SOUND,
							 EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG, 
							 EFFECTS_CMD_MIC_ANALOG_GAIN
};


static u16 mode7_groups[mode7_group_num] = {
							 EFFECTS_CMD_REVERB, 	 
							 EFFECTS_CMD_NOISE, 	 
							 EFFECTS_CMD_HIGH_SOUND,
							 EFFECTS_CMD_LOW_SOUND,
							 EFFECTS_CMD_SHOUT_WHEAT,
							 EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG, 
							 EFFECTS_CMD_MIC_ANALOG_GAIN
};

static const int groups_num[] = {
	mode0_group_num, 
	mode1_group_num, 
	mode2_group_num, 
	mode3_group_num, 
	mode4_group_num, 
	mode5_group_num, 
	mode6_group_num, 
	mode7_group_num,
};

static const u16 *groups[] = {
	mode0_groups, 
	mode1_groups, 
	mode2_groups, 
	mode3_groups, 
	mode4_groups,
	mode5_groups,
	mode6_groups,
	mode7_groups,
};



static u16 *name[16] = {
	(u16 *)mode0_name, 
	(u16 *)mode1_name, 
	(u16 *)mode2_name, 
	(u16 *)mode3_name, 
	(u16 *)mode4_name,
	(u16 *)mode5_name,
	(u16 *)mode6_name,
	(u16 *)mode7_name,
};
static u32 modes[] = {
	mode0_seq, 
	mode1_seq, 
	mode2_seq, 
	mode3_seq, 
	mode4_seq,
	mode5_seq,
	mode6_seq,
	mode7_seq,
};

static const struct eq_seg_info eq_tab_normal[EQ_SECTION_MAX_DEFAULT] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,    0 << 20, (int)(0.7f * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,    0 << 20, (int)(0.7f * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,   0 << 20, (int)(0.7f * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,   0 << 20, (int)(0.7f * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,   0 << 20, (int)(0.7f * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,  0 << 20, (int)(0.7f * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,  0 << 20, (int)(0.7f * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,  0 << 20, (int)(0.7f * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,  0 << 20, (int)(0.7f * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},

    {10, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},
    {11, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},
    {12, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},
    {13, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},
    {14, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},
    {15, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},
    {16, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},
    {17, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},
    {18, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},
    {19, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},

};

/*-----------------------------------------------------------*/
/*EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG*/
typedef struct eq_seg_info EQ_ONLINE_PARAMETER_SEG;
typedef struct eq_seg_info EQ_CFG_SEG;

typedef struct {
	float global_gain;
    int seg_num;
    int enable_section;
    EQ_CFG_SEG seg[EQ_SECTION_MAX];
}  EFFECTS_EQ_PARM_SET;


/*-----------------------------------------------------------*/

/*effectx file head*/
typedef struct {
	/* unsigned short crc; */
    unsigned short id;
    unsigned short len;
}  EFFECTS_FILE_HEAD;

typedef struct {
	EFFECTS_FILE_HEAD head;
	REVERBN_PARM_SET r_parm;
}  EFFECTS_REVERB;

typedef struct {
	EFFECTS_FILE_HEAD head;
	PITCH_PARM_SET2  p_parm2;
}  EFFECTS_PARM2;

typedef struct {
	EFFECTS_FILE_HEAD head;
	ECHO_PARM_SET  e_parm;
}  EFFECTS_ECHO;

typedef struct {
	EFFECTS_FILE_HEAD head;
	NOISE_PARM_SET  n_parm;
}  EFFECTS_NOISE;


typedef struct {
	EFFECTS_FILE_HEAD head;
	SHOUT_WHEAT_PARM_SET s_w_parm;
}  EFFECTS_SHOUT_WHEAT;


typedef struct {
	EFFECTS_FILE_HEAD head;
	LOW_SOUND_PARM_SET l_parm;
}  EFFECTS_LOW_SOUND;

typedef struct {
	EFFECTS_FILE_HEAD head;
	HIGH_SOUND_PARM_SET h_parm;
}  EFFECTS_HIGH_SOUND;

typedef struct {
	EFFECTS_FILE_HEAD head;
	EFFECTS_MIC_GAIN_PARM mic_parm;
}  EFFECTS_MIC_GAIN;
typedef struct {
	EFFECTS_FILE_HEAD head;
	EFFECTS_EQ_PARM_SET e_parm;
}  EFFECTS_EQ_PARM;

typedef struct {
	EFFECTS_REVERB reverb_parm;
	EFFECTS_PARM2  pitch_parm2;
	EFFECTS_ECHO   echo_parm; 
	EFFECTS_NOISE  noise_parm; 
	EFFECTS_SHOUT_WHEAT shout_wheat_parm;
	EFFECTS_LOW_SOUND  low_parm;
	EFFECTS_HIGH_SOUND high_parm;
	EFFECTS_MIC_GAIN   mic_gain;
	EFFECTS_EQ_PARM eq_parm;
}  EFFECTS_FILE;


/*effects online packet*/
typedef struct {
    int cmd;     			///<EQ_ONLINE_CMD
	/* int modeId; */
    int data[45];       	///<data
}  EFFECTS_ONLINE_PACKET;

typedef struct password{ 
	int len; 
	char pass[45]; 
} PASSWORD;




typedef struct {
    u32 eq_type : 3;
    spinlock_t lock;
    u16 cur_sr;
    u16 cur_mode_id;
    u32 online_updata[mode_num][16];
    u32 design_mask[mode_num];
    u32 seg_num[mode_num];
	float global_gain[mode_num];
    int EQ_Coeff_table[(EQ_SECTION_MAX + 3)* 5];
	EFFECTS_FILE e_file[mode_num];
	uint8_t password_ok;
	u8 effects_file_invalid;

} EFFECT_CFG;

/*-----------------------------------------------------------*/
static EFFECT_CFG *p_effects_cfg = NULL;
extern u16 crc_get_16bit(const void *src, u32 len);

__attribute__((weak))u8  get_reverb_eq_section_num()
{
	return EQ_SECTION_MAX;
}
u8 *get_reverb_high_and_low_sound_tab();
int eq_seg_design(struct eq_seg_info *seg, int sample_rate, int *coeff);
static void eq_coeff_set(int sr, u8 ch)
{
    int i;
    if (!sr) {
        sr = 44100;
        log_error("sr is zero");
    }
	for (i = 0; i < p_effects_cfg->seg_num[p_effects_cfg->cur_mode_id]; i++) {
		if (p_effects_cfg->design_mask[p_effects_cfg->cur_mode_id] & BIT(i)) {
			p_effects_cfg->design_mask[p_effects_cfg->cur_mode_id] &= ~BIT(i);
#if TCFG_EQ_ENABLE
			eq_seg_design(&p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].eq_parm.e_parm.seg[i], sr, &p_effects_cfg->EQ_Coeff_table[5 * i]);
#endif

#if 0
			log_i("p_effects_cfg->seg_num %d\n", p_effects_cfg->seg_num[[p_effects_cfg->cur_mode_id]]);
			log_i("i %d\n", i);
			log_i("index    %d \n
					iir_type %d \n
					freq %d \n
					gain %d \n
					q    %d \n",
					p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].eq_parm.e_parm.seg[i].index, 
					p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].eq_parm.e_parm.seg[i].iir_type, 
					p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].eq_parm.e_parm.seg[i].freq, 
					10*p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].eq_parm.e_parm.seg[i].gain/(1<<20),
					10*p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].eq_parm.e_parm.seg[i].q/(1<<24));

			log_i("cf0:%d, cf1:%d, cf2:%d, cf3:%d, cf4:%d ", p_effects_cfg->EQ_Coeff_table[5*i]
					, p_effects_cfg->EQ_Coeff_table[5*i + 1]
					, p_effects_cfg->EQ_Coeff_table[5*i + 2]
					, p_effects_cfg->EQ_Coeff_table[5*i + 3]
					, p_effects_cfg->EQ_Coeff_table[5*i + 4]
				  );
#endif
		}
	}
}

/*
 *配置文件结构
[FMT:1] [VER:4] {
  	[MODE_SEQ:2][MODE_LEN:2] (
      	[CRC:2][ID:2][LEN:2][PAYLOAD:LEN]) * N 
} * M
*/



static u8 find_config_param(const u16 *t, u16 cmd, int num)
{
	int i;
	for(i=0; i<num; i++)
	{
		if(cmd == t[i])
		{
			return 1;
		}
	}
	return 0;
}

static void effects_fill_default_config_spec(EFFECT_CFG *effects_cfg, u16 mode,  u16 cmd, char *name)
{
	switch(cmd)		
	{
		case EFFECTS_CMD_REVERB: 	 
			memcpy(&effects_cfg->e_file[mode].reverb_parm.r_parm, &reverb_defult_config,  sizeof(REVERBN_PARM_SET));
			break;
		case EFFECTS_CMD_PITCH2:
			if(0 == strcmp((const char *)name, (const char *)mode1_name))
			{
				memcpy(&effects_cfg->e_file[mode].pitch_parm2.p_parm2, &electric_pitch2_default_config, sizeof(PITCH_PARM_SET2));
			}
			else
			{
				memcpy(&effects_cfg->e_file[mode].pitch_parm2.p_parm2, &pitch2_default_config, sizeof(PITCH_PARM_SET2));
			}
			break;
		case EFFECTS_CMD_ECHO: 	 
			memcpy(&effects_cfg->e_file[mode].echo_parm.e_parm, &echo_default_config, sizeof(ECHO_PARM_SET));
			break;
		case EFFECTS_CMD_NOISE: 	 
			memcpy(&effects_cfg->e_file[mode].noise_parm.n_parm, &noise_gain_default_config, sizeof(NOISE_PARM_SET));
			break;
		case EFFECTS_CMD_SHOUT_WHEAT:
			memcpy(&effects_cfg->e_file[mode].shout_wheat_parm.s_w_parm, &shout_wheat_default_config, sizeof(SHOUT_WHEAT_PARM_SET));
			break;
		case EFFECTS_CMD_LOW_SOUND:
			memcpy(&effects_cfg->e_file[mode].low_parm.l_parm, &low_sound_default_config, sizeof(LOW_SOUND_PARM_SET));
			break;
		case EFFECTS_CMD_HIGH_SOUND:
			memcpy(&effects_cfg->e_file[mode].high_parm.h_parm, &high_sound_default_config, sizeof(HIGH_SOUND_PARM_SET));
			break;
		case EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG: 
			break;
		case EFFECTS_CMD_MIC_ANALOG_GAIN:
			memcpy(&effects_cfg->e_file[mode].mic_gain.mic_parm, &mic_gain_defualt_config, sizeof(EFFECTS_MIC_GAIN_PARM));
			break;
	}
}

static void effects_fill_default_config(EFFECT_CFG *effects_cfg)
{
	const u16 mode_cmd_all[] = {
		EFFECTS_CMD_REVERB, 	 
		EFFECTS_CMD_PITCH2,
		EFFECTS_CMD_ECHO, 	 
		EFFECTS_CMD_NOISE, 	 
		EFFECTS_CMD_SHOUT_WHEAT,
		EFFECTS_CMD_LOW_SOUND,
		EFFECTS_CMD_HIGH_SOUND,
		EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG, 
		EFFECTS_CMD_MIC_ANALOG_GAIN
	};
	u32 i, j;
	for(i=0; i<mode_num; i++)
	{
		for(j=0; j<(sizeof(mode_cmd_all)/sizeof(mode_cmd_all[0])); j++)
		{
			if(find_config_param(groups[i], mode_cmd_all[j], groups_num[i]))
			{
			}
			else
			{
				/* log_info("%s, effects mode use default config~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", name[i]); */
				effects_fill_default_config_spec(effects_cfg, i, mode_cmd_all[j], (char *)name[i]);
			}
		}
	}


#if 0
	for(i=0; i<mode_num; i++)
	{
		printf("effects_fill_default_config +++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		memcpy(&effects_cfg->e_file[i].reverb_parm.r_parm, &reverb_defult_config,  sizeof(REVERBN_PARM_SET));
		memcpy(&effects_cfg->e_file[i].pitch_parm2.p_parm2, &pitch2_default_config, sizeof(PITCH_PARM_SET2));
		memcpy(&effects_cfg->e_file[i].echo_parm.e_parm, &echo_default_config, sizeof(ECHO_PARM_SET));
		memcpy(&effects_cfg->e_file[i].noise_parm.n_parm, &noise_gain_default_config, sizeof(NOISE_PARM_SET));
		memcpy(&effects_cfg->e_file[i].shout_wheat_parm.s_w_parm, &shout_wheat_default_config, sizeof(SHOUT_WHEAT_PARM_SET));
		memcpy(&effects_cfg->e_file[i].low_parm.l_parm, &low_sound_default_config, sizeof(LOW_SOUND_PARM_SET));
		memcpy(&effects_cfg->e_file[i].high_parm.h_parm, &high_sound_default_config, sizeof(HIGH_SOUND_PARM_SET));
		memcpy(&effects_cfg->e_file[i].mic_gain.mic_parm, &mic_gain_defualt_config, sizeof(EFFECTS_MIC_GAIN_PARM));
	}
#endif
}


static void set_effects_mode_do(u16 mode) 
{
	if(mode >= EFFECTS_MODE_MAX)
	{
		log_e("set_effects_mode_do over limit\n");
		return ;		
	}

	log_i("set_effects_mode_do =============================================%d\n", mode);
	p_effects_cfg->cur_mode_id = mode;
	p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_reverb] = 1;
	p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_pitch2] = 1;
	p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_echo]   = 1;
	p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_noise]  = 1;
	p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_shout_wheat] = 1;
	p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_low_sound]   = 1;
	p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_high_sound]  = 1;
	p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_mic_gain]    = 1;
	p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_eq_seg] = 1;
	set_eq_online_updata(1);
}


static s32 effects_file_get_cfg(EFFECT_CFG *effects_cfg)
{
	/* return 0; */
    u32 magic_flag[mode_num] = {0};
    unsigned short magic;
    int ret = 0;
    int read_size;
    FILE *file = NULL;
    u8 *file_data = NULL;
	u8 *head_buf = NULL;

    if (effects_cfg == NULL) {
        return  -EINVAL;
    }
    file = fopen(EFFECTS_FILE_NAME, "r");
    if (file == NULL) {
        log_error("effects file open err\n");
        return  -ENOENT;
    }
    log_info("effects file open ok \n");

	// effects ver
	u8 fmt = 0;	
	if (1 != fread(file, &fmt, 1)) {
		ret = -EIO;
		goto err_exit;
	}

	u8 ver[4] = {0};
	if (4 != fread(file, ver, 4)) {
		ret = -EIO;
		goto err_exit;
	}
	if (memcmp(ver, audio_effects_ver, sizeof(audio_effects_ver))) {
		log_info("effects ver err \n");
		log_info_hexdump(ver, 4);
		fseek(file, 0, SEEK_SET);
	}
	unsigned short mode_seq;
	unsigned short mode_len;
	u8 cur_mode_id = 0;
	u8 mode_cnt = 0;

__next_mode:
	if (sizeof(unsigned short) != fread(file, &mode_seq, sizeof(unsigned short))) {
		ret = 0;
		goto err_exit;
	}
	if (sizeof(unsigned short) != fread(file, &mode_len, sizeof(unsigned short))) {
		ret = 0;
		goto err_exit;
	}
	int mode_start = fpos(file);
	for(int i = 0; i < mode_num/* ARRAY_SIZE(modes) */; i++){

#ifdef EFFECTS_DEBUG
		log_info("==================modes[%d] %x %x\n", i, modes[i], mode_seq);
#endif
		if (modes[i] == mode_seq){//识别当前读到模式的序号
			cur_mode_id = i;
			break;
		}
	}

	while (1) {

#ifdef EFFECTS_DEBUG
		log_info("modes seq [%d] %x\n",cur_mode_id , modes[cur_mode_id]);
#endif

		{
			//read crc
			if (sizeof(unsigned short) != fread(file, &magic, sizeof(unsigned short))) {
				ret = 0;
				break;
			}

			int pos = fpos(file);
			//read id len
			EFFECTS_FILE_HEAD *effect_file_h;
			int head_size = sizeof(EFFECTS_FILE_HEAD); 
			head_buf = malloc(head_size);
			if (sizeof(EFFECTS_FILE_HEAD) != fread(file, head_buf, sizeof(EFFECTS_FILE_HEAD))) {
				ret = -EIO;
				break;
			}
			effect_file_h = (EFFECTS_FILE_HEAD *)head_buf; 

			if ((effect_file_h->id >= EFFECTS_CMD_REVERB) && (effect_file_h->id < EFFECTS_CMD_MAX)){
				//reread id len  and read data	
				fseek(file, pos, SEEK_SET);
				int data_size = effect_file_h->len + sizeof(EFFECTS_FILE_HEAD); 
				file_data = malloc(data_size);
				if (file_data == NULL) {
					ret = -ENOMEM;
					break;
				}
				if (data_size != fread(file, file_data,data_size)) {
					ret = -EIO;
					break;
				}

				//compare crc
				if (magic == crc_get_16bit(file_data, data_size)) {
					spin_lock(&effects_cfg->lock);
					log_info("effect_file_h->id %x, %d\n", effect_file_h->id, cur_mode_id);
					if (effect_file_h->id == EFFECTS_CMD_REVERB){
						memcpy(&effects_cfg->e_file[cur_mode_id].reverb_parm, file_data, data_size);
						magic_flag[cur_mode_id] |= BIT(MAGIC_FLAG_reverb);
						effects_cfg->online_updata[cur_mode_id][MAGIC_FLAG_reverb] = 1;
#ifdef EFFECTS_DEBUG
						{//test
							REVERBN_PARM_SET parm;
							memcpy(&parm, &effects_cfg->e_file[cur_mode_id].reverb_parm.r_parm,sizeof(REVERBN_PARM_SET));
							{
								log_info("parm.dry        :%d\n", parm.dry);
								log_info("parm.wet        :%d\n", parm.wet);
								log_info("parm.delay      :%d\n", parm.delay);
								log_info("parm.rot60      :%d\n", parm.rot60);
								log_info("parm.Erwet 	  :%d\n", parm.Erwet);
								log_info("parm.Erfactor   :%d\n", parm.Erfactor);
								log_info("parm.Ewidth     :%d\n", parm.Ewidth);
								log_info("parm.Ertolate   :%d\n", parm.Ertolate);
								log_info("parm.predelay   :%d\n", parm.predelay);
								log_info("parm.width      :%d\n", parm.width);
								log_info("parm.diffusion  :%d\n", parm.diffusion);
								log_info("parm.dampinglpf :%d\n", parm.dampinglpf);
								log_info("parm.basslpf    :%d\n", parm.basslpf);
								log_info("parm.bassB      :%d\n", parm.bassB);
								log_info("parm.inputlpf   :%d\n", parm.inputlpf);
								log_info("parm.outputlpf  :%d\n", parm.outputlpf);
							}
						}
#endif
					}else if (effect_file_h->id == EFFECTS_CMD_PITCH2){
						memcpy(&effects_cfg->e_file[cur_mode_id].pitch_parm2, file_data, data_size);
						magic_flag[cur_mode_id] |= BIT(MAGIC_FLAG_pitch2);
						effects_cfg->online_updata[cur_mode_id][MAGIC_FLAG_pitch2] = 1;

#ifdef EFFECTS_DEBUG
						{//test
							PITCH_PARM_SET2 parm2;
							memcpy(&parm2, &effects_cfg->e_file[cur_mode_id].pitch_parm2.p_parm2, sizeof(PITCH_PARM_SET2 ));
							{
								log_i("parm2.effect_v       :%d\n", parm2.effect_v);
								log_i("parm2.pitch          :%d\n", parm2.pitch);
								log_i("parm2.formant_shift  :%d\n", parm2.formant_shift);
							}
						}
#endif
					}else if (effect_file_h->id == EFFECTS_CMD_ECHO){
						memcpy(&effects_cfg->e_file[cur_mode_id].echo_parm, file_data, data_size);
						magic_flag[cur_mode_id] |= BIT(MAGIC_FLAG_echo);
						effects_cfg->online_updata[cur_mode_id][MAGIC_FLAG_echo] = 1;

#ifdef EFFECTS_DEBUG
						{//test
							ECHO_PARM_SET  e_parm;
							memcpy(&e_parm, &effects_cfg->e_file[cur_mode_id].echo_parm.e_parm, sizeof(ECHO_PARM_SET));
							{
								log_i("e_parm.delay  :%d\n", e_parm.delay);
								log_i("e_parm.decayval  :%d\n", e_parm.decayval);
								log_i("e_parm.dir_s_en:%d\n", e_parm.direct_sound_enable);
								log_i("e_parm.filt_enable  :%d\n", e_parm.filt_enable);
							}
						}
#endif

					}else if (effect_file_h->id == EFFECTS_CMD_NOISE){
						memcpy(&effects_cfg->e_file[cur_mode_id].noise_parm, file_data, data_size);
						magic_flag[cur_mode_id] |= BIT(MAGIC_FLAG_noise);
						effects_cfg->online_updata[cur_mode_id][MAGIC_FLAG_noise] = 1;
#ifdef EFFECTS_DEBUG
						{//test
							NOISE_PARM_SET  n_parm;
							memcpy(&n_parm, &effects_cfg->e_file[cur_mode_id].noise_parm.n_parm, sizeof(NOISE_PARM_SET));
							{
								log_i("n_parm.attacktime   :%d\n", n_parm.attacktime);
								log_i("n_parm.releasetime  :%d\n", n_parm.releasetime);
								log_i("n_parm.threadhold   :%d\n", n_parm.threadhold);
								log_i("n_parm.gain         :%d\n", n_parm.gain);
							}
						}
#endif
					}else if (effect_file_h->id == EFFECTS_CMD_SHOUT_WHEAT){
						memcpy(&effects_cfg->e_file[cur_mode_id].shout_wheat_parm, file_data, data_size);
						magic_flag[cur_mode_id] |= BIT(MAGIC_FLAG_shout_wheat);
						effects_cfg->online_updata[cur_mode_id][MAGIC_FLAG_shout_wheat] = 1;

#ifdef EFFECTS_DEBUG
						{
							SHOUT_WHEAT_PARM_SET s_w_parm;
							memcpy(&s_w_parm, &effects_cfg->e_file[cur_mode_id].shout_wheat_parm.s_w_parm, sizeof(SHOUT_WHEAT_PARM_SET));
							{
								log_i("s_w_parm.center_frequency   :%d\n", s_w_parm.center_frequency);
								log_i("s_w_parm.bandwidth          :%d\n", s_w_parm.bandwidth);
								log_i("s_w_parm.occupy             :%d\n", s_w_parm.occupy);
							}
						}
#endif

					}else if (effect_file_h->id == EFFECTS_CMD_LOW_SOUND){
						memcpy(&effects_cfg->e_file[cur_mode_id].low_parm, file_data, data_size);
						magic_flag[cur_mode_id] |= BIT(MAGIC_FLAG_low_sound);
						effects_cfg->online_updata[cur_mode_id][MAGIC_FLAG_low_sound] = 1;
#ifdef EFFECTS_DEBUG
						{
							LOW_SOUND_PARM_SET  l_parm;
							memcpy(&l_parm, &effects_cfg->e_file[cur_mode_id].low_parm.l_parm, sizeof(LOW_SOUND_PARM_SET));
							{
								log_i("l_parm.cutoff_frequency   :%d\n", l_parm.cutoff_frequency);
								log_i("l_parm.highest_gain       :%d\n", l_parm.highest_gain);
								log_i("l_parm.lowest_gain        :%d\n", l_parm.lowest_gain);
							}
						}
#endif	
					}else if (effect_file_h->id == EFFECTS_CMD_HIGH_SOUND){
						memcpy(&effects_cfg->e_file[cur_mode_id].high_parm, file_data, data_size);
						magic_flag[cur_mode_id] |= BIT(MAGIC_FLAG_high_sound);
						effects_cfg->online_updata[cur_mode_id][MAGIC_FLAG_high_sound] = 1;
#ifdef EFFECTS_DEBUG
						{
							HIGH_SOUND_PARM_SET     h_parm;
							memcpy(&h_parm, &effects_cfg->e_file[cur_mode_id].high_parm.h_parm, sizeof(HIGH_SOUND_PARM_SET));
							{
								log_i("h_parm.cutoff_frequency   :%d\n", h_parm.cutoff_frequency);
								log_i("h_parm.highest_gain       :%d\n", h_parm.highest_gain);
								log_i("h_parm.lowest_gain        :%d\n", h_parm.lowest_gain);
							}
						}
#endif			
					}else if (effect_file_h->id == EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG){
						memcpy(&effects_cfg->e_file[cur_mode_id].eq_parm, file_data, data_size);
						effects_cfg->global_gain[cur_mode_id] =  effects_cfg->e_file[cur_mode_id].eq_parm.e_parm.global_gain;
						effects_cfg->seg_num[cur_mode_id] = effects_cfg->e_file[cur_mode_id].eq_parm.e_parm.seg_num;
						effects_cfg->design_mask[cur_mode_id] = (u32) - 1;
						eq_coeff_set(effects_cfg->cur_sr, 0);

						magic_flag[cur_mode_id] |= BIT(MAGIC_FLAG_eq_seg);
						effects_cfg->online_updata[cur_mode_id][MAGIC_FLAG_eq_seg] = 1;
#ifdef EFFECTS_DEBUG
						{
							log_i("effects_cfg->seg_num %d\n", effects_cfg->seg_num[cur_mode_id]);	
							log_i("effects_cfg->global_gain %d\n", effects_cfg->global_gain[cur_mode_id]);
						}
#endif
					}else if (effect_file_h->id == EFFECTS_CMD_MIC_ANALOG_GAIN){
						memcpy(&effects_cfg->e_file[cur_mode_id].mic_gain, file_data, data_size);
						magic_flag[cur_mode_id] |= BIT(MAGIC_FLAG_mic_gain);
						effects_cfg->online_updata[cur_mode_id][MAGIC_FLAG_mic_gain] = 1;
#ifdef EFFECTS_DEBUG
						{
							log_i("effects_cfg->e_file[cur_mode_id].mic_gain.mic_parm.gain %d\n", effects_cfg->e_file[cur_mode_id].mic_gain.mic_parm.gain);

						}
#endif
					}
					spin_unlock(&effects_cfg->lock);
				}else{
					log_error("effects_cfg_info crc err\n");
					ret = -ENOEXEC;
				}	

				free(head_buf);
				head_buf = NULL;

				free(file_data);
				file_data = NULL;
			}


			int mode_end = fpos(file);
				if ((mode_end - mode_start) == mode_len){
#ifdef EFFECTS_DEBUG
					log_info("mode_end %d\n", mode_end);
					log_info("mode_start %d\n", mode_start);
					log_info("mode_len %d\n", mode_len);
					log_info("read next mode ================\n");
#endif
				goto __next_mode;
			} 
		}
	}
err_exit:
	if (head_buf){
		free(head_buf);
		head_buf = NULL;
	}	

    if (file_data) {
        free(file_data);
		file_data = NULL;
    }
    fclose(file);
	if (ret == 0) {
		for (int i = 0; i < mode_num; i++){
			if (!(magic_flag[i] & BIT(MAGIC_FLAG_reverb))) {
				log_error("mode %x\n", modes[i]);
				log_error("cfg_info reverb err\n");
				/* ret = magic_flag[i]; */
			}
			if (!(magic_flag[i] & BIT(MAGIC_FLAG_pitch2))) {
				log_error("mode %x\n", modes[i]);
				log_error("cfg_info pitch2 err\n");
				/* ret = magic_flag[i]; */
			}
			if (!(magic_flag[i] & BIT(MAGIC_FLAG_echo))) {
				log_error("mode %x\n", modes[i]);
				log_error("cfg_info echo err\n");
				/* ret = magic_flag[i]; */
			}
			if (!(magic_flag[i] & BIT(MAGIC_FLAG_noise))) {
				log_error("mode %x\n", modes[i]);
				log_error("cfg_info noise err\n");
				/* ret = magic_flag[i]; */
			}

			if (!(magic_flag[i] & BIT(MAGIC_FLAG_shout_wheat))) {
				log_error("mode %x\n", modes[i]);
				log_error("cfg_info shout wheat err\n");
				/* ret = magic_flag[i]; */
			}

			if (!(magic_flag[i] & BIT(MAGIC_FLAG_low_sound))) {
				log_error("mode %x\n", modes[i]);
				log_error("cfg_info low sound err\n");
				/* ret = magic_flag[i]; */
			}

			if (!(magic_flag[i] & BIT(MAGIC_FLAG_high_sound))) {
				log_error("mode %x\n", modes[i]);
				log_error("cfg_info high sound err\n");
				/* ret = magic_flag[i]; */
			}


			if (!(magic_flag[i] & BIT(MAGIC_FLAG_eq_seg))) {
				log_error("mode %x\n", modes[i]);
				log_error("cfg_info eq err\n");
				ret = magic_flag[i];
			}

			if (!(magic_flag[i] & BIT(MAGIC_FLAG_mic_gain))) {
				log_error("mode %x\n", modes[i]);
				log_error("cfg_info mic_gain err\n");
				/* ret = magic_flag[i]; */
			}
		}

	}

    if (ret == 0) {
        log_info("effects cfg_info ok \n");
    }
    return ret;
}


__attribute__((weak))u8 *get_reverb_high_and_low_sound_tab()
{
	return NULL;
}
int reverb_eq_online_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
//	puts("reverb_eq_online_get_filter_info\n");
    int *coeff = NULL;
    if (!sr) {
        return -1;
    }
    ASSERT(p_effects_cfg);
    if ((sr != p_effects_cfg->cur_sr) || (p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_eq_seg])) {
        //在线请求coeff
        spin_lock(&p_effects_cfg->lock);
        if (sr != p_effects_cfg->cur_sr) {
            p_effects_cfg->design_mask[p_effects_cfg->cur_mode_id] = (u32) - 1;
        }
        eq_coeff_set(sr, 0);//在线算前五段eq
        p_effects_cfg->cur_sr = sr;
        p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_eq_seg] = 0;
#if TCFG_EQ_ENABLE
		set_eq_online_updata(0);
#endif
        spin_unlock(&p_effects_cfg->lock);
    }
	if (get_reverb_high_and_low_sound_tab()){
		memcpy(&p_effects_cfg->EQ_Coeff_table[EQ_SECTION_MAX*5], get_reverb_high_and_low_sound_tab(), 3*5*sizeof(int));//将高低音三段eq，连接到前五段表后,组成8段
	}

    coeff = p_effects_cfg->EQ_Coeff_table;
    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = p_effects_cfg->global_gain[p_effects_cfg->cur_mode_id];


    info->nsection = get_reverb_eq_section_num();
    return 0;
}

int reverb_eq_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    //log_i("reverb_eq_get_filter_info \n");
    int *coeff = NULL;
	ASSERT(p_effects_cfg);
	if ((sr != p_effects_cfg->cur_sr) || (p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_eq_seg])) {
		spin_lock(&p_effects_cfg->lock);
		p_effects_cfg->design_mask[p_effects_cfg->cur_mode_id] = (u32) - 1;
		eq_coeff_set(sr, 0);//算前五段eq
		p_effects_cfg->cur_sr = sr;
		p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_eq_seg] = 0;
#if TCFG_EQ_ENABLE
		set_eq_online_updata(0);
#endif
		spin_unlock(&p_effects_cfg->lock);
	}

	if (get_reverb_high_and_low_sound_tab()){
		memcpy(&p_effects_cfg->EQ_Coeff_table[EQ_SECTION_MAX*5], get_reverb_high_and_low_sound_tab(), 3*5*sizeof(int));//将高低音三段eq，连接到前五段表后,组成8段
	}
	coeff = p_effects_cfg->EQ_Coeff_table;
    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = p_effects_cfg->global_gain[p_effects_cfg->cur_mode_id];

    info->nsection = get_reverb_eq_section_num();
    return 0;
}

/*-----------------------------------------------------------*/


/*-----------------------------------------------------------*/
// effects online
#if (defined(TCFG_EFFECTS_ONLINE_ENABLE) && (TCFG_EFFECTS_ONLINE_ENABLE != 0))
#include "config/config_interface.h"

static s32 effects_online_update(EFFECTS_ONLINE_PACKET *packet)
{
	REVERBN_PARM_SET parm;
	PITCH_PARM_SET2 parm2;
	ECHO_PARM_SET  e_parm;
	NOISE_PARM_SET  n_parm;
	SHOUT_WHEAT_PARM_SET s_w_parm;
	LOW_SOUND_PARM_SET  l_parm;
	HIGH_SOUND_PARM_SET     h_parm;

	EQ_ONLINE_PARAMETER_SEG seg;

	EFFECTS_MIC_GAIN_PARM mic_gain;

	log_info("effects_cmd:0x%x ", packet->cmd);
	if (p_effects_cfg->eq_type != EFFECTS_TYPE_ONLINE) {
        return -EPERM;
    }
	if (!p_effects_cfg->password_ok){
        return -EPERM;
	}
	u32 cur_mode_id = 0;
	switch (packet->cmd) {
		case EFFECTS_CMD_REVERB:
			spin_lock(&p_effects_cfg->lock);
			memcpy(&parm, &packet->data[1], sizeof(REVERBN_PARM_SET));
			cur_mode_id = packet->data[0];
			memcpy(&p_effects_cfg->e_file[/* p_effects_cfg-> */cur_mode_id].reverb_parm.r_parm, &parm, sizeof(REVERBN_PARM_SET));
			spin_unlock(&p_effects_cfg->lock);
#ifdef EFFECTS_DEBUG
			{

				log_info("parm.dry        :%d\n", parm.dry);
				log_info("parm.wet        :%d\n", parm.wet);
				log_info("parm.delay      :%d\n", parm.delay);
				log_info("parm.rot60      :%d\n", parm.rot60);
				log_info("parm.Erwet 	  :%d\n", parm.Erwet);
				log_info("parm.Erfactor   :%d\n", parm.Erfactor);
				log_info("parm.Ewidth     :%d\n", parm.Ewidth);
				log_info("parm.Ertolate   :%d\n", parm.Ertolate);
				log_info("parm.predelay   :%d\n", parm.predelay);
				log_info("parm.width      :%d\n", parm.width);
				log_info("parm.diffusion  :%d\n", parm.diffusion);
				log_info("parm.dampinglpf :%d\n", parm.dampinglpf);
				log_info("parm.basslpf    :%d\n", parm.basslpf);
				log_info("parm.bassB      :%d\n", parm.bassB);
				log_info("parm.inputlpf   :%d\n", parm.inputlpf);
				log_info("parm.outputlpf  :%d\n", parm.outputlpf);
			}
#endif

			break;
		case EFFECTS_CMD_PITCH2:
			spin_lock(&p_effects_cfg->lock);
			memcpy(&parm2, &packet->data[1], sizeof(PITCH_PARM_SET2));

			cur_mode_id = packet->data[0];
			memcpy(&p_effects_cfg->e_file[/* p_effects_cfg-> */cur_mode_id].pitch_parm2.p_parm2, &parm2, sizeof(PITCH_PARM_SET2));
			spin_unlock(&p_effects_cfg->lock);
#ifdef EFFECTS_DEBUG
			{
				log_info("parm2.pitch          :%d\n", parm2.pitch);
				log_info("parm2.formant_shift  :%d\n", parm2.formant_shift);
			}
#endif

			break;
		case EFFECTS_CMD_ECHO:
			spin_lock(&p_effects_cfg->lock);
			memcpy(&e_parm, &packet->data[1], sizeof(ECHO_PARM_SET));

			cur_mode_id = packet->data[0];
			memcpy(&p_effects_cfg->e_file[/* p_effects_cfg-> */cur_mode_id].echo_parm.e_parm, &e_parm, sizeof(ECHO_PARM_SET));
			spin_unlock(&p_effects_cfg->lock);
#ifdef EFFECTS_DEBUG
			{
				log_info("e_parm.delay  :%d\n", e_parm.delay);
				log_info("e_parm.decayval  :%d\n", e_parm.decayval);
				log_info("e_parm.dir_s_en:%d\n", e_parm.direct_sound_enable);
				log_info("e_parm.filt_enable  :%d\n", e_parm.filt_enable);
			}
#endif

			break;
		case EFFECTS_CMD_NOISE:
			spin_lock(&p_effects_cfg->lock);
			memcpy(&n_parm, &packet->data[1], sizeof(NOISE_PARM_SET));

			cur_mode_id = packet->data[0];
			memcpy(&p_effects_cfg->e_file[/* p_effects_cfg-> */cur_mode_id].noise_parm.n_parm, &n_parm, sizeof(NOISE_PARM_SET));
			spin_unlock(&p_effects_cfg->lock);
#ifdef EFFECTS_DEBUG
			{
				log_info("n_parm.attacktime   :%d\n", n_parm.attacktime);
				log_info("n_parm.releasetime  :%d\n", n_parm.releasetime);
				log_info("n_parm.threadhold   :%d\n", n_parm.threadhold);
				log_info("n_parm.gain         :%d\n", n_parm.gain);
			}
#endif
		case EFFECTS_CMD_SHOUT_WHEAT:
			spin_lock(&p_effects_cfg->lock);
			memcpy(&s_w_parm, &packet->data[1], sizeof(SHOUT_WHEAT_PARM_SET));

			cur_mode_id = packet->data[0];
			memcpy(&p_effects_cfg->e_file[/* p_effects_cfg-> */cur_mode_id].shout_wheat_parm.s_w_parm, &s_w_parm, sizeof(SHOUT_WHEAT_PARM_SET));
			spin_unlock(&p_effects_cfg->lock);
#ifdef EFFECTS_DEBUG
			{
				log_info("s_w_parm.center_frequency   :%d\n", s_w_parm.center_frequency);
				log_info("s_w_parm.bandwidth          :%d\n", s_w_parm.bandwidth);
				log_info("s_w_parm.occupy             :%d\n", s_w_parm.occupy);
			}
#endif

			break;

		case EFFECTS_CMD_LOW_SOUND:
			spin_lock(&p_effects_cfg->lock);
			memcpy(&l_parm, &packet->data[1], sizeof(LOW_SOUND_PARM_SET));

			cur_mode_id = packet->data[0];
			memcpy(&p_effects_cfg->e_file[/* p_effects_cfg-> */cur_mode_id].low_parm.l_parm, &l_parm, sizeof(LOW_SOUND_PARM_SET));
			spin_unlock(&p_effects_cfg->lock);
#ifdef EFFECTS_DEBUG
			{
				log_info("l_parm.cutoff_frequency   :%d\n", l_parm.cutoff_frequency);
				log_info("l_parm.highest_gain       :%d\n", l_parm.highest_gain);
				log_info("l_parm.lowest_gain        :%d\n", l_parm.lowest_gain);
			}
#endif

			break;

		case EFFECTS_CMD_HIGH_SOUND:
			spin_lock(&p_effects_cfg->lock);
			memcpy(&h_parm, &packet->data[1], sizeof(HIGH_SOUND_PARM_SET));

			cur_mode_id = packet->data[0];
			memcpy(&p_effects_cfg->e_file[/* p_effects_cfg-> */cur_mode_id].high_parm.h_parm, &h_parm, sizeof(HIGH_SOUND_PARM_SET));
			spin_unlock(&p_effects_cfg->lock);
#ifdef EFFECTS_DEBUG
			{
				log_info("h_parm.cutoff_frequency   :%d\n", h_parm.cutoff_frequency);
				log_info("h_parm.highest_gain       :%d\n", h_parm.highest_gain);
				log_info("h_parm.lowest_gain        :%d\n", h_parm.lowest_gain);
			}
#endif
			break;

		case EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG:
			spin_lock(&p_effects_cfg->lock);
			memcpy(&seg, &packet->data[1], sizeof(EQ_ONLINE_PARAMETER_SEG));
			cur_mode_id = packet->data[0];
			if (seg.index == (u16)-1){
				float global_gain;
				memcpy(&global_gain, &seg.iir_type, sizeof(float));
				p_effects_cfg->global_gain[cur_mode_id] = global_gain;
#ifdef EFFECTS_DEBUG
				log_info("p_effects_cfg->global_gain[cur_mode_id] %d\n", p_effects_cfg->global_gain[cur_mode_id]);
#endif
			}
			if (seg.index >= EQ_SECTION_MAX) {
				if (seg.index != (u16)-1){
					log_error("index:%d ", seg.index);
				}
				spin_unlock(&p_effects_cfg->lock);
				break;
			}
			memcpy(&p_effects_cfg->e_file[/* p_effects_cfg-> */cur_mode_id].eq_parm.e_parm.seg[seg.index], &seg, sizeof(EQ_ONLINE_PARAMETER_SEG));
			p_effects_cfg->design_mask[/* p_effects_cfg-> */cur_mode_id] |= BIT(seg.index);
			spin_unlock(&p_effects_cfg->lock);

#ifdef EFFECTS_DEBUG
		log_info("idx:%d, iir:%d, frq:%d, gain:%d, q:%d \n", seg.index, seg.iir_type, seg.freq, seg.gain, seg.q);
        log_info("idx:%d, iir:%d, frq:%d, gain:%d, q:%d \n", seg.index, seg.iir_type, seg.freq, 10 * seg.gain / (1 << 20), 10 * seg.q / (1 << 24));

#endif

			break;
		case EFFECTS_CMD_MIC_ANALOG_GAIN:
			spin_lock(&p_effects_cfg->lock);
			memcpy(&mic_gain, &packet->data[1], sizeof(EFFECTS_MIC_GAIN_PARM));

			cur_mode_id = packet->data[0];
			p_effects_cfg->e_file[/* p_effects_cfg-> */cur_mode_id].mic_gain.mic_parm.gain = mic_gain.gain;
			spin_unlock(&p_effects_cfg->lock);
#ifdef EFFECTS_DEBUG
			log_info("mic_gain:%d\n", p_effects_cfg->e_file[/* p_effects_cfg-> */cur_mode_id].mic_gain.mic_parm.gain);
#endif

			break;
		default:
			return -EINVAL;
	}
    return 0;
}


static void set_online_effects_mode(u32 mode)
{
	for (int i = 0; i < mode_num; i++){
		if (mode == modes[i]){
			set_effects_mode_do(i);
			break;
		}	
	}
}


/* // query mode information */
/* #define ONLINE_SUB_OP_QUERY_MODE_COUNT              0x100 */
/* #define ONLINE_SUB_OP_QUERY_MODE_NAME               0x101 */
/* #define ONLINE_SUB_OP_QUERY_MODE_GROUP_COUNT        0x102 */
/* #define ONLINE_SUB_OP_QUERY_MODE_GROUP_RANGE        0x103 */
/* // query eq groups */
/* #define ONLINE_SUB_OP_QUERY_EQ_GROUP_COUNT          0x104 */
/* #define ONLINE_SUB_OP_QUERY_EQ_GROUP_RANGE          0x105 */


static int effects_online_nor_cmd(EFFECTS_ONLINE_PACKET *packet)
{
	if (p_effects_cfg->eq_type != EFFECTS_TYPE_ONLINE) {
		return -EPERM;
	}
#ifdef EFFECTS_DEBUG
	log_info("packet->cmd %x\n", packet->cmd); 
#endif
	if (packet->cmd == EFFECTS_ONLINE_CMD_GETVER) {
		struct effects_ver_info {
			char sdkname[16];
			u8 eqver[4];
		};
		struct effects_ver_info effects_ver_info = {0};
		memcpy(effects_ver_info.sdkname, audio_effects_sdk_name, sizeof(audio_effects_sdk_name));
		memcpy(effects_ver_info.eqver, audio_effects_ver, sizeof(audio_effects_ver));
		ci_send_packet(EFFECTS_CONFIG_ID, (u8 *)&effects_ver_info, sizeof(struct effects_ver_info));
		return 0;
	}else if (packet->cmd == EFFECTS_ONLINE_CMD_PASSWORD){
		uint8_t password = 1;
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)&password, sizeof(uint8_t));
		return 0;
	}else if (packet->cmd == EFFECTS_ONLINE_CMD_VERIFY_PASSWORD){
		//check password 	
		int len = 0;	
		char pass[64];
		PASSWORD ps = {0};
		spin_lock(&p_effects_cfg->lock);
		memcpy(&ps, packet->data, sizeof(PASSWORD));
		/* printf("ps.len %d\n",ps.len); */
		memcpy(&ps, packet->data, sizeof(int) + ps.len);
		/* put_buf(ps.pass, ps.len); */
		spin_unlock(&p_effects_cfg->lock);

		//strcmp xxx
		uint8_t password_ok = 0;
		if (!strcmp(ps.pass, audio_effects_password)){
			password_ok = 1;
		}else{
			log_error("password verify fail \n");
		}
		p_effects_cfg->password_ok = password_ok;
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)&password_ok, sizeof(uint8_t));
		return 0;
	}else if (packet->cmd == EFFECTS_ONLINE_CMD_FILE_SIZE){
		if (!p_effects_cfg->password_ok){
			log_error("pass not verify\n");
			return -EINVAL;
		}
		struct file_s{
			int id; 
			int fileid;
		};	
		struct file_s fs;
		memcpy(&fs, packet, sizeof(struct file_s));
		if (fs.fileid != 0x2){
			log_error("fs.fileid %d\n", fs.fileid);
			return -EINVAL;
		}

		FILE *file = NULL;
		file = fopen(EFFECTS_FILE_NAME, "r");
		u32 file_size  = 0;
		if (!file){
			log_error("EFFECTS_FILE_NAME err %s\n", EFFECTS_FILE_NAME);
			/* return -EINVAL; */
		}else{
			file_size = flen(file);
			fclose(file);
		}
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)&file_size, sizeof(u32));
		return 0;
	}else if (packet->cmd == EFFECTS_ONLINE_CMD_FILE){
		if (!p_effects_cfg->password_ok){
			log_error("pass not verify\n");
			return -EINVAL;
		}

		struct file_s{
			int id; 
			int fileid; 
			int offset; 
			int size; 
		};
		struct file_s fs;
		memcpy(&fs, packet, sizeof(struct file_s));
		if (fs.fileid != 0x2){
			log_error("fs.fileid %d\n", fs.fileid);
			return -EINVAL;
		}
		FILE *file = NULL;
		file = fopen(EFFECTS_FILE_NAME, "r");
		if (!file){
			return -EINVAL;
		}
		fseek(file, fs.offset, SEEK_SET);
		u8 *data = malloc(fs.size);
		if (!data){
			fclose(file);
			return -EINVAL;
		}
		int ret = fread(file, data, fs.size);
		if (ret != fs.size){
		}
		fclose(file);
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)data, fs.size);
		free(data);

		return 0;	
	}else if (packet->cmd == EFFECTS_EQ_ONLINE_CMD_GET_SECTION_NUM) {
		uint8_t hw_section = EQ_SECTION_MAX;
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)&hw_section, sizeof(uint8_t));
		log_i("id %x, hw_section %d\n", id, hw_section);
		return 0;
	}else if (packet->cmd == EFFECTS_ONLINE_CMD_MODE_COUNT){
		//模式个数
		int mode_cnt = mode_num;
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)&mode_cnt, sizeof(int));
		return 0;
	}else if (packet->cmd == EFFECTS_ONLINE_CMD_MODE_NAME){
		//utf8编码得名字
		struct cmd {
			int id; 
			int modeId; 
		};
		struct cmd cmd;
		memcpy(&cmd, packet, sizeof(struct cmd));
		
		/* printf("cmd.modeId %d\n", cmd.modeId); */
		/* printf("sizeof(name[modeId])+ 1 %d\n", sizeof(name[cmd.modeId])); */
		u8 tmp[16] ={0};
		memcpy(tmp, name[cmd.modeId], strlen(name[cmd.modeId]));
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)tmp, 16);
		/* put_buf(name[cmd.modeId], 16); */
		/* printf("name[modeId] %s\n", name[cmd.modeId]); */
		return 0;
	}else if (packet->cmd == EFFECTS_ONLINE_CMD_MODE_GROUP_COUNT){
		struct cmd {
			int id; 
			int modeId; 
		};
		struct cmd cmd;
		memcpy(&cmd, packet, sizeof(struct cmd));

#ifdef EFFECTS_DEBUG
		log_info("group count cmd.modeId %d %d\n", cmd.modeId ,groups_num[cmd.modeId]);
#endif
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)&groups_num[cmd.modeId], sizeof(int));
		return 0;

	}else if (packet->cmd == EFFECTS_ONLINE_CMD_MODE_GROUP_RANGE){//摸下是组的id
		/* u16 *groups[] = {mode0_groups, mode1_groups, mode2_groups, mode3_groups, mode4_groups}; */
		struct cmd {
			int id; 
			int modeId; 
			int offset; 
			int count; 
		};
		struct cmd cmd;
		memcpy(&cmd, packet, sizeof(struct cmd));

#ifdef EFFECTS_DEBUG
		log_info("group range cmd.modeId %d\n", cmd.modeId);
#endif
		/* log_info("cmd.offset %d, cmd.count %d\n", cmd.offset, cmd.count); */
		u16 *group_tmp = groups[cmd.modeId];
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)&group_tmp[cmd.offset], cmd.count*sizeof(u16));
		/* put_buf(&group_tmp[cmd.offset], cmd.count*sizeof(u16)); */
		return 0;
	}else if (packet->cmd == EFFECTS_ONLINE_CMD_EQ_GROUP_COUNT){
		u32 eq_group_num = 1; 
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)&eq_group_num, sizeof(u32));

		return 0;
	}else if (packet->cmd == EFFECTS_ONLINE_CMD_EQ_GROUP_RANGE){
		u16 groups_cnt[] = {0x1009};
		struct cmd {
			int id; 
			int offset; 
			int count; 
		};
		struct cmd cmd;
		memcpy(&cmd, packet, sizeof(struct cmd));

#ifdef EFFECTS_DEBUG
		log_info("eq group cmd.offset %d, cmd.count %d\n", cmd.offset, cmd.count);
#endif
		u16 g_id[32];
		memcpy(g_id, &groups_cnt[cmd.offset], cmd.count*sizeof(u16));
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)&g_id[cmd.offset], cmd.count*sizeof(u16));

		return 0;
	}else if (packet->cmd == EFFECTS_EQ_ONLINE_CMD_CHANGE_MODE){
		struct cmd {
			int id; 
			int modeId; 
		};
		struct cmd cmd;
		memcpy(&cmd, packet, sizeof(struct cmd));
#ifdef EFFECTS_DEBUG
		log_info("change mode cmd.modeid %d\n", cmd.modeId);
#endif
		p_effects_cfg->cur_mode_id = cmd.modeId;
		u32 id = packet->cmd;
		set_online_effects_mode(modes[cmd.modeId]);
        ci_send_packet(id, (u8 *)"OK", 2);
		return 0;
	}else if (packet->cmd == EFFECTS_ONLINE_CMD_MODE_SEQ_NUMBER){
		struct cmd {
			int id; 
			int modeId; 
		}; 
		struct cmd cmd;
		memcpy(&cmd, packet, sizeof(struct cmd));
#ifdef EFFECTS_DEBUG
		log_info("mode seq number  cmd.modeid %d %x\n", cmd.modeId, modes[cmd.modeId]);
#endif
		u32 id = packet->cmd;
		ci_send_packet(id, (u8 *)&modes[cmd.modeId], sizeof(u32));

		return 0;
	}


	return -EINVAL;
}

static void effects_online_callback(uint8_t *packet, uint16_t size)
{
    s32 res;
    if (!p_effects_cfg) {
        return ;
    }

    ASSERT(((int)packet & 0x3) == 0, "buf %x size %d\n", packet, size);
    res = effects_online_update((EFFECTS_ONLINE_PACKET *)packet);
	/* log_info("effects payload"); */
	/* log_info_hexdump(packet, sizeof(EFFECTS_ONLINE_PACKET)); */

    u32 id = EFFECTS_CONFIG_ID;

    if (res == 0) {
        log_info("Ack");
        ci_send_packet(id, (u8 *)"OK", 2);
        spin_lock(&p_effects_cfg->lock);
		int cmd = ((EFFECTS_ONLINE_PACKET *)packet)->cmd;
		u32 cur_mode_id = ((EFFECTS_ONLINE_PACKET *)packet)->data[0];
		
		if (cmd == EFFECTS_CMD_REVERB){
			p_effects_cfg->online_updata[/* p_effects_cfg-> */cur_mode_id][MAGIC_FLAG_reverb] = 1;
		}else if (cmd == EFFECTS_CMD_PITCH1){

		}else if (cmd == EFFECTS_CMD_PITCH2){
			p_effects_cfg->online_updata[/* p_effects_cfg-> */cur_mode_id][MAGIC_FLAG_pitch2] = 1;
		}else if (cmd == EFFECTS_CMD_ECHO){
			p_effects_cfg->online_updata[/* p_effects_cfg-> */cur_mode_id][MAGIC_FLAG_echo] = 1;
		}else if (cmd == EFFECTS_CMD_NOISE){
			p_effects_cfg->online_updata[/* p_effects_cfg-> */cur_mode_id][MAGIC_FLAG_noise] = 1;
		}else if (cmd == EFFECTS_CMD_SHOUT_WHEAT){
			p_effects_cfg->online_updata[/* p_effects_cfg-> */cur_mode_id][MAGIC_FLAG_shout_wheat] = 1;
		}else if (cmd == EFFECTS_CMD_LOW_SOUND){
			p_effects_cfg->online_updata[/* p_effects_cfg-> */cur_mode_id][MAGIC_FLAG_low_sound] = 1;
		}else if (cmd == EFFECTS_CMD_HIGH_SOUND){
			p_effects_cfg->online_updata[/* p_effects_cfg-> */cur_mode_id][MAGIC_FLAG_high_sound] = 1;
		}else if ((cmd == EFFECTS_EQ_ONLINE_CMD_PARAMETER_SEG) /* || (cmd == EFFECTS_EQ_ONLINE_CMD_PARAMETER_TOTAL_GAIN) */){
			p_effects_cfg->online_updata[/* p_effects_cfg-> */cur_mode_id][MAGIC_FLAG_eq_seg] = 1;
#if TCFG_EQ_ENABLE
			set_eq_online_updata(1);
#endif
		}else if (cmd == EFFECTS_CMD_MIC_ANALOG_GAIN){
			p_effects_cfg->online_updata[/* p_effects_cfg-> */cur_mode_id][MAGIC_FLAG_mic_gain] = 1;
		}
        spin_unlock(&p_effects_cfg->lock);
    } else {
        res = effects_online_nor_cmd((EFFECTS_ONLINE_PACKET *)packet);
        if (res == 0) {
            return ;
        }
		log_info("Nack");
        ci_send_packet(id, (u8 *)"ER", 2);
    }
}



static int effects_online_open(void)
{
    int i;
    int ch;
    spin_lock(&p_effects_cfg->lock);
    p_effects_cfg->eq_type = EFFECTS_TYPE_ONLINE;
    spin_unlock(&p_effects_cfg->lock);

    return 0;
}

static void effects_online_close(void)
{
}

REGISTER_CONFIG_TARGET(effects_config_target) = {
    .id         = EFFECTS_CONFIG_ID,
    .callback   = effects_online_callback,
};

//在线调试不进power down
static u8 effects_online_idle_query(void)
{
    if (!p_effects_cfg) {
        return 1;
    }
    return 0;
}

REGISTER_LP_TARGET(effects_online_lp_target) = {
    .name = "effects_online",
    .is_idle = effects_online_idle_query,
};
#endif /*TCFG_EFFECTS_ONLINE_ENABLE*/



/*-----------------------------------------------------------*/
// app
void effects_app_run_check(void *reverb)
{
	if (p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_reverb]){
		p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_reverb] = 0;
		effects_run_check(reverb, MAGIC_FLAG_reverb, &p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].reverb_parm.r_parm, modes[p_effects_cfg->cur_mode_id]);	
	}

	if (p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_pitch2]){
		p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_pitch2] = 0;
		effects_run_check(reverb, MAGIC_FLAG_pitch2,(void *)&p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].pitch_parm2.p_parm2,  modes[p_effects_cfg->cur_mode_id]);
	}
	if (p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_echo]){
		p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_echo] = 0;
		effects_run_check(reverb, MAGIC_FLAG_echo,(void *)&p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].echo_parm.e_parm,  modes[p_effects_cfg->cur_mode_id]);
	}

	if (p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_noise]){
		p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_noise] = 0;
		effects_run_check(reverb, MAGIC_FLAG_noise,(void *)&p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].noise_parm.n_parm,  modes[p_effects_cfg->cur_mode_id]);
	}

	if (p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_shout_wheat]){
		p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_shout_wheat] = 0;
		effects_run_check(reverb, MAGIC_FLAG_shout_wheat,(void *)&p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].shout_wheat_parm.s_w_parm,  modes[p_effects_cfg->cur_mode_id]);
	}

	if (p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_low_sound]){
		p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_low_sound] = 0;
		effects_run_check(reverb, MAGIC_FLAG_low_sound,(void *)&p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].low_parm.l_parm,  modes[p_effects_cfg->cur_mode_id]);
	}

	if (p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_high_sound]){
		p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_high_sound] = 0;
		effects_run_check(reverb, MAGIC_FLAG_high_sound,(void *)&p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].high_parm.h_parm,  modes[p_effects_cfg->cur_mode_id]);
	}

	if (p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_mic_gain]){
		p_effects_cfg->online_updata[p_effects_cfg->cur_mode_id][MAGIC_FLAG_mic_gain] = 0;
		effects_run_check(reverb, MAGIC_FLAG_mic_gain,(void *)p_effects_cfg->e_file[p_effects_cfg->cur_mode_id].mic_gain.mic_parm.gain,  modes[p_effects_cfg->cur_mode_id]);
	}

}

/*-----------------------------------------------------------*/
void effects_cfg_open(void)
{
    if (p_effects_cfg == NULL) {
        p_effects_cfg = zalloc(sizeof(EFFECT_CFG));
    }
    memset(p_effects_cfg, 0, sizeof(EFFECT_CFG));


	for(int i = 0; i < ARRAY_SIZE(modes); i++){
		u16 cur_mode_id = i;
		p_effects_cfg->design_mask[cur_mode_id] = (u32) - 1;
	}
	p_effects_cfg->cur_sr = 44100;

    spin_lock_init(&p_effects_cfg->lock);
    if (effects_file_get_cfg(p_effects_cfg)){//获取EFFECTS文件失败
		p_effects_cfg->effects_file_invalid = 1;
		for(int i = 0; i < mode_num/* ARRAY_SIZE(modes) */; i++){
			u16 cur_mode_id = i;
			p_effects_cfg->seg_num[cur_mode_id] = EQ_SECTION_MAX;
			p_effects_cfg->global_gain[cur_mode_id] = 0;
			log_i("p_effects_cfg->seg_num %d, EQ_SECTION_MAX_DEFAULT %d\n", p_effects_cfg->seg_num[cur_mode_id] ,EQ_SECTION_MAX_DEFAULT);

			memcpy(p_effects_cfg->e_file[cur_mode_id].eq_parm.e_parm.seg, eq_tab_normal, sizeof(EQ_CFG_SEG) * p_effects_cfg->seg_num[cur_mode_id]);//前五段默认系数表填直通 
			/* p_effects_cfg->online_updata[cur_mode_id][MAGIC_FLAG_eq_seg] = 1; */
			p_effects_cfg->design_mask[cur_mode_id] = (u32) - 1;
		}
	}

	effects_fill_default_config(p_effects_cfg);

#if (defined(TCFG_EFFECTS_ONLINE_ENABLE) && (TCFG_EFFECTS_ONLINE_ENABLE != 0))
    effects_online_open();
#endif
}

void effects_cfg_close(void)
{
    if (!p_effects_cfg) {
        return ;
    }

#if (defined(TCFG_EFFECTS_ONLINE_ENABLE) && (TCFG_EFFECTS_ONLINE_ENABLE != 0))
    effects_online_close();
#endif

    void *ptr = p_effects_cfg;
    p_effects_cfg = NULL;
    free(ptr);
}

int effects_init(void)
{
    effects_cfg_open();
    return 0;
}
__initcall(effects_init);
/*
 *切换音效模式，同时设置音效状态更新
 * mode:
*  mode0_seq 
*  mode1_seq 
*  mode2_seq 
*  mode3_seq 
*  mode4_seq 
* */




void set_effects_mode(u16 mode)
{
	if(p_effects_cfg)
	{
		/* if (p_effects_cfg->eq_type == EFFECTS_TYPE_ONLINE){ */
		if(p_effects_cfg->password_ok){
			log_i("warning !! EFFECTS_TYPE_ONLINE, set mode fail\n", mode);
			return ;
		}

		set_effects_mode_do(mode);
	}
}



u16 get_efects_mode()
{
	return modes[p_effects_cfg->cur_mode_id];
}

u16 get_efects_mode_index()
{
	return p_effects_cfg->cur_mode_id;
}


u8 get_effects_online()
{
	/* if (p_effects_cfg->eq_type == EFFECTS_TYPE_ONLINE){ */
	if(p_effects_cfg->password_ok){
		return true;	
	}
	return false;	
}


static const u32 electric_mode_formant_shift_tab[] = 
{
	A_MAJOR,
	Ashop_MAJOR,
	B_MAJOR,
	C_MAJOR,
	Cshop_MAJOR,
	D_MAJOR,
	Dshop_MAJOR,
	E_MAJOR,
	F_MAJOR,
	Fshop_MAJOR,
	G_MAJOR,
	Gshop_MAJOR,
};


u8 get_effects_electric_mode_max(void)
{
	return (ARRAY_SIZE(electric_mode_formant_shift_tab) - 1);
}


void set_effects_electric_mode(u16 mode)
{
	if(mode>=ARRAY_SIZE(electric_mode_formant_shift_tab))
	{
		return ;		
	}

	log_i("set_effects_electric_mode = %d\n", mode);
	if(p_effects_cfg)
	{
		p_effects_cfg->e_file[EFFECTS_MODE_ELECTRIC].pitch_parm2.p_parm2.formant_shift = electric_mode_formant_shift_tab[mode];
		set_effects_mode(EFFECTS_MODE_ELECTRIC);
	}
}




#endif

