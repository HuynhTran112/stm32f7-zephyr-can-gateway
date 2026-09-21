/**
 * ==============================================================================
 * File: zephyr_project/src/can_gateway.c
 * Mục đích: Hiện thực hóa giao tiếp CAN Subsystem, STB Pin và Bus-Off FSM
 * ==============================================================================
 */

#include "can_gateway.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(can_gateway, LOG_LEVEL_INF);

/* 1. Định nghĩa hàng đợi chứa tối đa 16 gói tin CAN */
K_MSGQ_DEFINE(raw_can_msgq, sizeof(struct can_frame), CAN_RX_QUEUE_SIZE, 4);

/* 2. Lấy thiết bị CAN từ Devicetree */
static const struct device *const can_dev = DEVICE_DT_GET(DT_ALIAS(can_primary));

/* 3. Lấy chân STB điều khiển IC Transceiver từ Devicetree */
static const struct gpio_dt_spec stb_spec = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(can_stb), gpios, {0});

/* Callback giám sát máy trạng thái lỗi phần cứng Bus-Off */
static void can_state_change_handler(const struct device *dev, enum can_state state,
                                     struct can_bus_err_cnt err_cnt, void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    if (state == CAN_STATE_BUS_OFF) {
        LOG_ERR("CAN ALARM: Phat hien trang thai BUS-OFF! TEC=%d, REC=%d", 
                err_cnt.tx_err_cnt, err_cnt.rx_err_cnt);
        /* Kích hoạt tự động phục hồi an toàn */
#if defined(CONFIG_CAN_MANUAL_RECOVERY_MODE)
        can_recover(can_dev, K_MSEC(100));
#else
        /* Trên STM32 bxCAN, phần cứng tự quản lý Bus-Off (ABOM) hoặc phục hồi bằng stop/start */
        can_stop(can_dev);
        k_msleep(100);
        can_start(can_dev);
#endif
    }
}

int can_gateway_init(void)
{
    int ret;

    /* A. Đánh thức Transceiver: Kéo chân STB xuống mức LOW (0V) */
    if (stb_spec.port != NULL && gpio_is_ready_dt(&stb_spec)) {
        gpio_pin_configure_dt(&stb_spec, GPIO_OUTPUT_INACTIVE);
        gpio_pin_set_dt(&stb_spec, 0);
        LOG_INF("CAN Transceiver da thuc tinh (STB = LOW).");
    }

    /* B. Kiểm tra tính sẵn sàng của bộ điều khiển CAN */
    if (!device_is_ready(can_dev)) {
        LOG_ERR("Loi: Ngoai vi CAN1 chua san sang!");
        return -ENODEV;
    }

    /* C. Đăng ký callback giám sát lỗi Bus-Off */
    can_set_state_change_callback(can_dev, can_state_change_handler, NULL);

    /* D. Bật chế độ Loopback để tự truyền/nhận độc lập trên 1 board mà không cần CAN Transceiver ngoài */
    can_set_mode(can_dev, CAN_MODE_LOOPBACK);

    /* E. Bắt đầu kích hoạt bộ điều khiển CAN */
    ret = can_start(can_dev);
    if (ret != 0) {
        LOG_ERR("Khong the khoi dong CAN controller! Ma loi: %d", ret);
        return ret;
    }

    /* E. Cấu hình bộ lọc phần cứng nhận ID 0x123 (Standard ID) */
    const struct can_filter rx_filter = {
        .id = 0x123,
        .mask = 0x7FF, /* So khớp chính xác ID 0x123 */
        .flags = 0
    };

    /* Gắn trực tiếp bộ lọc phần cứng vào hàng đợi k_msgq (Zero-Lock) */
    ret = can_add_rx_filter_msgq(can_dev, &raw_can_msgq, &rx_filter);
    if (ret < 0) {
        LOG_ERR("Loi dang ky can_add_rx_filter_msgq: %d", ret);
        return ret;
    }

    LOG_INF("CAN Gateway da san sang! Filter ID=0x123 da nap thanh cong.");
    return 0;
}

int can_gateway_send_frame(const struct can_frame *frame)
{
    if (!device_is_ready(can_dev)) return -ENODEV;
    return can_send(can_dev, frame, K_MSEC(100), NULL, NULL);
}
