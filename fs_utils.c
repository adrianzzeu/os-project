#include "fs_utils.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int write_default_config(const char *path);
int create_empty_log(const char *path);
int print_stat_entry(const char *label, const char *path, int use_lstat);
int parse_threshold_value(const char *text, int *threshold);
void describe_access(int need_read, int need_write, int need_execute, char *buf, size_t buflen);
int symlink_points_to_existing_file(const char *link_path);

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
    int created = 0;

    if (mkdir(path, mode) == -1) {
        if (errno != EEXIST) {
            cm_errno(path);
            return -1;
        }
    } else {
        created = 1;
    }

    if (stat(path, &st) == -1) {
        cm_errno(path);
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        cm_error("%s exists but is not a directory\n", path);
        return -1;
    }

    if (created && chmod(path, mode) == -1) {
        cm_errno(path);
        return -1;
    }

    return 0;
}

int ensure_storage_layout(void)
{
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

int build_district_dir(const char *district, char *buf, size_t buflen)
{
    int written;

    if (validate_name(district, "district") == -1) {
        return -1;
    }

    written = snprintf(buf, buflen, "%s", district);
    if (written < 0 || (size_t)written >= buflen) {
        cm_error("district path is too long for %s\n", district);
        return -1;
    }

    return 0;
}

int build_report_path(const char *district, char *buf, size_t buflen)
{
    int written;
    char dir[PATH_MAX];

    if (build_district_dir(district, dir, sizeof(dir)) == -1) {
        return -1;
    }

    written = snprintf(buf, buflen, "%s/%s", dir, CM_REPORT_FILE);
    if (written < 0 || (size_t)written >= buflen) {
        cm_error("report path is too long for district %s\n", district);
        return -1;
    }

    return 0;
}

int build_config_path(const char *district, char *buf, size_t buflen)
{
    int written;
    char dir[PATH_MAX];

    if (build_district_dir(district, dir, sizeof(dir)) == -1) {
        return -1;
    }

    written = snprintf(buf, buflen, "%s/%s", dir, CM_CONFIG_FILE);
    if (written < 0 || (size_t)written >= buflen) {
        cm_error("config path is too long for district %s\n", district);
        return -1;
    }

    return 0;
}

int build_log_path(const char *district, char *buf, size_t buflen)
{
    int written;
    char dir[PATH_MAX];

    if (build_district_dir(district, dir, sizeof(dir)) == -1) {
        return -1;
    }

    written = snprintf(buf, buflen, "%s/%s", dir, CM_LOG_FILE);
    if (written < 0 || (size_t)written >= buflen) {
        cm_error("log path is too long for district %s\n", district);
        return -1;
    }

    return 0;
}

int build_latest_link_path(const char *district, char *buf, size_t buflen)
{
    int written;
    char dir[PATH_MAX];

    if (build_district_dir(district, dir, sizeof(dir)) == -1) {
        return -1;
    }

    written = snprintf(buf, buflen, "%s/%s", dir, CM_LATEST_LINK);
    if (written < 0 || (size_t)written >= buflen) {
        cm_error("symlink path is too long for district %s\n", district);
        return -1;
    }

    return 0;
}

int build_active_link_path(const char *district, char *buf, size_t buflen)
{
    int written;

    if (validate_name(district, "district") == -1) {
        return -1;
    }

    written = snprintf(buf, buflen, "%s%s", CM_ACTIVE_LINK_PREFIX, district);
    if (written < 0 || (size_t)written >= buflen) {
        cm_error("active reports symlink path is too long for district %s\n", district);
        return -1;
    }

    return 0;
}

int write_default_config(const char *path)
{
    int fd;
    char buffer[128];
    int written;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, CM_CONFIG_MODE);
    if (fd == -1) {
        cm_errno(path);
        return -1;
    }

    if (fchmod(fd, CM_CONFIG_MODE) == -1) {
        cm_errno(path);
        close(fd);
        return -1;
    }

    written = snprintf(buffer,
                       sizeof(buffer),
                       "severity_threshold=%d\n",
                       CM_DEFAULT_THRESHOLD);
    if (written < 0 || (size_t)written >= sizeof(buffer) ||
        cm_write_all(fd, buffer, (size_t)written) == -1 ||
        fsync(fd) == -1) {
        cm_errno(path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int create_empty_log(const char *path)
{
    struct stat st;
    int existed = 0;
    int fd;

    if (stat(path, &st) == 0) {
        existed = 1;
    } else if (errno != ENOENT) {
        cm_errno(path);
        return -1;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, CM_LOG_MODE);

    if (fd == -1) {
        cm_errno(path);
        return -1;
    }

    if (!existed && fchmod(fd, CM_LOG_MODE) == -1) {
        cm_errno(path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int ensure_district_layout(const char *district)
{
    char dir[PATH_MAX];
    char cfg_path[PATH_MAX];
    char log_path[PATH_MAX];
    struct stat st;

    if (build_district_dir(district, dir, sizeof(dir)) == -1) {
        return -1;
    }

    if (ensure_directory(dir, CM_DISTRICT_MODE) == -1 ||
        build_config_path(district, cfg_path, sizeof(cfg_path)) == -1 ||
        build_log_path(district, log_path, sizeof(log_path)) == -1) {
        return -1;
    }

    if (stat(cfg_path, &st) == -1) {
        if (errno != ENOENT) {
            cm_errno(cfg_path);
            return -1;
        }
        if (write_default_config(cfg_path) == -1) {
            return -1;
        }
    } else if (!S_ISREG(st.st_mode)) {
        cm_error("%s exists but is not a regular file\n", cfg_path);
        return -1;
    }

    if (create_empty_log(log_path) == -1) {
        return -1;
    }

    return 0;
}

void describe_access(int need_read, int need_write, int need_execute, char *buf, size_t buflen)
{
    snprintf(buf,
             buflen,
             "%s%s%s",
             need_read ? "read" : "",
             need_write ? (need_read ? "+write" : "write") : "",
             need_execute ? ((need_read || need_write) ? "+execute" : "execute") : "");
}

int role_has_access(mode_t mode,
                    const char *role,
                    int need_read,
                    int need_write,
                    int need_execute)
{
    int shift;
    mode_t bits;

    if (strcmp(role, "manager") == 0) {
        shift = 6;
    } else if (strcmp(role, "inspector") == 0) {
        shift = 3;
    } else {
        return 0;
    }

    bits = (mode >> shift) & 07;
    if (need_read && !(bits & 04)) {
        return 0;
    }
    if (need_write && !(bits & 02)) {
        return 0;
    }
    if (need_execute && !(bits & 01)) {
        return 0;
    }

    return 1;
}

int check_role_access(const char *path,
                      const char *role,
                      int need_read,
                      int need_write,
                      int need_execute)
{
    struct stat st;
    char mode_buf[11];
    char access_buf[32];

    if (stat(path, &st) == -1) {
        cm_errno(path);
        return -1;
    }

    if (role_has_access(st.st_mode, role, need_read, need_write, need_execute)) {
        return 0;
    }

    format_mode(st.st_mode, mode_buf, sizeof(mode_buf));
    describe_access(need_read, need_write, need_execute, access_buf, sizeof(access_buf));
    cm_error("permission denied for role=%s on %s: need %s, current mode %s (%03o)\n",
             role,
             path,
             access_buf,
             mode_buf,
             st.st_mode & 0777);
    return -1;
}

int check_exact_permissions(const char *path, mode_t expected, const char *label)
{
    struct stat st;
    char current[11];
    char wanted[11];

    if (stat(path, &st) == -1) {
        cm_errno(path);
        return -1;
    }

    if ((st.st_mode & 0777) == expected) {
        return 0;
    }

    format_mode(st.st_mode, current, sizeof(current));
    format_mode(expected, wanted, sizeof(wanted));
    cm_error("%s permission mismatch on %s: expected %s (%03o), found %s (%03o)\n",
             label,
             path,
             wanted + 1,
             expected,
             current,
             st.st_mode & 0777);
    return -1;
}

int update_latest_symlink(const char *district)
{
    char link_path[PATH_MAX];

    if (build_latest_link_path(district, link_path, sizeof(link_path)) == -1) {
        return -1;
    }

    if (unlink(link_path) == -1 && errno != ENOENT) {
        cm_errno(link_path);
        return -1;
    }

    if (symlink(CM_REPORT_FILE, link_path) == -1) {
        cm_errno(link_path);
        return -1;
    }

    if (update_active_report_symlink(district) == -1) {
        return -1;
    }

    return 0;
}

int symlink_points_to_existing_file(const char *link_path)
{
    struct stat st;

    if (stat(link_path, &st) == -1) {
        if (errno == ENOENT) {
            return 0;
        }
        cm_errno(link_path);
        return -1;
    }

    return S_ISREG(st.st_mode) ? 1 : 0;
}

int update_active_report_symlink(const char *district)
{
    char link_path[PATH_MAX];
    char target[PATH_MAX];
    struct stat st;
    int written;

    if (build_active_link_path(district, link_path, sizeof(link_path)) == -1) {
        return -1;
    }

    written = snprintf(target,
                       sizeof(target),
                       "%s/%s",
                       district,
                       CM_REPORT_FILE);
    if (written < 0 || (size_t)written >= sizeof(target)) {
        cm_error("active reports symlink target is too long for district %s\n", district);
        return -1;
    }

    if (lstat(link_path, &st) == -1) {
        if (errno != ENOENT) {
            cm_errno(link_path);
            return -1;
        }
    } else if (!S_ISLNK(st.st_mode)) {
        cm_error("%s exists but is not a symbolic link\n", link_path);
        return -1;
    } else {
        int target_ok = symlink_points_to_existing_file(link_path);
        if (target_ok == 0) {
            cm_error("warning: dangling symlink detected and replaced: %s\n", link_path);
        } else if (target_ok == -1) {
            return -1;
        }

        if (unlink(link_path) == -1) {
            cm_errno(link_path);
            return -1;
        }
    }

    if (symlink(target, link_path) == -1) {
        cm_errno(link_path);
        return -1;
    }

    return 0;
}

int parse_threshold_value(const char *text, int *threshold)
{
    long parsed;
    char *end = NULL;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || parsed < 1 || parsed > 3) {
        return -1;
    }

    *threshold = (int)parsed;
    return 0;
}

int read_severity_threshold(const char *district, const char *role, int *threshold)
{
    char cfg_path[PATH_MAX];
    char buffer[256];
    ssize_t nread;
    int fd;
    char *value;

    *threshold = CM_DEFAULT_THRESHOLD;

    if (ensure_district_layout(district) == -1 ||
        build_config_path(district, cfg_path, sizeof(cfg_path)) == -1) {
        return -1;
    }

    if (check_role_access(cfg_path, role, 1, 0, 0) == -1) {
        return -1;
    }

    fd = open(cfg_path, O_RDONLY);
    if (fd == -1) {
        cm_errno(cfg_path);
        return -1;
    }

    nread = read(fd, buffer, sizeof(buffer) - 1);
    if (nread == -1) {
        cm_errno(cfg_path);
        close(fd);
        return -1;
    }
    buffer[nread] = '\0';
    close(fd);

    value = strstr(buffer, "severity_threshold=");
    if (value == NULL) {
        return 0;
    }

    value += strlen("severity_threshold=");
    if (parse_threshold_value(value, threshold) == -1) {
        cm_error("%s has invalid severity_threshold; expected 1, 2, or 3\n",
                 cfg_path);
        return -1;
    }

    return 0;
}

int write_severity_threshold(const char *district, const char *role, int threshold)
{
    char cfg_path[PATH_MAX];
    int fd;
    char buffer[128];
    int written;

    if (threshold < 1 || threshold > 3) {
        cm_error("severity threshold must be 1, 2, or 3\n");
        return -1;
    }

    if (ensure_district_layout(district) == -1 ||
        build_config_path(district, cfg_path, sizeof(cfg_path)) == -1) {
        return -1;
    }

    if (check_exact_permissions(cfg_path,
                                CM_CONFIG_MODE,
                                "district.cfg") == -1 ||
        check_role_access(cfg_path, role, 1, 1, 0) == -1) {
        return -1;
    }

    fd = open(cfg_path, O_WRONLY | O_TRUNC, CM_CONFIG_MODE);
    if (fd == -1) {
        cm_errno(cfg_path);
        return -1;
    }

    written = snprintf(buffer, sizeof(buffer), "severity_threshold=%d\n", threshold);
    if (written < 0 || (size_t)written >= sizeof(buffer) ||
        cm_write_all(fd, buffer, (size_t)written) == -1 ||
        fsync(fd) == -1) {
        cm_errno(cfg_path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int append_district_log(const char *district,
                        const char *role,
                        const char *user,
                        const char *action)
{
    char log_path[PATH_MAX];
    time_t now = time(NULL);
    int fd;

    if (ensure_district_layout(district) == -1 ||
        build_log_path(district, log_path, sizeof(log_path)) == -1) {
        return -1;
    }

    if (check_role_access(log_path, role, 0, 1, 0) == -1) {
        cm_error("operation log write refused for role=%s\n", role);
        return -1;
    }

    fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, CM_LOG_MODE);
    if (fd == -1) {
        cm_errno(log_path);
        return -1;
    }

    if (cm_writef(fd,
                  "%lld %s %s %s\n",
                  (long long)now,
                  user == NULL ? "unknown" : user,
                  role == NULL ? "unknown" : role,
                  action == NULL ? "unknown" : action) == -1 ||
        fsync(fd) == -1) {
        cm_errno(log_path);
        close(fd);
        return -1;
    }

    close(fd);
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

int print_report_file_info(const char *district)
{
    char report_path[PATH_MAX];
    struct stat st;
    char mode_buf[11];
    char time_buf[32];
    struct tm tm_value;

    if (build_report_path(district, report_path, sizeof(report_path)) == -1) {
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

    cm_writef(STDOUT_FILENO,
              "reports.dat: permissions=%s size=%lld modified=%s\n",
              mode_buf + 1,
              (long long)st.st_size,
              time_buf);
    return 0;
}

int print_active_report_link_metadata(const char *district)
{
    char link_path[PATH_MAX];
    char target[PATH_MAX];
    struct stat lst;
    ssize_t len;

    if (build_active_link_path(district, link_path, sizeof(link_path)) == -1) {
        return -1;
    }

    if (lstat(link_path, &lst) == -1) {
        if (errno == ENOENT) {
            cm_writef(STDOUT_FILENO,
                      "Active reports symlink: missing (%s)\n",
                      link_path);
            return 0;
        }
        cm_errno(link_path);
        return -1;
    }

    if (!S_ISLNK(lst.st_mode)) {
        cm_writef(STDOUT_FILENO,
                  "Active reports symlink: %s exists but is not a symlink\n",
                  link_path);
        return 0;
    }

    len = readlink(link_path, target, sizeof(target) - 1);
    if (len == -1) {
        cm_errno(link_path);
        return -1;
    }
    target[len] = '\0';

    cm_writef(STDOUT_FILENO,
              "Active reports symlink: %s -> %s\n",
              link_path,
              target);

    int target_ok = symlink_points_to_existing_file(link_path);
    if (target_ok == 0) {
        cm_writef(STDOUT_FILENO,
                  "warning: dangling symlink detected: %s\n",
                  link_path);
    } else if (target_ok == -1) {
        return -1;
    }

    return 0;
}

int print_stat_entry(const char *label, const char *path, int use_lstat)
{
    struct stat st;
    char mode_buf[11];
    char time_buf[32];
    struct tm tm_value;

    if ((use_lstat ? lstat(path, &st) : stat(path, &st)) == -1) {
        if (errno == ENOENT) {
            cm_writef(STDOUT_FILENO, "%s: missing (%s)\n", label, path);
            return 0;
        }
        cm_errno(path);
        return -1;
    }

    format_mode(st.st_mode, mode_buf, sizeof(mode_buf));
    if (localtime_r(&st.st_mtime, &tm_value) == NULL) {
        snprintf(time_buf, sizeof(time_buf), "unknown");
    } else {
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_value);
    }

    cm_writef(STDOUT_FILENO,
              "%s: %s | size=%lld | mode=%s (%03o) | modified=%s\n",
              label,
              path,
              (long long)st.st_size,
              mode_buf,
              st.st_mode & 0777,
              time_buf);
    return 0;
}

int print_file_metadata(const char *district)
{
    char dir[PATH_MAX];
    char report_path[PATH_MAX];
    char cfg_path[PATH_MAX];
    char log_path[PATH_MAX];
    char link_path[PATH_MAX];
    char target[PATH_MAX];
    ssize_t len;

    if (build_district_dir(district, dir, sizeof(dir)) == -1 ||
        build_report_path(district, report_path, sizeof(report_path)) == -1 ||
        build_config_path(district, cfg_path, sizeof(cfg_path)) == -1 ||
        build_log_path(district, log_path, sizeof(log_path)) == -1 ||
        build_latest_link_path(district, link_path, sizeof(link_path)) == -1) {
        return -1;
    }

    if (print_stat_entry("District directory", dir, 0) == -1 ||
        print_stat_entry("Binary reports", report_path, 0) == -1 ||
        print_stat_entry("Config", cfg_path, 0) == -1 ||
        print_stat_entry("Operation log", log_path, 0) == -1 ||
        print_stat_entry("Report symlink", link_path, 1) == -1 ||
        print_active_report_link_metadata(district) == -1) {
        return -1;
    }

    len = readlink(link_path, target, sizeof(target) - 1);
    if (len != -1) {
        target[len] = '\0';
        cm_writef(STDOUT_FILENO, "Report symlink target: %s\n", target);
    } else if (errno != ENOENT) {
        cm_errno(link_path);
        return -1;
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
