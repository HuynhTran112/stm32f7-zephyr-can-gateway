/**
 * ==============================================================================
 * File: zephyr_project/src/diag_shell.c
 * Mục đích: Giao diện dòng lệnh Zephyr Shell CLI phục vụ chẩn đoán kỹ thuật
 * ==============================================================================
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include "safety_monitor.h"
#include "can_gateway.h"

extern VehicleTelemetry_t g_current_telemetry;
extern struct k_mutex g_telemetry_mutex;

static int cmd_vehicle_status(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    k_mutex_lock(&g_telemetry_mutex, K_FOREVER);
    shell_print(sh, "========================================");
    shell_print(sh, "   THONG SO VAN HANH XE HOI (TELEMETRY) ");
    shell_print(sh, "========================================");
    shell_print(sh, "• Toc do hien tai : %u km/h", g_current_telemetry.speed_kmh);
    shell_print(sh, "• Vong tua may    : %u RPM", g_current_telemetry.engine_rpm);
    shell_print(sh, "• Nhiet do nuoc   : %d degC", g_current_telemetry.coolant_temp);
    shell_print(sh, "• Rolling Counter : %u", g_current_telemetry.rolling_cnt);
    shell_print(sh, "• E2E Integrity   : %s", g_current_telemetry.is_e2e_valid ? "VALID (OK)" : "CORRUPT (FAIL)");
    shell_print(sh, "========================================");
    k_mutex_unlock(&g_telemetry_mutex);

    return 0;
}

static int cmd_dtc_read(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    uint16_t dtcs[8];
    uint8_t count = safety_monitor_get_active_dtc(dtcs, 8);

    shell_print(sh, "Danh sach ma loi chan doan (DTC) hien huu (%u loi):", count);
    if (count == 0) {
        shell_print(sh, "➔ Khong co ma loi nao. He thong AN TOAN tuyet doi.");
    } else {
        for (uint8_t i = 0; i < count; i++) {
            shell_print(sh, " [%u] DTC: 0x%04X", i + 1, dtcs[i]);
        }
    }
    return 0;
}

static int cmd_dtc_clear(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    safety_monitor_clear_dtc();
    shell_print(sh, "➔ Da xoa toan bo ma loi chan doan thanh cong.");
    return 0;
}

/* --------------------------------------------------------------------------
 * BỘ MÔ PHỎNG PHÁT BẢN TIN CAN XE HƠI (CAN SIMULATOR & INJECTOR)
 * -------------------------------------------------------------------------- */
static bool s_auto_sim = true; /* Mặc định bật tự động để có số liệu xe chạy ngay khi nạp */

static void send_sim_frame(uint16_t speed, uint16_t rpm, int8_t temp, bool corrupt_crc)
{
    static uint8_t sim_counter = 0;
    sim_counter = (sim_counter + 1) % 16;

    uint16_t raw_rpm = rpm * 4;
    uint8_t temp_raw = (uint8_t)(temp + 40);

    struct can_frame sim_frame = {
        .id = 0x123,
        .dlc = 8,
        .data = {
            0, /* Byte 0: CRC */
            sim_counter & 0x0F,
            (uint8_t)speed,
            (uint8_t)(raw_rpm & 0xFF),
            (uint8_t)((raw_rpm >> 8) & 0xFF),
            temp_raw,
            0x00, 0x00
        }
    };

    /* Tính E2E CRC-8 cho payload Byte 1 đến Byte 7 */
    uint8_t crc = 0xFF;
    uint8_t id_bytes[2] = { (uint8_t)(VEHICLE_DATA_ID & 0xFF), (uint8_t)((VEHICLE_DATA_ID >> 8) & 0xFF) };
    for (int i = 0; i < 2; i++) {
        crc ^= id_bytes[i];
        for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? ((crc << 1) ^ 0x2F) : (crc << 1);
    }
    for (int i = 1; i < 8; i++) {
        crc ^= sim_frame.data[i];
        for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? ((crc << 1) ^ 0x2F) : (crc << 1);
    }
    sim_frame.data[0] = corrupt_crc ? (crc ^ 0xAA) : (crc ^ 0xFF);

    can_gateway_send_frame(&sim_frame);
}

