#include "report.h"

#include "fs_utils.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define REPORT_STORE_VERSION 2U
#define REPORT_MAGIC "CMRPT02"

typedef struct ReportStoreHeader {
    char magic[8];
    uint32_t version;
    uint32_t record_size;
    uint32_t next_id;
    uint32_t reserved;
} ReportStoreHeader;

typedef struct ReportRecord {
    uint32_t id;
    char inspector[REPORT_USER_LEN];
    double latitude;
    double longitude;
    char category[REPORT_CATEGORY_LEN];
    int32_t severity;
    time_t timestamp;
    char description[REPORT_DESCRIPTION_LEN];
    uint8_t active;
    uint8_t reserved[7];
} ReportRecord;

int write_all_at(int fd, const void *buf, size_t count, off_t offset);
int read_all_at(int fd, void *buf, size_t count, off_t offset);
int open_store(const char *district, int create, int *fd, ReportStoreHeader *header);
int shift_records_left(int fd, off_t remove_offset, off_t end_offset);
void copy_field(char *dest, size_t dest_size, const char *src);
void format_timestamp(time_t timestamp, char *buf, size_t buflen);
char *trim(char *text);
int parse_int_value(const char *text, int *value);
int contains_case_insensitive(const char *haystack, const char *needle);
int record_matches_filter(const ReportRecord *record, const ReportFilter *filter);

int write_all_at(int fd, const void *buf, size_t count, off_t offset)
{
    const char *cursor = (const char *)buf;
    size_t written_total = 0;

    while (written_total < count) {
        ssize_t written = pwrite(fd,
                                 cursor + written_total,
                                 count - written_total,
                                 offset + (off_t)written_total);
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

int read_all_at(int fd, void *buf, size_t count, off_t offset)
{
    char *cursor = (char *)buf;
    size_t read_total = 0;

    while (read_total < count) {
        ssize_t nread = pread(fd,
                              cursor + read_total,
                              count - read_total,
                              offset + (off_t)read_total);
        if (nread == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (nread == 0) {
            errno = EIO;
            return -1;
        }
        read_total += (size_t)nread;
    }

    return 0;
}

int open_store(const char *district, int create, int *fd, ReportStoreHeader *header)
{
    char path[PATH_MAX];
    struct stat st;
    int existed = 0;
    int flags = O_RDWR;

    if (create) {
        if (ensure_district_layout(district) == -1) {
            return -1;
        }
    } else if (ensure_storage_layout() == -1) {
        return -1;
    }

    if (build_report_path(district, path, sizeof(path)) == -1) {
        return -1;
    }

    if (stat(path, &st) == 0) {
        existed = 1;
    } else if (errno != ENOENT) {
        cm_errno(path);
        return -1;
    }

    if (create) {
        flags |= O_CREAT;
    }

    *fd = open(path, flags, CM_REPORT_MODE);
    if (*fd == -1) {
        if (!create && errno == ENOENT) {
            cm_error("report store for %s does not exist\n", district);
        } else {
            cm_errno(path);
        }
        return -1;
    }

    if (!existed && fchmod(*fd, CM_REPORT_MODE) == -1) {
        cm_errno(path);
        close(*fd);
        return -1;
    }

    if (fstat(*fd, &st) == -1) {
        cm_errno(path);
        close(*fd);
        return -1;
    }

    if (st.st_size == 0) {
        if (!create) {
            cm_error("report store for %s is empty\n", district);
            close(*fd);
            return -1;
        }

        memset(header, 0, sizeof(*header));
        memcpy(header->magic, REPORT_MAGIC, sizeof(header->magic));
        header->version = REPORT_STORE_VERSION;
        header->record_size = (uint32_t)sizeof(ReportRecord);
        header->next_id = 1;

        if (write_all_at(*fd, header, sizeof(*header), 0) == -1) {
            cm_errno(path);
            close(*fd);
            return -1;
        }
    } else {
        if ((size_t)st.st_size < sizeof(*header)) {
            cm_error("%s is too small to be a report store\n", path);
            close(*fd);
            return -1;
        }
        if (read_all_at(*fd, header, sizeof(*header), 0) == -1) {
            cm_errno(path);
            close(*fd);
            return -1;
        }
        if (memcmp(header->magic, REPORT_MAGIC, sizeof(header->magic)) != 0 ||
            header->version != REPORT_STORE_VERSION ||
            header->record_size != sizeof(ReportRecord)) {
            cm_error("%s has an incompatible report-store format\n", path);
            close(*fd);
            return -1;
        }
    }

    if (create && update_latest_symlink(district) == -1) {
        close(*fd);
        return -1;
    }

    return 0;
}

int shift_records_left(int fd, off_t remove_offset, off_t end_offset)
{
    ReportRecord record;
    off_t read_offset = remove_offset + (off_t)sizeof(record);
    off_t write_offset = remove_offset;

    while (read_offset < end_offset) {
        if (lseek(fd, read_offset, SEEK_SET) == (off_t)-1) {
            return -1;
        }
        if (read(fd, &record, sizeof(record)) != (ssize_t)sizeof(record)) {
            return -1;
        }
        if (lseek(fd, write_offset, SEEK_SET) == (off_t)-1) {
            return -1;
        }
        if (write(fd, &record, sizeof(record)) != (ssize_t)sizeof(record)) {
            return -1;
        }

        read_offset += (off_t)sizeof(record);
        write_offset += (off_t)sizeof(record);
    }

    if (ftruncate(fd, end_offset - (off_t)sizeof(record)) == -1) {
        return -1;
    }

    return 0;
}

void copy_field(char *dest, size_t dest_size, const char *src)
{
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }

    snprintf(dest, dest_size, "%s", src);
}

void format_timestamp(time_t timestamp, char *buf, size_t buflen)
{
    struct tm tm_value;

    if (localtime_r(&timestamp, &tm_value) == NULL) {
        snprintf(buf, buflen, "unknown");
        return;
    }

    strftime(buf, buflen, "%Y-%m-%d %H:%M:%S", &tm_value);
}

char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text)) {
        text++;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return text;
}

