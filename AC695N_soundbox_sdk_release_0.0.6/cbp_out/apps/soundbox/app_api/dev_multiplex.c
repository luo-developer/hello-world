#include "app_api/dev_multiplex.h"


extern u8 sd_io_suspend(u8 sdx, u8 sd_io);
extern u8 sd_io_resume(u8 sdx, u8 sd_io);
extern int music_play_stop(void);
usb_dev g_usb_id = (usb_dev) - 1;
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
#define USB_ACTIVE 0x1
#define SD_ACTIVE  0x2

static u8 sd_first_online = 0;
static u8 mult_curr_dev = 0;

static int resume = 0;
void m_sdio_resume(u8 num)
{
    if (resume) {
        resume--;
        if (!resume) {
            sd_io_resume(0, num);
        }
    }
}

int m_sdio_supend(u8 num)
{
__try_t:
    if (sd_io_suspend(0, num)) {
        os_time_dly(1);
        goto __try_t;
    }
    resume++;
    return 0;
}
#endif
int dev_sd_change_usb()
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    if (mult_curr_dev != SD_ACTIVE) {
        return 0;
    }
    printf("sd_change_usb start \n");

#if 1
    m_sdio_resume(2);
__try_1:
    if (m_sdio_supend(2)) {
        os_time_dly(1);
        goto __try_1;
    }
#endif
    gpio_set_direction(TCFG_USB_SD_MULTIPLEX_IO, 1);
    gpio_set_pull_up(TCFG_USB_SD_MULTIPLEX_IO, 0);
    gpio_set_pull_down(TCFG_USB_SD_MULTIPLEX_IO, 0);
    gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 0);
    printf("sd_change_usb finsh \n");
    usb_otg_io_resume(g_usb_id);
    /* usb_host_resume(usb_id); */
    if (usb_host_remount(g_usb_id, MOUNT_RETRY, MOUNT_RESET, MOUNT_TIMEOUT, 0)) {
        log_error("udisk remount fail\n");
    } else {
        if (file_opr_dev_check("udisk")) {
            file_opr_dev_del((void *)"udisk");
            file_opr_dev_add("udisk");
        }
    }
    mult_curr_dev = USB_ACTIVE;
#endif
    return 0;
}

int dev_usb_change_sd()
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    if (mult_curr_dev != USB_ACTIVE) {
        return 0;
    }

    if (g_usb_id != (usb_dev) - 1 && usb_otg_online(g_usb_id) == HOST_MODE) {
        printf("dev_usb_change_sd start \n");
        usb_otg_io_suspend(g_usb_id);
#if 1
        m_sdio_resume(2);
#endif
        mult_curr_dev = SD_ACTIVE;
        os_time_dly(3);
        printf("dev_usb_change_sd finsh \n");
        return 0;
    }
    return -1;
#endif
    return true;
}


int sd_online_mount_before(usb_dev usb_id)
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    sd_first_online = 1;
    if (usb_id != (usb_dev) - 1 && usb_otg_online(usb_id) == HOST_MODE) {
        printf("suspend dm\n");
        music_play_stop();
        usb_otg_io_suspend(usb_id);
#if 1
        m_sdio_resume(2);
#endif
    }
#endif
    return 0;
}

int sd_online_mount_fail(usb_dev usb_id)
{

    int ret = 0;
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    struct storage_dev *udisk_stor;
    if (usb_id != (usb_dev) - 1 && usb_otg_online(usb_id) == HOST_MODE) {
        m_sdio_supend(2);
        gpio_set_direction(TCFG_USB_SD_MULTIPLEX_IO, 1);
        gpio_set_pull_up(TCFG_USB_SD_MULTIPLEX_IO, 0);
        gpio_set_pull_down(TCFG_USB_SD_MULTIPLEX_IO, 0);
        gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 0);
        usb_otg_io_resume(usb_id);
        /* usb_host_resume(usb_id); */
        if (usb_host_remount(usb_id, MOUNT_RETRY, MOUNT_RESET, MOUNT_TIMEOUT, 0)) {
            log_error("udisk remount fail\n");
        } else {
            if (file_opr_dev_check("udisk")) {
                file_opr_dev_del((void *)"udisk");
                file_opr_dev_add("udisk");
            }
            mult_curr_dev = USB_ACTIVE;
        }
    }
