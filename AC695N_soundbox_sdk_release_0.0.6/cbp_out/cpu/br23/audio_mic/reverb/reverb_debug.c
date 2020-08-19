#include "reverb_debug.h"

#define LOG_TAG     "[APP-REVERB]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"


void reverb_parm_printf(REVERBN_PARM_SET *parm)
{
	if(parm)
	{
		log_info("<reverb_parm>\n");
		log_info("dry        :%d\n", parm->dry);
		log_info("wet        :%d\n", parm->wet);
		log_info("delay      :%d\n", parm->delay);
		log_info("rot60      :%d\n", parm->rot60);
		log_info("Erwet 	 :%d\n", parm->Erwet);
		log_info("Erfactor   :%d\n", parm->Erfactor);
		log_info("Ewidth     :%d\n", parm->Ewidth);
		log_info("Ertolate   :%d\n", parm->Ertolate);
		log_info("predelay   :%d\n", parm->predelay);
		log_info("width      :%d\n", parm->width);
		log_info("diffusion  :%d\n", parm->diffusion);
		log_info("dampinglpf :%d\n", parm->dampinglpf);
		log_info("basslpf    :%d\n", parm->basslpf);
		log_info("bassB      :%d\n", parm->bassB);
		log_info("inputlpf   :%d\n", parm->inputlpf);
		log_info("outputlpf  :%d\n", parm->outputlpf);
	}
}

void pitch_parm_printf(PITCH_PARM_SET2 *parm)
{
	if(parm)
	{
		log_info("<pitch_parm>\n");
		log_info("effect_v 	 :%d\n", parm->effect_v);
		log_info("shiftv	 :%d\n", parm->pitch);
		log_info("formant 	 :%d\n", parm->formant_shift);
	}
}

void noisegate_parm_printf(NOISE_PARM_SET *parm)
{
	if(parm)
	{
		log_info("<noisegate_parm>\n");
		log_info("attackTime :%d\n", parm->attacktime);
		log_info("releaseTime:%d\n", parm->releasetime);
		log_info("threshold  :%d\n", parm->threadhold);
		log_info("gain 		 :%d\n", parm->gain);
	}
}




