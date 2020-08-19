#ifndef __DEV_MULTIPLEX_H__
#define __DEV_MULTIPLEX_H__

#include "generic/typedef.h"
#include "app_config.h"
#include "usb/host/usb_host.h"

extern usb_dev g_usb_id;

void m_sdio_resume(u8 num);
int m_sdio_supend(u8 num);
int dev_sd_change_usb();
int dev_usb_change_sd();
int sd_online_mount_before(usb_dev usb_id);
int sd_online_mount_fail(usb_dev usb_id);
int sd_online_mount_after();
int sd_offline_before(void *logo, usb_dev usb_id);
int usb_mount_before(usb_dev usb_id);
int usb_mount_fail(usb_dev usb_id);
int usb_online_mount_after(usb_dev usb_id);
int usb_mount_offline(usb_dev usb_id);

#endif//__DEV_MULTIPLEX_H__
