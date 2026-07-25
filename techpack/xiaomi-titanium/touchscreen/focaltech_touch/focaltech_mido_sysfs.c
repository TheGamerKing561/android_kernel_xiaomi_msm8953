#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "focaltech_core.h"

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/

/*****************************************************************************
* Static variables
*****************************************************************************/
static struct proc_dir_entry *fts_touchpanel_proc_dir;

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/* sysfs attributes for wakeup_gesture and disable_keys */
static ssize_t fts_wakeup_gesture_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = fts_data;
	const char c = ts_data->gesture_mode ? '1' : '0';
	return sprintf(buf, "%c\n", c);
}

static ssize_t fts_wakeup_gesture_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = fts_data;
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		mutex_lock(&ts_data->input_dev->mutex);
		if (i == 1) {
			FTS_DEBUG("enable gesture");
			ts_data->gesture_mode = ENABLE;
		} else {
			FTS_DEBUG("disable gesture");
			ts_data->gesture_mode = DISABLE;
		}
		mutex_unlock(&ts_data->input_dev->mutex);
		return count;
	} else {
		dev_dbg(dev, "wakeup_gesture write error\n");
		return -EINVAL;
	}
}

static ssize_t fts_disable_keys_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = fts_data;
	const char c = ts_data->disable_keys ? '1' : '0';
	return sprintf(buf, "%c\n", c);
}

static ssize_t fts_disable_keys_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = fts_data;
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		if (i == 1) {
			FTS_DEBUG("disable keys");
			ts_data->disable_keys = true;
		} else {
			FTS_DEBUG("enable keys");
			ts_data->disable_keys = false;
		}
		return count;
	} else {
		dev_dbg(dev, "disable_keys write error\n");
		return -EINVAL;
	}
}

static DEVICE_ATTR(wakeup_gesture, S_IWUSR | S_IRUSR, fts_wakeup_gesture_show,
		   fts_wakeup_gesture_store);
static DEVICE_ATTR(disable_keys, S_IWUSR | S_IRUSR, fts_disable_keys_show,
		   fts_disable_keys_store);

static struct attribute *fts_mido_attrs[] = {
	&dev_attr_disable_keys.attr,
	&dev_attr_wakeup_gesture.attr,
	NULL
};

static const struct attribute_group fts_mido_attr_group = {
	.attrs = fts_mido_attrs,
};

/*****************************************************************************
* Global functions
*****************************************************************************/

/**
 * fts_touchpanel_proc_init - Create /proc/touchpanel with symlinks
 * @sysfs_node_parent: sysfs node parent (from client->dev.kobj.sd)
 *
 * This function creates the /proc/touchpanel directory and creates symlinks
 * pointing to the sysfs attributes (wakeup_gesture and disable_keys).
 * This mimics the Goodix driver's approach for better compatibility.
 *
 * Return: 0 on success, error code on failure
 */
static int fts_touchpanel_proc_init(struct kernfs_node *sysfs_node_parent)
{
	int len, ret = 0;
	char *buf;
	char *wakeup_gesture_sysfs_node, *disable_keys_sysfs_node;
	struct proc_dir_entry *proc_entry_tp = NULL;
	struct proc_dir_entry *proc_symlink_tmp = NULL;

	buf = kzalloc(PATH_MAX, GFP_KERNEL);
	if (buf) {
		len = kernfs_path(sysfs_node_parent, buf, PATH_MAX);
		if (unlikely(len >= PATH_MAX)) {
			pr_err("%s: Buffer too long: %d\n", __func__, len);
			ret = -ERANGE;
			goto exit;
		}
	}

	/* Create /proc/touchpanel directory */
	proc_entry_tp = proc_mkdir("touchpanel", NULL);
	if (proc_entry_tp == NULL) {
		pr_err("%s: Couldn't create touchpanel dir in procfs\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}
	fts_touchpanel_proc_dir = proc_entry_tp;

	/* Create symlink for disable_keys */
	disable_keys_sysfs_node = kzalloc(PATH_MAX, GFP_KERNEL);
	if (disable_keys_sysfs_node)
		sprintf(disable_keys_sysfs_node, "/sys%s/%s", buf, "disable_keys");
	proc_symlink_tmp = proc_symlink("disable_keys",
					proc_entry_tp, disable_keys_sysfs_node);
	if (proc_symlink_tmp == NULL) {
		pr_err("%s: Couldn't create disable_keys symlink\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	/* Create symlink for wakeup_gesture */
	wakeup_gesture_sysfs_node = kzalloc(PATH_MAX, GFP_KERNEL);
	if (wakeup_gesture_sysfs_node)
		sprintf(wakeup_gesture_sysfs_node, "/sys%s/%s", buf, "wakeup_gesture");
	proc_symlink_tmp = proc_symlink("wakeup_gesture",
		proc_entry_tp, wakeup_gesture_sysfs_node);
	if (proc_symlink_tmp == NULL) {
		pr_err("%s: Couldn't create wakeup_gesture symlink\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

exit:
	kfree(buf);
	kfree(disable_keys_sysfs_node);
	kfree(wakeup_gesture_sysfs_node);
	return ret;
}

/**
 * fts_touchpanel_proc_deinit - Remove /proc/touchpanel
 *
 * This function removes the /proc/touchpanel directory and its symlinks.
 * Called during driver removal.
 */
static void fts_touchpanel_proc_deinit(void)
{
	if (fts_touchpanel_proc_dir) {
		/* Symlinks are automatically removed when parent is removed */
		proc_remove(fts_touchpanel_proc_dir);
		fts_touchpanel_proc_dir = NULL;
	}
}

/**
 * fts_create_mido_sysfs - Create sysfs group for mido attributes
 * @client: i2c client device
 *
 * Creates the wakeup_gesture and disable_keys sysfs attributes on the device,
 * and then creates symlinks in /proc/touchpanel pointing to these attributes.
 *
 * Return: 0 on success, error code on failure
 */
int fts_create_mido_sysfs(struct i2c_client *client)
{
	int ret = 0;

	/* Create sysfs attributes */
	ret = sysfs_create_group(&client->dev.kobj, &fts_mido_attr_group);
	if (ret) {
		FTS_ERROR("Failed to create sysfs group");
		sysfs_remove_group(&client->dev.kobj, &fts_mido_attr_group);
		return ret;
	}
	FTS_INFO("Successfully created sysfs group");

	/* Create /proc/touchpanel with symlinks to sysfs */
	ret = fts_touchpanel_proc_init(client->dev.kobj.sd);
	if (ret) {
		FTS_ERROR("Failed to create /proc/touchpanel symlinks");
		sysfs_remove_group(&client->dev.kobj, &fts_mido_attr_group);
		return ret;
	}
	FTS_INFO("Successfully created /proc/touchpanel symlinks");

	return 0;
}

/**
 * fts_remove_mido_sysfs - Remove sysfs group and proc entries
 * @client: i2c client device
 *
 * Removes the sysfs attributes and /proc/touchpanel symlinks created by fts_create_mido_sysfs.
 */
void fts_remove_mido_sysfs(struct i2c_client *client)
{
	fts_touchpanel_proc_deinit();
	sysfs_remove_group(&client->dev.kobj, &fts_mido_attr_group);
	FTS_INFO("Removed mido sysfs and proc entries");
}
