#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <stddef.h>
#include <sys/stat.h>

#define CM_DATA_DIR "data"
#define CM_REPORT_DIR "data/reports"
#define CM_LATEST_DIR "data/latest"

int ensure_storage_layout(void);
int validate_name(const char *name, const char *label);
int build_report_path(const char *district, char *buf, size_t buflen);
int build_latest_link_path(const char *district, char *buf, size_t buflen);
int update_latest_symlink(const char *district);
int print_file_metadata(const char *district);
int parse_u32(const char *text, unsigned int *value);
void format_mode(mode_t mode, char *buf, size_t buflen);

#endif
