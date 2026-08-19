#include "string.h"
#include "uart.h"

int printk(const char *fmt, ...)
{
	int ret;
	va_list args;
	char buf[MAX_BUF_LEN] = {0};

	va_start(args, fmt);
	ret = vsprintf(buf, fmt, args);
	va_end(args);
	uart_puts(buf);

	return ret;
}