#endif
    return ret;
}

int sd_online_mount_after()
{
    int ret = 0;
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    mult_curr_dev = SD_ACTIVE;
#endif
    return ret;
}
int sd_offline_before(void *logo, usb_dev usb_id)
{
    int ret = 0 ;

#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    if (!sd_first_online) { //没有收到过上线消息不处理下线
        return 0;
    }

    struct storage_dev *udisk_stor;
    if (usb_id != (usb_dev) - 1 && usb_otg_online(usb_id) == HOST_MODE && mult_curr_dev != USB_ACTIVE) {
        printf("resume dm\n");

        music_play_stop();

        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s %d \n", __FUNCTION__, __LINE__);

        if (file_opr_dev_check(logo)) {
            file_opr_dev_del((void *)logo);
        }
#if 1

        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s %d \n", __FUNCTION__, __LINE__);

        m_sdio_resume(2);
__try_2:
        if (m_sdio_supend(2)) {
            os_time_dly(1);
            goto __try_2;
        }

        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s %d \n", __FUNCTION__, __LINE__);
        /* sd_clk_resume(2); */
#endif

        gpio_set_direction(TCFG_USB_SD_MULTIPLEX_IO, 1);
        gpio_set_pull_up(TCFG_USB_SD_MULTIPLEX_IO, 0);
        gpio_set_pull_down(TCFG_USB_SD_MULTIPLEX_IO, 0);
        gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 0);

        usb_otg_io_resume(usb_id);
        /* usb_host_resume(usb_id); */
        if (usb_host_remount(usb_id, MOUNT_RETRY, MOUNT_RESET, MOUNT_TIMEOUT, 0)) {
            log_error(">>>>>>>>>>> udisk remount fail\n");
        } else {
            if (file_opr_dev_check("udisk")) {
                file_opr_dev_del((void *)"udisk");
                file_opr_dev_add("udisk");
                mult_curr_dev = USB_ACTIVE;
            }
        }
    } else {
        m_sdio_resume(2);
        file_opr_dev_del((void *)logo);
        mult_curr_dev = 0;
    }
#endif
    return ret;
}

int usb_mount_before(usb_dev usb_id)
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    music_play_stop();
    os_time_dly(3);
#if 1
    m_sdio_resume(2);
__try_1:
    if (m_sdio_supend(2)) {
        os_time_dly(1);
        goto __try_1;
    }
    /* sd_clk_resume(2); */
#endif

    gpio_set_direction(TCFG_USB_SD_MULTIPLEX_IO, 1);
    gpio_set_pull_up(TCFG_USB_SD_MULTIPLEX_IO, 0);
    gpio_set_pull_down(TCFG_USB_SD_MULTIPLEX_IO, 0);
    gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 0);
#endif
    return 0;
}


int usb_mount_fail(usb_dev usb_id)
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0

#if 1
    m_sdio_resume(2);
#endif
    /* gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 1); */
#endif
    return 0;

}

int usb_online_mount_after(usb_dev usb_id)
{
    int ret = 0;
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    mult_curr_dev = USB_ACTIVE;
#endif
    return ret;
}

int usb_mount_offline(usb_dev usb_id)
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0

    printf(">>>>>>>>>>>>> %s %d \n", __FUNCTION__, __LINE__);
    usb_otg_io_suspend(usb_id);

#if 1
    m_sdio_resume(2);
#endif
    /* gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 1); */
    if (mult_curr_dev == USB_ACTIVE) {
        mult_curr_dev = SD_ACTIVE;
    } else {
        mult_curr_dev = 0;
    }

    printf(">>>>>>>>>>>>> %s %d \n", __FUNCTION__, __LINE__);
#endif
    return 0;

}

