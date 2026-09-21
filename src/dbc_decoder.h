/**
 * ==============================================================================
 * File: zephyr_project/src/dbc_decoder.h
 * Mục đích: Khai báo giao diện giải mã tín hiệu Vector DBC và AUTOSAR E2E
 * ==============================================================================
 */

#ifndef DBC_DECODER_H
#define DBC_DECODER_H

#include <stdint.h>
#include <stdbool.h>

#define VEHICLE_DATA_ID 0x1A2B /* Data ID bí mật của bản tin xe hơi */

typedef struct {
    uint16_t speed_kmh;     /* Tốc độ xe (0 - 240 km/h) */
    uint16_t engine_rpm;    /* Vòng tua động cơ (0 - 8000 RPM) */
    int16_t  coolant_temp;  /* Nhiệt độ nước làm mát (-40 đến +150 degC) */
    uint8_t  rolling_cnt;   /* Bộ đếm vòng E2E */
    bool     is_e2e_valid;  /* Cờ xác thực toàn vẹn dữ liệu */
} VehicleTelemetry_t;

/**
 * @brief Giải mã gói tin CAN 8-byte, kiểm tra mã AUTOSAR E2E CRC-8 và bóc tách tín hiệu
 * @param data Con trỏ dữ liệu 8 bytes
 * @param dlc Độ dài bản tin
 * @param out Con trỏ lưu trữ thông số sau giải mã
 * @return true nếu giải mã và kiểm tra E2E thành công
 */
bool dbc_decode_vehicle_frame(const uint8_t *data, uint8_t dlc, VehicleTelemetry_t *out);

#endif /* DBC_DECODER_H */
