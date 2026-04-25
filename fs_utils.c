#include "fs_utils.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int cm_write_all(int fd, const char *buf, size_t count)
{
    size_t written_total = 0;

    while (written_total < count) {
        ssize_t written = write(fd, buf + written_total, count - written_total);
        if (written == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (written == 0) {
            errno = EIO;
            return -1;
        }
        written_total += (size_t)written;
    }

    return 0;
}

int cm_writef(int fd, const char *format, ...)
{
    char stack_buf[1024];
    char *buf = stack_buf;
    size_t buf_size = sizeof(stack_buf);
    va_list args;
    va_list copy;
    int needed;
    int result;

    va_start(args, format);
    va_copy(copy, args);
    needed = vsnprintf(buf, buf_size, format, args);
    va_end(args);

    if (needed < 0) {
        va_end(copy);
        return -1;
    }

    if ((size_t)needed >= buf_size) {
        buf_size = (size_t)needed + 1;
        buf = malloc(buf_size);
        if (buf == NULL) {
            va_end(copy);
            return -1;
        }
        needed = vsnprintf(buf, buf_size, format, copy);
        if (needed < 0) {
            free(buf);
            va_end(copy);
            return -1;
        }
    }

    va_end(copy);
    result = cm_write_all(fd, buf, (size_t)needed);
    if (buf != stack_buf) {
        free(buf);
    }
    return result;
}

void cm_write_text(int fd, const char *text)
{
    if (text != NULL) {
        (void)cm_write_all(fd, text, strlen(text));
    }
}

void cm_error(const char *format, ...)
{
    char stack_buf[1024];
    char *buf = stack_buf;
    size_t buf_size = sizeof(stack_buf);
    va_list args;
    va_list copy;
    int needed;

    va_start(args, format);
    va_copy(copy, args);
    needed = vsnprintf(buf, buf_size, format, args);
    va_end(args);

    if (needed < 0) {
        va_end(copy);
        return;
    }

    if ((size_t)needed >= buf_size) {
        buf_size = (size_t)needed + 1;
        buf = malloc(buf_size);
        if (buf == NULL) {
            va_end(copy);
            return;
        }
        needed = vsnprintf(buf, buf_size, format, copy);
        if (needed < 0) {
            free(buf);
            va_end(copy);
            return;
        }
    }

    va_end(copy);
    (void)cm_write_all(STDERR_FILENO, buf, (size_t)needed);
    if (buf != stack_buf) {
        free(buf);
    }
}

void cm_errno(const char *context)
{
    int saved_errno = errno;
    cm_error("%s: %s\n", context, strerror(saved_errno));
}

int ensure_directory(const char *path, mode_t mode)
{
    struct stat st;

    if (mkdir(path, mode) == -1 && errno != EEXIST) {
        cm_errno(path);
        return -1;
    }

    if (stat(path, &st) == -1) {
        cm_errno(path);
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        cm_error("%s exists but is not a directory\n", path);
        return -1;
    }

    if (chmod(path, mode) == -1) {
        cm_errno(path);
        return -1;
    }

    return 0;
}

int ensure_storage_layout(void)
{
    if (ensure_directory(CM_DATA_DIR, 0750) == -1) {
        return -1;
    }
    if (ensure_directory(CM_REPORT_DIR, 0750) == -1) {
        return -1;
    }
    if (ensure_directory(CM_LATEST_DIR, 0750) == -1) {
        return -1;
    }
    return 0;
}

int validate_name(const char *name, const char *label)
{
    size_t i;

    if (name == NULL || name[0] == '\0') {
        cm_error("%s must not be empty\n", label);
        return -1;
    }

    for (i = 0; name[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)name[i];
        if (!isalnum(ch) && ch != '_' && ch != '-') {
            cm_error("%s may contain only letters, digits, '_' and '-': %s\n",
                     label,
                     name);
            return -1;
        }
    }

    return 0;
}

int build_report_path(const char *district, char *buf, size_t buflen)
{
    int written;

    if (validate_name(district, "district") == -1) {
        return -1;
    }

    written = snprintf(buf, buflen, "%s/%s.bin", CM_REPORT_DIR, district);
    if (written < 0 || (size_t)written >= buflen) {
        cm_error("report path is too long for district %s\n", district);
        return -1;
    }

    return 0;
}

int build_latest_link_path(const char *district, char *buf, size_t buflen)
{
    int written;

    if (validate_name(district, "district") == -1) {
        return -1;
    }

    written = snprintf(buf, buflen, "%s/%s.bin", CM_LATEST_DIR, district);
    if (written < 0 || (size_t)written >= buflen) {
        cm_error("symlink path is too long for district %s\n", district);
        return -1;
    }

    return 0;
}

int update_latest_symlink(const char *district)
{
    char link_path[PATH_MAX];
    char target[PATH_MAX];
    int written;

    if (ensure_storage_layout() == -1) {
        return -1;
    }
    if (build_latest_link_path(district, link_path, sizeof(link_path)) == -1) {
        return -1;
    }

    written = snprintf(target, sizeof(target), "../reports/%s.bin", district);
    if (written < 0 || (size_t)written >= sizeof(target)) {
        cm_error("symlink target is too long for district %s\n", district);
        return -1;
    }

    if (unlink(link_path) == -1 && errno != ENOENT) {
        cm_errno(link_path);
        return -1;
    }

    if (symlink(target, link_path) == -1) {
        cm_errno(link_path);
        return -1;
    }

    return 0;
}

void format_mode(mode_t mode, char *buf, size_t buflen)
{
    if (buflen < 11) {
        return;
    }

    buf[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : '-';
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
}

int print_file_metadata(const char *district)
{
    char report_path[PATH_MAX];
    char link_path[PATH_MAX];
    char mode_buf[11];
    struct stat st;
    struct stat lst;
    char time_buf[32];
    struct tm tm_value;

    if (build_report_path(district, report_path, sizeof(report_path)) == -1 ||
        build_latest_link_path(district, link_path, sizeof(link_path)) == -1) {
        return -1;
    }

    if (stat(report_path, &st) == -1) {
        cm_errno(report_path);
        return -1;
    }

    format_mode(st.st_mode, mode_buf, sizeof(mode_buf));
    if (localtime_r(&st.st_mtime, &tm_value) == NULL) {
        snprintf(time_buf, sizeof(time_buf), "unknown");
    } else {
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_value);
    }

    cm_writef(STDOUT_FILENO, "Report file: %s\n", report_path);
    cm_writef(STDOUT_FILENO, "Size: %lld bytes\n", (long long)st.st_size);
    cm_writef(STDOUT_FILENO, "Mode: %s (%03o)\n", mode_buf, st.st_mode & 0777);
    cm_writef(STDOUT_FILENO, "Modified: %s\n", time_buf);

    if (lstat(link_path, &lst) == -1) {
        if (errno == ENOENT) {
            cm_write_text(STDOUT_FILENO, "Latest symlink: missing\n");
        } else {
            cm_errno(link_path);
            return -1;
        }
    } else {
        char target[PATH_MAX];
        ssize_t len;

        len = readlink(link_path, target, sizeof(target) - 1);
        if (len == -1) {
            cm_errno(link_path);
            return -1;
        }
        target[len] = '\0';
        format_mode(lst.st_mode, mode_buf, sizeof(mode_buf));
        cm_writef(STDOUT_FILENO, "Latest symlink: %s -> %s\n", link_path, target);
        cm_writef(STDOUT_FILENO, "Symlink mode: %s\n", mode_buf);
    }

    return 0;
}

int parse_u32(const char *text, unsigned int *value)
{
    unsigned long parsed;
    char *end = NULL;

    if (text == NULL || text[0] == '\0') {
        return -1;
    }

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > 4294967295UL) {
        return -1;
    }

    *value = (unsigned int)parsed;
    return 0;
}
