/*
 * Copyright (c) 2025 sekigon-gonnoc
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>

#if IS_ENABLED(CONFIG_RETENTION_BOOT_MODE)
#include <zephyr/retention/bootmode.h>
#endif

#include <zmk/events/usb_conn_state_changed.h>

LOG_MODULE_REGISTER(zmk_cdc_acm_bootloader_trigger, CONFIG_ZMK_LOG_LEVEL);

#define DT_DRV_COMPAT zmk_cdc_acm_bootloader_trigger

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// Check if there's at least one zephyr,cdc-acm-uart device available in the system
#if !DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
#error "CDC ACM Bootloader Trigger requires at least one zephyr,cdc-acm-uart device"
#endif

#define RST_UF2 0x57

#define ZMK_CDC_ACM_BOOTLOADER_TRIGGER_INST(n) DT_INST(n, zmk_cdc_acm_bootloader_trigger)

#if IS_ENABLED(CONFIG_ZMK_SETTINGS)
#include <zephyr/settings/settings.h>
#endif

struct cdc_acm_bootloader_trigger_config {
    const struct device *cdc_acm_dev; /* Can be NULL if auto-detect is used */
};

struct cdc_acm_bootloader_trigger_data {
    uint32_t baud_rate;
    bool port_open;
    bool usb_connected;
    struct k_work_delayable poll_work;
    const struct device *cdc_acm_dev; /* Store the CDC ACM device reference */
};

/* Forward declaration for work handlers */
static void enter_bootloader_work_handler(struct k_work *work);
static void poll_cdc_state_work_handler(struct k_work *work);

K_WORK_DEFINE(enter_bootloader_work, enter_bootloader_work_handler);

/* Reference to the single bootloader trigger instance */
static struct cdc_acm_bootloader_trigger_data *bootloader_trigger_data = NULL;

static void enter_bootloader_work_handler(struct k_work *work) {
    /* Small delay to let USB communications complete */
    k_sleep(K_MSEC(CONFIG_ZMK_CDC_ACM_BOOTLOADER_TRIGGER_DELAY_MS));

#if IS_ENABLED(CONFIG_RETENTION_BOOT_MODE)
    LOG_INF("Setting retention bootmode and rebooting into bootloader");
    bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);
    sys_reboot(SYS_REBOOT_WARM);
#else
    /* Reboot into bootloader mode using the configured reset code */
    LOG_INF("Rebooting into bootloader with reset code 0x%x", CONFIG_ZMK_CDC_ACM_BOOTLOADER_TRIGGER_RESET_CODE);
    sys_reboot(CONFIG_ZMK_CDC_ACM_BOOTLOADER_TRIGGER_RESET_CODE);
#endif
}

static void poll_cdc_state_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct cdc_acm_bootloader_trigger_data *data = 
        CONTAINER_OF(dwork, struct cdc_acm_bootloader_trigger_data, poll_work);
    
    const struct device *dev = data->cdc_acm_dev;
    if (dev == NULL || !device_is_ready(dev)) {
        k_work_schedule(dwork, K_MSEC(CONFIG_ZMK_CDC_ACM_BOOTLOADER_TRIGGER_POLL_MS));
        return;
    }

    uint32_t dtr = 0, baud_rate = 0;
    int ret_dtr = uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
    int ret_baud = uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &baud_rate);

    if (ret_baud == 0) {
        if (baud_rate == 1200) {
            LOG_INF("1200 baud detected on CDC ACM, triggering bootloader!");
            k_work_submit(&enter_bootloader_work);
            return;
        }
        data->baud_rate = baud_rate;
    }

    if (ret_dtr == 0) {
        if (!dtr && data->port_open && data->baud_rate == 1200) {
            LOG_INF("CDC ACM port closed after 1200 baud, triggering bootloader!");
            k_work_submit(&enter_bootloader_work);
            return;
        }
        data->port_open = (dtr != 0);
    }
    
    /* Schedule next poll */
    k_work_schedule(dwork, K_MSEC(CONFIG_ZMK_CDC_ACM_BOOTLOADER_TRIGGER_POLL_MS));
}

