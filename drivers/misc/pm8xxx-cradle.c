/*
 * Copyright LG Electronics (c) 2011
 * All rights reserved.
 * Author: Fred Cho <fred.cho@lge.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/cradle.h>
#include <linux/gpio.h>
#include <linux/switch.h>
#include <linux/delay.h>
#include <linux/wakelock.h>

static int pre_set_flag;
struct pm8xxx_cradle {
    struct switch_dev sdev;
    struct work_struct work;
    struct device *dev;
	struct wake_lock wake_lock;
    const struct pm8xxx_cradle_platform_data *pdata;
    int carkit;
    int pouch;
    spinlock_t lock;
    int state;
};

static struct workqueue_struct *cradle_wq;
static struct pm8xxx_cradle *cradle;

static struct input_dev  *cradle_input;

static void boot_cradle_det_func(void)
{
    int state;

    if (cradle->pdata->pouch_detect_pin)
    cradle->pouch = !gpio_get_value(cradle->pdata->pouch_detect_pin);

    printk("%s : boot pouch === > %d \n", __func__ , cradle->pouch);

      if (cradle->pouch == 1)
        state = CRADLE_SMARTCOVER;
      else
        state = CRADLE_SMARTCOVER_NO_DEV;

    printk("%s : [Cradle] boot cradle value is %d\n", __func__ , state);
    cradle->state = state;
    switch_set_state(&cradle->sdev, cradle->state);
    input_report_switch(cradle_input, SW_LID, 
                cradle->state == CRADLE_SMARTCOVER_NO_DEV ? 0 : 1);
    input_sync(cradle_input);
}

static void pm8xxx_cradle_work_func(struct work_struct *work)
{
    int state;
    unsigned long flags;

    if (cradle->pdata->pouch_detect_pin)
        cradle->pouch = !gpio_get_value_cansleep(cradle->pdata->pouch_detect_pin);

    printk("%s : pouch === > %d \n", __func__ , cradle->pouch);

    spin_lock_irqsave(&cradle->lock, flags);

      if (cradle->pouch == 1)
        state = CRADLE_SMARTCOVER;
      else
        state = CRADLE_SMARTCOVER_NO_DEV;

    printk("%s : [Cradle] cradle value is %d\n", __func__ , state);
    cradle->state = state;
    spin_unlock_irqrestore(&cradle->lock, flags);
//  msleep(500);//test
	wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(3000));

    switch_set_state(&cradle->sdev, cradle->state);
    input_report_switch(cradle_input, SW_LID, 
                cradle->state == CRADLE_SMARTCOVER_NO_DEV ? 0 : 1);
    input_sync(cradle_input);

}

void cradle_set_deskdock(int state)
{
    unsigned long flags;

    if (&cradle->sdev) {
        spin_lock_irqsave(&cradle->lock, flags);
        cradle->state = state;
        spin_unlock_irqrestore(&cradle->lock, flags);
        switch_set_state(&cradle->sdev, cradle->state);
    } else {
        pre_set_flag = state;
    }
}

int cradle_get_deskdock(void)
{
    if (!cradle)
        return pre_set_flag;

    return cradle->state;
}

#if 0
static irqreturn_t pm8xxx_carkit_irq_handler(int irq, void *handle)
{
    struct pm8xxx_cradle *cradle_handle = handle;
    printk("carkit irq!!!!!\n");
    queue_work(cradle_wq, &cradle_handle->work);
    return IRQ_HANDLED;
}
#endif

static irqreturn_t pm8xxx_pouch_irq_handler(int irq, void *handle)
{
    struct pm8xxx_cradle *cradle_handle = handle;
    printk("pouch irq!!!!\n");
    queue_work(cradle_wq, &cradle_handle->work);
    return IRQ_HANDLED;
}


static ssize_t
cradle_pouch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len;
    struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
    len = snprintf(buf, PAGE_SIZE, "pouch : %d\n", cradle->pouch);

    return len;

}

static ssize_t
cradle_sensing_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len;
    struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
    len = snprintf(buf, PAGE_SIZE, "sensing : %d\n", cradle->state);
    return len;

}

static struct device_attribute cradle_device_attrs[] = {
    __ATTR(pouch, S_IRUGO | S_IWUSR, cradle_pouch_show, NULL),
    __ATTR(sensing,  S_IRUGO | S_IWUSR, cradle_sensing_show, NULL),
};

static ssize_t cradle_print_name(struct switch_dev *sdev, char *buf)
{
    switch (switch_get_state(sdev)) {
    case 0:
        return sprintf(buf, "UNDOCKED\n");
    case 2:
        return sprintf(buf, "CARKIT\n");
    }
    return -EINVAL;
}

static int __devinit pm8xxx_cradle_probe(struct platform_device *pdev)
#if 1
{
	int ret, i;

	  unsigned int hall_gpio_irq;

	const struct pm8xxx_cradle_platform_data *pdata =
	                    pdev->dev.platform_data;
	if (!pdata)
	    return -EINVAL;

	cradle = kzalloc(sizeof(*cradle), GFP_KERNEL);
	if (!cradle)
	    return -ENOMEM;

	cradle->pdata    = pdata;

	//cradle->sdev.name = "dock";
	cradle->sdev.name = "smartcover";
	cradle->sdev.print_name = cradle_print_name;
	cradle->pouch = cradle->carkit = 0;


	spin_lock_init(&cradle->lock);

	ret = switch_dev_register(&cradle->sdev);

	if (ret < 0)
	    goto err_switch_dev_register;

	if (pre_set_flag) {
	    cradle_set_deskdock(pre_set_flag);
	    cradle->state = pre_set_flag;
	}
	wake_lock_init(&cradle->wake_lock, WAKE_LOCK_SUSPEND, "hall_ic_wakeups");

	INIT_WORK(&cradle->work, pm8xxx_cradle_work_func);

	printk("%s : init cradle\n", __func__);


	/* initialize irq of gpio_hall */
	hall_gpio_irq = gpio_to_irq(cradle->pdata->pouch_detect_pin);
	printk("%s : hall_gpio_irq = [%d]\n", __func__,hall_gpio_irq);

	if (hall_gpio_irq < 0) {
	    printk("Failed : GPIO TO IRQ \n");
	    ret = hall_gpio_irq;
	    goto err_request_irq;
	}


	printk("%s : pdata->irq_flags = [%d]\n", __func__,(int)pdata->irq_flags);

	ret = request_irq(hall_gpio_irq, pm8xxx_pouch_irq_handler, pdata->irq_flags, HALL_IC_DEV_NAME, cradle);
	if (ret > 0) {
		printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n", __func__, hall_gpio_irq, ret);
		goto err_request_irq;
	}

	printk("%s : enable_irq_wake \n", __func__);


	if (enable_irq_wake(hall_gpio_irq) == 0)
		printk("%s :enable_irq_wake Enable\n",__func__);
	else
		printk("%s :enable_irq_wake failed\n",__func__);

	printk("%s :boot_cradle_det_func START\n",__func__);
	boot_cradle_det_func();

	for (i = 0; i < ARRAY_SIZE(cradle_device_attrs); i++) {
	ret = device_create_file(&pdev->dev, &cradle_device_attrs[i]);
	if (ret){
	    goto err_request_irq;
	}
	}

	platform_set_drvdata(pdev, cradle);
	return 0;



