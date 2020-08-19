#ifndef __BT_PROFILE_DEF_H__
#define __BT_PROFILE_DEF_H__

///<<<注意此文件不要放函数声明, 只允许宏定义, 并且差异化定义可以根据需求在对应板卡中重新定义, 除非新增，否则不要直接修改这里
///<<<注意此文件不要放函数声明, 只允许宏定义, 并且差异化定义可以根据需求在对应板卡中重新定义, 除非新增，否则不要直接修改这里
///<<<注意此文件不要放函数声明, 只允许宏定义, 并且差异化定义可以根据需求在对应板卡中重新定义, 除非新增，否则不要直接修改这里
//
///---sdp service record profile- 用户选择支持协议--///

#define USER_SUPPORT_PROFILE_SPP    0
#define USER_SUPPORT_PROFILE_HFP    1
#define USER_SUPPORT_PROFILE_A2DP   1
#define USER_SUPPORT_PROFILE_AVCTP  1
#define USER_SUPPORT_PROFILE_HID    1
#define USER_SUPPORT_PROFILE_PNP    1
#define USER_SUPPORT_PROFILE_PBAP   0
#define USER_SUPPORT_PROFILE_HFP_AG 0

#endif//__BT_PROFILE_DEF_H__

