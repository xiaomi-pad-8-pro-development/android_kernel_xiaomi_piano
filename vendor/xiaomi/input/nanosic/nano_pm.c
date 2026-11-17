// SPDX-License-Identifier: GPL-2.0
/*
 * Power management of the 803x
 *
 * The 803x sleeps while its sleep pin is low. Before every transfer the
 * host raises the pin and waits for the controller to raise its interrupt
 * line, and once the bus has been idle for a while it drops the pin again.
 * A controller that never wakes up has no firmware running, so it is reset
 * and gets its firmware uploaded again.
 *
 * Once a cover is attached, and after resume with a cover attached, a check
 * asks the 803x for the hall status and expects an answer and the keyboard
 * input devices within HEALTH_CHECK_MS. If either is missing the 803x is
 * reset the same way, at most HEALTH_RETRIES times with a growing delay.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include "nano_macro.h"

/* how long the 803x stays awake after the last transfer */
#define PM_IDLE_SCREEN_ON_MS 20000
#define PM_IDLE_SCREEN_OFF_MS 1000

/* how long the 803x gets to raise its interrupt line once woken up */
#define PM_WAKEUP_TIMEOUT_MS 100

#define HEALTH_CHECK_MS 2000
#define HEALTH_RETRIES 3
#define HEALTH_BACKOFF_MS 1000

static void Nanosic_PM_expire(struct timer_list *t)
{
	struct nano_i2c_client *i2c_client = from_timer(i2c_client, t, pm_timer);

	dbgprint(DEBUG_LEVEL, "going to sleep\n");
	Nanosic_GPIO_sleep(false);
	pm_relax(i2c_client->dev);
}

static void Nanosic_PM_hold(struct nano_i2c_client *i2c_client)
{
	unsigned int idle_ms = g_panel_status ? PM_IDLE_SCREEN_ON_MS :
						PM_IDLE_SCREEN_OFF_MS;

	pm_stay_awake(i2c_client->dev);
	mod_timer(&i2c_client->pm_timer, jiffies + msecs_to_jiffies(idle_ms));
}

/* pm_mutex held: reset the 803x and upload its firmware again */
static void Nanosic_PM_reload(struct nano_i2c_client *i2c_client)
{
	Nanosic_GPIO_reset();
	Nanosic_firmware_load(i2c_client);
	/* let the firmware start */
	msleep(500);
	Nanosic_PM_hold(i2c_client);
}

void Nanosic_PM_try_wakeup(struct nano_i2c_client *i2c_client)
{
	int i;

	mutex_lock(&i2c_client->pm_mutex);

	Nanosic_PM_hold(i2c_client);
	Nanosic_GPIO_sleep(true);

	if (Nanosic_GPIO_irqpin_get() <= 0) {
		dbgprint(DEBUG_LEVEL, "waiting for wakeup\n");
		for (i = 0; i < PM_WAKEUP_TIMEOUT_MS; i++) {
			usleep_range(1000, 1100);
			if (Nanosic_GPIO_irqpin_get() > 0)
				break;
		}

		if (i == PM_WAKEUP_TIMEOUT_MS) {
			dbgprint(ERROR_LEVEL, "803x did not wake up, reloading it\n");
			Nanosic_PM_reload(i2c_client);
		}

		/* give the i2c controller of the 803x time to come up */
		usleep_range(5000, 5500);
	}

	mutex_unlock(&i2c_client->pm_mutex);
}

static void Nanosic_PM_health_work(struct work_struct *work)
{
	struct nano_i2c_client *i2c_client =
		container_of(to_delayed_work(work), struct nano_i2c_client,
			     health_work);
	bool answered, ok;

	if (READ_ONCE(i2c_client->health_off))
		goto done;

	if (!i2c_client->health_sent) {
		i2c_client->health_baseline =
			atomic_read(&i2c_client->health_replies);
		i2c_client->health_sent = true;
		Nanosic_i2c_request_hall_status(i2c_client);
		queue_delayed_work(system_freezable_wq, &i2c_client->health_work,
				   msecs_to_jiffies(HEALTH_CHECK_MS));
		return;
	}
	i2c_client->health_sent = false;

	answered = atomic_read(&i2c_client->health_replies) !=
		   i2c_client->health_baseline;
	/* the input devices are only expected with a cover attached */
	ok = answered && (!(gHallStatus & 0x1) || Nanosic_input_attached());
	if (ok) {
		if (i2c_client->health_attempt)
			dbgprint(ALERT_LEVEL, "803x answers again after %d resets\n",
				 i2c_client->health_attempt);
		goto done;
	}

	if (i2c_client->health_attempt >= HEALTH_RETRIES) {
		dbgprint(ERROR_LEVEL,
			 "keyboard cover not working after %d controller resets: %s\n",
			 HEALTH_RETRIES,
			 answered ? "no keyboard input device" : "803x does not answer");
		goto done;
	}

	i2c_client->health_attempt++;
	dbgprint(INFO_LEVEL, "%s, resetting the 803x (%d of %d)\n",
		 answered ? "no keyboard input device" : "803x does not answer",
		 i2c_client->health_attempt, HEALTH_RETRIES);
	mutex_lock(&i2c_client->pm_mutex);
	Nanosic_PM_reload(i2c_client);
	mutex_unlock(&i2c_client->pm_mutex);
	queue_delayed_work(system_freezable_wq, &i2c_client->health_work,
			   msecs_to_jiffies(HEALTH_BACKOFF_MS
					    << (i2c_client->health_attempt - 1)));
	return;

done:
	i2c_client->health_attempt = 0;
	atomic_set(&i2c_client->health_busy, 0);
}

/* starts a check unless one is already running */
void Nanosic_PM_health_start(struct nano_i2c_client *i2c_client)
{
	if (IS_ERR_OR_NULL(i2c_client) || READ_ONCE(i2c_client->health_off))
		return;
	if (atomic_cmpxchg(&i2c_client->health_busy, 0, 1))
		return;
	queue_delayed_work(system_freezable_wq, &i2c_client->health_work, 0);
}

/* the 803x answered a hall status request */
void Nanosic_PM_health_reply(struct nano_i2c_client *i2c_client)
{
	if (!IS_ERR_OR_NULL(i2c_client))
		atomic_inc(&i2c_client->health_replies);
}

void Nanosic_PM_health_stop(struct nano_i2c_client *i2c_client)
{
	if (IS_ERR_OR_NULL(i2c_client))
		return;
	WRITE_ONCE(i2c_client->health_off, true);
	cancel_delayed_work_sync(&i2c_client->health_work);
}

void Nanosic_PM_init(struct nano_i2c_client *i2c_client)
{
	mutex_init(&i2c_client->pm_mutex);
	timer_setup(&i2c_client->pm_timer, Nanosic_PM_expire, 0);
	INIT_DELAYED_WORK(&i2c_client->health_work, Nanosic_PM_health_work);
}

void Nanosic_PM_free(struct nano_i2c_client *i2c_client)
{
	Nanosic_PM_health_stop(i2c_client);
	timer_delete_sync(&i2c_client->pm_timer);
	pm_relax(i2c_client->dev);
}
