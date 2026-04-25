#ifndef REPORT_H
#define REPORT_H

#include <stdint.h>

#define REPORT_TITLE_LEN 96
#define REPORT_DESCRIPTION_LEN 320
#define REPORT_STATUS_LEN 24
#define REPORT_DISTRICT_LEN 64

typedef struct ReportInput {
    char title[REPORT_TITLE_LEN];
    char description[REPORT_DESCRIPTION_LEN];
    char status[REPORT_STATUS_LEN];
    int severity;
} ReportInput;

typedef struct ReportFilter {
    int active;          /* -1 means all records. */
    int min_severity;    /* -1 means no minimum. */
    int max_severity;    /* -1 means no maximum. */
    unsigned int id;     /* 0 means no id filter. */
    char status[REPORT_STATUS_LEN];
    char text[REPORT_DESCRIPTION_LEN];
} ReportFilter;

void report_input_defaults(ReportInput *input);
void report_filter_defaults(ReportFilter *filter);
int parse_filter_expression(const char *expression, ReportFilter *filter);

int add_report(const char *district, const ReportInput *input);
int remove_report(const char *district, unsigned int id);
int list_reports(const char *district, const ReportFilter *filter);
int show_report(const char *district, unsigned int id);

#endif
