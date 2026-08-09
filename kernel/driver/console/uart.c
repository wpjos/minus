#include "uart.h"
#include "platform.h"
#include "module.h"
#include "types.h"
#include "mmu.h"
#include "printk.h"
#include "irq.h"
#include "sched.h"
#include "task.h"
#include "irqflags.h"
#include "wait.h"

/* PL011 UART register offsets */
#define UART_DR(base)	(*(volatile uint32_t *)((base) + 0x00))
#define UART_FR(base)	(*(volatile uint32_t *)((base) + 0x18))
#define UART_CR(base)	(*(volatile uint32_t *)((base) + 0x30))
#define UART_IMSC(base)	(*(volatile uint32_t *)((base) + 0x38))
#define UART_RIS(base)	(*(volatile uint32_t *)((base) + 0x3c))
#define UART_MIS(base)	(*(volatile uint32_t *)((base) + 0x40))
#define UART_ICR(base)	(*(volatile uint32_t *)((base) + 0x44))

#define UART_CR_UARTEN	(1U << 0)
#define UART_CR_TXE	(1U << 8)
#define UART_CR_RXE	(1U << 9)

#define UART_FR_RXFE	(1 << 4)
#define UART_FR_TXFF	(1 << 5)

#define UART_IMSC_RXIM	(1U << 4)
#define UART_IMSC_RTIM	(1U << 6)

#define UART_ICR_RXIC	(1U << 4)
#define UART_ICR_RTIC	(1U << 6)

/* Cached base address, set during probe */
static uintptr_t g_uart_base = CONFIG_EARLY_UART;

/* Interrupt-driven RX ring buffer. */
#define UART_RX_BUF_SIZE	64

static uint8_t g_uart_rx_buf[UART_RX_BUF_SIZE];
static unsigned int g_uart_rx_head;
static unsigned int g_uart_rx_tail;

/* Wait queue for readers blocked waiting for RX input. */
static DECLARE_WAIT_QUEUE_HEAD(g_uart_rx_wq);

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

int uart_rx_ready(void)
{
	if (!g_uart_base)
		return 0;
	return !(UART_FR(g_uart_base) & UART_FR_RXFE);
}

static int uart_rx_buf_empty(void)
{
	return g_uart_rx_head == g_uart_rx_tail;
}

static int uart_rx_buf_full(void)
{
	return ((g_uart_rx_tail + 1) % UART_RX_BUF_SIZE) == g_uart_rx_head;
}

static void uart_rx_buf_push(uint8_t c)
{
	if (uart_rx_buf_full())
		return;
	g_uart_rx_buf[g_uart_rx_tail] = c;
	g_uart_rx_tail = (g_uart_rx_tail + 1) % UART_RX_BUF_SIZE;
}

static int uart_rx_buf_pop(void)
{
	uint8_t c;

	if (uart_rx_buf_empty())
		return -1;

	c = g_uart_rx_buf[g_uart_rx_head];
	g_uart_rx_head = (g_uart_rx_head + 1) % UART_RX_BUF_SIZE;
	return (int)c;
}

static int uart_irq_handler(unsigned int irq, void *dev_id)
{
	(void)irq;
	(void)dev_id;

	while (uart_rx_ready())
		uart_rx_buf_push((uint8_t)UART_DR(g_uart_base));

	UART_ICR(g_uart_base) = UART_ICR_RXIC | UART_ICR_RTIC;

	wake_up(g_uart_rx_wq);

	return 0;
}

char uart_getc(void)
{
	int c;

	if (!g_uart_base)
		return '\0';

	wait_event(g_uart_rx_wq, !uart_rx_buf_empty());

	c = uart_rx_buf_pop();
	/* wait_event guarantees the condition is true, so c >= 0. */
	return (char)c;
}

static int uart_probe(struct platform_device *pdev)
{
	struct resource *res;
	int irq;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		printk("uart prob fail\n");
		return -1;
	}

	g_uart_base = (uintptr_t)mmu_ioremap(res->start, resource_size(res));
	if (!g_uart_base)
		return -1;

	UART_CR(g_uart_base) = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		printk("uart: no IRQ resource\n");
		return -1;
	}

	if (request_irq((unsigned int)irq, uart_irq_handler, NULL) != 0) {
		printk("uart: failed to request IRQ %d\n", irq);
		return -1;
	}

	if (enable_irq((unsigned int)irq) != 0) {
		printk("uart: failed to enable IRQ %d\n", irq);
		return -1;
	}

	/* Enable receive and receive-timeout interrupts. */
	UART_IMSC(g_uart_base) = UART_IMSC_RXIM | UART_IMSC_RTIM;

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
