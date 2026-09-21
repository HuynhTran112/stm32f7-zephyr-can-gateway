/**
 * ==============================================================================
 * File: zephyr_project/src/safety_monitor.c
 * Mục đích: Hiện thực hóa máy trạng thái giám sát an toàn và quản lý DTC
 * ==============================================================================
 */

#include "safety_monitor.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(safety_monitor, LOG_LEVEL_INF);

static K_MUTEX_DEFINE(s_dtc_mutex);
static uint16_t s_active_dtcs[8];
static uint8_t  s_dtc_count = 0;
static uint32_t s_last_msg_time = 0;

static void add_dtc_internal(uint16_t dtc)
{
    for (uint8_t i = 0; i < s_dtc_count; i++) {
        if (s_active_dtcs[i] == dtc) return; /* Đã tồn tại */
    }
    if (s_dtc_count < 8) {
        s_active_dtcs[s_dtc_count++] = dtc;
        LOG_ERR("DTC ALARM: Phat sinh ma loi moi: 0x%04X!", dtc);
    }
}

void safety_monitor_init(void)
{
    s_dtc_count = 0;
    s_last_msg_time = k_uptime_get_32();
}

void safety_monitor_update(const VehicleTelemetry_t *tel)
{
    k_mutex_lock(&s_dtc_mutex, K_FOREVER);

    s_last_msg_time = k_uptime_get_32();

    /* Kiểm tra quá nhiệt */
    if (tel->coolant_temp > 105) {
        add_dtc_internal(DTC_P0115);
    }

    /* Kiểm tra quá vòng tua */
    if (tel->engine_rpm > 6500) {
        add_dtc_internal(DTC_P0219);
    }

    k_mutex_unlock(&s_dtc_mutex);
}

uint8_t safety_monitor_get_active_dtc(uint16_t *dtc_list, uint8_t max_count)
{
    k_mutex_lock(&s_dtc_mutex, K_FOREVER);

    /* Rà soát lỗi mất kết nối CAN quá 1000ms */
    if ((uint32_t)(k_uptime_get_32() - s_last_msg_time) > 1000) {
        add_dtc_internal(DTC_U0100);
    }

    uint8_t count = (s_dtc_count < max_count) ? s_dtc_count : max_count;
    for (uint8_t i = 0; i < count; i++) {
        dtc_list[i] = s_active_dtcs[i];
    }

    k_mutex_unlock(&s_dtc_mutex);
    return count;
}

void safety_monitor_clear_dtc(void)
{
    k_mutex_lock(&s_dtc_mutex, K_FOREVER);
    s_dtc_count = 0;
    s_last_msg_time = k_uptime_get_32();
    LOG_INF("DTC Reset: Toan bo ma loi chan doan da duoc xoa sach.");
    k_mutex_unlock(&s_dtc_mutex);
}

bool safety_monitor_is_fault_active(void)
{
    k_mutex_lock(&s_dtc_mutex, K_FOREVER);
    bool active = (s_dtc_count > 0);
    k_mutex_unlock(&s_dtc_mutex);
    return active;
}
