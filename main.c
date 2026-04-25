#include "fs_utils.h"
#include "report.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum Role {
    ROLE_UNKNOWN = 0,
    ROLE_INSPECTOR,
    ROLE_MANAGER
} Role;

typedef enum Command {
    CMD_UNKNOWN = 0,
    CMD_ADD,
    CMD_REMOVE_REPORT,
    CMD_LIST,
    CMD_SHOW,
    CMD_METADATA,
    CMD_SET_THRESHOLD,
    CMD_FILTER
} Command;

void usage(int fd);
Role parse_role(const char *text);
const char *role_name(Role role);
int need_arg(int argc, char **argv, int index, const char *option);
int set_command(Command *command, Command next);
int copy_option(char *dest, size_t dest_size, const char *value, const char *name);
int parse_double_arg(const char *text, double *value);
int parse_severity_arg(const char *text, int *severity);
int authorize(Role role, Command command);
int log_success(const char *district,
                Role role,
                const char *user,
                const char *action);

void usage(int fd)
{
    cm_write_text(fd,
                  "Usage:\n"
                  "  city_manager --role inspector|manager --user USER --add DISTRICT [--lat LAT] [--lon LON] [--category CATEGORY] [--severity 1|2|3] [--description TEXT]\n"
                  "  city_manager --role manager --user USER --remove_report DISTRICT ID\n"
                  "  city_manager --role manager --user USER --update_threshold DISTRICT 1|2|3\n"
                  "  city_manager --role inspector|manager --user USER --list DISTRICT\n"
                  "  city_manager --role inspector|manager --user USER --view DISTRICT ID\n"
                  "  city_manager --role inspector|manager --user USER --filter DISTRICT CONDITION...\n"
                  "  city_manager --role inspector|manager --user USER --metadata DISTRICT\n"
                  "\n"
                  "Each district is stored under data/DISTRICT/ with reports.dat, district.cfg, and logged_district.\n"
                  "Filter examples: 'severity:>=2' 'category:==road' or severity>=2,category=road\n");
}

Role parse_role(const char *text)
{
    if (strcmp(text, "inspector") == 0) {
        return ROLE_INSPECTOR;
    }
    if (strcmp(text, "manager") == 0) {
        return ROLE_MANAGER;
    }
    return ROLE_UNKNOWN;
}

const char *role_name(Role role)
{
    switch (role) {
    case ROLE_INSPECTOR:
        return "inspector";
    case ROLE_MANAGER:
        return "manager";
    default:
        return "unknown";
    }
}

int need_arg(int argc, char **argv, int index, const char *option)
{
    if (index + 1 >= argc) {
        cm_error("%s requires an argument\n", option);
        return -1;
    }
    if (strncmp(argv[index + 1], "--", 2) == 0) {
        cm_error("%s requires an argument\n", option);
        return -1;
    }
    return 0;
}

int set_command(Command *command, Command next)
{
    if (*command != CMD_UNKNOWN) {
        cm_error("only one command may be used per invocation\n");
        return -1;
    }
    *command = next;
    return 0;
}

int copy_option(char *dest, size_t dest_size, const char *value, const char *name)
{
    int written = snprintf(dest, dest_size, "%s", value);
    if (written < 0 || (size_t)written >= dest_size) {
        cm_error("%s is too long\n", name);
        return -1;
    }
    return 0;
}

int parse_double_arg(const char *text, double *value)
{
    char *end = NULL;

    errno = 0;
    *value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0') {
        return -1;
    }

    return 0;
}

int parse_severity_arg(const char *text, int *severity)
{
    unsigned int parsed;

    if (parse_u32(text, &parsed) == -1 || parsed < 1 || parsed > 3) {
        return -1;
    }

    *severity = (int)parsed;
    return 0;
}

int authorize(Role role, Command command)
{
    if (role == ROLE_UNKNOWN) {
        cm_error("a valid --role is required: inspector or manager\n");
        return -1;
    }

    if ((command == CMD_REMOVE_REPORT || command == CMD_SET_THRESHOLD) &&
        role != ROLE_MANAGER) {
        cm_error("permission denied: this command requires role manager\n");
        return -1;
    }

    return 0;
}

