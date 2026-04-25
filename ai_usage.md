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
  `city_manager --role inspector --user alice --add downtown --lat 51.5074 --lon -0.1278 --category road --severity 3 --description cracked_bridge`
- I implemented report removal with:
  `city_manager --role manager --user manager1 --remove_report downtown 17`
- I added listing, showing, metadata display, and report filtering.
- I added a `build.sh` script so the project can be compiled into the
  `city_manager` executable.
- I changed the storage layout so each district has its own directory under
  `data/`.
- I added the required district files:
  `reports.dat`, `district.cfg`, and `logged_district`.
- I updated the binary report records to store report ID, inspector name from
  `--user`, latitude, longitude, issue category, severity, timestamp, and
  description.
- I added district severity thresholds in `district.cfg` and made high-severity
  reports print an escalation alert when they meet the threshold.
- I added operation logging so successful actions write the timestamp, declared
  role, declared user, and action into `logged_district`.

### Where AI guided me

- AI gave light guidance while I organized my original code into separate files:
  `main.c`, `report.c`, `fs_utils.c`, `report.h`, and `fs_utils.h`.
- AI helped me check that my use of POSIX system calls fit the Operating
  Systems focus of the assignment.
- AI helped me verify that report storage uses binary file operations such as
  `open`, `pread`, `pwrite`, `lseek`, `fsync`, and `close`.
- AI helped me review the filesystem-related system calls for metadata,
  permissions, and links, including `stat`, `lstat`, `chmod`, `fchmod`,
  `symlink`, `readlink`, `unlink`, and `mkdir`.
- AI helped me review the `write`-based helper functions so terminal output
  also goes through system-call-style code.
- AI gave me a little guidance while I changed the district layout and record
  format, but I directed the changes and kept the implementation aligned with
  my assignment requirements.
- AI guided me in testing compile errors, warning flags, and the main command
  flows.

### How I guided the AI

- I told AI the project goal and the required role-based commands.
- I asked it to fix compilation issues after the linkage changes.
- I asked it to add a bash build script.
- I asked it to make the code more clearly based on system calls because this
  is an Operating Systems project.
- I gave the exact directory and file layout that the project needed, and I
  used AI mostly to help check the details while I implemented it.
- I corrected the AI usage wording so it reflects that I did the project and AI
  only gave guidance where I needed it.
- Most of the direction came from me; AI was mainly used for small checks,
  wording, and implementation guidance when I wanted a second pass.

### Current status

- The project builds with `./build.sh`.
- The project also builds with `make`.
- The program has been tested with add, list, show, metadata, filter, threshold
  update, inspector denial, manager removal, and operation log creation.
- Generated runtime data is stored under `data/DISTRICT/` and can be recreated
  by running the program.

### Next steps

- I can add more report fields if needed by the assignment.
- I can add more filter options if the project rubric requires them.
- I can add a short demo section to the report showing the exact commands I ran
  and their outputs.
