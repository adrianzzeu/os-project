# AI Usage Log

## 2026-04-25

I used AI as a guidance tool while building my Operating Systems project. I
kept control of the requirements, command behavior, and final decisions, while
AI helped me reason through the implementation details and make the code more
system-call-oriented.

### What I worked on

- I built the `city_manager` command-line program for managing infrastructure
  reports.
- I added explicit roles on the command line, including `inspector` and
  `manager`.
- I implemented report creation with:
  `city_manager --role inspector --add downtown`
- I implemented report removal with:
  `city_manager --role manager --remove_report downtown 17`
- I added listing, showing, metadata display, and report filtering.
- I added a `build.sh` script so the project can be compiled into the
  `city_manager` executable.

### Where AI guided me

- AI helped me organize my original code into separate files:
  `main.c`, `report.c`, `fs_utils.c`, `report.h`, and `fs_utils.h`.
- AI guided me toward using POSIX system calls instead of normal buffered file
  I/O, which fits the Operating Systems focus of the assignment.
- AI helped me check that report storage uses binary file operations such as
  `open`, `pread`, `pwrite`, `lseek`, `fsync`, and `close`.
- AI helped me add filesystem-related system calls for metadata, permissions,
  and links, including `stat`, `lstat`, `chmod`, `fchmod`, `symlink`,
  `readlink`, `unlink`, and `mkdir`.
- AI helped me replace terminal output with `write`-based helper functions so
  output also goes through system-call-style code.
- AI guided me in testing compile errors, warning flags, and the main command
  flows.

### How I guided the AI

- I told AI the project goal and the required role-based commands.
- I asked it to fix compilation issues after the linkage changes.
- I asked it to add a bash build script.
- I asked it to make the code more clearly based on system calls because this
  is an Operating Systems project.
- I corrected the AI usage wording so it reflects that I did the project and AI
  only guided the process.

### Current status

- The project builds with `./build.sh`.
- The project also builds with `make`.
- The program has been tested with add, list, show, metadata, filter, inspector
  denial, and manager removal commands.
- Generated runtime data is stored under `data/` and can be recreated by
  running the program.

### Next steps

- I can add more report fields if needed by the assignment.
- I can add more filter options if the project rubric requires them.
- I can add a short demo section to the report showing the exact commands I ran
  and their outputs.