int parse_int_value(const char *text, int *value)
{
    long parsed;
    char *end = NULL;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed < -2147483647L || parsed > 2147483647L) {
        return -1;
    }

    *value = (int)parsed;
    return 0;
}

int contains_case_insensitive(const char *haystack, const char *needle)
{
    size_t needle_len;
    size_t i;

    if (needle[0] == '\0') {
        return 1;
    }

    needle_len = strlen(needle);
    for (i = 0; haystack[i] != '\0'; i++) {
        size_t j = 0;
        while (haystack[i + j] != '\0' && j < needle_len &&
               tolower((unsigned char)haystack[i + j]) ==
                   tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == needle_len) {
            return 1;
        }
    }

    return 0;
}

int record_matches_filter(const ReportRecord *record, const ReportFilter *filter)
{
    if (filter == NULL) {
        return record->active != 0;
    }

    if (filter->active != -1 && (int)record->active != filter->active) {
        return 0;
    }
    if (filter->id != 0 && record->id != filter->id) {
        return 0;
    }
    if (filter->min_severity != -1 && record->severity < filter->min_severity) {
        return 0;
    }
    if (filter->max_severity != -1 && record->severity > filter->max_severity) {
        return 0;
    }
    if (filter->inspector[0] != '\0' &&
        strcmp(record->inspector, filter->inspector) != 0) {
        return 0;
    }
    if (filter->category[0] != '\0' &&
        strcmp(record->category, filter->category) != 0) {
        return 0;
    }
    if (filter->text[0] != '\0' &&
        !contains_case_insensitive(record->description, filter->text) &&
        !contains_case_insensitive(record->category, filter->text) &&
        !contains_case_insensitive(record->inspector, filter->text)) {
        return 0;
    }

    return 1;
}

void report_input_defaults(ReportInput *input)
{
    memset(input, 0, sizeof(*input));
    copy_field(input->inspector, sizeof(input->inspector), "unknown");
    copy_field(input->category, sizeof(input->category), "general");
    copy_field(input->description,
               sizeof(input->description),
               "Report created from command line.");
    input->severity = 1;
    input->latitude = 0.0;
    input->longitude = 0.0;
}

