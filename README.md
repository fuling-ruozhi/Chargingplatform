# NCS Charging Station Platform

## Overview

NCS is a Linux desktop charging station management platform built with Qt and C++17. It includes an administration client, a user client, and a TCP/JSON service hosted by the administration application. Clients use the service layer for business operations; SQLite is accessed on the server side.

This repository's GitHub release is scoped to the Qt desktop application: C++17, Qt 6, SQLite, TCP, and Linux/Ubuntu. The small HTML resource embedded by the Qt user client supports its in-app map view; it is not a separate web application.

## Features

- User and administrator authentication and management
- Charging station and charger management
- TCP client/server communication using framed JSON messages
- SQLite persistence, schema initialization, and migrations
- Charging orders, reservation, settlement, and history
- User ratings and station reviews
- Station recommendations and smart charging support
- Operation log auditing and security event recording
- Administrator dashboards and analytics

## Architecture

```text
User Client
    |
TCP JSON Protocol
    |
Admin Server
    |
SQLite Database
```

The user client does not connect to SQLite directly. The administration executable starts the TCP server (default loopback address `127.0.0.1`, port `9527`) and also provides the administration UI.

## Technology Stack

- C++17
- Qt 6 (minimum 6.2)
- CMake
- SQLite through Qt SQL
- TCP sockets through Qt Network
- Ubuntu/Linux desktop

## Build

Install the Qt development modules used by the project and the standard C++ build tools. On Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev qt6-tools-dev libqt6charts6-dev qt6-multimedia-dev
```

Configure and build from the repository root:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

To run the automated test suite:

```bash
ctest --test-dir build --output-on-failure
```

The Ubuntu release validation passed all 65 CTest tests, including database migrations, SQLite-backed services, TCP client/server integration, and both desktop UIs.

## Run

Start the administration application first. It hosts the TCP service and initializes the local SQLite database on startup:

```bash
./build/client_admin/ncs_admin
```

Then start the user application in another terminal:

```bash
./build/client_user/ncs_user
```

The service listens on `127.0.0.1:9527` by default. The database is stored under Qt's per-user application data directory. Register or use a configured account through the login screens. For local OTP demonstrations, see [`docs/protocol.md`](docs/protocol.md).

## Project Structure

```text
client_admin/   Administration UI and TCP server startup
client_common/  Shared client-side components
client_user/    User UI and embedded Qt resources
core/           Models, repositories, services, database, and TCP protocol
db/             SQLite schema and Qt resource bundle
docs/           Desktop application design and protocol documentation
tests/          CMake/CTest unit and integration tests
```

## Screenshots

Desktop UI screenshots are available in [`docs/reports/phase4-final-demo-screenshots/`](docs/reports/phase4-final-demo-screenshots/) and [`docs/reports/screenshots/ui-ecommerce-marketplace-redesign/`](docs/reports/screenshots/ui-ecommerce-marketplace-redesign/).

## Author

[fuling-ruozhi](https://github.com/fuling-ruozhi)
