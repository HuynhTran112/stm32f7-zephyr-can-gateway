/**
 * ==============================================================================
 * File: zephyr_project/src/can_gateway.h
 * Mục đích: Giao diện tiếp nhận mạng CAN và hàng đợi k_msgq trong Zephyr
 * ==============================================================================
 */

#ifndef CAN_GATEWAY_H
#define CAN_GATEWAY_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/can.h>

#define CAN_RX_QUEUE_SIZE 16

/* Hàng đợi toàn cục trung chuyển gói tin CAN từ driver ngầm sang Worker Thread */
extern struct k_msgq raw_can_msgq;

/**
 * @brief Khởi tạo ngoại vi CAN1, thức tỉnh Transceiver và gắn filter vào k_msgq
 * @return 0 nếu thành công, mã lỗi âm nếu thất bại
 */
int can_gateway_init(void);

/**
 * @brief Bắn một bản tin CAN ra mạng vật lý
 * @param frame Con trỏ gói tin can_frame
 * @return 0 nếu phát thành công
 */
int can_gateway_send_frame(const struct can_frame *frame);

#endif /* CAN_GATEWAY_H */
