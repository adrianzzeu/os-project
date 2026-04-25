# City Manager Infrastructure Reports

`city_manager` is a C command-line program for storing and managing city
infrastructure reports by district. It is written for an Operating Systems
project, so it intentionally uses POSIX system calls for binary file I/O,
metadata checks, symbolic links, permission-bit simulation, and file updates.

## Build

Build with the provided script:

```sh
./build.sh
```

Or build with `make`:

```sh
make
```

Both commands create the executable:

```sh
./city_manager
```

Clean build output with:

```sh
make clean
```

## Roles

The program does not use real Unix users. Instead, every command receives a
declared role and declared user name:

```sh
./city_manager --role manager --user alice --add downtown
./city_manager --role inspector --user bob --list downtown
```

Supported roles:

- `manager`: treated as the file owner in the permission simulation.
- `inspector`: treated as the file group user in the permission simulation.

The `--user` value is also stored as the inspector name when adding a report.

## Directory Layout

Each district is stored in its own directory directly under the project folder.
For example, adding a report to `downtown` creates:

```text
downtown/
  district.cfg
  logged_district
  latest_report -> reports.dat
  reports.dat
active_reports-downtown -> downtown/reports.dat
```

File purposes:

- `reports.dat`: binary file containing packed fixed-size `Report` records.
- `district.cfg`: plain-text district configuration file.
- `logged_district`: plain-text operation log.
- `latest_report`: symlink inside the district pointing to `reports.dat`.
- `active_reports-<district>`: root-level symlink pointing to the district
  report file.

## Permissions

The program sets permissions explicitly when files are created:

| Path | Mode | Meaning |
| --- | --- | --- |
| District directory | `750` | Manager full access; inspector read and execute. |
| `reports.dat` | `664` | Both roles may read; both roles may append/write reports. |
| `district.cfg` | `640` | Manager may read/write; inspector may read. |
| `logged_district` | `644` | Anyone may read; only manager may write in the simulation. |

Before role-restricted actions, the program calls `stat()` and checks
permission bits from `st_mode`. If the declared role does not have the needed
simulated permission, the command refuses the operation.

Because `logged_district` is mode `644`, inspector commands can complete but
the final log write is refused with a diagnostic. This demonstrates the
permission check required by the assignment.

## Binary Report Format

`reports.dat` contains only packed `Report` records. There is no file header.

```text
offset 0                  -> record 0
offset sizeof(Report)     -> record 1
offset 2 * sizeof(Report) -> record 2
```

Each record contains:

- report ID
- inspector name from `--user`
- latitude
- longitude
- category
- severity level
- timestamp
- description
- active flag

The current `Report` struct is defined in `report.h`.

Report IDs are assigned by scanning existing records and using the highest ID
plus one.

Removing a report physically shifts later records one position left and then
truncates the file with `ftruncate()`.

## Commands

Show help:

```sh
./city_manager --help
```

### Add Report

Both managers and inspectors may add reports:

```sh
./city_manager --role manager --user alice --add downtown
```

If report fields are omitted, the program prompts for them:

```text
Latitude:
Longitude:
Category (road/lighting/flooding/other):
Severity level (1/2/3):
Description:
```

All fields can also be passed directly:

```sh
./city_manager --role inspector --user bob --add downtown \
  --lat 12.2 \
  --lon 21.1 \
  --category road \
  --severity 3 \
  --description "Road closed"
```

If the report severity is greater than or equal to the district threshold, the
program prints an escalation alert.

### List Reports

Both roles may list reports:

```sh
./city_manager --role inspector --user bob --list downtown
```

The output includes the current symbolic permission bits, file size, and
modification time for `reports.dat`.

### View One Report

Both roles may view one report by ID:

```sh
./city_manager --role manager --user alice --view downtown 1
```

`--show` is also accepted:

```sh
./city_manager --role manager --user alice --show downtown 1
```

### Remove Report

Only managers may remove reports:

```sh
./city_manager --role manager --user alice --remove_report downtown 1
```

