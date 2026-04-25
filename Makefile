CC ?= cc
CFLAGS ?= -Wall -Wextra -Wpedantic -std=c11 -D_POSIX_C_SOURCE=200809L -g
LDFLAGS ?=

TARGET := city_manager
OBJS := main.o report.o fs_utils.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

main.o: main.c report.h fs_utils.h
report.o: report.c report.h fs_utils.h
fs_utils.o: fs_utils.c fs_utils.h

clean:
	rm -f $(TARGET) $(OBJS)