/* Luồng phát xe chạy tự động 5Hz (Mỗi 200ms) */
static void sim_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
    uint16_t speed = 50;
    int dir = 1;

    while (1) {
        k_msleep(200);
        if (s_auto_sim) {
            speed += dir * 2;
            if (speed >= 115) dir = -1;
            else if (speed <= 45) dir = 1;

            uint16_t rpm = 1200 + speed * 25; /* RPM: 2325 - 4075 RPM */
            send_sim_frame(speed, rpm, 88, false);
        }
    }
}

K_THREAD_DEFINE(sim_tid, 1024, sim_thread_entry, NULL, NULL, NULL, 7, 0, 0);

/* Lệnh giả lập phát tín hiệu xe hơi thủ công */
static int cmd_can_sim(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "Cu phap: can sim <speed_kmh>");
        return -EINVAL;
    }

    uint16_t speed = (uint16_t)atoi(argv[1]);
    uint16_t rpm = 1200 + speed * 25;
    send_sim_frame(speed, rpm, 88, false);
    shell_print(sh, "➔ Da phat 1 goi tin: Toc do = %u km/h, RPM = %u", speed, rpm);
    return 0;
}

/* Bật / tắt luồng tự động phát */
static int cmd_can_auto(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "Trang thai hien tai: %s. Cu phap: can auto <on|off>", s_auto_sim ? "ON (DANG CHAY)" : "OFF (DA DUNG)");
        return 0;
    }

    if (strcmp(argv[1], "on") == 0) {
        s_auto_sim = true;
        shell_print(sh, "➔ Da BAT mo phong CAN tu dong! Xe dang van hanh tren duong.");
    } else if (strcmp(argv[1], "off") == 0) {
        s_auto_sim = false;
        shell_print(sh, "➔ Da TAT mo phong CAN. Mat tin hieu CAN sau 1s se gay loi DTC_U0100!");
    } else {
        shell_print(sh, "Tham so khong hop le! Dung 'can auto on' hoac 'can auto off'");
    }
    return 0;
}

/* Bơm lỗi cố tình để kiểm tra an toàn AUTOSAR */
static int cmd_can_inject(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "Cu phap: can inject <overheat | overspeed | corrupt>");
        return -EINVAL;
    }

    if (strcmp(argv[1], "overheat") == 0) {
        send_sim_frame(90, 3000, 115, false); /* 115 degC > 105 degC -> DTC_P0115 */
        shell_print(sh, "➔ Da bom loi QUA NHIET (115 degC)! Kiem tra bang 'dtc read'");
    } else if (strcmp(argv[1], "overspeed") == 0) {
        send_sim_frame(140, 6800, 88, false); /* 6800 RPM > 6500 RPM -> DTC_P0219 */
        shell_print(sh, "➔ Da bom loi QUA VONG TUA (6800 RPM)! Kiem tra bang 'dtc read'");
    } else if (strcmp(argv[1], "corrupt") == 0) {
        send_sim_frame(80, 2500, 88, true); /* Sai CRC */
        shell_print(sh, "➔ Da bom goi tin SAI MA CRC-8! Kiem tra bang 'vehicle status'");
    } else {
        shell_print(sh, "Loi khong ho tro: overheat, overspeed, corrupt");
    }
    return 0;
}

/* Đăng ký các nhóm lệnh vào Zephyr Shell */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vehicle,
    SHELL_CMD(status, NULL, "Hien thi thong so xe hoi truc quan", cmd_vehicle_status),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(vehicle, &sub_vehicle, "Lenh theo doi xe hoi", NULL);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_dtc,
    SHELL_CMD(read, NULL, "Doc danh sach ma loi chan doan", cmd_dtc_read),
    SHELL_CMD(clear, NULL, "Xoa toan bo ma loi chan doan", cmd_dtc_clear),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(dtc, &sub_dtc, "Lenh chan doan ma loi", NULL);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_can,
    SHELL_CMD(sim, NULL, "Gia lap phat 1 goi tin CAN (can sim <speed>)", cmd_can_sim),
    SHELL_CMD(auto, NULL, "Bat/tat mo phong xe chay tu dong (can auto <on|off>)", cmd_can_auto),
    SHELL_CMD(inject, NULL, "Bom loi an toan (can inject <overheat|overspeed|corrupt>)", cmd_can_inject),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(can, &sub_can, "Lenh mo phong mang CAN", NULL);