err_request_irq:
    if (hall_gpio_irq)
        free_irq(hall_gpio_irq, 0);

err_switch_dev_register:
    switch_dev_unregister(&cradle->sdev);
    kfree(cradle);
    return ret;
}


#else

{
    int ret, i;
    const struct pm8xxx_cradle_platform_data *pdata =
                        pdev->dev.platform_data;
    if (!pdata)
        return -EINVAL;

    cradle = kzalloc(sizeof(*cradle), GFP_KERNEL);
    if (!cradle)
        return -ENOMEM;

    cradle->pdata    = pdata;

    cradle->sdev.name = "dock";
    cradle->sdev.print_name = cradle_print_name;
    cradle->pouch = cradle->carkit = 0;

    spin_lock_init(&cradle->lock);

    ret = switch_dev_register(&cradle->sdev);
    if (ret < 0)
        goto err_switch_dev_register;

    if (pre_set_flag) {
        cradle_set_deskdock(pre_set_flag);
        cradle->state = pre_set_flag;
    }
    INIT_WORK(&cradle->work, pm8xxx_cradle_work_func);

    printk("%s : init cradle\n", __func__);

    if (pdata->carkit_irq) {
        ret = request_irq(pdata->carkit_irq, pm8xxx_carkit_irq_handler, pdata->irq_flags, PM8XXX_CRADLE_DEV_NAME, cradle);
        if (ret > 0) {
            printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n", __func__, pdata->carkit_irq, ret);
            goto err_request_irq;
        }
    }

    if (pdata->pouch_irq) {
        ret = request_irq(pdata->pouch_irq, pm8xxx_pouch_irq_handler, pdata->irq_flags, PM8XXX_CRADLE_DEV_NAME, cradle);
        if (ret > 0) {
            printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n", __func__, pdata->pouch_irq, ret);
            goto err_request_irq;
        }
    }

    for (i = 0; i < ARRAY_SIZE(cradle_device_attrs); i++) {
        ret = device_create_file(&pdev->dev, &cradle_device_attrs[i]);
        if (ret)
            goto err_request_irq;
    }

    platform_set_drvdata(pdev, cradle);
    return 0;

err_request_irq:
    if (pdata->carkit_irq)
        free_irq(pdata->carkit_irq, 0);
    if (pdata->pouch_irq)
        free_irq(pdata->pouch_irq, 0);

err_switch_dev_register:
    switch_dev_unregister(&cradle->sdev);
    kfree(cradle);
    return ret;
}
#endif

