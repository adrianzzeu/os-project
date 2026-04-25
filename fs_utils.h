#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <stddef.h>
#include <sys/stat.h>

#define CM_DATA_DIR "data"
#define CM_REPORT_FILE "reports.dat"
#define CM_CONFIG_FILE "district.cfg"
#define CM_LOG_FILE "logged_district"
#define CM_LATEST_LINK "latest_report"
#define CM_DEFAULT_THRESHOLD 3

int cm_write_all(int fd, const char *buf, size_t count);
int cm_writef(int fd, const char *format, ...);
void cm_write_text(int fd, const char *text);
void cm_error(const char *format, ...);
void cm_errno(const char *context);

int ensure_directory(const char *path, mode_t mode);
int ensure_storage_layout(void);
int ensure_district_layout(const char *district);
int validate_name(const char *name, const char *label);
int build_district_dir(const char *district, char *buf, size_t buflen);
int build_report_path(const char *district, char *buf, size_t buflen);
int build_config_path(const char *district, char *buf, size_t buflen);
int build_log_path(const char *district, char *buf, size_t buflen);
int build_latest_link_path(const char *district, char *buf, size_t buflen);
int update_latest_symlink(const char *district);
int read_severity_threshold(const char *district, int *threshold);
int write_severity_threshold(const char *district, int threshold);
int append_district_log(const char *district,
                        const char *role,
                        const char *user,
                        const char *action);
int print_file_metadata(const char *district);
int parse_u32(const char *text, unsigned int *value);
void format_mode(mode_t mode, char *buf, size_t buflen);

#endif
