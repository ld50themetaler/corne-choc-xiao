#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/uart/cdc_acm.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define RST_UF2 0x57

static void reboot_to_bootloader(void) {
    LOG_INF("Entering UF2 bootloader mode...");
    k_msleep(50);
#if defined(NRF_POWER)
    NRF_POWER->GPREGRET = 0x57;
#endif
    sys_reboot(RST_UF2);
}

#if defined(CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT)
static void cdc_rate_callback(const struct device *dev, uint32_t rate) {
    if (rate == 1200) {
        LOG_INF("1200 baud touch detected! Rebooting...");
        reboot_to_bootloader();
    }
}
#endif

#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
static char rx_buf[32];
static size_t rx_len = 0;

static void uart_rx_handler(const struct device *dev, void *user_data) {
    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            uint8_t c;
            while (uart_fifo_read(dev, &c, 1) == 1) {
                if (c == '\r' || c == '\n') {
                    rx_buf[rx_len] = '\0';
                    if (strcmp(rx_buf, "bootloader") == 0 || 
                        strcmp(rx_buf, "dfu") == 0 ||
                        strcmp(rx_buf, "reset") == 0) {
                        reboot_to_bootloader();
                    }
                    rx_len = 0;
                } else if (rx_len < sizeof(rx_buf) - 1) {
                    rx_buf[rx_len++] = c;
                } else {
                    rx_len = 0;
                }
            }
        }
    }
}
#endif

static struct k_work_delayable init_work;

static void bootloader_init_work_fn(struct k_work *work) {
#if DT_NODE_EXISTS(DT_NODELABEL(board_cdc_acm_uart))
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(board_cdc_acm_uart));
    if (device_is_ready(dev)) {
#if defined(CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT)
        cdc_acm_dte_rate_callback_set(dev, cdc_rate_callback);
#endif
#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
        uart_irq_callback_user_data_set(dev, uart_rx_handler, NULL);
        uart_irq_rx_enable(dev);
#endif
        LOG_INF("Bootloader UART trigger successfully armed!");
    } else {
        k_work_reschedule(&init_work, K_MSEC(500));
    }
#endif
}

static int bootloader_trigger_init(void) {
    k_work_init_delayable(&init_work, bootloader_init_work_fn);
    k_work_schedule(&init_work, K_MSEC(1000));
    return 0;
}

SYS_INIT(bootloader_trigger_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