void report_filter_defaults(ReportFilter *filter)
{
    memset(filter, 0, sizeof(*filter));
    filter->active = 1;
    filter->min_severity = -1;
    filter->max_severity = -1;
    filter->id = 0;
}

int parse_filter_expression(const char *expression, ReportFilter *filter)
{
    char buffer[512];
    char *token;

    report_filter_defaults(filter);

    if (expression == NULL || expression[0] == '\0') {
        return 0;
    }

    copy_field(buffer, sizeof(buffer), expression);

    for (token = strtok(buffer, ", "); token != NULL; token = strtok(NULL, ", ")) {
        char *part = trim(token);
        int value;

        if (strncmp(part, "severity:>=", 11) == 0) {
            if (parse_int_value(part + 11, &value) == -1) {
                cm_error("invalid severity filter: %s\n", part);
                return -1;
            }
            filter->min_severity = value;
        } else if (strncmp(part, "severity:<=", 11) == 0) {
            if (parse_int_value(part + 11, &value) == -1) {
                cm_error("invalid severity filter: %s\n", part);
                return -1;
            }
            filter->max_severity = value;
        } else if (strncmp(part, "severity:==", 11) == 0) {
            if (parse_int_value(part + 11, &value) == -1) {
                cm_error("invalid severity filter: %s\n", part);
                return -1;
            }
            filter->min_severity = value;
            filter->max_severity = value;
        } else if (strncmp(part, "category:==", 11) == 0) {
            copy_field(filter->category, sizeof(filter->category), part + 11);
        } else if (strncmp(part, "inspector:==", 12) == 0) {
            copy_field(filter->inspector, sizeof(filter->inspector), part + 12);
        } else if (strncmp(part, "text:==", 7) == 0) {
            copy_field(filter->text, sizeof(filter->text), part + 7);
        } else if (strncmp(part, "severity>=", 10) == 0) {
            if (parse_int_value(part + 10, &value) == -1) {
                cm_error("invalid severity filter: %s\n", part);
                return -1;
            }
            filter->min_severity = value;
        } else if (strncmp(part, "severity<=", 10) == 0) {
            if (parse_int_value(part + 10, &value) == -1) {
                cm_error("invalid severity filter: %s\n", part);
                return -1;
            }
            filter->max_severity = value;
        } else if (strncmp(part, "severity=", 9) == 0) {
            if (parse_int_value(part + 9, &value) == -1) {
                cm_error("invalid severity filter: %s\n", part);
                return -1;
            }
            filter->min_severity = value;
            filter->max_severity = value;
        } else if (strncmp(part, "category=", 9) == 0) {
            copy_field(filter->category, sizeof(filter->category), part + 9);
        } else if (strncmp(part, "inspector=", 10) == 0) {
            copy_field(filter->inspector, sizeof(filter->inspector), part + 10);
        } else if (strncmp(part, "text=", 5) == 0) {
            copy_field(filter->text, sizeof(filter->text), part + 5);
        } else if (strncmp(part, "id=", 3) == 0) {
            unsigned int parsed;
            if (parse_u32(part + 3, &parsed) == -1) {
                cm_error("invalid id filter: %s\n", part);
                return -1;
            }
            filter->id = parsed;
        } else if (strncmp(part, "active=", 7) == 0) {
            const char *active_value = part + 7;
            if (strcmp(active_value, "all") == 0) {
                filter->active = -1;
            } else if (strcmp(active_value, "1") == 0 ||
                       strcmp(active_value, "true") == 0 ||
                       strcmp(active_value, "yes") == 0) {
                filter->active = 1;
            } else if (strcmp(active_value, "0") == 0 ||
                       strcmp(active_value, "false") == 0 ||
                       strcmp(active_value, "no") == 0) {
                filter->active = 0;
            } else {
                cm_error("invalid active filter: %s\n", part);
                return -1;
            }
        } else {
            cm_error("unknown filter term: %s\n", part);
            return -1;
        }
    }

    return 0;
}

