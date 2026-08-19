#include "switch_notify.h"
#include "errno.h"

/*
 * The switch-notification bus is a core mechanism, on par with cap and
 * svc: core owns the broadcast, subscribers own their per-CPU mirrors.
 * The bus transports opaque envelopes prepared by proc; core never looks
 * inside them and never touches task_struct.  See switch_notify.h.
 */

#define SWITCH_NOTIFY_MAX 8

static switch_notify_fn switch_notifiers[SWITCH_NOTIFY_MAX];
static int switch_notifier_count;

int switch_notifier_register(switch_notify_fn fn)
{
	if (!fn || switch_notifier_count >= SWITCH_NOTIFY_MAX)
		return -ENOMEM;

	switch_notifiers[switch_notifier_count++] = fn;
	/*
	 * Seed the subscriber's mirror to its empty state; the first real
	 * switch publishes the first real envelope.  Early-boot context
	 * (idle task) carries an all-CAP_NULL envelope, which subscribers
	 * mirror as NULL.
	 */
	fn(NULL, NULL);
	return 0;
}

void switch_notify(const struct switch_envelope *prev,
		   const struct switch_envelope *next)
{
	int i;

	for (i = 0; i < switch_notifier_count; i++)
		switch_notifiers[i](prev, next);
}

void switch_finish(const struct switch_envelope *prev,
		   const struct switch_envelope *next)
{
	switch_notify(prev, next);
}
