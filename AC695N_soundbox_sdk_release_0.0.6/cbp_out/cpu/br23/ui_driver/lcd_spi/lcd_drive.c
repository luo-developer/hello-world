/* LCD 调试等级，
 * 0只打印错误，
 * 1打印错误和警告，
 * 2全部内容都调试内容打印 */
#define SPI_LCD_DEBUG_ENABLE    0

#include "includes.h"
#include "app_config.h"
#include "ui/ui_api.h"
#include "system/includes.h"
#include "system/timer.h"
#include "asm/spi.h"


#if TCFG_SPI_LCD_ENABLE

/* 选择SPIx模块作为推屏通道，0、1、2 */
/* 但SPI0通常作为外部falsh使用，一般不用SPI0 */
#define SPI_MODULE_CHOOSE   1

/* 中断使能，一般推屏不需要 */
/* #define SPI_INTERRUPT_ENABLE */


#ifdef SPI_INTERRUPT_ENABLE
#if SPI_MODULE_CHOOSE == 0
#define IRQ_SPI_IDX     IRQ_SPI0_IDX
#elif SPI_MODULE_CHOOSE == 1
#define IRQ_SPI_IDX     IRQ_SPI1_IDX
#elif SPI_MODULE_CHOOSE == 2
#define IRQ_SPI_IDX     IRQ_SPI2_IDX
#else
#error  "error! SPI_MODULE_CHOOSE defien error!"
#endif
#endif


/* 屏幕驱动的接口 */
extern struct spi_lcd_init dev_drive;
struct lcd_spi_platform_data *spi_dat = NULL;
#define __this (&dev_drive)

#ifdef SPI_INTERRUPT_ENABLE
/* SPI中断函数 */
// 注：dma模式在发送数据时内部已经清理中断pnd
__attribute__((interrupt("")))
static void spi_isr()
{
    /* if (spi_get_pending(spi_hdl)) { */
		/* spi_clear_pending(spi_hdl); */
    /* } */
}
#endif


static void spi_init(int spi_cfg)
{
    int err;
    // spi gpio init

    err = spi_open(spi_cfg);
    if (err < 0) {
        lcd_e("open spi falid\n");
    }
#ifdef SPI_INTERRUPT_ENABLE
    // 配置中断优先级，中断函数
    spi_set_ie(spi_cfg, 1);
    request_irq(IRQ_SPI_IDX, 3, spi_isr, 0);
#endif
}

// io口操作
void lcd_reset_l()
{
    gpio_direction_output((u32)spi_dat->pin_reset, 0);
}
void lcd_reset_h()
{
    gpio_direction_output((u32)spi_dat->pin_reset, 1);
}
void lcd_cs_l()
{
    gpio_direction_output((u32)spi_dat->pin_cs, 0);
}
void lcd_cs_h()
{
    gpio_direction_output((u32)spi_dat->pin_cs, 1);
}
void lcd_rs_l()
{
    gpio_direction_output((u32)spi_dat->pin_rs, 0);
}
void lcd_rs_h()
{
    gpio_direction_output((u32)spi_dat->pin_rs, 1);
}

void lcd_bl_l()
{
    gpio_direction_output((u32)spi_dat->pin_bl,0);
}

void lcd_bl_h()
{
    gpio_direction_output((u32)spi_dat->pin_bl,1);
}

void spi_dma_send_byte(u8 dat)
{
    int err = 0;
	u32 _dat __attribute__((aligned(4)));

	((u8 *)(&_dat))[0] = dat;

    if (spi_dat) {
		err = spi_dma_send(spi_dat->spi_cfg, &_dat, 1);
    }

    if (err < 0) {
        lcd_e("spi dma send byte timeout\n");
    }
}

void spi_dma_send_map(u8 *map, u32 size)
{
    int err = 0;

    if (spi_dat) {
		err = spi_dma_send(spi_dat->spi_cfg, map, size);
    }

    if (err < 0) {
        lcd_e("spi dma send map timeout\n");
    }

}

void spi_dma_recv_data(u8 *buf, u32 size)
{
    int err = 0;

    if (spi_dat) {
		err = spi_dma_recv(spi_dat->spi_cfg, buf, size);
    }

    if (err < 0) {
        lcd_e("spi dma recv timeout\n");
    }
}


static void spi_init_code(const InitCode *code, u8 cnt)
{
	u8 i, j;

	for (i = 0; i < cnt; i++) {
		if (code[i].cmd == REGFLAG_DELAY) {
			extern void wdt_clear(void);
			wdt_clear();
            delay2ms(code[i].cnt/2);
		} else {
			__this->WriteComm(code[i].cmd);
			lcd_d("cmd:%x ", code[i].cmd);
			for (j = 0; j < code[i].cnt; j++) {
				__this->WriteData(code[i].dat[j]);
				lcd_d("%02x ", code[i].dat[j]);
			}
			lcd_d("\n");
		}
	}
}

