#ifndef __UART_H__
#define __UART_H__

void uart_puts(const char *str);
void uart_putc(char c);
char uart_getc(void);
int uart_rx_ready(void);

#endif
