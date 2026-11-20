# ClimateLog — Linux IoT Climate Telemetry Daemon & CLI

A lightweight, fault-tolerant Linux daemon and read-only inspection CLI written in C for simulating and persisting IoT environmental telemetry (temperature and humidity) into SQLite. Built around production Linux systems engineering practices: modular information hiding, encapsulated transactional batching, SQLite WAL concurrency, async-signal-safe shutdowns, and `systemd` least-privilege sandboxing.

---

## Architecture & Key Design Decisions

```text
                       +-----------------------------------+

                       |   Orchestrator (climate_log.c)    |
                       |   - CLI argument routing (-s/-t)  |
                       |   - SIGINT/SIGTERM signal safety  |
                       |   - 1 Hz daemon sampling loop     |
                       +-----------------------------------+

                          |              |              |
         samples data     |              | ingests      | read-only queries
         v                |              v              v
+-------------------+     |     +-------------------+   +-------------------+

| Sensor Simulator  |     |     |   Core DB Engine  |   |   CLI Inspector   |
| (src/sensor.c)    |     |     |   (src/db.c)      |   |   (src/cli.c)     |
| - Diurnal cosine  |     |     | - Batch txns      |   | - Status summary  |
| - Fault injection |     |     | - Device cache    |   | - Recent log tail |
+-------------------+     |     | - 30d retention   |   +-------------------+

                          |     +-------------------+             |
                          |              |                        |
                          |              +-----------+------------+

                          |                          |
                          v                          v
                 +-----------------+      +---------------------+

                 | systemd/journald|      | SQLite3 (WAL Mode)  |
                 | - <3>/<4>/<6>   |      | - Concurrent R/W    |
                 | - Sandboxed user|      | - Prepared stmts    |
                 +-----------------+      +---------------------+
```

* **Strict Modular Encapsulation:** Public headers in `include/` expose only opaque handles (`struct db_context;`) and minimal contracts, while SQLite handles, `SQL_TRY` error macros, and internal state stay isolated inside `src/db_internal.h`.
* **Encapsulated Transactional Batching:** The database engine (`db_ingest_log` / `db_flush`) manages SQLite transactions (`BEGIN` / `COMMIT` / `ROLLBACK`), batch accumulation (`DB_BATCH_LIMIT`), dynamic device registration, and rolling log retention (`DB_RETENTION_PERIOD`) internally—keeping the main daemon loop completely decoupled from SQLite state.
* **SQLite WAL Mode & Persistent Statements:** Uses Write-Ahead Logging (`PRAGMA journal_mode = WAL`) and `SQLITE_PREPARE_PERSISTENT` statements so concurrent read-only CLI queries (`-s`, `-t`) never block continuous 1 Hz daemon writes.
* **Async-Signal-Safe Shutdown:** Intercepts `SIGINT` and `SIGTERM` via `sigaction()` and a `volatile sig_atomic_t` flag to flush any pending in-flight batch to disk before finalizing statements and closing database handles.
* **Hardened `systemd` Integration:** Runs under a dedicated unprivileged system account (`climatelog`) with `StateDirectory` management, kernel sandboxing (`ProtectSystem=strict`, `ProtectHome=yes`, `NoNewPrivileges=yes`), and `journald` severity prefixes (`<3>`, `<4>`, `<6>`, `<7>`).

---

## Project Structure

```text
.
├── include/                  # Public module interfaces
│   ├── cli.h                 # Read-only CLI query commands (-s, -t)
│   ├── config.h              # Simulation bounds, batch limits, and retention defaults
│   ├── db.h                  # Opaque db_context handle & high-level ingestion API
│   ├── log.h                 # systemd-journald priority logging macros
│   └── sensor.h              # Climate telemetry DTO struct & simulator interface
├── src/                      # Private subsystem implementations
│   ├── cli.c                 # SQLite read-only status and tail queries
│   ├── climate_log.c         # Entry point, getopt routing, signal setup, 1 Hz loop
│   ├── db.c                  # SQLite lifecycle, batch transactions, and retention engine
│   ├── db_internal.c         # Shared internal device-cache bounds & lookup helpers
│   ├── db_internal.h         # Private DB subsystem header (SQLite handles & SQL_TRY)
│   └── sensor.c              # Cosine diurnal temperature/humidity curve & fault injection
├── climatelog.service        # Hardened systemd unit file
├── Makefile                  # Production build (-O2) and system installation targets
└── Makefile.dev              # Development build (-Og, ASan/UBSan, strict warnings, ctags)
```

---

## Prerequisites

* **OS:** Linux with `systemd`
* **Compiler & Build Tools:** `gcc`, `make` (optional: `ctags` for development indexing)
* **Libraries:** `libsqlite3-dev` and `libm`

On Debian/Ubuntu:
```bash
sudo apt update && sudo apt install -y build-essential libsqlite3-dev
```

---

## Building & Development

### Development Build (Sanitizers + Local Database)
`Makefile.dev` compiles with `-Wall -Wextra -Wshadow -Wmissing-prototypes -Og -fsanitize=address,undefined` and defaults the database path to `./climatelog.db`:
```bash
make -f Makefile.dev run
```
Press `Ctrl+C` (`SIGINT`) to trigger a graceful shutdown and flush the active batch.

### Production Build
```bash
make clean && make
```

---

## CLI Inspection & Environment Overrides

You can query the database safely while the daemon is running (using `SQLITE_OPEN_READONLY` over WAL mode):

* **Print device, record, and sensor-fault totals:**
  ```bash
  climatelog -s
  ```
* **Tail the last `<num>` most recent records:**
  ```bash
  climatelog -t 10
  ```
* **Override the database file path at runtime:**
  ```bash
  CLIMATELOG_DB_FILE=./climatelog.db ./dev/climatelog -s -t 5
  ```

---

## Systemd Installation & Usage

Install the binary to `/usr/local/bin/climatelog`, create the unprivileged `climatelog` system user, and configure `/var/lib/climatelog`:

```bash
sudo make install
sudo systemctl daemon-reload
sudo systemctl enable --now climatelog
```

### Monitoring & Service Management
* **Check service status:**
  ```bash
  systemctl status climatelog
  ```
* **Follow live logs (`journald`):**
  ```bash
  journalctl -u climatelog -f
  ```
* **Filter warnings and errors only (sensor faults or out-of-ceiling device IDs):**
  ```bash
  journalctl -u climatelog -p warning
  ```
* **Query the production database via CLI:**
  ```bash
  sudo -u climatelog climatelog -s -t 10
  ```

### Uninstallation
Stop the service and remove installed binaries and unit files (database history in `/var/lib/climatelog` is preserved by default):
```bash
sudo systemctl disable --now climatelog
sudo make uninstall
sudo systemctl daemon-reload
```

---

## Roadmap (v2.0)

* **Runtime Verbose Flag (`-v`):** Wire `-v` in `getopt` to toggle `LOG_DEBUG` (`<7>`) diagnostic output at runtime.
* **External Configuration File:** Parse `/etc/climatelog.conf` to configure batch sizes, retention windows, and device ID ceilings without recompiling.
* **Relational Schema & Hourly Rollups:** Enforce `FOREIGN KEY` / `CHECK` constraints and aggregate expired raw logs into hourly min/max/avg summary tables prior to deletion.

---

## Acknowledgments

Designed, implemented, and configured from scratch as a Linux systems programming project, using Gemini as an architectural reviewer and code-audit sounding board.

