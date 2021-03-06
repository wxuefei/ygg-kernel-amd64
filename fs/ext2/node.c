#include "fs/ext2/alloc.h"
#include "fs/ext2/block.h"
#include "fs/ext2/dir.h"
#include "fs/ext2/ext2.h"
#include "fs/ext2/node.h"
#include "fs/fs.h"
#include "fs/node.h"
#include "fs/ofile.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/mem/slab.h"
#include "sys/panic.h"
#include "sys/string.h"
#include "user/errno.h"
#include "user/fcntl.h"

static int ext2_vnode_find(struct vnode *at, const char *name, struct vnode **res);
static ssize_t ext2_vnode_read(struct ofile *fd, void *buf, size_t count);
static ssize_t ext2_vnode_write(struct ofile *fd, const void *buf, size_t count);
static off_t ext2_vnode_lseek(struct ofile *fd, off_t pos, int whence);
static int ext2_vnode_open(struct ofile *fd, int opt);
static int ext2_vnode_opendir(struct ofile *fd);
static int ext2_vnode_chmod(struct vnode *vn, mode_t new_mode);
static int ext2_vnode_chown(struct vnode *node, uid_t new_uid, gid_t new_gid);
static ssize_t ext2_vnode_readdir(struct ofile *fd, struct dirent *ent);
static int ext2_vnode_stat(struct vnode *at, struct stat *st);
static int ext2_vnode_truncate(struct vnode *at, size_t new_size);
static int ext2_vnode_creat(struct vnode *at, const char *name, uid_t uid, gid_t gid, mode_t mode);
static int ext2_vnode_mkdir(struct vnode *at, const char *name, uid_t uid, gid_t gid, mode_t mode);
static int ext2_vnode_unlink(struct vnode *node);

////

struct vnode_operations g_ext2_vnode_ops = {
    .find = ext2_vnode_find,

    .opendir = ext2_vnode_opendir,
    .readdir = ext2_vnode_readdir,

    .stat = ext2_vnode_stat,
    .chmod = ext2_vnode_chmod,
    .chown = ext2_vnode_chown,

    .creat = ext2_vnode_creat,
    .mkdir = ext2_vnode_mkdir,
    .unlink = ext2_vnode_unlink,

    .open = ext2_vnode_open,
    .read = ext2_vnode_read,
    .write = ext2_vnode_write,
    .truncate = ext2_vnode_truncate,
    .lseek = ext2_vnode_lseek,
};

////

static int ext2_vnode_find(struct vnode *at, const char *name, struct vnode **result) {
    _assert(at && at->type == VN_DIR);
    _assert(name);
    struct ext2_inode *at_inode = at->fs_data;
    _assert(at_inode);
    struct fs *ext2 = at->fs;
    _assert(ext2);
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    char block_buffer[data->block_size];
    uint32_t block_offset;
    uint32_t dir_blocks = at_inode->size_lower / data->block_size;
    size_t name_length = strlen(name);
    int res;

    // Because
    _assert(name_length < 255);

    for (uint32_t block_index = 0; block_index < dir_blocks; ++block_index) {
        block_offset = 0;

        if ((res = ext2_read_inode_block(ext2, at_inode, block_buffer, block_index)) != 0) {
            return res;
        }

        while (block_offset < data->block_size) {
            struct ext2_dirent *dirent = (struct ext2_dirent *) (block_buffer + block_offset);
            if (dirent->ino && (dirent->name_length_low == name_length)) {
                if (!strncmp(dirent->name, name, name_length)) {
                    // Found the dirent
                    if (!result) {
                        // Just return if no entry needs to be loaded
                        return 0;
                    }
                    struct ext2_inode *res_inode = slab_calloc(data->inode_cache);
                    _assert(res_inode);
                    struct vnode *node = vnode_create(VN_DIR, name);

                    if ((res = ext2_read_inode(ext2, res_inode, dirent->ino)) != 0) {
                        kfree(res_inode);
                        return res;
                    }

                    node->fs = ext2;
                    ext2_inode_to_vnode(node, res_inode, dirent->ino);

                    *result = node;
                    return 0;
                }
            }

            block_offset += dirent->ent_size;
        }
    }

    return -ENOENT;
}

static int ext2_vnode_open(struct ofile *fd, int opt) {
    if ((opt & O_APPEND) && (opt & O_ACCMODE) == O_RDONLY) {
        // Impossible, I guess
        return -EINVAL;
    }
    _assert(!(opt & O_DIRECTORY));

    fd->file.pos = 0;

    return 0;
}

