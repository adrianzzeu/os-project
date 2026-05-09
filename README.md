# City Manager Reports

`city_manager` is a C command-line application for storing and managing city
infrastructure reports by district.

It was built for an Operating Systems project, so it uses POSIX file,
directory, process, and signal APIs instead of hiding everything behind
high-level helpers.

## Build

```sh
make
```

or:

```sh
./build.sh
```

Both commands build:

```text
city_manager
monitor_reports
```

Clean build files:

```sh
make clean
```

## Included Data

The repository includes two districts and six total reports:

```text
downtown/
  district.cfg
  latest_report -> reports.dat
  logged_district
  reports.dat      # 3 reports

riverside/
  district.cfg
  latest_report -> reports.dat
  logged_district
  reports.dat      # 3 reports

active_reports-downtown -> downtown/reports.dat
active_reports-riverside -> riverside/reports.dat
```

## Roles

Every command receives a declared role and user:

```sh
./city_manager --role manager --user alice --list downtown
./city_manager --role inspector --user bob --list riverside
```

Roles:

- `manager`: treated as the file owner.
- `inspector`: treated as the group user.

Permission layout:

| Item | Mode |
| --- | --- |
| district directory | `750` |
| `reports.dat` | `664` |
| `district.cfg` | `640` |
| `logged_district` | `644` |

## Main Commands

Add a report:

```sh
./city_manager --role manager --user alice --add downtown \
  --lat 12.8 \
  --lon 21.7 \
  --category flooding \
  --severity 2 \
  --description "Drain overflow near market"
```

List reports:

```sh
./city_manager --role inspector --user bob --list downtown
```

View one report:

```sh
./city_manager --role manager --user alice --view downtown 1
```

Filter reports:

```sh
./city_manager --role inspector --user bob --filter downtown severity:>=2
./city_manager --role inspector --user bob --filter downtown category:==road
```

Remove one report:

```sh
./city_manager --role manager --user alice --remove_report downtown 1
```

Update district threshold:

```sh
./city_manager --role manager --user alice --update_threshold downtown 2
```

Show file metadata:

```sh
./city_manager --role manager --user alice --metadata downtown
```

Remove a whole district:

```sh
./city_manager --role manager --user alice --remove_district downtown
```

`remove_district` is manager-only. It forks a child process, runs:

```sh
rm -rf -- <district>
```

and then removes the matching `active_reports-<district>` symlink.

## Monitor Program

Start the monitor from the project directory:

```sh
./monitor_reports
```

Behavior:

- writes its PID to `.monitor_pid`;
- prints a message when it receives SIGUSR1;
- exits only after SIGINT;
- deletes `.monitor_pid` before exiting.

When `city_manager` adds a report, it reads `.monitor_pid` and sends SIGUSR1
with `kill()`. The district log records either successful notification or an
explicit monitor failure message.

## Storage

Each `reports.dat` file stores fixed-size binary `Report` records directly:

```text
record 0 -> offset 0
record 1 -> offset sizeof(Report)
record 2 -> offset 2 * sizeof(Report)
```

Each district also has:

- `district.cfg`: severity threshold;
- `logged_district`: timestamped operation log;
- `latest_report`: symlink to `reports.dat`.

Root-level `active_reports-<district>` symlinks point to each district report
file.

## System Calls Used

Important APIs used in the project:

```text
open close read write pread pwrite lseek ftruncate fsync
stat lstat mkdir chmod fchmod symlink readlink unlink
fork exec waitpid kill sigaction
```

The program does not use `signal()`.

## Quick Check

```sh
make clean && make
./city_manager --role inspector --user tester --list downtown
./city_manager --role inspector --user tester --list riverside
```

Expected sample count:

```text
downtown: 3 reports
riverside: 3 reports
```

## AI Usage

Combined AI usage notes are in:

```text
AI_usage-phases_1_and_2.md
```
