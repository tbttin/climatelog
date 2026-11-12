# ClimateLog — Linux IoT Climate Telemetry Daemon

A lightweight, fault-tolerant Linux daemon written in C for simulating and persisting high-frequency IoT environmental telemetry (temperature and humidity) into SQLite. Designed around production systems engineering principles: transactional batching, SQLite WAL mode, graceful signal handling, and `systemd` least-privilege sandboxing.

---

## Architecture & Key Design Decisions

```text
+-------------------+      +-----------------------+      +---------------------------+

|  Sensor Simulator | ---> |  Event Loop (main.c)  | ---> |  SQLite Engine (WAL Mode) |
|  (src/sensor.c)   |      |  - 1Hz sampling       |      |  - Prepared statements    |
+-------------------+      |  - Batch accumulator  |      |  - Atomic batch commits   |

                           |  - SIGINT/SIGTERM     |      |  - Automated log retention|
                           +-----------------------+      +---------------------------+
                                       |
                                       v
                           +-----------------------+

                           |  systemd / journald   |
                           |  - Priority prefixes  |
                           |  - Sandboxed user     |
                           +-----------------------+
```

* **Transactional Batching:** Incoming 1 Hz sensor readings are grouped inside explicit SQLite transactions (`BEGIN` / `COMMIT`) and flushed in configurable batches (`DB_BATCH_LIMIT`), drastically reducing disk I/O and flash wear.
* **SQLite WAL Mode & Prepared Statements:** Uses Write-Ahead Logging (`PRAGMA journal_mode = WAL`) alongside persistent prepared statements (`SQLITE_PREPARE_PERSISTENT`) to allow non-blocking concurrent reads while the daemon writes continuously.
* **Automated Data Retention:** Periodically purges expired records (`DB_RETENTION_PERIOD`) after batch commits to keep disk usage bounded over long-term operation.
* **Graceful Signal Handling:** Intercepts `SIGINT` and `SIGTERM` via `sigaction` and an async-signal-safe `volatile sig_atomic_t` flag to flush any pending in-flight batch to disk before closing database handles.
* **Least-Privilege `systemd` Integration:** Runs under a dedicated unprivileged system account (`climatelog`) with `StateDirectory` management, filesystem sandboxing (`ProtectSystem=strict`, `ProtectHome=yes`, `NoNewPrivileges=yes`), and syslog priority prefixes (`<3>`, `<4>`, `<6>`) for `journald` filtering.

---

## Project Structure

```text
.
├── include/
│   ├── config.h          # Simulation bounds, batch size, and retention settings
│   ├── db.h              # Database API, status codes, and systemd logging macros
│   └── sensor.h          # Climate telemetry struct and generator interface
├── src/
│   ├── climate_log.c     # Daemon entry point, signal handlers, and main loop
│   ├── db.c              # SQLite lifecycle, dynamic device cache, and batch I/O
│   └── sensor.c          # Diurnal cosine temperature/humidity curve & fault injection
├── climatelog.service    # Hardened systemd unit file
├── Makefile              # Production build and system installation targets
└── Makefile.dev          # Development build with debug symbols and local SQLite DB
```

---

## Prerequisites

* **OS:** Linux with `systemd`
* **Compiler & Build Tools:** `gcc`, `make`
* **Libraries:** `libsqlite3-dev` (SQLite3 C development headers) and `libm`

On Debian/Ubuntu:
```bash
sudo apt update && sudo apt install -y build-essential libsqlite3-dev
```

---

## Building & Development

### Development Mode (Local Database)
Use `Makefile.dev` to compile with `-Wall -Wextra -g -Og` and store the database locally at `./climatelog.db`:
```bash
make -f Makefile.dev run
```
Press `Ctrl+C` (`SIGINT`) to trigger a graceful shutdown and flush the remaining batch.

### Production Build
```bash
make clean && make
```

---

## Systemd Installation & Usage

Install the binary to `/usr/local/bin/climatelog`, create the `climatelog` system user, and configure `/var/lib/climatelog`:

```bash
sudo make install
sudo systemctl daemon-reload
sudo systemctl enable --now climatelog
```

### Monitoring & Inspection
* **Check service status:**
  ```bash
  systemctl status climatelog
  ```
* **Follow live logs (`journald`):**
  ```bash
  journalctl -u climatelog -f
  ```
* **Filter warnings and errors only (sensor faults or out-of-bounds device IDs):**
  ```bash
  journalctl -u climatelog -p warning
  ```
* **Inspect the live database safely (read-only):**
  ```bash
  sudo -u climatelog sqlite3 -readonly /var/lib/climatelog/climatelog.db \
    "SELECT d.hardware_id, d.location, l.temperature, l.humidity, datetime(l.timestamp, 'unixepoch', 'localtime') AS time FROM climate_logs l JOIN devices d ON l.device_id = d.hardware_id ORDER BY l.timestamp DESC LIMIT 10;"
  ```

### Uninstallation
Stop the service and remove installed files (database history in `/var/lib/climatelog` is preserved by default):
```bash
sudo systemctl disable --now climatelog
sudo make uninstall
sudo systemctl daemon-reload
```

## Acknowlegement
I am the the one who create this project, I write the code and set it up myself with Gemini (free version) as a "supervisor". Thanks!

---

## Roadmap (v2.0)

* **Companion CLI (`climatectl`):** Read-only terminal utility for querying device health, recent logs, and fault summaries without invoking `sqlite3` directly.
* **Relational Constraints & Rollups:** Enforce `FOREIGN KEY` / `CHECK` constraints and aggregate expired raw logs into hourly min/max/avg summary tables prior to deletion.
* **Runtime CLI Flags:** Add `getopt` support (`-v` verbose logging, `-d` custom database path).