int log_success(const char *district,
                Role role,
                const char *user,
                const char *action)
{
    if (append_district_log(district, role_name(role), user, action) == -1) {
        cm_error("operation succeeded, but logged_district was not updated for role=%s\n",
                 role_name(role));
        return 0;
    }

    return 0;
}

int main(int argc, char **argv)
{
    Role role = ROLE_UNKNOWN;
    Command command = CMD_UNKNOWN;
    ReportInput input;
    ReportFilter filter;
    const char *district = NULL;
    const char *filter_expression = NULL;
    char filter_buffer[512] = "";
    char declared_user[REPORT_USER_LEN] = "";
    unsigned int id = 0;
    int threshold = 0;
    int i;
    int result;
    char action[256];
    unsigned int created_id = 0;

    report_input_defaults(&input);
    report_filter_defaults(&filter);

    if (argc == 1) {
        usage(STDERR_FILENO);
        return 2;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(STDOUT_FILENO);
            return 0;
        } else if (strcmp(argv[i], "--role") == 0) {
            if (need_arg(argc, argv, i, "--role") == -1) {
                return 2;
            }
            role = parse_role(argv[++i]);
            if (role == ROLE_UNKNOWN) {
                cm_error("unknown role: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--user") == 0) {
            if (need_arg(argc, argv, i, "--user") == -1) {
                return 2;
            }
            if (validate_name(argv[i + 1], "user") == -1 ||
                copy_option(declared_user,
                            sizeof(declared_user),
                            argv[++i],
                            "--user") == -1) {
                return 2;
            }
        } else if (strcmp(argv[i], "--add") == 0) {
            if (set_command(&command, CMD_ADD) == -1 ||
                need_arg(argc, argv, i, "--add") == -1) {
                return 2;
            }
            district = argv[++i];
        } else if (strcmp(argv[i], "--remove_report") == 0) {
            if (set_command(&command, CMD_REMOVE_REPORT) == -1 ||
                need_arg(argc, argv, i, "--remove_report") == -1) {
                return 2;
            }
            district = argv[++i];
            if (need_arg(argc, argv, i, "--remove_report ID") == -1) {
                return 2;
            }
            if (parse_u32(argv[++i], &id) == -1 || id == 0) {
                cm_error("report id must be a positive integer\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--set_threshold") == 0 ||
                   strcmp(argv[i], "--update_threshold") == 0) {
            if (set_command(&command, CMD_SET_THRESHOLD) == -1 ||
                need_arg(argc, argv, i, argv[i]) == -1) {
                return 2;
            }
            district = argv[++i];
            if (need_arg(argc, argv, i, "--update_threshold VALUE") == -1) {
                return 2;
            }
            if (parse_severity_arg(argv[++i], &threshold) == -1) {
                cm_error("threshold must be 1, 2, or 3\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--list") == 0) {
            if (set_command(&command, CMD_LIST) == -1 ||
                need_arg(argc, argv, i, "--list") == -1) {
                return 2;
            }
            district = argv[++i];
        } else if (strcmp(argv[i], "--show") == 0 ||
                   strcmp(argv[i], "--view") == 0) {
            if (set_command(&command, CMD_SHOW) == -1 ||
                need_arg(argc, argv, i, argv[i]) == -1) {
                return 2;
            }
            district = argv[++i];
            if (need_arg(argc, argv, i, "--view ID") == -1) {
                return 2;
            }
            if (parse_u32(argv[++i], &id) == -1 || id == 0) {
                cm_error("report id must be a positive integer\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--metadata") == 0) {
            if (set_command(&command, CMD_METADATA) == -1 ||
                need_arg(argc, argv, i, "--metadata") == -1) {
                return 2;
            }
            district = argv[++i];
        } else if (strcmp(argv[i], "--lat") == 0) {
            if (need_arg(argc, argv, i, "--lat") == -1 ||
                parse_double_arg(argv[++i], &input.latitude) == -1) {
                cm_error("--lat must be a floating-point number\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--lon") == 0) {
            if (need_arg(argc, argv, i, "--lon") == -1 ||
                parse_double_arg(argv[++i], &input.longitude) == -1) {
                cm_error("--lon must be a floating-point number\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--category") == 0) {
            if (need_arg(argc, argv, i, "--category") == -1) {
                return 2;
            }
            if (validate_name(argv[i + 1], "category") == -1 ||
                copy_option(input.category,
                            sizeof(input.category),
                            argv[++i],
                            "--category") == -1) {
                return 2;
            }
        } else if (strcmp(argv[i], "--description") == 0) {
            if (need_arg(argc, argv, i, "--description") == -1) {
                return 2;
            }
            if (copy_option(input.description,
                            sizeof(input.description),
                            argv[++i],
                            "--description") == -1) {
                return 2;
            }
        } else if (strcmp(argv[i], "--severity") == 0) {
            if (need_arg(argc, argv, i, "--severity") == -1 ||
                parse_severity_arg(argv[++i], &input.severity) == -1) {
                cm_error("--severity must be 1, 2, or 3\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--filter") == 0) {
            if (command == CMD_LIST) {
                if (need_arg(argc, argv, i, "--filter") == -1) {
                    return 2;
                }
                filter_expression = argv[++i];
            } else {
                size_t used = 0;

                if (set_command(&command, CMD_FILTER) == -1 ||
                    need_arg(argc, argv, i, "--filter") == -1) {
                    return 2;
                }
                district = argv[++i];

                while (i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0) {
                    size_t len = strlen(argv[i + 1]);
                    if (used + len + 2 >= sizeof(filter_buffer)) {
                        cm_error("--filter expression is too long\n");
                        return 2;
                    }
                    if (used > 0) {
                        filter_buffer[used++] = ' ';
                    }
                    memcpy(filter_buffer + used, argv[i + 1], len + 1);
                    used += len;
                    i++;
                }
                filter_expression = filter_buffer;
            }
        } else {
            cm_error("unknown option: %s\n", argv[i]);
            usage(STDERR_FILENO);
            return 2;
        }
    }

    if (command == CMD_UNKNOWN || district == NULL) {
        usage(STDERR_FILENO);
        return 2;
    }

    if (validate_name(district, "district") == -1) {
        return 2;
    }

    if (declared_user[0] == '\0') {
        cm_error("--user is required so actions can be written to logged_district\n");
        return 2;
    }

    if (authorize(role, command) == -1) {
        return 1;
    }

    if (command == CMD_ADD) {
        if (copy_option(input.inspector,
                        sizeof(input.inspector),
                        declared_user,
                        "--user") == -1) {
            return 2;
        }
    }

    if (filter_expression != NULL &&
        parse_filter_expression(filter_expression, &filter) == -1) {
        return 2;
    }

    cm_writef(STDOUT_FILENO, "Role: %s | User: %s\n", role_name(role), declared_user);

    switch (command) {
    case CMD_ADD:
        result = add_report(district, &input, role_name(role), &created_id);
        if (result == 0) {
            snprintf(action, sizeof(action), "add report_id=%u", created_id);
        }
        break;
    case CMD_REMOVE_REPORT:
        result = remove_report(district, id, role_name(role));
        if (result == 0) {
            snprintf(action, sizeof(action), "remove_report report_id=%u", id);
        }
        break;
    case CMD_LIST:
        result = list_reports(district, &filter, role_name(role));
        if (result == 0) {
            snprintf(action, sizeof(action), "list");
        }
        break;
    case CMD_FILTER:
        result = list_reports(district, &filter, role_name(role));
        if (result == 0) {
            snprintf(action, sizeof(action), "filter");
        }
        break;
    case CMD_SHOW:
        result = show_report(district, id, role_name(role));
        if (result == 0) {
            snprintf(action, sizeof(action), "show report_id=%u", id);
        }
        break;
    case CMD_METADATA:
        result = print_file_metadata(district);
        if (result == 0) {
            snprintf(action, sizeof(action), "metadata");
        }
        break;
    case CMD_SET_THRESHOLD:
        result = write_severity_threshold(district, role_name(role), threshold);
        if (result == 0) {
            cm_writef(STDOUT_FILENO,
                      "Set severity threshold for %s to %d\n",
                      district,
                      threshold);
            snprintf(action,
                     sizeof(action),
                     "set_threshold threshold=%d",
                     threshold);
        }
        break;
    default:
        usage(STDERR_FILENO);
        return 2;
    }

    if (result == -1) {
        return 1;
    }

    if (log_success(district, role, declared_user, action) == -1) {
        return 1;
    }

    return 0;
}
