#ifndef NOISE_GATE_H
#define NOISE_GATE_H

#include "QuasiFloat.h"

#define EXPAND_BIT  15
#define LOG2_TABV(x)      ((x+(1L<<(24-EXPAND_BIT)))>>(25-EXPAND_BIT)) 

typedef struct _noiseGate {
	int follow_gain[2];
	int attackTime;/*1~1500ms*/
	int releaseTime;/*1~300ms*/
	QuasiFloat attFactor;
	QuasiFloat relFactor;
	QuasiFloat attFactor_sub;
	QuasiFloat relFactor_sub;
	int threshold;/*-92~0 *(1000)db*/

	int low_th_gain;/*0.00~1.00 * (1<<3000)*/
	int channel;
	int sampleRate;
}NOISEGATE_PARM;
typedef struct _NOISEGATE_API_STRUCT_ {
	NOISEGATE_PARM parm; 
    unsigned int 	*ptr;           	//运算buf指针
} NOISEGATE_API_STRUCT;

int noiseGate_buf();
void noiseGate_init(void *work_buf, int attackTime, int releaseTime,int threshold, int low_th_gain, int sample_rate, int channel);
int noiseGate_run(void *work_buf, short *in_buf, short *out_buf, int len);
#endif
