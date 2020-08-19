#include "audio_splicing.h"

#define AUDIO_SPLICING_ASM_ENABLE    1

void pcm_single_to_dual(void *out, void *in, u16 len)
{
    s16 *outbuf = out;
    s16 *inbuf = in;
    len >>= 1;
    while (len--) {
        *outbuf++ = *inbuf;
        *outbuf++ = *inbuf;
        inbuf++;
    }
}

void pcm_single_to_qual(void *out, void *in, u16 len)
{
    s16 *outbuf = out;
    s16 *inbuf = in;
    len >>= 1;
    while (len--) {
        *outbuf++ = *inbuf;
        *outbuf++ = *inbuf;
        *outbuf++ = *inbuf;
        *outbuf++ = *inbuf;
        inbuf++;
    }
}

void pcm_dual_to_qual(void *out, void *in, u16 len)
{
    s16 *outbuf = out;
    s16 *inbuf = in;
    len >>= 2;
    while (len--) {
        *outbuf++ = *(inbuf+0);
        *outbuf++ = *(inbuf+1);
        *outbuf++ = *(inbuf+0);
        *outbuf++ = *(inbuf+1);
        inbuf+=2;
    }
}

void pcm_dual_mix_to_dual(void *out, void *in, u16 len)
{
    s16 *outbuf = out;
    s16 *inbuf = in;
	s32 tmp32;
    len >>= 2;
	while (len--) {
		tmp32 = (inbuf[0] + inbuf[1]);
		if (tmp32 < -32768) {
			tmp32 = -32768;
		} else if (tmp32 > 32767) {
			tmp32 = 32767;
		}
		*outbuf++ = tmp32;
		*outbuf++ = tmp32;
		inbuf += 2;
    }
}

void pcm_dual_to_single(void *out, void *in, u16 len)
{
    s16 *outbuf = out;
    s16 *inbuf = in;
	s32 tmp32;
    len >>= 2;
	while (len--) {
		tmp32 = (inbuf[0] + inbuf[1]);
		if (tmp32 < -32768) {
			tmp32 = -32768;
		} else if (tmp32 > 32767) {
			tmp32 = 32767;
		}
		*outbuf++ = tmp32;
		inbuf += 2;
    }
}

void pcm_qual_to_single(void *out, void *in, u16 len)
{
    s16 *outbuf = out;
    s16 *inbuf = in;
	s32 tmp32;
    len >>= 3;
    while (len--) {
		tmp32 = (inbuf[0] + inbuf[1] + inbuf[2] + inbuf[3]);
		if (tmp32 < -32768) {
			tmp32 = -32768;
		} else if (tmp32 > 32767) {
			tmp32 = 32767;
		}

		*outbuf++ = tmp32;
        inbuf += 4;
    }
}

void pcm_single_l_r_2_dual(void *out, void *in_l, void *in_r, u16 in_len)
{
    s16 *outbuf = out;
    s16 *inbuf_l = in_l;
    s16 *inbuf_r = in_r;
    in_len >>= 1;
    while (in_len--) {
        *outbuf++ = *inbuf_l++;
        *outbuf++ = *inbuf_r++;
    }
}


void pcm_fl_fr_rl_rr_2_qual(void *out, void *in_fl, void *in_fr, void *in_rl, void *in_rr, u16 in_len)
{
    s16 *outbuf = out;
    s16 *inbuf_fl = in_fl;
    s16 *inbuf_fr = in_fr;
    s16 *inbuf_rl = in_rl;
    s16 *inbuf_rr = in_rr;
    in_len >>= 1;
    while (in_len--) {
        *outbuf++ = *inbuf_fl++;
        *outbuf++ = *inbuf_fr++;
        *outbuf++ = *inbuf_rl++;
        *outbuf++ = *inbuf_rr++;
    }
}

///indx=0、1、2、3表示dac通道
void pcm_fill_single_2_qual(void *out, void *in, u16 in_len, u8 idx)
{
    s16 *outbuf = out;
    s16 *inbuf_single = in;
    in_len >>= 1;
	while (in_len--) {
		*(outbuf + idx) = *inbuf_single++;	
		outbuf += 4;
    }
}


