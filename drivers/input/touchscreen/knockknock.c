/*
 * drivers/input/touchscreen/knock_knock.c
 *
 *
 * Copyright (c) 2012, Braydon Saporito <altabraydon@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *              Version 1.0
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/input/knockknock.h>

/* Editables */
#define DEFAULT_KNOCK_KNOCK_DELAY_MS          400
#define DEFAULT_HEIGHT_TO_KNOCK               200
#define MS_TO_NS(x)	               (x * 1E6L)
#define DEBUG                                   1

int knock_knock_enabled = 1;
int double_touch_delay = DEFAULT_KNOCK_KNOCK_DELAY_MS;
int knocks = 0;
bool new_knock = true;
bool scre_suspended = false;
static struct input_dev * knock_knock_pwrdev;
static DEFINE_MUTEX(pwrkeyworklock);
static struct hrtimer hr_timer;
ktime_t ktime;

enum hrtimer_restart knock_knock_hrtimer_callback( struct hrtimer *timer )
{
	printk( "double tap didn't occur.\n");
	knocks = 0;
	return HRTIMER_NORESTART;
}

/* PowerKey setter */
void knock_knock_setdev(struct input_dev * input_device) {
	knock_knock_pwrdev = input_device;
	return;
}
EXPORT_SYMBOL(knock_knock_setdev);

/* PowerKey work func */
static void knock_knock_presspwr(struct work_struct * knock_knock_presspwr_work) {
	if (!mutex_trylock(&pwrkeyworklock))
                return;
	input_event(knock_knock_pwrdev, EV_KEY, KEY_POWER, 1);
	input_event(knock_knock_pwrdev, EV_SYN, 0, 0);
	msleep(60);
	input_event(knock_knock_pwrdev, EV_KEY, KEY_POWER, 0);
	input_event(knock_knock_pwrdev, EV_SYN, 0, 0);
	msleep(60);
        mutex_unlock(&pwrkeyworklock);
	return;
}
static DECLARE_WORK(knock_knock_presspwr_work, knock_knock_presspwr);

/* PowerKey trigger */
void knock_knock_pwrtrigger(void) {
	schedule_work(&knock_knock_presspwr_work);
        return;
}

/* Main work */
void detect_knock(int touch_height) {
	if(knock_knock_enabled > 0 && knocks == 1 && new_knock) {
		if(scre_suspended) {
			printk(KERN_INFO "[knockknock]: screen off");
			knock_knock_pwrtrigger();
			
		} else {
			// printk(KERN_INFO "[knockknock]: screen on");
			// knock_knock_pwrtrigger();
			/* Disabled while I develop a java reporting method for the status bar */
		}
		knocks = 0;
		new_knock = false;
	}
	if((knock_knock_enabled > 0) && (knocks == 0) && new_knock) {
		knocks++;
		new_knock = false;
		hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
		hr_timer.function = &knock_knock_hrtimer_callback;
		printk( "Starting timer to fire in %idms (%ld)\n", DEFAULT_KNOCK_KNOCK_DELAY_MS, jiffies );
		hrtimer_start( &hr_timer, ktime, HRTIMER_MODE_REL );
	}
}

/* sysfs interface */

static ssize_t knock_knock_enabled_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%i\n", knock_knock_enabled);
}

static ssize_t knock_knock_enabled_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int data;
	if(sscanf(buf, "%i\n", &data) == 1)
		knock_knock_enabled = data;
	else
		pr_info("%s: unknown input!\n", __FUNCTION__);
	return count;
}

static ssize_t double_touch_delay_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%i\n", double_touch_delay);
}

static ssize_t double_touch_delay_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int data;
	if(sscanf(buf, "%i\n", &data) == 1)
		double_touch_delay = data;
	else
		pr_info("%s: unknown input!\n", __FUNCTION__);
	return count;
}

static struct kobj_attribute knock_knock_enabled_attribute =
	__ATTR(knock_knock_enabled,
		0666,
		knock_knock_enabled_show,
		knock_knock_enabled_store);

static struct kobj_attribute knock_knock_double_touch_delay_attribute =
	__ATTR(double_touch_delay,
		0666,
		double_touch_delay_show,
		double_touch_delay_store);

static struct attribute *knock_knock_attrs[] =
	{
		&knock_knock_enabled_attribute.attr,
		&knock_knock_double_touch_delay_attribute.attr,
		NULL,
	};

static struct attribute_group knock_knock_attr_group =
	{
		.attrs = knock_knock_attrs,
	};

static struct kobject *knock_knock_kobj;

static int __init knock_knock_init(void)
{
	int sysfs_result;

	knock_knock_kobj = kobject_create_and_add("knock_knock", kernel_kobj);
	if (!knock_knock_kobj) {
		pr_err("%s kobject create failed!\n", __FUNCTION__);
		return -ENOMEM;
        }

	sysfs_result = sysfs_create_group(knock_knock_kobj, &knock_knock_attr_group);

        if (sysfs_result) {
		pr_info("%s sysfs create failed!\n", __FUNCTION__);
		kobject_put(knock_knock_kobj);
	}

	ktime = ktime_set( 0, MS_TO_NS(DEFAULT_KNOCK_KNOCK_DELAY_MS) );

	pr_info("[knockknock]: %s done\n", __func__);
	return sysfs_result;
}

static void __exit knock_knock_exit(void)
{
	return;
}

module_init(knock_knock_init);
module_exit(knock_knock_exit);

MODULE_DESCRIPTION("Knock Knock");
MODULE_LICENSE("GPLv2");