int add_report(const char *district,
               const ReportInput *input,
               const char *role,
               unsigned int *created_id)
{
    int fd;
    int threshold;
    ReportStoreHeader header;
    ReportRecord record;
    off_t offset;
    char created[32];
    char report_path[PATH_MAX];
    struct stat st;

    if (read_severity_threshold(district, role, &threshold) == -1 ||
        build_report_path(district, report_path, sizeof(report_path)) == -1) {
        return -1;
    }

    if (stat(report_path, &st) == 0) {
        if (check_role_access(report_path, role, 1, 1, 0) == -1) {
            return -1;
        }
    } else if (errno != ENOENT) {
        cm_errno(report_path);
        return -1;
    }

    if (open_store(district, 1, &fd, &header) == -1) {
        return -1;
    }

    memset(&record, 0, sizeof(record));
    record.id = header.next_id++;
    copy_field(record.inspector, sizeof(record.inspector), input->inspector);
    record.latitude = input->latitude;
    record.longitude = input->longitude;
    copy_field(record.category, sizeof(record.category), input->category);
    record.severity = input->severity;
    record.timestamp = time(NULL);
    copy_field(record.description, sizeof(record.description), input->description);
    record.active = 1;

    offset = lseek(fd, 0, SEEK_END);
    if (offset == (off_t)-1) {
        cm_errno("lseek");
        close(fd);
        return -1;
    }

    if (write_all_at(fd, &record, sizeof(record), offset) == -1 ||
        write_all_at(fd, &header, sizeof(header), 0) == -1 ||
        fsync(fd) == -1) {
        cm_errno("write report");
        close(fd);
        return -1;
    }

    close(fd);

    if (created_id != NULL) {
        *created_id = record.id;
    }

    format_timestamp(record.timestamp, created, sizeof(created));
    cm_writef(STDOUT_FILENO, "Added report %u for %s\n", record.id, district);
    cm_writef(STDOUT_FILENO,
              "Inspector: %s | Category: %s | Severity: %d | GPS: %.6f, %.6f | Created: %s\n",
              record.inspector,
              record.category,
              record.severity,
              record.latitude,
              record.longitude,
              created);

    if (record.severity >= threshold) {
        cm_writef(STDOUT_FILENO,
                  "ESCALATION ALERT: severity %d reached threshold %d for %s\n",
                  record.severity,
                  threshold,
                  district);
    }

    return 0;
}

int remove_report(const char *district, unsigned int id, const char *role)
{
    int fd;
    ReportStoreHeader header;
    ReportRecord record;
    off_t offset;
    off_t end_offset;
    char report_path[PATH_MAX];

    if (build_report_path(district, report_path, sizeof(report_path)) == -1 ||
        check_role_access(report_path, role, 1, 1, 0) == -1) {
        return -1;
    }

    if (open_store(district, 0, &fd, &header) == -1) {
        return -1;
    }

    end_offset = lseek(fd, 0, SEEK_END);
    if (end_offset == (off_t)-1) {
        cm_errno("lseek");
        close(fd);
        return -1;
    }

    for (offset = (off_t)sizeof(header);; offset += (off_t)sizeof(record)) {
        ssize_t nread = pread(fd, &record, sizeof(record), offset);

        if (nread == -1) {
            if (errno == EINTR) {
                offset -= (off_t)sizeof(record);
                continue;
            }
            cm_errno("read report");
            close(fd);
            return -1;
        }
        if (nread == 0) {
            break;
        }
        if ((size_t)nread != sizeof(record)) {
            cm_error("report store ended with a partial record\n");
            close(fd);
            return -1;
        }

        if (record.id == id) {
            if (shift_records_left(fd, offset, end_offset) == -1 ||
                fsync(fd) == -1) {
                cm_errno("remove report");
                close(fd);
                return -1;
            }

            cm_writef(STDOUT_FILENO, "Removed report %u from %s\n", id, district);
            close(fd);
            return 0;
        }
    }

    close(fd);
    cm_error("report %u was not found in %s\n", id, district);
    return -1;
}

