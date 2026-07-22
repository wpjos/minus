#include "uart.h"
#include "platform.h"
#include "module.h"
#include "types.h"
#include "mmu.h"
#include "printk.h"

/* PL011 UART register offsets */
#define UART_DR(base)	(*(volatile uint32_t *)((base) + 0x00))
#define UART_FR(base)	(*(volatile uint32_t *)((base) + 0x18))
#define UART_CR(base)	(*(volatile uint32_t *)((base) + 0x30))

/* Cached base address, set during probe */
static uint64_t g_uart_base = 0;

void uart_putc(char c)
{
	if (!g_uart_base)
		return;
	while (UART_FR(g_uart_base) & (1 << 5))
		;
	UART_DR(g_uart_base) = c;
}

void uart_puts(const char *str)
{
	while (*str)
		uart_putc(*str++);
}

static int uart_probe(struct platform_device *pdev)
{
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		printk("uart prob fail\n");
		return -1;
	}

	g_uart_base = (uint64_t)mmu_ioremap(res->start, resource_size(res));
	if (!g_uart_base)
		return -1;

	UART_CR(g_uart_base) = 0x301;	/* enable TX/RX */
	return 0;
}

static int uart_remove(struct platform_device *pdev)
{
	if (g_uart_base)
		UART_CR(g_uart_base) = 0;
	g_uart_base = 0;
	return 0;
}

static const struct of_device_id uart_of_match[] = {
	{ .compatible = "arm,pl011" },
	{ /* sentinel */ }
};

static struct platform_driver uart_driver = {
	.drv = { .name = "uart-pl011" },
	.probe = uart_probe,
	.remove = uart_remove,
	.of_match_table = uart_of_match,
};

static void uart_init(void)
{
	platform_driver_register(&uart_driver);
}
module_register(uart, MODULE_LEVEL_HIGH, uart_init);
