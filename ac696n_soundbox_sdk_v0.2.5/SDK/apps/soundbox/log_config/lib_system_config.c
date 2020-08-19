/*********************************************************************************************
    *   Filename        : lib_system_config.c

    *   Description     : Optimized Code & RAM (编译优化配置)

    *   Author          : Bingquan

    *   Email           : caibingquan@zh-jieli.com

    *   Last modifiled  : 2019-03-18 15:22

    *   Copyright:(c)JIELI  2011-2019  @ , All Rights Reserved.
*********************************************************************************************/

#include "app_config.h"
#include "system/includes.h"


///打印是否时间打印信息
const int config_printf_time         = 1;


///异常中断，asser打印开启
#ifdef CONFIG_RELEASE_ENABLE
const int config_asser         = 0;
#else
const int config_asser         = 1;
#endif

/**
 * @brief Log (Verbose/Info/Debug/Warn/Error)
 */
/*-----------------------------------------------------------*/
const char log_tag_const_v_SYS_TMR AT(.LOG_TAG_CONST) = LIB_DEBUG &  FALSE;
const char log_tag_const_i_SYS_TMR AT(.LOG_TAG_CONST) = LIB_DEBUG &  FALSE;
const char log_tag_const_d_SYS_TMR AT(.LOG_TAG_CONST) = LIB_DEBUG &  FALSE;
const char log_tag_const_w_SYS_TMR AT(.LOG_TAG_CONST) = LIB_DEBUG &  TRUE;
const char log_tag_const_e_SYS_TMR AT(.LOG_TAG_CONST) = LIB_DEBUG &  TRUE;

const char log_tag_const_v_JLFS AT(.LOG_TAG_CONST) = LIB_DEBUG &  FALSE;
const char log_tag_const_i_JLFS AT(.LOG_TAG_CONST) = LIB_DEBUG &  FALSE;
const char log_tag_const_d_JLFS AT(.LOG_TAG_CONST) = LIB_DEBUG &  FALSE;
const char log_tag_const_w_JLFS AT(.LOG_TAG_CONST) = LIB_DEBUG &  TRUE;
const char log_tag_const_e_JLFS AT(.LOG_TAG_CONST) = LIB_DEBUG &  TRUE;

//FreeRTOS
const char log_tag_const_v_PORT AT(.LOG_TAG_CONST) = LIB_DEBUG & FALSE;
const char log_tag_const_i_PORT AT(.LOG_TAG_CONST) = LIB_DEBUG & 0;
const char log_tag_const_d_PORT AT(.LOG_TAG_CONST) = LIB_DEBUG & FALSE;
const char log_tag_const_w_PORT AT(.LOG_TAG_CONST) = LIB_DEBUG & TRUE;
const char log_tag_const_e_PORT AT(.LOG_TAG_CONST) = LIB_DEBUG & TRUE;

const char log_tag_const_v_KTASK AT(.LOG_TAG_CONST) = LIB_DEBUG & FALSE;
const char log_tag_const_i_KTASK AT(.LOG_TAG_CONST) = LIB_DEBUG & 0;
const char log_tag_const_d_KTASK AT(.LOG_TAG_CONST) = LIB_DEBUG & FALSE;
const char log_tag_const_w_KTASK AT(.LOG_TAG_CONST) = LIB_DEBUG & TRUE;
const char log_tag_const_e_KTASK AT(.LOG_TAG_CONST) = LIB_DEBUG & TRUE;

const char log_tag_const_v_uECC AT(.LOG_TAG_CONST) = LIB_DEBUG & FALSE;
const char log_tag_const_i_uECC AT(.LOG_TAG_CONST) = LIB_DEBUG & 0;
const char log_tag_const_d_uECC AT(.LOG_TAG_CONST) = LIB_DEBUG & FALSE;
const char log_tag_const_w_uECC AT(.LOG_TAG_CONST) = LIB_DEBUG & TRUE;
const char log_tag_const_e_uECC AT(.LOG_TAG_CONST) = LIB_DEBUG & TRUE;