static ssize_t ext2_vnode_read(struct ofile *fd, void *buf, size_t count) {
    _assert(fd);
    _assert(buf);
    struct vnode *node = fd->file.vnode;
    _assert(node);
    struct ext2_inode *inode = node->fs_data;
    _assert(inode);
    struct fs *ext2 = node->fs;
    _assert(ext2);
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    // TODO
    _assert(!inode->size_upper);

    if (fd->file.pos >= inode->size_lower) {
        return 0;
    }

    char block_buffer[data->block_size];
    size_t rem = MIN(count, inode->size_lower - fd->file.pos);
    size_t bread = 0;
    int res;

    while (rem) {
        size_t block_offset = fd->file.pos % data->block_size;
        size_t can_read = MIN(rem, data->block_size - block_offset);
        uint32_t block_index = fd->file.pos / data->block_size;

        if ((res = ext2_read_inode_block(ext2, inode, block_buffer, block_index)) != 0) {
            return res;
        }

        memcpy(buf, block_buffer + block_offset, can_read);

        buf += can_read;
        fd->file.pos += can_read;
        bread += can_read;
        rem -= can_read;
    }

    return bread;
}

static off_t ext2_vnode_lseek(struct ofile *fd, off_t pos, int whence) {
    off_t calc;
    struct vnode *node = fd->file.vnode;
    _assert(node);
    struct ext2_inode *inode = node->fs_data;
    _assert(inode);

    switch (whence) {
    case SEEK_SET:
        calc = pos;
        break;
    case SEEK_CUR:
        calc = pos + fd->file.pos;
        break;
    default:
        panic("Invalid seek whence: %d\n", whence);
    }

    if (calc < 0 || calc >= inode->size_lower) {
        return (off_t) -ESPIPE;
    }

    fd->file.pos = calc;

    return fd->file.pos;
}

static int ext2_vnode_opendir(struct ofile *fd) {
    _assert(fd && fd->file.vnode);
    _assert(fd->file.vnode->type == VN_DIR);
    fd->file.pos = 0;
    return 0;
}

static ssize_t ext2_vnode_readdir(struct ofile *fd, struct dirent *ent) {
    _assert(fd);
    _assert(ent);
    struct vnode *node = fd->file.vnode;
    _assert(node);
    struct ext2_inode *inode = node->fs_data;
    _assert(inode);
    struct fs *ext2 = node->fs;
    _assert(ext2);
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    if (fd->file.pos >= inode->size_lower) {
        return 0;
    }

    char block_buffer[data->block_size];
    uint32_t block_index = fd->file.pos / data->block_size;
    struct ext2_dirent *dirent = (struct ext2_dirent *) (block_buffer + fd->file.pos % data->block_size);
    int res;

    if ((res = ext2_read_inode_block(ext2, inode, block_buffer, block_index)) != 0) {
        return res;
    }

    if (!dirent->ino) {
        fd->file.pos += dirent->ent_size;

        if (fd->file.pos / data->block_size != block_index) {
            // Dirent reclen cannot point beyond block size
            _assert(!(fd->file.pos % data->block_size));
            // Requires reading a next block for dirent
            return ext2_vnode_readdir(fd, ent);
        }

        // Don't think the scenario of two empty dirents following each other is likely
        dirent = (struct ext2_dirent *) (block_buffer + fd->file.pos % data->block_size);
        _assert(dirent->ino);
    }

    uint32_t name_length = dirent->name_length_low;
    ent->d_type = DT_UNKNOWN;
    // Even if provided that hint, just ignore it. stat() should be sufficient
    if (!(data->sb.required_features & EXT2_REQ_ENT_TYPE)) {
        name_length += (uint16_t) dirent->name_length_high << 8;
    }
    strncpy(ent->d_name, dirent->name, name_length);
    ent->d_name[name_length] = 0;
    ent->d_off = fd->file.pos;
    ent->d_reclen = dirent->ent_size;

    // Calculate next dirent position
    fd->file.pos += dirent->ent_size;

    return ent->d_reclen;
}

static int ext2_vnode_stat(struct vnode *node, struct stat *st) {
    _assert(node && st);
    struct ext2_inode *inode = node->fs_data;
    _assert(inode);
    struct fs *ext2 = node->fs;
    _assert(ext2);
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    _assert(inode->uid == node->uid);
    _assert(inode->gid == node->gid);
    _assert((inode->mode & 0xFFF) == node->mode);

    st->st_mode = inode->mode;
    st->st_uid = inode->uid;
    st->st_gid = inode->gid;

    st->st_size = inode->size_lower;
    _assert(!inode->size_upper);
    st->st_blksize = data->block_size;
    st->st_blocks = (st->st_size + data->block_size - 1) / data->block_size;

    st->st_dev = 0;
    st->st_rdev = 0;

    st->st_mtime = inode->mtime;
    st->st_atime = inode->atime;
    st->st_ctime = inode->ctime;

    st->st_nlink = inode->hard_links;

    return 0;
}