The implementation finds the matching fixed-size record, shifts all later
records left, and truncates `reports.dat`.

### Update Severity Threshold

Only managers may update the district threshold:

```sh
./city_manager --role manager --user alice --update_threshold downtown 2
```

`--set_threshold` is also accepted:

```sh
./city_manager --role manager --user alice --set_threshold downtown 2
```

The command checks that `district.cfg` still has exact mode `640` before
writing. If the permissions were changed, the command refuses to update it.

### Filter Reports

Both roles may filter reports:

```sh
./city_manager --role inspector --user bob --filter downtown severity:>=2
```

Multiple conditions are joined with AND:

```sh
./city_manager --role inspector --user bob --filter downtown \
  severity:>=2 \
  category:==road
```

Supported fields:

- `severity`
- `category`
- `inspector`
- `timestamp`

Supported operators:

- `=`
- `==`
- `!=`
- `<`
- `<=`
- `>`
- `>=`

Examples:

```sh
./city_manager --role inspector --user bob --filter downtown severity:>=2
./city_manager --role inspector --user bob --filter downtown category:==road
./city_manager --role inspector --user bob --filter downtown inspector:==alice
./city_manager --role inspector --user bob --filter downtown timestamp:>1777000000
```

### Metadata

Both roles may print file and symlink metadata:

```sh
./city_manager --role manager --user alice --metadata downtown
```

This prints metadata for:

- district directory
- `reports.dat`
- `district.cfg`
- `logged_district`
- `latest_report`
- `active_reports-<district>`

The program uses `lstat()` for symlinks so it can inspect the link itself
instead of following it silently. Dangling `active_reports-*` links are reported
with a warning.

## Operation Log

Successful actions attempt to append to:

```text
<district>/logged_district
```

The log format is:

```text
<epoch_timestamp> <user> <role> <action>
```

Example:

```text
1777149255 alice manager add report_id=1
```

## Example Session

Build:

```sh
./build.sh
```

Add a report interactively:

```sh
./city_manager --role manager --user alice --add downtown
```

Add a report without prompts:

```sh
./city_manager --role inspector --user bob --add downtown \
  --lat 42.1 \
  --lon 21.3 \
  --category lighting \
  --severity 2 \
  --description "Street light outage"
```

Inspect generated files:

```sh
find downtown -maxdepth 1 -printf "%M %p\n"
cat downtown/district.cfg
cat downtown/logged_district
ls -l active_reports-downtown
```

List reports:

```sh
./city_manager --role inspector --user bob --list downtown
```

Filter critical reports:

```sh
./city_manager --role inspector --user bob --filter downtown severity:>=3
```

Remove a report:

```sh
./city_manager --role manager --user alice --remove_report downtown 1
```

## Checking the Binary File

Use `stat` to confirm the file size is a multiple of `sizeof(Report)`:

```sh
stat -c %s downtown/reports.dat
```

Use `xxd` or `od` to inspect the binary contents:

```sh
xxd downtown/reports.dat
```

If `xxd` is not installed:

```sh
od -Ax -tx1z downtown/reports.dat
```

For this build, one report is currently 480 bytes on the tested system. The
exact size depends on the compiled C struct layout.

## System Calls Practiced

The project uses system-call-oriented APIs including:

- `open()`
- `close()`
- `read()`
- `write()`
- `pread()`
- `pwrite()`
- `lseek()`
- `ftruncate()`
- `fsync()`
- `stat()`
- `lstat()`
- `mkdir()`
- `chmod()`
- `fchmod()`
- `symlink()`
- `readlink()`
- `unlink()`

## Cleanup

Remove build output:

```sh
make clean
```

Remove a test district and its root symlink:

```sh
rm -rf downtown active_reports-downtown
```

Older test data from earlier versions may exist under `data/`. The current
program stores new districts directly under `./DISTRICT/`.

## AI Usage Log

The required AI usage notes are stored separately in:

```text
ai_usage.md
```

