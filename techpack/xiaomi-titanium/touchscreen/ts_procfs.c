#include <linux/device.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE)
#include <linux/input/doubletap2wake.h>
#endif
#include <xiaomi-titanium/touchscreen.h>

struct xiaomi_msm8953_touchscreen_operations_t *xiaomi_msm8953_touchscreen_operations;

static bool use_software_dt2w = true;
static int xiaomi_msm8953_touchscreen_enable_dt2w_val = 0;
static int xiaomi_msm8953_touchscreen_disable_keys_val = 0;

static struct proc_dir_entry *touchpanel_dir;

int xiaomi_msm8953_touchscreen_register_operations(struct xiaomi_msm8953_touchscreen_operations_t *ts_ops)
{
	if (IS_ERR_OR_NULL(ts_ops))
		return -EINVAL;

	if (!dev_get_drvdata(ts_ops->dev))
		return -EINVAL;

	xiaomi_msm8953_touchscreen_operations = ts_ops;

	if (!IS_ERR_OR_NULL(xiaomi_msm8953_touchscreen_operations->enable_dt2w))
		xiaomi_msm8953_touchscreen_operations->enable_dt2w(xiaomi_msm8953_touchscreen_operations->dev, false);
	if (!IS_ERR_OR_NULL(xiaomi_msm8953_touchscreen_operations->disable_keys))
		xiaomi_msm8953_touchscreen_operations->disable_keys(xiaomi_msm8953_touchscreen_operations->dev, false);

	return 0;
}
EXPORT_SYMBOL(xiaomi_msm8953_touchscreen_register_operations);

/* --- wakeup_gesture handlers --- */
static ssize_t wakeup_gesture_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	char page[32];
	int len = snprintf(page, sizeof(page), "%d\n", xiaomi_msm8953_touchscreen_enable_dt2w_val);
	return simple_read_from_buffer(buffer, count, ppos, page, len);
}

static ssize_t wakeup_gesture_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	int val, rc = 0;
	char kbuf[32];

	if (count > sizeof(kbuf) - 1)
		return -EINVAL;

	if (copy_from_user(kbuf, buffer, count))
		return -EFAULT;

	kbuf[count] = '\0';

	if (kstrtoint(kbuf, 10, &val))
		return -EINVAL;

	if (IS_ERR_OR_NULL(xiaomi_msm8953_touchscreen_operations) ||
		IS_ERR_OR_NULL(xiaomi_msm8953_touchscreen_operations->enable_dt2w))
		return -EFAULT;

	if (val != xiaomi_msm8953_touchscreen_enable_dt2w_val) {
		switch (val) {
			case 0:
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE)
				if (use_software_dt2w)
					dt2w_switch = 0;
				else
#endif
				rc = xiaomi_msm8953_touchscreen_operations->enable_dt2w(xiaomi_msm8953_touchscreen_operations->dev, false);
				break;
			case 1:
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE)
				if (use_software_dt2w)
					dt2w_switch = 1;
				else
#endif
				rc = xiaomi_msm8953_touchscreen_operations->enable_dt2w(xiaomi_msm8953_touchscreen_operations->dev, true);
				break;
			default:
				return -EINVAL;
		}

		if (rc < 0)
			return rc;

		xiaomi_msm8953_touchscreen_enable_dt2w_val = val;
	}

	return count;
}

/* --- disable_keys handlers --- */
static ssize_t disable_keys_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	char page[32];
	int len = snprintf(page, sizeof(page), "%d\n", xiaomi_msm8953_touchscreen_disable_keys_val);
	return simple_read_from_buffer(buffer, count, ppos, page, len);
}

static ssize_t disable_keys_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	int val, rc = 0;
	char kbuf[32];

	if (count > sizeof(kbuf) - 1)
		return -EINVAL;

	if (copy_from_user(kbuf, buffer, count))
		return -EFAULT;

	kbuf[count] = '\0';

	if (kstrtoint(kbuf, 10, &val))
		return -EINVAL;

	if (IS_ERR_OR_NULL(xiaomi_msm8953_touchscreen_operations) ||
		IS_ERR_OR_NULL(xiaomi_msm8953_touchscreen_operations->disable_keys))
		return -EFAULT;

	if (val != xiaomi_msm8953_touchscreen_disable_keys_val) {
		switch (val) {
			case 0:
				rc = xiaomi_msm8953_touchscreen_operations->disable_keys(xiaomi_msm8953_touchscreen_operations->dev, false);
				break;
			case 1:
				rc = xiaomi_msm8953_touchscreen_operations->disable_keys(xiaomi_msm8953_touchscreen_operations->dev, true);
				break;
			default:
				return -EINVAL;
		}

		if (rc < 0)
			return rc;

		xiaomi_msm8953_touchscreen_disable_keys_val = val;
	}

	return count;
}

static const struct file_operations wakeup_gesture_proc_ops = {
	.read = wakeup_gesture_read,
	.write = wakeup_gesture_write,
};

static const struct file_operations disable_keys_proc_ops = {
	.read = disable_keys_read,
	.write = disable_keys_write,
};

static int __init xiaomi_msm8953_touchscreen_proc_init(void)
{
	use_software_dt2w = true;

	touchpanel_dir = proc_mkdir("touchpanel", NULL);
	if (!touchpanel_dir)
		return -ENOMEM;

	if (!proc_create("wakeup_gesture", 0666, touchpanel_dir, &wakeup_gesture_proc_ops))
		goto err_node1;

	if (!proc_create("disable_keys", 0666, touchpanel_dir, &disable_keys_proc_ops))
		goto err_node2;

	return 0;

err_node2:
	remove_proc_entry("wakeup_gesture", touchpanel_dir);
err_node1:
	remove_proc_entry("touchpanel", NULL);
	return -ENOMEM;
}

static void __exit xiaomi_msm8953_touchscreen_proc_exit(void)
{
	remove_proc_entry("wakeup_gesture", touchpanel_dir);
	remove_proc_entry("disable_keys", touchpanel_dir);
	remove_proc_entry("touchpanel", NULL);
}

module_init(xiaomi_msm8953_touchscreen_proc_init);
module_exit(xiaomi_msm8953_touchscreen_proc_exit);