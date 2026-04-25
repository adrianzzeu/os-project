#include "fs_utils.h"
#include "report.h"

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
    CMD_METADATA
} Command;

void usage(int fd);
Role parse_role(const char *text);
const char *role_name(Role role);
int need_arg(int argc, char **argv, int index, const char *option);
int set_command(Command *command, Command next);
int copy_option(char *dest, size_t dest_size, const char *value, const char *name);
int authorize(Role role, Command command);

void usage(int fd)
{
    cm_write_text(fd,
                  "Usage:\n"
                  "  city_manager --role inspector --add DISTRICT [--title TEXT] [--description TEXT] [--severity N] [--status TEXT]\n"
                  "  city_manager --role manager --remove_report DISTRICT ID\n"
                  "  city_manager --role inspector|manager --list DISTRICT [--filter EXPR]\n"
                  "  city_manager --role inspector|manager --show DISTRICT ID\n"
                  "  city_manager --role inspector|manager --metadata DISTRICT\n"
                  "\n"
                  "Filter terms are comma separated: severity>=3,status=open,text=bridge,active=all\n");
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

int authorize(Role role, Command command)
{
    if (role == ROLE_UNKNOWN) {
        cm_error("a valid --role is required: inspector or manager\n");
        return -1;
    }

    if (command == CMD_ADD && role != ROLE_INSPECTOR) {
        cm_error("permission denied: --add requires role inspector\n");
        return -1;
    }

    if (command == CMD_REMOVE_REPORT && role != ROLE_MANAGER) {
        cm_error("permission denied: --remove_report requires role manager\n");
        return -1;
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
    unsigned int id = 0;
    int i;

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
        } else if (strcmp(argv[i], "--list") == 0) {
            if (set_command(&command, CMD_LIST) == -1 ||
                need_arg(argc, argv, i, "--list") == -1) {
                return 2;
            }
            district = argv[++i];
        } else if (strcmp(argv[i], "--show") == 0) {
            if (set_command(&command, CMD_SHOW) == -1 ||
                need_arg(argc, argv, i, "--show") == -1) {
                return 2;
            }
            district = argv[++i];
            if (need_arg(argc, argv, i, "--show ID") == -1) {
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
        } else if (strcmp(argv[i], "--title") == 0) {
            if (need_arg(argc, argv, i, "--title") == -1) {
                return 2;
            }
            if (copy_option(input.title, sizeof(input.title), argv[++i], "--title") == -1) {
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
        } else if (strcmp(argv[i], "--status") == 0) {
            if (need_arg(argc, argv, i, "--status") == -1) {
                return 2;
            }
            if (validate_name(argv[i + 1], "status") == -1) {
                return 2;
            }
            if (copy_option(input.status, sizeof(input.status), argv[++i], "--status") == -1) {
                return 2;
            }
        } else if (strcmp(argv[i], "--severity") == 0) {
            unsigned int parsed;
            if (need_arg(argc, argv, i, "--severity") == -1) {
                return 2;
            }
            if (parse_u32(argv[++i], &parsed) == -1 || parsed > 5) {
                cm_error("--severity must be an integer from 0 to 5\n");
                return 2;
            }
            input.severity = (int)parsed;
        } else if (strcmp(argv[i], "--filter") == 0) {
            if (need_arg(argc, argv, i, "--filter") == -1) {
                return 2;
            }
            filter_expression = argv[++i];
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

    if (authorize(role, command) == -1) {
        return 1;
    }

    if (filter_expression != NULL &&
        parse_filter_expression(filter_expression, &filter) == -1) {
        return 2;
    }

    cm_writef(STDOUT_FILENO, "Role: %s\n", role_name(role));

    switch (command) {
    case CMD_ADD:
        return add_report(district, &input) == 0 ? 0 : 1;
    case CMD_REMOVE_REPORT:
        return remove_report(district, id) == 0 ? 0 : 1;
    case CMD_LIST:
        return list_reports(district, &filter) == 0 ? 0 : 1;
    case CMD_SHOW:
        return show_report(district, id) == 0 ? 0 : 1;
    case CMD_METADATA:
        return print_file_metadata(district) == 0 ? 0 : 1;
    default:
        usage(STDERR_FILENO);
        return 2;
    }
}