////

// Map inode parameters to vnode fields
void ext2_inode_to_vnode(struct vnode *vnode, struct ext2_inode *inode, uint32_t ino) {
    vnode->ino = ino;
    vnode->uid = inode->uid;
    vnode->gid = inode->gid;
    vnode->mode = inode->mode & 0xFFF;
    vnode->fs_data = inode;
    vnode->op = &g_ext2_vnode_ops;

    switch (inode->mode & 0xF000) {
    case EXT2_IFDIR:
        vnode->type = VN_DIR;
        break;
    case EXT2_IFREG:
        vnode->type = VN_REG;
        break;
    default:
        panic("Unsupported inode type: %04x\n", inode->mode & 0xF000);
    }
}

// Mutation operations
static ssize_t ext2_vnode_write(struct ofile *fd, const void *buf, size_t count) {
    _assert(fd);
    _assert(buf);
    struct vnode *node = fd->file.vnode;
    _assert(node);
    struct ext2_inode *inode = node->fs_data;
    _assert(inode);
    struct fs *ext2 = node->fs;
    _assert(ext2);
    struct ext2_data *data = ext2->fs_private;
    _assert(data);
    int res;

    // TODO
    _assert(!inode->size_upper);

    if (fd->file.pos > inode->size_lower) {
        return 0;
    }

    char block_buf[1024];
    size_t req_size = MAX(fd->file.pos + count, inode->size_lower);
    uint32_t block_index;
    uint32_t block_offset;
    uint32_t can_write;
    size_t rem = count;
    size_t off = 0;

    if ((res = ext2_file_resize(ext2, node->ino, inode, req_size)) != 0) {
        return res;
    }

    while (rem) {
        block_index = fd->file.pos / data->block_size;
        block_offset = fd->file.pos % data->block_size;
        can_write = MIN(rem, data->block_size - block_offset);
        _assert(can_write);

        if (can_write != data->block_size) {
            if (ext2_read_inode_block(ext2, inode, block_buf, block_index) != 0) {
                panic("PANIC\n");
            }

            memcpy(block_buf + block_offset, buf + off, can_write);

            if (ext2_write_inode_block(ext2, inode, block_buf, block_index) != 0) {
                panic("PANIC\n");
            }
        } else {
            if (ext2_write_inode_block(ext2, inode, buf + off, block_index) != 0) {
                panic("PANIC\n");
            }
        }

        rem -= can_write;
        off += can_write;
        fd->file.pos += can_write;
    }

    inode->mtime = time();

    if ((res = ext2_write_inode(ext2, inode, node->ino)) != 0) {
        return res;
    }

    return off;
}

static int ext2_vnode_truncate(struct vnode *node, size_t new_size) {
    _assert(node);
    struct ext2_inode *inode = node->fs_data;
    struct fs *ext2 = node->fs;
    _assert(ext2);
    int res;

    if (node->type != VN_REG) {
        return -EPERM;
    }

    if ((res = ext2_file_resize(ext2, node->ino, inode, new_size)) != 0) {
        return res;
    }

    inode->mtime = time();

    return ext2_write_inode(ext2, inode, node->ino);
}

static int ext2_vnode_chmod(struct vnode *node, mode_t new_mode) {
    _assert(node);
    struct ext2_inode *inode = node->fs_data;
    _assert(inode);
    struct fs *ext2 = node->fs;
    _assert(ext2);

    // Only rewrite inode if something really changed
    if ((inode->mode & 0xFFF) != (new_mode & 0xFFF)) {
        // chmod() only updates file mode, don't touch file type
        inode->mode &= ~0xFFF;
        inode->mode |= new_mode & 0xFFF;
        inode->mtime = time();
        node->mode = new_mode & 0xFFF;

        return ext2_write_inode(ext2, inode, node->ino);
    }

    return 0;
}

static int ext2_vnode_chown(struct vnode *node, uid_t new_uid, gid_t new_gid) {
    _assert(node);
    struct ext2_inode *inode = node->fs_data;
    _assert(inode);
    struct fs *ext2 = node->fs;
    _assert(ext2);

    if (new_uid != inode->uid || new_gid != inode->gid) {
        inode->uid = new_uid;
        inode->gid = new_gid;
        inode->mtime = time();
        node->uid = new_uid;
        node->gid = new_gid;

        return ext2_write_inode(ext2, inode, node->ino);
    }

    return 0;
}

