# AI Usage: Phases 1 and 2

I used AI as a guidance, review, debugging, and documentation tool. I wrote
and changed the code myself, made the final design decisions, and tested the
program behavior myself.

## Phase 1

### Where I Used AI

- To review my file organization:
  `main.c`, `report.c`, `fs_utils.c`, `report.h`, and `fs_utils.h`.
- To check that my program used OS-style calls such as `open()`, `read()`,
  `write()`, `close()`, `stat()`, `lstat()`, `chmod()`, `fchmod()`,
  `mkdir()`, `symlink()`, `readlink()`, `unlink()`, `lseek()`,
  `ftruncate()`, `pread()`, and `pwrite()`.
- To review my binary `reports.dat` layout.
- To check my district directory layout:
  `reports.dat`, `district.cfg`, and `logged_district`.
- To review my permission simulation:
  `750` for district directories, `664` for `reports.dat`, `640` for
  `district.cfg`, and `644` for `logged_district`.
- To review my role checks for `manager` and `inspector`.
- To check my report filtering logic:
  `parse_condition()` and `match_condition()`.
- To review root symlink behavior for `active_reports-<district>`.
- To help polish `README.md` and the original `ai_usage.md`.

### Why I Used AI

- To catch edge cases in binary file storage.
- To verify that permission checks used `st_mode` bits correctly.
- To make sure symbolic links were checked with `lstat()` when needed.
- To check that filters used predictable parsing and AND logic.
- To make sure the documentation matched the real program behavior.

### How I Used AI

- I wrote the code and asked AI to review parts of it.
- I compared AI suggestions with the assignment requirements.
- I accepted only changes that matched my program design.
- I compiled and tested the commands myself.

## Phase 2

### Where I Used AI

- To review my new manager-only command:
  `--remove_district <district>`.
- To check my `fork()` and `execlp()` approach for:

  ```sh
  rm -rf -- <district>
  ```

- To review my `waitpid()` handling after the child process runs `rm`.
- To check that I removed the matching `active_reports-<district>` symlink.
- To review my new `monitor_reports.c` program.
- To check that I used `sigaction()` instead of `signal()`.
- To review `.monitor_pid` creation and deletion.
- To check that `city_manager` sends SIGUSR1 with `kill()` after adding a
  report.
- To check that monitor notification success or failure is written to
  `logged_district`.
- To review the updated `Makefile`, `build.sh`, `.gitignore`, and `README.md`.

### Why I Used AI

- Process creation and signals have small edge cases.
- I wanted a second check that I was not building unsafe shell command strings.
- I wanted to confirm that the child process used `exec*()` correctly.
- I wanted to confirm that signal handlers were installed with `sigaction()`.
- I wanted to make sure failed monitor notification was logged clearly.

### How I Used AI

- I implemented the changes myself.
- I used AI as a checklist for process and signal edge cases.
- I used AI to help diagnose the editor warning for `struct sigaction`.
- I fixed that warning by adding `_POSIX_C_SOURCE` before the headers.
- I rebuilt and tested the program myself.

## Testing

I verified the current project with:

```sh
make clean && make
```

I also manually tested:

- adding reports;
- sending SIGUSR1 to `monitor_reports`;
- monitor output on SIGUSR1;
- monitor exit on SIGINT;
- `.monitor_pid` cleanup;
- missing monitor logging;
- inspector denial for district removal;
- manager district removal;
- removal of `active_reports-<district>`.

## Included Sample Data

The project includes two districts and six total reports:

- `downtown`: 3 reports;
- `riverside`: 3 reports.

Included structure:

```text
downtown/
  district.cfg
  latest_report -> reports.dat
  logged_district
  reports.dat

riverside/
  district.cfg
  latest_report -> reports.dat
  logged_district
  reports.dat

active_reports-downtown -> downtown/reports.dat
active_reports-riverside -> riverside/reports.dat
```

Signed,
adrx
