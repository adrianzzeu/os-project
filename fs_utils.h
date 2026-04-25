#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <stddef.h>
#include <sys/stat.h>

#define CM_REPORT_FILE "reports.dat"
#define CM_CONFIG_FILE "district.cfg"
#define CM_LOG_FILE "logged_district"
#define CM_LATEST_LINK "latest_report"
#define CM_ACTIVE_LINK_PREFIX "active_reports-"
#define CM_DEFAULT_THRESHOLD 3
#define CM_DISTRICT_MODE 0750
#define CM_REPORT_MODE 0664
#define CM_CONFIG_MODE 0640
#define CM_LOG_MODE 0644

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
int build_active_link_path(const char *district, char *buf, size_t buflen);
int update_latest_symlink(const char *district);
int update_active_report_symlink(const char *district);
int print_active_report_link_metadata(const char *district);
int role_has_access(mode_t mode,
                    const char *role,
                    int need_read,
                    int need_write,
                    int need_execute);
int check_role_access(const char *path,
                      const char *role,
                      int need_read,
                      int need_write,
                      int need_execute);
int check_exact_permissions(const char *path, mode_t expected, const char *label);
int print_report_file_info(const char *district);
int read_severity_threshold(const char *district, const char *role, int *threshold);
int write_severity_threshold(const char *district, const char *role, int threshold);
int append_district_log(const char *district,
                        const char *role,
                        const char *user,
                        const char *action);
int print_file_metadata(const char *district);
int parse_u32(const char *text, unsigned int *value);
void format_mode(mode_t mode, char *buf, size_t buflen);

#endif
