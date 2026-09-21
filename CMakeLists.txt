# ==============================================================================
# File: zephyr_project/CMakeLists.txt
# Mục đích: File điều phối biên dịch chuẩn của ứng dụng Zephyr RTOS
# ==============================================================================

cmake_minimum_required(VERSION 3.20.0)

# Tìm kiếm và tích hợp hệ điều hành Zephyr RTOS vào dự án
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Đặt tên định danh cho dự án ô tô
project(automotive_can_gateway_cluster)

# Đăng ký các file mã nguồn C vào target biên dịch
target_sources(app PRIVATE
    src/main.c
    src/can_gateway.c
    src/dbc_decoder.c
    src/safety_monitor.c
    src/diag_shell.c
)
