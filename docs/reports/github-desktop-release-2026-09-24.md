# GitHub Release Report

## Scope

Prepare the Qt/Linux desktop charging station management platform for `fuling-ruozhi/Chargingplatform`. The release tree contains the Qt/C++ clients, shared client components, core services, SQLite schema, tests, docs, CMake files, and license. Standalone Vue/H5, Spring Boot, MySQL, Redis, and Docker components are not part of this release. The embedded HTML resource used by the Qt client's in-app map remains a Qt resource.

## Logging module

Included the administrator log audit page; logging schema migration; administrator route validation and audit routes; authentication rate limiting; log service; and `ncs_logging_service` / `ncs_auth_rate_limit` tests. Related DB, TCP, service, UI, schema, and test integration changes are included.

## Repository cleanup and documentation

- `.gitignore` excludes build directories, SQLite/database files, logs, IDE files, and user-specific files.
- Added `README.md` and `README_CN.md` with project overview, architecture, functionality, build/run instructions, tests, screenshots, and author details.
- Existing desktop screenshots under `docs/reports/` are referenced from both READMEs.
- Release staging is limited to Qt project paths; standalone `web/`, `ml/`, and build products are excluded.

## Build and tests

- From the existing `build/` directory, `cmake ..`: PASS.
- `cmake --build . -j$(nproc)`: PASS; built `ncs_admin`, `ncs_user`, and test targets.
- Initial sandbox `ctest --output-on-failure`: 43/65 passed; loopback network restrictions caused failures.
- Re-run with local loopback access, `ctest --output-on-failure`: **65/65 passed**.
- Includes logging service, auth rate limiting, schema migration, SQLite-backed integration, TCP client/server, admin UI, user UI, and station recommendation coverage.
- Offscreen startup smoke: `ncs_admin` initialized SQLite (18 tables; `PRAGMA integrity_check` = `ok`) and started the TCP server; its admin client and `ncs_user` each connected successfully (two client-connection log entries).

## GitHub status

- Target: <https://github.com/fuling-ruozhi/Chargingplatform>
- Added `chargingplatform` remote at `git@github.com:fuling-ruozhi/Chargingplatform.git`; the existing Gitee `origin` and previous `github` remote were preserved.
- HTTPS push could not prompt for credentials in the non-interactive environment. SSH authenticated as `fuling-ruozhi` and confirmed repository access.
- `git push -u chargingplatform main`: **PASS**. The release history was pushed and local `main` now tracks `chargingplatform/main`.

## Commit and working tree

The local `main` branch is based on the existing Qt-only snapshot. The client, core, database, and test trees match the validated feature worktree, including all listed logging sources and tests. The original feature branch worktree is kept intact; no reset or force push is used. The release commit hash is recorded in the final release report/update.

- Release commit: `7b4716f673afeafc14071dff42c0bb71a33b00f9` (`Release Qt charging station platform with logging audit module`).
- Local branch: `main`.
- Release worktree status is clean after the report and README updates.

## Open items

No local code, test, or publication blockers remain.
