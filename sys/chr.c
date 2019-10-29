#include "sys/chr.h"
#include "sys/errno.h"

ssize_t chr_write(struct chrdev *chr, const void *buf, size_t off, size_t count) {
    if (!chr) {
        return -ENODEV;
    }
    if (!chr->write) {
        return -EROFS;
    }
    return chr->write(chr, buf, off, count);
}

ssize_t chr_read(struct chrdev *chr, void *buf, size_t off, size_t count) {
    if (!chr) {
        return -ENODEV;
    }
    if (!chr->read) {
        return -EINVAL;
    }
    return chr->read(chr, buf, off, count);
}