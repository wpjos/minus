#include "uart.h"
#include "module.h"

#define UART_BASE 0x09000000
#define UART_DR   (*(volatile unsigned int *)(UART_BASE + 0x00))
#define UART_FR   (*(volatile unsigned int *)(UART_BASE + 0x18))
#define UART_CR   (*(volatile unsigned int *)(UART_BASE + 0x30))

// 初始化串口
static void uart_init(void) {
    UART_CR = 0x301;  // 启用 TX/RX，开启 UART
}

// 发送单个字符
void uart_putc(char c) {
    // 等待发送缓冲区为空
    while ((UART_FR & (1 << 5)) != 0);
    // 写入字符
    UART_DR = c;
}

// 发送字符串
void uart_puts(const char *str) {
    while (*str) {
        uart_putc(*str++);
    }
}

module_register(uart, uart_init);