static void lcd_dev_init(void *p)
{
    struct ui_devices_cfg *cfg =(struct ui_devices_cfg *)p;
	int err = 0;
	spi_dat = (struct lcd_spi_platform_data *)cfg->private_data;
	ASSERT(spi_dat, "Error! spi io not config");
	lcd_d("spi pin rest:%d, cs:%d, rs:%d, spi:%d\n", spi_dat->pin_reset, spi_dat->pin_cs, spi_dat->pin_rs, spi_dat->spi_cfg);

	gpio_direction_output((u32)spi_dat->pin_reset, 0);
	gpio_direction_output((u32)spi_dat->pin_cs, 0);
	gpio_direction_output((u32)spi_dat->pin_rs, 1);

	spi_init(spi_dat->spi_cfg);


	if (__this->Reset) { // 如果有硬件复位
		__this->Reset();
	}

    if(__this->initcode && __this->initcode_cnt) {
	    spi_init_code(__this->initcode, __this->initcode_cnt);  // 初始化屏幕
    } else if(__this->Init) {
        __this->Init();
    }

	/* if (dev_drive.BackLightCtrl)	// 如果有背光控制 */
		/* dev_drive.BackLightCtrl(true); */
}

void SPI_LcdTest()
{
    unsigned int i;
	int c = 0;
	extern struct ui_devices_cfg ui_cfg_data;
    lcd_dev_init((void*)&ui_cfg_data);

    while(1) {
        const u8 color_tab[]={
			0xf8,0x00,0x07,0xe0,0x00,0x1f
		};
		__this->SetDrawArea(0, 240 - 1, 0, 240 - 1);
        for(i=0;i<240*240;i++) {
			__this->WriteData(color_tab[2 * c]);
			__this->WriteData(color_tab[2 * c + 1]);
		}

		c++;
        if(c == 3) {
			c = 0;
		}
		os_time_dly(100);
	}
}

static int lcd_init(void *p)
{
    lcd_dev_init(p);
	return 0;
}

static int lcd_get_screen_info(struct lcd_info *info)
{
	info->width = __this->lcd_width;
	info->height = __this->lcd_height;
	info->color_format = __this->color_format;
	info->interface = __this->interface;
	return 0;
}


static int lcd_buffer_malloc(u8 **buf, u16 *size)
{
	*buf = __this->dispbuf;
	*size = __this->bufsize;
	return 0;
}

static int lcd_buffer_free(u8 *buf)
{
	return 0;
}

static int lcd_draw(u8 *buf, u16 len)
{
	__this->WriteMap(buf, len);
	return 0;
}

static int lcd_set_draw_area(u16 xs, u16 xe, u16 ys, u16 ye)
{
	__this->SetDrawArea(xs, xe, ys, ye);
	return 0;
}

static int lcd_clear_screen(u16 color)
{
	int i;
	int buffer_lines;
	int remain;
	int draw_line;

	__this->SetDrawArea(0, __this->lcd_width - 1, 0, __this->lcd_height - 1);
	buffer_lines = __this->bufsize / __this->lcd_width / 2;
	for (i = 0; i < buffer_lines * __this->lcd_width; i++) {
		__this->dispbuf[2 * i] = color >> 8;
		__this->dispbuf[2 * i + 1] = color;
    } 

	remain = __this->lcd_height;
    while(remain) {
		draw_line = buffer_lines > remain ? remain : buffer_lines;
		__this->WriteMap(__this->dispbuf, draw_line * __this->lcd_width * 2);
		remain -= draw_line;
	}
	return 0;
}

static int lcd_backlight_ctrl(u8 on)
{
	static u8 first_power_on = true;
    if(first_power_on) {
		lcd_clear_screen(0x0000);
		first_power_on = false;
	}

	if (__this->BackLightCtrl) {
		__this->BackLightCtrl(on);
	}

	return 0;
}

struct lcd_interface *lcd_get_hdl()
{
	struct lcd_interface *p;

	ASSERT(lcd_interface_begin != lcd_interface_end, "don't find lcd interface!");
    for(p = lcd_interface_begin; p < lcd_interface_end; p++) {
       return p; 
	}
	return NULL;
}

REGISTER_LCD_INTERFACE(lcd) = {
	.init = lcd_init,
	.get_screen_info = lcd_get_screen_info,
	.buffer_malloc = lcd_buffer_malloc,
	.buffer_free = lcd_buffer_free,
	.draw = lcd_draw,
	.set_draw_area = lcd_set_draw_area,
	.backlight_ctrl = lcd_backlight_ctrl,
};

#endif
