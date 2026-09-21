/**
 * ==============================================================================
 * File: zephyr_project/src/dbc_decoder.c
 * Mục đích: Cài đặt thuật toán kiểm tra AUTOSAR E2E Profile 1 và giải nén bit DBC
 * ==============================================================================
 */

#include "dbc_decoder.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dbc_decoder, LOG_LEVEL_INF);

/* Thuật toán AUTOSAR E2E CRC-8 (Đa thức SAE J1850 0x2F, Init 0xFF, XOR 0xFF) */
static uint8_t compute_e2e_crc8(const uint8_t *data, uint8_t len, uint16_t data_id)
{
    uint8_t crc = 0xFF;

    /* Nhồi Data ID bí mật vào trước */
    uint8_t id_bytes[2] = { (uint8_t)(data_id & 0xFF), (uint8_t)((data_id >> 8) & 0xFF) };
    for (int i = 0; i < 2; i++) {
        crc ^= id_bytes[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x2F;
            } else {
                crc <<= 1;
            }
        }
    }

    /* Tính tiếp qua payload từ Byte 1 đến Byte 7 */
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x2F;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc ^ 0xFF;
}

bool dbc_decode_vehicle_frame(const uint8_t *data, uint8_t dlc, VehicleTelemetry_t *out)
{
    if (dlc < 8 || out == NULL) return false;

    static uint8_t last_counter = 0xFF;

    /* 1. KIỂM TRA LỚP 1: E2E CRC-8 */
    uint8_t received_crc = data[0];
    uint8_t expected_crc = compute_e2e_crc8(&data[1], 7, VEHICLE_DATA_ID);

    if (received_crc != expected_crc) {
        LOG_WRN("E2E CANH BAO: Sai ma CRC-8! Nhan: 0x%02X, Tinh: 0x%02X", received_crc, expected_crc);
        out->is_e2e_valid = false;
        return false;
    }

    /* 2. KIỂM TRA LỚP 2: ROLLING COUNTER */
    uint8_t current_counter = data[1] & 0x0F;
    if (last_counter != 0xFF) {
        uint8_t next_counter = (last_counter + 1) % 16;
        if (current_counter != next_counter) {
            LOG_WRN("E2E CANH BAO: Sai Rolling Counter! Hien tai: %d, Mong doi: %d", 
                    current_counter, next_counter);
            out->is_e2e_valid = false;
            return false;
        }
    }
    last_counter = current_counter;
    out->rolling_cnt = current_counter;
    out->is_e2e_valid = true;

    /* 3. GIẢI MÃ TÍN HIỆU VECTOR DBC (TOÁN FIXED-POINT KHÔNG DÙNG FLOAT) */
    /* Signal 1: Tốc độ xe (km/h) tại Byte 2 */
    out->speed_kmh = (uint16_t)data[2];

    /* Signal 2: Vòng tua máy RPM (Byte 3 LSB, Byte 4 MSB, Factor = 0.25 -> chia 4) */
    uint16_t raw_rpm = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
    out->engine_rpm = (raw_rpm >> 2);

    /* Signal 3: Nhiệt độ nước làm mát tại Byte 5 (Factor = 1, Offset = -40) */
    out->coolant_temp = (int16_t)data[5] - 40;

    return true;
}
