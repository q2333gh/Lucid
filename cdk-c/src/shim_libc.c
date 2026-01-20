// Minimal libc shim for stdio/fd/mmap on IC/WASM builds.
#include "shim/shim.h"

#include "idl/cdk_alloc.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(SHIM_LIBC_ENABLE)
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#define SHIM_FD_BASE 3
#define SHIM_FD_MAX 64
#define SHIM_MMAP_MAX 64

typedef struct {
    char  *name;
    size_t size;
    size_t offset;
    int    used;
} shim_fd_entry_t;

typedef struct {
    void  *addr;
    size_t len;
} shim_map_entry_t;

typedef struct {
    char  *name;
    size_t size;
    size_t offset;
} shim_file_t;

static shim_fd_entry_t  g_fds[SHIM_FD_MAX];
static shim_map_entry_t g_maps[SHIM_MMAP_MAX];

static shim_fd_entry_t *shim_fd_get(int fd) {
    int idx = fd - SHIM_FD_BASE;
    if (idx < 0 || idx >= SHIM_FD_MAX) {
        return NULL;
    }
    if (!g_fds[idx].used) {
        return NULL;
    }
    return &g_fds[idx];
}

static int shim_fd_alloc(const char *path, size_t size) {
    for (int i = 0; i < SHIM_FD_MAX; i++) {
        if (!g_fds[i].used) {
            size_t name_len = strlen(path);
            char  *copy = (char *)cdk_malloc(name_len + 1);
            if (copy == NULL) {
                return -1;
            }
            memcpy(copy, path, name_len + 1);
            g_fds[i].name = copy;
            g_fds[i].size = size;
            g_fds[i].offset = 0;
            g_fds[i].used = 1;
            return i + SHIM_FD_BASE;
        }
    }
    return -1;
}

int open(const char *path, int flags, ...) {
    (void)flags;
    size_t size = 0;
    if (shim_blob_size(path, &size) != SHIM_OK) {
        errno = ENOENT;
        return -1;
    }

    int fd = shim_fd_alloc(path, size);
    if (fd < 0) {
        errno = EMFILE;
        return -1;
    }
    return fd;
}

int close(int fd) {
    shim_fd_entry_t *entry = shim_fd_get(fd);
    if (entry == NULL) {
        errno = EBADF;
        return -1;
    }
    cdk_free(entry->name);
    entry->name = NULL;
    entry->size = 0;
    entry->offset = 0;
    entry->used = 0;
    return 0;
}

