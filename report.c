#include "report.h"

#include "fs_utils.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define REPORT_STORE_VERSION 1U
#define REPORT_MAGIC "CMRPT01"

typedef struct ReportStoreHeader {
    char magic[8];
    uint32_t version;
    uint32_t record_size;
    uint32_t next_id;
    uint32_t reserved;
} ReportStoreHeader;

typedef struct ReportRecord {
    uint32_t id;
    char district[REPORT_DISTRICT_LEN];
    char title[REPORT_TITLE_LEN];
    char description[REPORT_DESCRIPTION_LEN];
    char status[REPORT_STATUS_LEN];
    int32_t severity;
    int64_t created_at;
    int64_t updated_at;
    uint8_t active;
    uint8_t reserved[7];
} ReportRecord;

int write_all_at(int fd, const void *buf, size_t count, off_t offset);
int read_all_at(int fd, void *buf, size_t count, off_t offset);
int open_store(const char *district, int create, int *fd, ReportStoreHeader *header);
void copy_field(char *dest, size_t dest_size, const char *src);
void format_timestamp(int64_t timestamp, char *buf, size_t buflen);
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
    int flags = O_RDWR;

    if (ensure_storage_layout() == -1 ||
        build_report_path(district, path, sizeof(path)) == -1) {
        return -1;
    }

    if (create) {
        flags |= O_CREAT;
    }

    *fd = open(path, flags, 0640);
    if (*fd == -1) {
        perror(path);
        return -1;
    }

    if (fchmod(*fd, 0640) == -1) {
        perror(path);
        close(*fd);
        return -1;
    }

    if (fstat(*fd, &st) == -1) {
        perror(path);
        close(*fd);
        return -1;
    }

    if (st.st_size == 0) {
        if (!create) {
            fprintf(stderr, "report store for %s does not exist\n", district);
            close(*fd);
            return -1;
        }

        memset(header, 0, sizeof(*header));
        memcpy(header->magic, REPORT_MAGIC, sizeof(header->magic));
        header->version = REPORT_STORE_VERSION;
        header->record_size = (uint32_t)sizeof(ReportRecord);
        header->next_id = 1;

        if (write_all_at(*fd, header, sizeof(*header), 0) == -1) {
            perror(path);
            close(*fd);
            return -1;
        }
    } else {
        if ((size_t)st.st_size < sizeof(*header)) {
            fprintf(stderr, "%s is too small to be a report store\n", path);
            close(*fd);
            return -1;
        }
        if (read_all_at(*fd, header, sizeof(*header), 0) == -1) {
            perror(path);
            close(*fd);
            return -1;
        }
        if (memcmp(header->magic, REPORT_MAGIC, sizeof(header->magic)) != 0 ||
            header->version != REPORT_STORE_VERSION ||
            header->record_size != sizeof(ReportRecord)) {
            fprintf(stderr, "%s has an incompatible report-store format\n", path);
            close(*fd);
            return -1;
        }
    }

    if (update_latest_symlink(district) == -1) {
        close(*fd);
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

void format_timestamp(int64_t timestamp, char *buf, size_t buflen)
{
    time_t raw = (time_t)timestamp;
    struct tm tm_value;

    if (localtime_r(&raw, &tm_value) == NULL) {
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
    if (filter->status[0] != '\0' && strcmp(record->status, filter->status) != 0) {
        return 0;
    }
    if (filter->text[0] != '\0' &&
        !contains_case_insensitive(record->title, filter->text) &&
        !contains_case_insensitive(record->description, filter->text) &&
        !contains_case_insensitive(record->district, filter->text)) {
        return 0;
    }

    return 1;
}

void report_input_defaults(ReportInput *input)
{
    memset(input, 0, sizeof(*input));
    copy_field(input->title, sizeof(input->title), "Infrastructure inspection");
    copy_field(input->description,
               sizeof(input->description),
               "Report created from command line.");
    copy_field(input->status, sizeof(input->status), "open");
    input->severity = 3;
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

    for (token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ",")) {
        char *part = trim(token);
        int value;

        if (strncmp(part, "severity>=", 10) == 0) {
            if (parse_int_value(part + 10, &value) == -1) {
                fprintf(stderr, "invalid severity filter: %s\n", part);
                return -1;
            }
            filter->min_severity = value;
        } else if (strncmp(part, "severity<=", 10) == 0) {
            if (parse_int_value(part + 10, &value) == -1) {
                fprintf(stderr, "invalid severity filter: %s\n", part);
                return -1;
            }
            filter->max_severity = value;
        } else if (strncmp(part, "severity=", 9) == 0) {
            if (parse_int_value(part + 9, &value) == -1) {
                fprintf(stderr, "invalid severity filter: %s\n", part);
                return -1;
            }
            filter->min_severity = value;
            filter->max_severity = value;
        } else if (strncmp(part, "status=", 7) == 0) {
            copy_field(filter->status, sizeof(filter->status), part + 7);
        } else if (strncmp(part, "text=", 5) == 0) {
            copy_field(filter->text, sizeof(filter->text), part + 5);
        } else if (strncmp(part, "id=", 3) == 0) {
            unsigned int parsed;
            if (parse_u32(part + 3, &parsed) == -1) {
                fprintf(stderr, "invalid id filter: %s\n", part);
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
                fprintf(stderr, "invalid active filter: %s\n", part);
                return -1;
            }
        } else {
            fprintf(stderr, "unknown filter term: %s\n", part);
            return -1;
        }
    }

    return 0;
}

int add_report(const char *district, const ReportInput *input)
{
    int fd;
    ReportStoreHeader header;
    ReportRecord record;
    off_t offset;
    char created[32];

    if (open_store(district, 1, &fd, &header) == -1) {
        return -1;
    }

    memset(&record, 0, sizeof(record));
    record.id = header.next_id++;
    copy_field(record.district, sizeof(record.district), district);
    copy_field(record.title, sizeof(record.title), input->title);
    copy_field(record.description, sizeof(record.description), input->description);
    copy_field(record.status, sizeof(record.status), input->status);
    record.severity = input->severity;
    record.created_at = (int64_t)time(NULL);
    record.updated_at = record.created_at;
    record.active = 1;

    offset = lseek(fd, 0, SEEK_END);
    if (offset == (off_t)-1) {
        perror("lseek");
        close(fd);
        return -1;
    }

    if (write_all_at(fd, &record, sizeof(record), offset) == -1 ||
        write_all_at(fd, &header, sizeof(header), 0) == -1 ||
        fsync(fd) == -1) {
        perror("write report");
        close(fd);
        return -1;
    }

    close(fd);

    format_timestamp(record.created_at, created, sizeof(created));
    printf("Added report %u for %s\n", record.id, district);
    printf("Status: %s | Severity: %d | Created: %s\n",
           record.status,
           record.severity,
           created);
    return 0;
}

int remove_report(const char *district, unsigned int id)
{
    int fd;
    ReportStoreHeader header;
    ReportRecord record;
    off_t offset;
    int found = 0;

    if (open_store(district, 0, &fd, &header) == -1) {
        return -1;
    }

    for (offset = (off_t)sizeof(header);; offset += (off_t)sizeof(record)) {
        ssize_t nread = pread(fd, &record, sizeof(record), offset);

        if (nread == -1) {
            if (errno == EINTR) {
                offset -= (off_t)sizeof(record);
                continue;
            }
            perror("read report");
            close(fd);
            return -1;
        }
        if (nread == 0) {
            break;
        }
        if ((size_t)nread != sizeof(record)) {
            fprintf(stderr, "report store ended with a partial record\n");
            close(fd);
            return -1;
        }

        if (record.id == id) {
            found = 1;
            if (!record.active) {
                printf("Report %u in %s is already removed\n", id, district);
                close(fd);
                return 0;
            }

            record.active = 0;
            record.updated_at = (int64_t)time(NULL);

            if (write_all_at(fd, &record, sizeof(record), offset) == -1 ||
                fsync(fd) == -1) {
                perror("remove report");
                close(fd);
                return -1;
            }

            printf("Removed report %u from %s\n", id, district);
            close(fd);
            return 0;
        }
    }

    close(fd);

    if (!found) {
        fprintf(stderr, "report %u was not found in %s\n", id, district);
        return -1;
    }

    return 0;
}

int list_reports(const char *district, const ReportFilter *filter)
{
    int fd;
    ReportStoreHeader header;
    ReportRecord record;
    off_t offset;
    int matches = 0;

    if (open_store(district, 0, &fd, &header) == -1) {
        return -1;
    }

    printf("%-5s %-8s %-7s %-19s %s\n", "ID", "Severity", "Active", "Created", "Title");
    for (offset = (off_t)sizeof(header);; offset += (off_t)sizeof(record)) {
        ssize_t nread = pread(fd, &record, sizeof(record), offset);
        char created[32];

        if (nread == -1) {
            if (errno == EINTR) {
                offset -= (off_t)sizeof(record);
                continue;
            }
            perror("read report");
            close(fd);
            return -1;
        }
        if (nread == 0) {
            break;
        }
        if ((size_t)nread != sizeof(record)) {
            fprintf(stderr, "report store ended with a partial record\n");
            close(fd);
            return -1;
        }

        if (!record_matches_filter(&record, filter)) {
            continue;
        }

        format_timestamp(record.created_at, created, sizeof(created));
        printf("%-5u %-8d %-7s %-19s %s\n",
               record.id,
               record.severity,
               record.active ? "yes" : "no",
               created,
               record.title);
        matches++;
    }

    close(fd);
    printf("%d report(s)\n", matches);
    return 0;
}

int show_report(const char *district, unsigned int id)
{
    ReportFilter filter;
    int fd;
    ReportStoreHeader header;
    ReportRecord record;
    off_t offset;

    report_filter_defaults(&filter);
    filter.active = -1;
    filter.id = id;

    if (open_store(district, 0, &fd, &header) == -1) {
        return -1;
    }

    for (offset = (off_t)sizeof(header);; offset += (off_t)sizeof(record)) {
        ssize_t nread = pread(fd, &record, sizeof(record), offset);

        if (nread == -1) {
            if (errno == EINTR) {
                offset -= (off_t)sizeof(record);
                continue;
            }
            perror("read report");
            close(fd);
            return -1;
        }
        if (nread == 0) {
            break;
        }
        if ((size_t)nread != sizeof(record)) {
            fprintf(stderr, "report store ended with a partial record\n");
            close(fd);
            return -1;
        }

        if (record_matches_filter(&record, &filter)) {
            char created[32];
            char updated[32];

            format_timestamp(record.created_at, created, sizeof(created));
            format_timestamp(record.updated_at, updated, sizeof(updated));
            printf("ID: %u\n", record.id);
            printf("District: %s\n", record.district);
            printf("Title: %s\n", record.title);
            printf("Description: %s\n", record.description);
            printf("Status: %s\n", record.status);
            printf("Severity: %d\n", record.severity);
            printf("Active: %s\n", record.active ? "yes" : "no");
            printf("Created: %s\n", created);
            printf("Updated: %s\n", updated);
            close(fd);
            return 0;
        }
    }

    close(fd);
    fprintf(stderr, "report %u was not found in %s\n", id, district);
    return -1;
}