void pcm_flfr_rlrr_2_qual(void *out, void *in_flfr, void *in_rlrr, u16 in_len)
{
    s16 *outbuf = out;
    s16 *inbuf_flfr = in_flfr;
    s16 *inbuf_rlrr = in_rlrr;
    in_len >>= 2;
    while (in_len--) {
        *outbuf++ = *(inbuf_flfr+0);
        *outbuf++ = *(inbuf_flfr+1);
		*outbuf++ = *(inbuf_rlrr+0);
		*outbuf++ = *(inbuf_rlrr+1);
		inbuf_flfr += 2;
		inbuf_rlrr += 2;
	}
}


void pcm_fill_flfr_2_qual(void *out, void *in_flfr, u16 in_len)
{
    s16 *outbuf = out;
    s16 *inbuf_flfr = in_flfr;
    in_len >>= 2;
#if (!AUDIO_SPLICING_ASM_ENABLE) 
	while (in_len--) {
		*outbuf++ = *(inbuf_flfr+0);
		*outbuf++ = *(inbuf_flfr+1);
		inbuf_flfr += 2;
		outbuf += 2;
	}
#else
	s16 tmp;
    __asm__ volatile(
        "1:							\n\t"
        "rep %3	{					\n\t"   
        "%2 = h[%1 ++= 2](s)		\n\t"		
        "h[%0 ++= 2] = %2			\n\t"		
        "%2 = h[%1 ++= 2](s)		\n\t"		
        "h[%0 ++= 2] = %2			\n\t"		
        "	%0 += 4					\n\t"
		"   %3 -= 1					\n\t"
        "}							\n\t"
        "if(%3 != 0) goto 1b		\n\t"
        :
        "=&r"(outbuf),
        "=&r"(inbuf_flfr),
        "=&r"(tmp),
        "=&r"(in_len)
        :
        "0"(outbuf),
        "1"(inbuf_flfr),
        "2"(tmp),
        "3"(in_len)
        :
		"cc");

#endif
}

void pcm_fill_rlrr_2_qual(void *out, void *in_rlrr, u16 in_len)
{
    s16 *outbuf = out;
    s16 *inbuf_rlrr = in_rlrr;
    in_len >>= 2;
#if (!AUDIO_SPLICING_ASM_ENABLE) 
	while (in_len--) {
		outbuf += 2;
		*outbuf++ = *(inbuf_rlrr+0);
		*outbuf++ = *(inbuf_rlrr+1);
		inbuf_rlrr += 2;
	}
#else
	s16 tmp;
    __asm__ volatile(
        "1:							\n\t"
        "rep %3	{					\n\t"   
        "	%0 += 4					\n\t"
        "%2 = h[%1 ++= 2](s)		\n\t"		
        "h[%0 ++= 2] = %2			\n\t"		
        "%2 = h[%1 ++= 2](s)		\n\t"		
        "h[%0 ++= 2] = %2			\n\t"		
		"   %3 -= 1					\n\t"
        "}							\n\t"
        "if(%3 != 0) goto 1b		\n\t"
        :
        "=&r"(outbuf),
        "=&r"(inbuf_rlrr),
        "=&r"(tmp),
        "=&r"(in_len)
        :
        "0"(outbuf),
        "1"(inbuf_rlrr),
        "2"(tmp),
        "3"(in_len)
        :
		"cc");

#endif
}

void pcm_mix_buf(s32 *obuf, s16 *ibuf, u16 len)
{
    u16 i;
    for (i = 0; i < len; i++) {
        obuf[i] += ibuf[i];
    }
}

void pcm_mix_buf_limit(s16 *obuf, s32 *ibuf, u16 len)
{
    u16 i;
    for (i = 0; i < len; i++) {
        if (ibuf[i] > 32767) {
            ibuf[i] = 32767;
        } else if (ibuf[i] < -32768) {
            ibuf[i] = -32768;
        }
        obuf[i] = (s16)ibuf[i];
    }
}





