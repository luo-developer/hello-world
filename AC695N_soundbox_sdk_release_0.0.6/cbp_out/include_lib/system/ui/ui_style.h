#ifndef UI_STYLE_H
#define UI_STYLE_H


#define STYLE_JL_01              (1)
#define STYLE_JL_02              (2)
#define STYLE_JL_03              (3)

#define CONFIG_UI_STYLE           STYLE_JL_02

#if(CONFIG_UI_STYLE == STYLE_JL_01)
#include "ui/style_jl01.h"//彩屏

#define ID_WINDOW_BT       PAGE_0
#define ID_WINDOW_MUSIC    PAGE_1
#define ID_WINDOW_LINEIN   PAGE_1
#define ID_WINDOW_FM       PAGE_5
#define ID_WINDOW_BT_MENU  PAGE_3
#endif



#if(CONFIG_UI_STYLE == STYLE_JL_02) 
#include "ui/style_jl02.h"//点阵

#define ID_WINDOW_MAIN     PAGE_0
#define ID_WINDOW_BT       PAGE_1
#define ID_WINDOW_FM       PAGE_2
#define ID_WINDOW_MUSIC    PAGE_0
#define ID_WINDOW_LINEIN   PAGE_0
#define ID_WINDOW_BT_MENU  PAGE_0

#endif



#endif
