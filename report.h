#ifndef REPORT_H
#define REPORT_H

#include <stdint.h>

#define REPORT_USER_LEN 64
#define REPORT_CATEGORY_LEN 48
#define REPORT_DESCRIPTION_LEN 320

typedef struct ReportInput {
    char inspector[REPORT_USER_LEN];
    double latitude;
    double longitude;
    char category[REPORT_CATEGORY_LEN];
    int severity;
    char description[REPORT_DESCRIPTION_LEN];
} ReportInput;

typedef struct ReportFilter {
    int active;          /* -1 means all records. */
    int min_severity;    /* -1 means no minimum. */
    int max_severity;    /* -1 means no maximum. */
    unsigned int id;     /* 0 means no id filter. */
    char inspector[REPORT_USER_LEN];
    char category[REPORT_CATEGORY_LEN];
    char text[REPORT_DESCRIPTION_LEN];
} ReportFilter;

void report_input_defaults(ReportInput *input);
void report_filter_defaults(ReportFilter *filter);
int parse_filter_expression(const char *expression, ReportFilter *filter);

int add_report(const char *district,
               const ReportInput *input,
               const char *role,
               unsigned int *created_id);
int remove_report(const char *district, unsigned int id, const char *role);
int list_reports(const char *district, const ReportFilter *filter, const char *role);
int show_report(const char *district, unsigned int id, const char *role);

#endif
