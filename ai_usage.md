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
- I wrote a project `README.md` documenting build steps, command usage, roles,
  file layout, permissions, binary report storage, filtering, metadata, logs,
  symlinks, and cleanup.
- I changed the storage layout so each district has its own directory directly
  under the project folder, such as `./downtown/`.
- I added the required district files:
  `reports.dat`, `district.cfg`, and `logged_district`.
- I updated the binary report records to store report ID, inspector name from
  `--user`, latitude, longitude, issue category, severity, timestamp, and
  description.
- I corrected `reports.dat` so it stores only packed fixed-size `Report`
  records starting at byte 0, without a separate file header before record 0.
- I made report ID allocation come from scanning the existing records for the
  highest ID, instead of keeping extra metadata inside `reports.dat`.
- I added district severity thresholds in `district.cfg` and made high-severity
  reports print an escalation alert when they meet the threshold.
- I added operation logging so successful actions write the timestamp, declared
  role, declared user, and action into `logged_district`.
- I adjusted `logged_district` entries to a simple terminal-friendly format:
  epoch timestamp, user, role, then action.
- I added the permission-bit simulation required by the assignment:
  district directories use `750`, `reports.dat` uses `664`, `district.cfg`
  uses `640`, and `logged_district` uses `644`.
- I added role-based permission checks using `stat()` and the permission bits
  from `st_mode`, where managers are treated as owners and inspectors are
  treated as group users.
- I changed `remove_report` so it physically removes a record by shifting later
  fixed-size records with `lseek()` and truncating the file with
  `ftruncate()`.
- I updated commands toward the assignment naming, including `view`,
  `update_threshold`, and `filter`.
- I made `list` print the current symbolic permissions, file size, and
  modification time for `reports.dat`.
- I added the Phase 1 AI-assisted condition matching functions:
  `parse_condition()` and `match_condition()`.
- I updated the `filter` command so it accepts one or more conditions like
  `severity:>=2` and `category:==road`, parses each condition, reads
  `reports.dat` record by record with `read()`, and prints records only when
  every condition matches.
- I added support for filtering by `severity`, `category`, `inspector`, and
  `timestamp`, with operators `=`, `==`, `!=`, `<`, `<=`, `>`, and `>=`.
- I added root-level `active_reports-<district>` symbolic links pointing to
  `<district>/reports.dat`.
- I used `lstat()` when checking symlinks and added warnings for dangling
  `active_reports-*` links instead of crashing.

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
- AI helped me notice that my first binary-file layout had extra metadata before
  record 0, and I adjusted the implementation so record offsets now match
  `report_index * sizeof(Report)`.
- AI helped me compare my terminal behavior against the sample output, and I
  adjusted the directory layout and interactive add prompts myself to match it
  more closely.
- I implemented the permission-bit simulation mostly myself, with only a small
  amount of AI help to check edge cases like exact `district.cfg` permissions,
  symbolic permission output, and physical record deletion.
- I used AI as a second-pass helper for the two required condition-matching
  functions. I described my record layout and fields, then reviewed the
  generated parsing and matching logic and adjusted it to fit my code.
- I implemented the final `filter` command integration myself, using AI mainly
  to check that it opened the report file, read records one by one, and applied
  all conditions with AND logic.
- AI helped me double-check the root `active_reports-*` symbolic link behavior,
  especially using `lstat()` and warning about dangling links.
- AI guided me in testing compile errors, warning flags, and the main command
  flows.
- AI helped me organize the project documentation, while I kept the wording
  aligned with the actual behavior and assignment requirements.

### How I guided the AI

- I told AI the project goal and the required role-based commands.
- I asked it to fix compilation issues after the linkage changes.
- I asked it to add a bash build script.
- I asked it to make the code more clearly based on system calls because this
  is an Operating Systems project.
- I gave the exact directory and file layout that the project needed, and I
  used AI mostly to help check the details while I implemented it.
- I gave the exact permission table and command behavior from the assignment,
  then used AI for small implementation checks while I made the changes.
- I gave the exact Phase 1 filter and symbolic-link requirements, then used AI
  mainly to help review the required function shapes and edge cases.
- I showed AI sample terminal output and used it as guidance while I updated
  `--add` to prompt for missing latitude, longitude, category, severity, and
  description fields.
- I asked AI to help document the whole project so the usage and implementation
  details are easier to explain during evaluation.
- I corrected the AI usage wording so it reflects that I did the project and AI
  only gave guidance where I needed it.
- Most of the direction came from me; AI was mainly used for small checks,
  wording, and implementation guidance when I wanted a second pass.

### Current status

- The project builds with `./build.sh`.
- The project also builds with `make`.
- The project now has a `README.md` with full usage and implementation
  documentation.
- I verified the packed binary layout by adding three reports, checking that
  `reports.dat` was exactly `3 * sizeof(Report)`, removing the middle report,
  and checking that the file shrank to `2 * sizeof(Report)`.
- The program has been tested with add, list, show, metadata, filter, threshold
  update, inspector denial, manager removal, root symbolic link creation, and
  dangling symbolic link warnings.
- Generated runtime data is stored under `./DISTRICT/` and can be recreated
  by running the program.

### Next steps

- I can add more report fields if needed by the assignment.
- I can add more filter options if the project rubric requires them.
- I can add a short demo section to the report showing the exact commands I ran
  and their outputs.

  -signed adrx
