/**
 * ==============================================================================
 * File: zephyr_project/src/safety_monitor.h
 * Mục đích: Quản lý giám sát an toàn, Timeout và bảng mã lỗi chẩn đoán DTC
 * ==============================================================================
 */

#ifndef SAFETY_MONITOR_H
#define SAFETY_MONITOR_H

#include "dbc_decoder.h"
#include <stdint.h>
#include <stdbool.h>

/* Bảng mã lỗi chẩn đoán chuẩn ô tô (Standard DTCs) */
#define DTC_U0100 0x0100 /* Mất kết nối CAN quá 1000ms (Lost Communication) */
#define DTC_P0115 0x0115 /* Quá nhiệt động cơ > 105 degC (Engine Coolant Overheat) */
#define DTC_P0219 0x0219 /* Quá vòng tua máy > 6500 RPM (Engine Overspeed) */

void safety_monitor_init(void);

/**
 * @brief Cập nhật thông số xe hơi và rà soát ngưỡng an toàn
 * @param tel Con trỏ dữ liệu xe sau giải mã
 */
void safety_monitor_update(const VehicleTelemetry_t *tel);

/**
 * @brief Đọc danh sách các mã lỗi DTC đang kích hoạt
 * @param dtc_list Mảng chứa mã lỗi trả về
 * @param max_count Số lượng tối đa
 * @return Số lượng mã lỗi hiện hữu
 */
uint8_t safety_monitor_get_active_dtc(uint16_t *dtc_list, uint8_t max_count);

/**
 * @brief Xóa toàn bộ mã lỗi chẩn đoán (DTC Clear)
 */
void safety_monitor_clear_dtc(void);

/**
 * @brief Kiểm tra xem hệ thống có đang ở trạng thái báo động không
 * @return true nếu có ít nhất 1 mã lỗi chưa xóa
 */
bool safety_monitor_is_fault_active(void);

#endif /* SAFETY_MONITOR_H */