static int ext2_vnode_creat(struct vnode *at, const char *name, uid_t uid, gid_t gid, mode_t mode) {
    _assert(at);
    struct ext2_inode *at_inode = at->fs_data;
    _assert(at_inode);
    struct fs *ext2 = at->fs;
    _assert(ext2);
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    if (ext2_vnode_find(at, name, NULL) == 0) {
        return -EEXIST;
    }

    if ((at_inode->mode & 0xF000) != EXT2_IFDIR) {
        panic("Not a directory\n");
    }

    struct ext2_inode *inode = slab_calloc(data->inode_cache);
    if (!inode) {
        panic("Failed to allocate (host) an inode\n");
    }

    uint32_t ino = ext2_alloc_inode(ext2, data, 0);
    if (!ino) {
        panic("Failed to allocate (disk) an inode\n");
    }

    inode->mtime = time();
    inode->atime = inode->mtime;
    inode->ctime = inode->mtime;
    inode->uid = uid;
    inode->gid = gid;
    inode->mode = (mode & 0xFFF) | EXT2_IFREG;
    inode->hard_links = 1;

    _assert(ext2_dir_insert_inode(ext2, at_inode, at->ino, name, ino, VN_REG) == 0);
    _assert(ext2_write_inode(ext2, inode, ino) == 0);

    return 0;
}

static int ext2_vnode_mkdir(struct vnode *at, const char *name, uid_t uid, gid_t gid, mode_t mode) {
    _assert(at);
    struct ext2_inode *at_inode = at->fs_data;
    _assert(at_inode);
    struct fs *ext2 = at->fs;
    _assert(ext2);
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    if (ext2_vnode_find(at, name, NULL) == 0) {
        return -EEXIST;
    }

    if ((at_inode->mode & 0xF000) != EXT2_IFDIR) {
        panic("Not a directory\n");
    }

    struct ext2_inode *inode = slab_calloc(data->inode_cache);
    if (!inode) {
        panic("Failed to allocate (host) an inode\n");
    }

    uint32_t ino = ext2_alloc_inode(ext2, data, 1);
    if (!ino) {
        panic("Failed to allocate (disk) an inode\n");
    }

    inode->mtime = time();
    inode->atime = inode->mtime;
    inode->ctime = inode->mtime;
    inode->uid = uid;
    inode->gid = gid;
    inode->mode = (mode & 0xFFF) | EXT2_IFDIR;
    inode->hard_links = 2; // "." and parent dirent

    // Setup the first block for this inode
    _assert(ext2_dir_insert_inode(ext2, inode, ino, ".", ino, VN_DIR) == 0);
    _assert(ext2_dir_insert_inode(ext2, inode, ino, "..", at->ino, VN_DIR) == 0);

    // Insert to parent
    _assert(ext2_dir_insert_inode(ext2, at_inode, at->ino, name, ino, VN_DIR) == 0);
    _assert(ext2_write_inode(ext2, inode, ino) == 0);
    // ".." points to the parent, so increment its refcount
    ++at_inode->hard_links;
    _assert(ext2_write_inode(ext2, at_inode, at->ino) == 0);

    return 0;
}

static int ext2_vnode_unlink(struct vnode *vn) {
    if (vn->ino == 2) {
        // Cannot remove root node
        return -EPERM;
    }

    struct vnode *at = vn->parent;
    _assert(at);
    struct fs *ext2 = vn->fs;
    _assert(ext2);
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    struct ext2_inode *at_inode = at->fs_data;
    struct ext2_inode *inode = vn->fs_data;
    _assert(at_inode && inode);

    if (ext2_vnode_find(at, vn->name, NULL) != 0) {
        return -ENOENT;
    }

    _assert(ext2_dir_del_inode(ext2, at_inode, at->ino, vn) == 0);

    if ((inode->mode & 0xF000) == EXT2_IFDIR) {
        // `vn`s ".." points to `at`, so decrement refcount
        --at_inode->hard_links;
        _assert(ext2_write_inode(ext2, at_inode, at->ino) == 0);
    }

    // Resize the inode to zero to free its blocks
    _assert(ext2_file_resize(ext2, vn->ino, inode, 0) == 0);

    inode->hard_links = 0;

    // Mark it as "deleted" (?)
    inode->dtime = time();

    _assert(ext2_write_inode(ext2, inode, vn->ino) == 0);

    // Free the inode
    ext2_free_inode(ext2, data, vn->ino, vn->type == VN_DIR);

    return 0;
}