ssize_t read(int fd, void *buf, size_t count) {
    shim_fd_entry_t *entry = shim_fd_get(fd);
    if (entry == NULL || buf == NULL) {
        errno = EBADF;
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    if (entry->offset >= entry->size) {
        return 0;
    }
    size_t remaining = entry->size - entry->offset;
    size_t to_read = count < remaining ? count : remaining;
    if (shim_blob_read(entry->name, entry->offset, buf, to_read) != SHIM_OK) {
        errno = EIO;
        return -1;
    }
    entry->offset += to_read;
    return (ssize_t)to_read;
}

off_t lseek(int fd, off_t offset, int whence) {
    shim_fd_entry_t *entry = shim_fd_get(fd);
    if (entry == NULL) {
        errno = EBADF;
        return (off_t)-1;
    }

    int64_t base = 0;
    if (whence == SEEK_SET) {
        base = 0;
    } else if (whence == SEEK_CUR) {
        base = (int64_t)entry->offset;
    } else if (whence == SEEK_END) {
        base = (int64_t)entry->size;
    } else {
        errno = EINVAL;
        return (off_t)-1;
    }

    int64_t next = base + (int64_t)offset;
    if (next < 0) {
        errno = EINVAL;
        return (off_t)-1;
    }
    if ((uint64_t)next > entry->size) {
        errno = EINVAL;
        return (off_t)-1;
    }

    entry->offset = (size_t)next;
    return (off_t)entry->offset;
}

int fstat(int fd, struct stat *st) {
    shim_fd_entry_t *entry = shim_fd_get(fd);
    if (entry == NULL) {
        errno = EBADF;
        return -1;
    }
    memset(st, 0, sizeof(*st));
    st->st_size = (off_t)entry->size;
    return 0;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t off) {
    (void)addr;
    (void)prot;
    (void)flags;

    shim_fd_entry_t *entry = shim_fd_get(fd);
    if (entry == NULL) {
        errno = EBADF;
        return MAP_FAILED;
    }

    if (off < 0 || (size_t)off > entry->size) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    size_t max_len = entry->size - (size_t)off;
    size_t map_len = length == 0 ? max_len : length;
    if (map_len > max_len) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    void *buffer = cdk_malloc(map_len);
    if (buffer == NULL) {
        errno = ENOMEM;
        return MAP_FAILED;
    }

    if (map_len > 0 &&
        shim_blob_read(entry->name, (size_t)off, buffer, map_len) != SHIM_OK) {
        cdk_free(buffer);
        errno = EIO;
        return MAP_FAILED;
    }

    for (int i = 0; i < SHIM_MMAP_MAX; i++) {
        if (g_maps[i].addr == NULL) {
            g_maps[i].addr = buffer;
            g_maps[i].len = map_len;
            return buffer;
        }
    }

    cdk_free(buffer);
    errno = ENOMEM;
    return MAP_FAILED;
}

int munmap(void *addr, size_t length) {
    (void)length;
    if (addr == NULL || addr == MAP_FAILED) {
        errno = EINVAL;
        return -1;
    }
    for (int i = 0; i < SHIM_MMAP_MAX; i++) {
        if (g_maps[i].addr == addr) {
            cdk_free(g_maps[i].addr);
            g_maps[i].addr = NULL;
            g_maps[i].len = 0;
            return 0;
        }
    }
    errno = EINVAL;
    return -1;
}

static shim_file_t *shim_file_open(const char *path) {
    size_t size = 0;
    if (shim_blob_size(path, &size) != SHIM_OK) {
        return NULL;
    }

    shim_file_t *file = (shim_file_t *)cdk_malloc(sizeof(shim_file_t));
    if (file == NULL) {
        return NULL;
    }

    size_t name_len = strlen(path);
    char  *copy = (char *)cdk_malloc(name_len + 1);
    if (copy == NULL) {
        cdk_free(file);
        return NULL;
    }
    memcpy(copy, path, name_len + 1);

    file->name = copy;
    file->size = size;
    file->offset = 0;
    return file;
}

FILE *fopen(const char *path, const char *mode) {
    if (path == NULL || mode == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL) {
        errno = EROFS;
        return NULL;
    }

    shim_file_t *file = shim_file_open(path);
    if (file == NULL) {
        errno = ENOENT;
        return NULL;
    }
    return (FILE *)file;
}

int fclose(FILE *stream) {
    if (stream == NULL) {
        errno = EINVAL;
        return -1;
    }
    shim_file_t *file = (shim_file_t *)stream;
    cdk_free(file->name);
    cdk_free(file);
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (ptr == NULL || stream == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (size == 0 || nmemb == 0) {
        return 0;
    }
    if (nmemb > SIZE_MAX / size) {
        errno = EINVAL;
        return 0;
    }

    shim_file_t *file = (shim_file_t *)stream;
    size_t       total = size * nmemb;
    size_t       remaining = file->size - file->offset;
    size_t       to_read = total < remaining ? total : remaining;

    if (to_read > 0 &&
        shim_blob_read(file->name, file->offset, ptr, to_read) != SHIM_OK) {
        errno = EIO;
        return 0;
    }

    file->offset += to_read;
    return to_read / size;
}

int fseek(FILE *stream, long offset, int whence) {
    if (stream == NULL) {
        errno = EINVAL;
        return -1;
    }
    shim_file_t *file = (shim_file_t *)stream;

    int64_t base = 0;
    if (whence == SEEK_SET) {
        base = 0;
    } else if (whence == SEEK_CUR) {
        base = (int64_t)file->offset;
    } else if (whence == SEEK_END) {
        base = (int64_t)file->size;
    } else {
        errno = EINVAL;
        return -1;
    }

    int64_t next = base + (int64_t)offset;
    if (next < 0 || (uint64_t)next > file->size) {
        errno = EINVAL;
        return -1;
    }
    file->offset = (size_t)next;
    return 0;
}

long ftell(FILE *stream) {
    if (stream == NULL) {
        errno = EINVAL;
        return -1;
    }
    shim_file_t *file = (shim_file_t *)stream;
    if (file->offset > (size_t)LONG_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (long)file->offset;
}

int fileno(FILE *stream) {
    (void)stream;
    errno = ENOTSUP;
    return -1;
}

#endif