/* Handle USB connection state change events */
static int cdc_acm_bootloader_on_usb_conn_state_changed(const zmk_event_t *eh) {
    const struct zmk_usb_conn_state_changed *ev = as_zmk_usb_conn_state_changed(eh);
    
    if (bootloader_trigger_data == NULL) {
        return 0;
    }

    if (ev->conn_state == ZMK_USB_CONN_HID) {
        bootloader_trigger_data->usb_connected = true;
    } else if (ev->conn_state == ZMK_USB_CONN_NONE) {
        bootloader_trigger_data->usb_connected = false;
        bootloader_trigger_data->port_open = false;
        bootloader_trigger_data->baud_rate = 0;
    }

    return 0;
}

ZMK_LISTENER(cdc_acm_bootloader, cdc_acm_bootloader_on_usb_conn_state_changed);
ZMK_SUBSCRIPTION(cdc_acm_bootloader, zmk_usb_conn_state_changed);

static int cdc_acm_bootloader_trigger_init(const struct device *dev) {
    const struct cdc_acm_bootloader_trigger_config *config = dev->config;
    struct cdc_acm_bootloader_trigger_data *data = dev->data;
    const struct device *cdc_dev = NULL;
    
    /* If we have a configured CDC ACM device, use it */
    if (config->cdc_acm_dev != NULL) {
        cdc_dev = config->cdc_acm_dev;
        if (!device_is_ready(cdc_dev)) {
            LOG_ERR("Configured CDC ACM device not ready");
            return -ENODEV;
        }
    } else {
        /* Auto-detect a zephyr,cdc-acm-uart compatible device */
        #define CDC_ACM_UART_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_cdc_acm_uart)
        #if DT_NODE_EXISTS(CDC_ACM_UART_NODE)
            cdc_dev = DEVICE_DT_GET(CDC_ACM_UART_NODE);
            if (!device_is_ready(cdc_dev)) {
                LOG_ERR("Found CDC ACM UART device is not ready");
                return -ENODEV;
            }
            LOG_INF("Auto-detected zephyr,cdc-acm-uart device: %s", cdc_dev->name);
        #else
            LOG_ERR("No zephyr,cdc-acm-uart compatible device found in device tree");
            return -ENODEV;
        #endif
    }
    
    /* Initial state setup */
    data->port_open = false;
    data->baud_rate = 0;
    data->usb_connected = true;
    data->cdc_acm_dev = cdc_dev; /* Store detected or provided CDC ACM device */

    /* Initialize the polling work */
    k_work_init_delayable(&data->poll_work, poll_cdc_state_work_handler);

    /* Store reference to this instance for polling */
    bootloader_trigger_data = data;

    /* Start polling immediately */
    k_work_schedule(&data->poll_work, K_MSEC(CONFIG_ZMK_CDC_ACM_BOOTLOADER_TRIGGER_POLL_MS));

    LOG_INF("CDC ACM bootloader trigger initialized and polling started");
    return 0;
}

#define CDC_ACM_BOOTLOADER_TRIGGER_INIT(n)                                                    \
    static struct cdc_acm_bootloader_trigger_data cdc_acm_bootloader_trigger_data_##n = {     \
        .baud_rate = 0,                                                                       \
        .port_open = false,                                                                   \
        .usb_connected = true,                                                                \
    };                                                                                         \
                                                                                               \
    static const struct cdc_acm_bootloader_trigger_config cdc_acm_bootloader_trigger_config_##n = { \
        .cdc_acm_dev = COND_CODE_1(DT_INST_NODE_HAS_PROP(n, cdc_port),                        \
                                 (DEVICE_DT_GET(DT_INST_PHANDLE(n, cdc_port))),               \
                                 (NULL)),                                                       \
    };                                                                                         \
                                                                                               \
    DEVICE_DT_INST_DEFINE(n, cdc_acm_bootloader_trigger_init, NULL,                           \
                         &cdc_acm_bootloader_trigger_data_##n,                                \
                         &cdc_acm_bootloader_trigger_config_##n, POST_KERNEL,                 \
                         CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL);

DT_INST_FOREACH_STATUS_OKAY(CDC_ACM_BOOTLOADER_TRIGGER_INIT)
#endif /* CONFIG_ZMK_CDC_ACM_BOOTLOADER_TRIGGER */
