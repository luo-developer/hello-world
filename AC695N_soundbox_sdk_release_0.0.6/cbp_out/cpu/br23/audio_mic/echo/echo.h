#ifndef __ECHO_H__
#define __ECHO_H__

#include "system/includes.h"

bool echo_start(void);
void echo_stop(void);
u8 echo_get_status(void);
void echo_set_dvol(u8 vol);
u8 echo_get_dvol(void);

#endif// __ECHO_H__