static int __devexit pm8xxx_cradle_remove(struct platform_device *pdev)
{
    struct pm8xxx_cradle *cradle = platform_get_drvdata(pdev);

    cancel_work_sync(&cradle->work);
    switch_dev_unregister(&cradle->sdev);
    platform_set_drvdata(pdev, NULL);
    kfree(cradle);

    return 0;
}

static int pm8xxx_cradle_suspend(struct device *dev)
{
    return 0;
}

static int pm8xxx_cradle_resume(struct device *dev)
{
    return 0;
}

static const struct dev_pm_ops pm8xxx_cradle_pm_ops = {
    .suspend = pm8xxx_cradle_suspend,
    .resume = pm8xxx_cradle_resume,
};

static struct platform_driver pm8xxx_cradle_driver = {
    .probe        = pm8xxx_cradle_probe,
    .remove        = __devexit_p(pm8xxx_cradle_remove),
    .driver        = {
        .name    = HALL_IC_DEV_NAME,
        .owner    = THIS_MODULE,
#ifdef CONFIG_PM
        .pm    = &pm8xxx_cradle_pm_ops,
#endif
    },
};

static int cradle_input_device_create(void){
	int err = 0;

	cradle_input = input_allocate_device();
	if (!cradle_input) {
		err = -ENOMEM;
		goto exit;
	}

	cradle_input->name = "smartcover";
	cradle_input->phys = "/dev/input/smartcover";

	set_bit(EV_SW, cradle_input->evbit);
	set_bit(SW_LID, cradle_input->swbit);

	err = input_register_device(cradle_input);
	if (err) {
		goto exit_free;
	}
	return 0;

exit_free:
	input_free_device(cradle_input);
	cradle_input = NULL;
exit:
	return err;

}

static int __init pm8xxx_cradle_init(void)
{
    cradle_input_device_create();
    cradle_wq = create_singlethread_workqueue("cradle_wq");
       printk(KERN_ERR "cradle init \n");
    if (!cradle_wq)
        return -ENOMEM;
    return platform_driver_register(&pm8xxx_cradle_driver);
}
module_init(pm8xxx_cradle_init);

static void __exit pm8xxx_cradle_exit(void)
{
    if (cradle_wq)
        destroy_workqueue(cradle_wq);
    input_unregister_device(cradle_input);
    platform_driver_unregister(&pm8xxx_cradle_driver);
}
module_exit(pm8xxx_cradle_exit);


#if 1
MODULE_ALIAS("platform:" HALL_IC_DEV_NAME);
MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("hall ic driver");
MODULE_LICENSE("GPL");

#else
MODULE_ALIAS("platform:" PM8XXX_CRADLE_DEV_NAME);
MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("pm8xxx cradle driver");
MODULE_LICENSE("GPL");
#endif