int list_reports(const char *district, const ReportFilter *filter, const char *role)
{
    int fd;
    ReportStoreHeader header;
    ReportRecord record;
    off_t offset;
    int matches = 0;

    char report_path[PATH_MAX];

    if (build_report_path(district, report_path, sizeof(report_path)) == -1 ||
        check_role_access(report_path, role, 1, 0, 0) == -1 ||
        print_report_file_info(district) == -1 ||
        open_store(district, 0, &fd, &header) == -1) {
        return -1;
    }

    cm_writef(STDOUT_FILENO,
              "%-5s %-3s %-6s %-19s %-14s %-10s %-11s %-11s %s\n",
              "ID",
              "Sev",
              "Active",
              "Timestamp",
              "Inspector",
              "Category",
              "Latitude",
              "Longitude",
              "Description");
    for (offset = (off_t)sizeof(header);; offset += (off_t)sizeof(record)) {
        ssize_t nread = pread(fd, &record, sizeof(record), offset);
        char timestamp[32];

        if (nread == -1) {
            if (errno == EINTR) {
                offset -= (off_t)sizeof(record);
                continue;
            }
            cm_errno("read report");
            close(fd);
            return -1;
        }
        if (nread == 0) {
            break;
        }
        if ((size_t)nread != sizeof(record)) {
            cm_error("report store ended with a partial record\n");
            close(fd);
            return -1;
        }

        if (!record_matches_filter(&record, filter)) {
            continue;
        }

        format_timestamp(record.timestamp, timestamp, sizeof(timestamp));
        cm_writef(STDOUT_FILENO,
                  "%-5u %-3d %-6s %-19s %-14s %-10s %-11.6f %-11.6f %s\n",
                  record.id,
                  record.severity,
                  record.active ? "yes" : "no",
                  timestamp,
                  record.inspector,
                  record.category,
                  record.latitude,
                  record.longitude,
                  record.description);
        matches++;
    }

    close(fd);
    cm_writef(STDOUT_FILENO, "%d report(s)\n", matches);
    return 0;
}

int show_report(const char *district, unsigned int id, const char *role)
{
    ReportFilter filter;
    int fd;
    ReportStoreHeader header;
    ReportRecord record;
    off_t offset;
    char report_path[PATH_MAX];

    report_filter_defaults(&filter);
    filter.active = -1;
    filter.id = id;

    if (build_report_path(district, report_path, sizeof(report_path)) == -1 ||
        check_role_access(report_path, role, 1, 0, 0) == -1 ||
        open_store(district, 0, &fd, &header) == -1) {
        return -1;
    }

    for (offset = (off_t)sizeof(header);; offset += (off_t)sizeof(record)) {
        ssize_t nread = pread(fd, &record, sizeof(record), offset);

        if (nread == -1) {
            if (errno == EINTR) {
                offset -= (off_t)sizeof(record);
                continue;
            }
            cm_errno("read report");
            close(fd);
            return -1;
        }
        if (nread == 0) {
            break;
        }
        if ((size_t)nread != sizeof(record)) {
            cm_error("report store ended with a partial record\n");
            close(fd);
            return -1;
        }

        if (record_matches_filter(&record, &filter)) {
            char timestamp[32];

            format_timestamp(record.timestamp, timestamp, sizeof(timestamp));
            cm_writef(STDOUT_FILENO, "ID: %u\n", record.id);
            cm_writef(STDOUT_FILENO, "District: %s\n", district);
            cm_writef(STDOUT_FILENO, "Inspector: %s\n", record.inspector);
            cm_writef(STDOUT_FILENO, "GPS: %.6f, %.6f\n", record.latitude, record.longitude);
            cm_writef(STDOUT_FILENO, "Category: %s\n", record.category);
            cm_writef(STDOUT_FILENO, "Severity: %d\n", record.severity);
            cm_writef(STDOUT_FILENO, "Timestamp: %s\n", timestamp);
            cm_writef(STDOUT_FILENO, "Description: %s\n", record.description);
            cm_writef(STDOUT_FILENO, "Active: %s\n", record.active ? "yes" : "no");
            close(fd);
            return 0;
        }
    }

    close(fd);
    cm_error("report %u was not found in %s\n", id, district);
    return -1;
}
