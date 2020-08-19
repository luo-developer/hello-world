#include "asm/includes.h"
//#include "asm/ldo.h"
//#include "asm/cache.h"
#include "asm/wdt.h"
#include "asm/debug.h"
#include "asm/efuse.h"
#include "asm/power/p33.h"
#include "system/task.h"
#include "timer.h"
#include "system/init.h"
#include "system/includes.h"

#include "app_config.h"
#include "gpio.h"
//#include "power_manage.h"
//
#define LOG_TAG_CONST       SETUP
#define LOG_TAG             "[SETUP]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

//extern void dv15_dac_early_init(u8 ldo_sel, u8 pwr_sel, u32 dly_msecs);
//
extern void sys_timer_init(void);

extern void tick_timer_init(void);

extern void vPortSysSleepInit(void);

extern void reset_source_dump(void);

extern u8 power_reset_source_dump(void);

extern void exception_irq_handler(void);
int __crc16_mutex_init();

extern int __crc16_mutex_init();

#define DEBUG_SINGAL_IDLE(x)        //if (x) IO_DEBUG_1(A, 7) else IO_DEBUG_0(A, 7)
#define DEBUG_SINGAL_1S(x)          //if (x) IO_DEBUG_1(A, 6) else IO_DEBUG_0(A, 6)

#if (defined CONFIG_DEBUG_ENABLE) || (defined CONFIG_DEBUG_LITE_ENABLE)
void debug_uart_init(const struct uart_platform_data *data);
#endif

#if 0
___interrupt
void exception_irq_handler(void)
{
    ___trig;

    exception_analyze();

    log_flush();
    while (1);
}
#endif



/*
 * 此函数在cpu0上电后首先被调用,负责初始化cpu内部模块
 *
 * 此函数返回后，操作系统才开始初始化并运行
 *
 */

#if 0
static void early_putchar(char a)
{
    if (a == '\n') {
        UT2_BUF = '\r';
        __asm_csync();
        while ((UT2_CON & BIT(15)) == 0);
    }
    UT2_BUF = a;
    __asm_csync();
    while ((UT2_CON & BIT(15)) == 0);
}

void early_puts(char *s)
{
    do {
        early_putchar(*s);
    } while (*(++s));
}
#endif

void cpu_assert_debug()
{
#ifdef CONFIG_DEBUG_ENABLE
    log_flush();
    local_irq_disable();
    while (1);
#else
    cpu_reset();
#endif
}

void timer(void *p)
{
    /* DEBUG_SINGAL_1S(1); */
    sys_timer_dump_time();
    /* DEBUG_SINGAL_1S(0);*/
}

u8 power_reset_src = 0;
extern void sputchar(char c);
extern void sput_buf(const u8 *buf, int len);
void sput_u32hex(u32 dat);
void *vmem_get_phy_adr(void *vaddr);

void test_fun()
{
    wdt_close();
    while (1);

}

AT_VOLATILE_RAM_CODE
void __lvd_irq_handler(void)
{
    VLVD_PND_CLR(1);
}

extern void fm_set_osc_cap(u8 sel_l, u8 sel_r);
extern void bt_set_osc_cap(u8 sel_l, u8 sel_r);

int app_chip_set_osc_cap(void)
{
    u8 cap_l, cap_r;
    cap_l = 0x0a;
    cap_r = 0x0a;
    fm_set_osc_cap(cap_l, cap_r);
    bt_set_osc_cap(cap_l, cap_r); ///电容 0~0x0f
    return 0;
}
early_initcall(app_chip_set_osc_cap);

void load_common_code();
void app_load_common_code()
{
#ifdef CONFIG_CODE_BANK_ENABLE
    /* load_common_code(); */
#endif
}

u32 stack_magic[4] sec(.stack_magic);
u32 stack_magic0[4] sec(.stack_magic0);

extern void lvd_enable(void);
void memory_init(void);
void setup_arch()
{
    memory_init();

    memset(stack_magic, 0x5a, sizeof(stack_magic));
    memset(stack_magic0, 0x5a, sizeof(stack_magic0));


    wdt_init(WDT_4S);
    /* wdt_close(); */

    clk_voltage_init(TCFG_CLOCK_MODE, SYSVDD_VOL_SEL_126V, TCFG_LOWPOWER_POWER_SEL);


    clk_early_init(TCFG_CLOCK_SYS_SRC, TCFG_CLOCK_OSC_HZ, TCFG_CLOCK_SYS_HZ);


    /* lvd_enable(); */
    /*interrupt_init();*/

#if (defined CONFIG_DEBUG_ENABLE) || (defined CONFIG_DEBUG_LITE_ENABLE)
    debug_uart_init(NULL);

#ifdef CONFIG_DEBUG_ENABLE
    log_early_init(1024);
#endif

#endif

#ifdef CONFIG_CODE_BANK_ENABLE
    extern void bank_syscall_entry();
    request_irq(IRQ_SYSCALL_IDX, 0, bank_syscall_entry, 0);
#endif


    printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("         setup_arch %s %s \n", __DATE__, __TIME__);
    printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");


    clock_dump();
    /* log_info("resour est: %d", get_boot_flag()); */
    //set_boot_flag(99);
    /* log_info("resour est: %d", get_boot_flag()); */

    reset_source_dump();

    power_reset_src = power_reset_source_dump();

    request_irq(1, 2, exception_irq_handler, 0);

    debug_init();

    sys_timer_init();

    /* sys_timer_add(NULL, timer, 10 * 1000); */

    tick_timer_init();

    __crc16_mutex_init();
}

/*-----------------------------------------------------------*/
