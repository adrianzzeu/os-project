CC ?= cc
CFLAGS ?= -Wall -Wextra -Wpedantic -std=c11 -D_POSIX_C_SOURCE=200809L -g
LDFLAGS ?=

TARGETS := city_manager monitor_reports
CITY_MANAGER_OBJS := main.o report.o fs_utils.o
MONITOR_REPORTS_OBJS := monitor_reports.o fs_utils.o

.PHONY: all clean

all: $(TARGETS)

city_manager: $(CITY_MANAGER_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CITY_MANAGER_OBJS) $(LDFLAGS)

monitor_reports: $(MONITOR_REPORTS_OBJS)
	$(CC) $(CFLAGS) -o $@ $(MONITOR_REPORTS_OBJS) $(LDFLAGS)

main.o: main.c report.h fs_utils.h
report.o: report.c report.h fs_utils.h
fs_utils.o: fs_utils.c fs_utils.h
monitor_reports.o: monitor_reports.c fs_utils.h

clean:
	rm -f $(TARGETS) main.o report.o fs_utils.o monitor_reports.o
