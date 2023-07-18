#include "dlpc4710.h"
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>

#define CNAME "dlpc4710"
int major = 0;
struct class* cls;
struct device* dev;
struct i2c_client* gclient;
char kbuf[128] = { 0 };

// RGB enable and disable
#define RGB_ENABLE 0xFF
#define RGB_DISABLE 0x00
#define RGB_REG 0x52

static int dlpc4710_open(struct inode *inode, struct file *file) {
	printk("dlpc4710_open\n");
	return 0;
}

static int dlpc4710_release(struct inode *inode, struct file *file) {
	printk("dlpc4710_release\n");
	return 0;
}

int i2c_set_command(u8 reg, u8 *data, int len) {
    int ret;
    struct i2c_msg w_msg[1];
    struct i2c_msg msg;
    uint8_t w_buf[128] = {0};
    w_buf[0] = reg;
    memcpy(w_buf + 1, data, len);

    msg.addr = gclient->addr;
    msg.flags = 0;
    msg.len = len + 1;
    msg.buf = w_buf;

    w_msg[0] = msg;

    ret = i2c_transfer(gclient->adapter, w_msg, 1);
    if (ret < 0) {
        printk("i2c write error\n");
        return -EIO;
    }
	return 0;
}

ssize_t dlpc4710_write(struct file* file,
    const char __user* ubuf, size_t size, loff_t* offs) {
    int ret;
    if (size < 2) {
       return -EIO;
    }
    if (size > sizeof(kbuf))
        size = sizeof(kbuf);
    ret = copy_from_user(kbuf, ubuf, size);
    if (ret) {
        printk("copy data from user error\n");
        // 在内核中错误码一共有4K
        return -EIO;
    }
    ret = i2c_set_command(kbuf[0], kbuf + 1, size - 1);
    if (ret) {
        printk("i2c write error\n");
        return -EIO;
    }
    return size;
}

long dlpc4710_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int data = 0, ret;
    u8 code;
    switch (cmd) {
    case SET_RGB_ENABLE:
        code = RGB_ENABLE;
        data = i2c_set_command(RGB_REG, &code, 1);
        ret = copy_to_user((void*)arg, &data, 4);
        if (ret) {
            printk("copy data to user error\n");
            return -EIO;
        }
        break;
    case SET_RGB_DISABLE:
        code = RGB_DISABLE;
        data = i2c_set_command(RGB_REG, &code, 1);
        ret = copy_to_user((void*)arg, &data, 4);
        if (ret) {
            printk("copy data to user error\n");
            return -EIO;
        }
        break;
    }
    return 0;
}

const struct file_operations fops = {
    .open = dlpc4710_open,
    .unlocked_ioctl = dlpc4710_ioctl,
    .release = dlpc4710_release,
    .write = dlpc4710_write,
};

/*i2c驱动的probe函数，当驱动与设备匹配以后此函数就会执行*/
static int dlpc4710_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk("dlpc4710_probe\r\n");
	printk("dlpc4710_probe addr111 =%x\r\n",client->addr);
	gclient = client;
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    // 1.注册字符设备驱动
    major = register_chrdev(0, CNAME, &fops);
    if (major < 0) {
        printk("register char device driver error\n");
        return -EAGAIN;
    }
    // 2.自动创建设备节点
    cls = class_create(THIS_MODULE, CNAME);
    if (IS_ERR(cls)) {
        printk("class create error\n");
        return PTR_ERR(cls);
    }
    dev = device_create(cls, NULL, MKDEV(major, 0), NULL, CNAME);
    if (IS_ERR(dev)) {
        printk("device create error\n");
        return PTR_ERR(dev);
    }
	return 0;
}

static int dlpc4710_remove(struct i2c_client *client)
{
	printk("dlpc4710_remove\r\n");
	// 1.删除设备节点
	device_destroy(cls, MKDEV(major, 0));
	// 2.删除设备类
	class_destroy(cls);
	// 3.注销字符设备驱动
	unregister_chrdev(major, CNAME);
	return 0;
}

/* 设备树匹配列表 */
static const struct of_device_id oftable[] = {
	{ .compatible = "dlpc,dlpc4710" },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, oftable);
static struct i2c_driver dlpc4710_driver = {
	.probe = dlpc4710_probe,
	.remove = dlpc4710_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "dlpc4710",//i2c驱动名字和i2c设备名字匹配一致，才能进入probe函数。与platform一致
		.of_match_table = oftable, //compatible用于匹配设备树
	}
};

module_i2c_driver(dlpc4710_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("jancsi");
MODULE_DESCRIPTION("dlpc4710 i2c driver");
