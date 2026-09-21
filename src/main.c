/**
 * ==============================================================================
 * File: zephyr_project/src/main.c
 * Mục đích: Ứng dụng chính Zephyr RTOS (CAN Gateway, DBC/E2E, Multi-threading, Safety Monitor)
 * ==============================================================================
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "can_gateway.h"
#include "dbc_decoder.h"
#include "safety_monitor.h"

LOG_MODULE_REGISTER(main_app, LOG_LEVEL_INF);

/* 1. Biến toàn cục lưu trữ thông số xe hơi, được bảo vệ bằng k_mutex (Priority Inheritance) */
VehicleTelemetry_t g_current_telemetry = { 0 };
K_MUTEX_DEFINE(g_telemetry_mutex);

/* 2. Lấy đèn LED cảnh báo người dùng từ Devicetree (Chân PI1) */
static const struct gpio_dt_spec warn_led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led_warn), gpios, {0});

/* ==============================================================================
 * LUỒNG 1: CAN INGESTION & DBC DECODER WORKER (Mức ưu tiên 5)
 * ============================================================================== */
#define CAN_WORKER_STACK_SIZE 2048
#define CAN_WORKER_PRIO       5

void can_worker_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    struct can_frame rx_frame;
    VehicleTelemetry_t local_tel;

    LOG_INF("CAN Worker Thread da khoi dong (Priority = %d).", CAN_WORKER_PRIO);

    while (1) {
        /* Chờ gói tin CAN mới từ hàng đợi (Zero-CPU khi bus rảnh) */
        k_msgq_get(&raw_can_msgq, &rx_frame, K_FOREVER);

        /* Giải mã Vector DBC và xác thực 3 lớp an toàn AUTOSAR E2E */
        if (dbc_decode_vehicle_frame(rx_frame.data, rx_frame.dlc, &local_tel)) {
            /* Cập nhật thông số an toàn đa luồng bằng Mutex */
            k_mutex_lock(&g_telemetry_mutex, K_FOREVER);
            g_current_telemetry = local_tel;
            k_mutex_unlock(&g_telemetry_mutex);

            /* Gửi cập nhật sang hệ thống giám sát sự cố */
            safety_monitor_update(&local_tel);

            LOG_INF("CAN Packet OK: Speed = %u km/h | RPM = %u | Temp = %d C",
                    local_tel.speed_kmh, local_tel.engine_rpm, local_tel.coolant_temp);
        } else {
            LOG_WRN("CAN Frame ID 0x%03X bi loi E2E hoac khong dung dinh dang!", rx_frame.id);
        }
    }
}

K_THREAD_DEFINE(can_worker_tid, CAN_WORKER_STACK_SIZE,
                can_worker_thread_entry, NULL, NULL, NULL,
                CAN_WORKER_PRIO, 0, 0);

/* ==============================================================================
 * LUỒNG 2: SAFETY SUPERVISOR & LED ALERT (Mức ưu tiên 6)
 * ============================================================================== */
#define SAFETY_STACK_SIZE 1024
#define SAFETY_PRIO       6

void safety_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    if (warn_led.port != NULL && gpio_is_ready_dt(&warn_led)) {
        gpio_pin_configure_dt(&warn_led, GPIO_OUTPUT_INACTIVE);
    }

    LOG_INF("Safety Supervisor Thread da khoi dong (Priority = %d).", SAFETY_PRIO);

    while (1) {
        /* Kiểm tra định kỳ mỗi 200ms */
        k_msleep(200);

        if (safety_monitor_is_fault_active()) {
            /* Nếu có lỗi phát sinh (Timeout mất CAN, Quá nhiệt...) -> Nhấp nháy LED cảnh báo */
            if (warn_led.port != NULL) {
                gpio_pin_toggle_dt(&warn_led);
            }
        } else {
            /* Hệ thống bình thường: Tắt đèn LED */
            if (warn_led.port != NULL) {
                gpio_pin_set_dt(&warn_led, 0);
            }
        }
    }
}

K_THREAD_DEFINE(safety_tid, SAFETY_STACK_SIZE,
                safety_thread_entry, NULL, NULL, NULL,
                SAFETY_PRIO, 0, 0);

/* ==============================================================================
 * LUỒNG CHÍNH (MAIN THREAD)
 * ============================================================================== */
int main(void)
{
    LOG_INF("==================================================");
    LOG_INF("   AUTOMOTIVE SMART CAN GATEWAY & CLUSTER OK!     ");
    LOG_INF("   RTOS Kernel: Zephyr OS on STM32F746 Cortex-M7  ");
    LOG_INF("   Build Time: %s %s", __DATE__, __TIME__);
    LOG_INF("==================================================");

    /* 1. Khởi tạo hệ thống giám sát an toàn */
    safety_monitor_init();

    /* 2. Khởi tạo CAN Subsystem và cấu hình bộ lọc */
    int ret = can_gateway_init();
    if (ret != 0) {
        LOG_ERR("Loi khoi tao CAN Gateway! Ma loi: %d", ret);
        return ret;
    }

    LOG_INF("Go lenh 'help' tren Console UART de xem danh sach lenh chan doan CLI.");
    return 0;
}
