#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shim/shim.h"

static const char    k_blob_name[] = "asset";
static const uint8_t k_blob_data[] = {0x10, 0x20, 0x30, 0x40, 0x50};

static shim_result_t
libc_blob_size(void *ctx, const char *name, size_t *out_size) {
    (void)ctx;
    if (name == NULL || out_size == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }
    if (strcmp(name, k_blob_name) != 0) {
        return SHIM_ERR_NOT_FOUND;
    }
    *out_size = sizeof(k_blob_data);
    return SHIM_OK;
}

static shim_result_t libc_blob_read(
    void *ctx, const char *name, size_t offset, uint8_t *dst, size_t len) {
    (void)ctx;
    if (name == NULL || dst == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }
    if (strcmp(name, k_blob_name) != 0) {
        return SHIM_ERR_NOT_FOUND;
    }
    if (offset > sizeof(k_blob_data) || len > sizeof(k_blob_data) - offset) {
        return SHIM_ERR_OUT_OF_BOUNDS;
    }
    memcpy(dst, k_blob_data + offset, len);
    return SHIM_OK;
}

static const shim_ops_t k_libc_ops = {
    .ctx = NULL,
    .blob_size = libc_blob_size,
    .blob_read = libc_blob_read,
    .map = NULL,
    .unmap = NULL,
    .log = NULL,
    .time_ns = NULL,
    .getrandom = NULL,
};

const shim_ops_t *shim_native_default_ops(void) { return &k_libc_ops; }
const shim_ops_t *shim_ic_default_ops(void) { return &k_libc_ops; }

static int test_open_read_seek_fstat(void) {
    shim_set_ops(&k_libc_ops);

    int fd = open(k_blob_name, O_RDONLY);
    if (fd < 0) {
        return 1;
    }

    uint8_t buffer[2] = {0};
    if (read(fd, buffer, sizeof(buffer)) != (ssize_t)sizeof(buffer)) {
        close(fd);
        return 1;
    }
    if (buffer[0] != 0x10 || buffer[1] != 0x20) {
        close(fd);
        return 1;
    }

    if (lseek(fd, 1, SEEK_SET) != (off_t)1) {
        close(fd);
        return 1;
    }
    if (read(fd, buffer, 1) != (ssize_t)1 || buffer[0] != 0x20) {
        close(fd);
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || (size_t)st.st_size != sizeof(k_blob_data)) {
        close(fd);
        return 1;
    }

    if (close(fd) != 0) {
        return 1;
    }
    return 0;
}

static int test_mmap_and_fread(void) {
    shim_set_ops(&k_libc_ops);

    int fd = open(k_blob_name, O_RDONLY);
    if (fd < 0) {
        return 1;
    }

    uint8_t *mapped = mmap(NULL, 0, 0, 0, fd, 0);
    if (mapped == MAP_FAILED) {
        close(fd);
        return 1;
    }
    if (mapped[0] != 0x10 || mapped[4] != 0x50) {
        munmap(mapped, sizeof(k_blob_data));
        close(fd);
        return 1;
    }
    if (munmap(mapped, sizeof(k_blob_data)) != 0) {
        close(fd);
        return 1;
    }
    if (close(fd) != 0) {
        return 1;
    }

    FILE *fp = fopen(k_blob_name, "rb");
    if (fp == NULL) {
        return 1;
    }
    uint8_t buffer[3] = {0};
    if (fread(buffer, 1, sizeof(buffer), fp) != sizeof(buffer)) {
        fclose(fp);
        return 1;
    }
    if (buffer[0] != 0x10 || buffer[1] != 0x20 || buffer[2] != 0x30) {
        fclose(fp);
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || ftell(fp) != (long)sizeof(k_blob_data)) {
        fclose(fp);
        return 1;
    }
    if (fclose(fp) != 0) {
        return 1;
    }
    return 0;
}

static int test_open_missing_sets_errno(void) {
    shim_set_ops(&k_libc_ops);

    errno = 0;
    int fd = open("missing", O_RDONLY);
    if (fd != -1 || errno != ENOENT) {
        return 1;
    }
    return 0;
}

static int test_fopen_write_rejected(void) {
    shim_set_ops(&k_libc_ops);

    errno = 0;
    FILE *fp = fopen(k_blob_name, "wb");
    if (fp != NULL || errno != EROFS) {
        return 1;
    }
    return 0;
}

int main(void) {
    int failures = 0;

    failures += test_open_read_seek_fstat();
    failures += test_mmap_and_fread();
    failures += test_open_missing_sets_errno();
    failures += test_fopen_write_rejected();

    if (failures != 0) {
        fprintf(stderr, "shim_libc tests failed: %d\n", failures);
        return 1;
    }

    return 0;
}
