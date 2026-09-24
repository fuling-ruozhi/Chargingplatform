CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS user (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    phone TEXT UNIQUE,
    nickname TEXT NOT NULL DEFAULT '',
    avatar_path TEXT NOT NULL DEFAULT '',
    balance REAL NOT NULL DEFAULT 0 CHECK(balance >= 0),
    status INTEGER NOT NULL DEFAULT 1 CHECK(status IN (0, 1)),
    created_at TEXT NOT NULL DEFAULT '',
    username TEXT UNIQUE,
    password_hash TEXT,
    salt TEXT,
    CHECK(phone IS NULL OR (
        length(phone) = 11
        AND substr(phone, 1, 1) = '1'
        AND phone NOT GLOB '*[^0-9]*'
    ))
);

CREATE INDEX IF NOT EXISTS idx_user_phone ON user(phone);

CREATE TABLE IF NOT EXISTS admin (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    salt TEXT NOT NULL,
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS admin_login_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    admin_id INTEGER,
    admin_username TEXT NOT NULL DEFAULT '',
    login_time TEXT NOT NULL,
    login_result TEXT NOT NULL,
    failure_reason TEXT NOT NULL DEFAULT '',
    client_ip TEXT NOT NULL DEFAULT '127.0.0.1',
    failed_attempts INTEGER NOT NULL DEFAULT 0,
    session_id TEXT NOT NULL DEFAULT '',
    logout_time TEXT,
    created_at TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_admin_login_time ON admin_login_logs(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_admin_login_admin ON admin_login_logs(admin_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_admin_login_username ON admin_login_logs(admin_username);

CREATE TABLE IF NOT EXISTS admin_operation_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    admin_id INTEGER,
    admin_username TEXT NOT NULL DEFAULT '',
    module TEXT NOT NULL,
    action TEXT NOT NULL,
    target_type TEXT NOT NULL DEFAULT '',
    target_id TEXT NOT NULL DEFAULT '',
    detail TEXT NOT NULL DEFAULT '',
    result TEXT NOT NULL,
    error_message TEXT NOT NULL DEFAULT '',
    client_ip TEXT NOT NULL DEFAULT '127.0.0.1',
    created_at TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_operation_created_at ON admin_operation_logs(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_operation_admin ON admin_operation_logs(admin_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_operation_module ON admin_operation_logs(module);

CREATE TABLE IF NOT EXISTS security_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_type TEXT NOT NULL,
    severity TEXT NOT NULL,
    admin_id INTEGER,
    username TEXT NOT NULL DEFAULT '',
    client_ip TEXT NOT NULL DEFAULT '127.0.0.1',
    target TEXT NOT NULL DEFAULT '',
    description TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_security_created_at ON security_logs(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_security_event_type ON security_logs(event_type);

CREATE TABLE IF NOT EXISTS station (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    address TEXT NOT NULL,
    longitude REAL NOT NULL DEFAULT 0 CHECK(longitude BETWEEN -180 AND 180),
    latitude REAL NOT NULL DEFAULT 0 CHECK(latitude BETWEEN -90 AND 90),
    price REAL NOT NULL CHECK(price > 0),
    total_slots INTEGER NOT NULL DEFAULT 0 CHECK(total_slots >= 0),
    created_at TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS charger (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id INTEGER NOT NULL,
    code TEXT NOT NULL UNIQUE,
    type INTEGER NOT NULL DEFAULT 0 CHECK(type IN (0, 1)),
    power_kw REAL NOT NULL DEFAULT 7 CHECK(power_kw > 0),
    status INTEGER NOT NULL DEFAULT 0 CHECK(status IN (0, 1, 2)),
    total_count INTEGER NOT NULL DEFAULT 0 CHECK(total_count >= 0),
    total_minutes INTEGER NOT NULL DEFAULT 0 CHECK(total_minutes >= 0),
    created_at TEXT NOT NULL DEFAULT '',
    FOREIGN KEY(station_id) REFERENCES station(id)
        ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_charger_station_id ON charger(station_id);
CREATE INDEX IF NOT EXISTS idx_charger_status ON charger(status);

CREATE TABLE IF NOT EXISTS charging_order (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    order_no TEXT NOT NULL UNIQUE,
    user_id INTEGER NOT NULL,
    charger_id INTEGER NOT NULL,
    station_id INTEGER NOT NULL,
    status INTEGER NOT NULL DEFAULT 0 CHECK(status IN (0, 1, 2, 3)),
    reserved_at TEXT NOT NULL DEFAULT '',
    expire_at TEXT NOT NULL DEFAULT '',
    start_time TEXT NOT NULL DEFAULT '',
    end_time TEXT NOT NULL DEFAULT '',
    energy REAL NOT NULL DEFAULT 0 CHECK(energy >= 0),
    amount REAL NOT NULL DEFAULT 0 CHECK(amount >= 0),
    debt_amount REAL NOT NULL DEFAULT 0 CHECK(debt_amount >= 0),
    balance_after REAL NOT NULL DEFAULT 0 CHECK(balance_after >= 0),
    price_per_kwh REAL NOT NULL DEFAULT 0 CHECK(price_per_kwh >= 0),
    power_kw REAL NOT NULL DEFAULT 0 CHECK(power_kw >= 0),
    time_scale INTEGER NOT NULL DEFAULT 60 CHECK(time_scale > 0),
    initial_soc REAL NOT NULL DEFAULT 20 CHECK(initial_soc BETWEEN 0 AND 100),
    final_soc REAL NOT NULL DEFAULT 20 CHECK(final_soc BETWEEN 0 AND 100),
    station_name_snapshot TEXT NOT NULL DEFAULT '',
    charger_code_snapshot TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT '',
    updated_at TEXT NOT NULL DEFAULT '',
    legacy_source TEXT NOT NULL DEFAULT '',
    legacy_id INTEGER,
    FOREIGN KEY(user_id) REFERENCES user(id)
        ON UPDATE CASCADE ON DELETE RESTRICT,
    FOREIGN KEY(charger_id) REFERENCES charger(id)
        ON UPDATE CASCADE ON DELETE RESTRICT,
    FOREIGN KEY(station_id) REFERENCES station(id)
        ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_order_one_active_user
ON charging_order(user_id) WHERE status IN (0, 1);

CREATE UNIQUE INDEX IF NOT EXISTS idx_order_one_active_charger
ON charging_order(charger_id) WHERE status IN (0, 1);

CREATE UNIQUE INDEX IF NOT EXISTS idx_order_legacy_source_id
ON charging_order(legacy_source, legacy_id)
WHERE legacy_source <> '' AND legacy_id IS NOT NULL;

CREATE INDEX IF NOT EXISTS idx_order_user_created
ON charging_order(user_id, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_order_status_start
ON charging_order(status, start_time);

CREATE TABLE IF NOT EXISTS ops_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    charger_id INTEGER,
    admin_id INTEGER,
    operation TEXT NOT NULL,
    detail TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL,
    FOREIGN KEY(charger_id) REFERENCES charger(id)
        ON UPDATE CASCADE ON DELETE SET NULL,
    FOREIGN KEY(admin_id) REFERENCES admin(id)
        ON UPDATE CASCADE ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_ops_charger_created
ON ops_log(charger_id, created_at DESC);

CREATE TABLE IF NOT EXISTS load_prediction (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id INTEGER NOT NULL,
    generated_at TEXT NOT NULL,
    target_time TEXT NOT NULL,
    horizon_hours INTEGER NOT NULL DEFAULT 24 CHECK(horizon_hours IN (1, 6, 24)),
    predicted_energy REAL NOT NULL CHECK(predicted_energy >= 0),
    predicted_free_chargers INTEGER NOT NULL CHECK(predicted_free_chargers >= 0),
    is_peak INTEGER NOT NULL DEFAULT 0 CHECK(is_peak IN (0, 1)),
    FOREIGN KEY(station_id) REFERENCES station(id)
        ON UPDATE CASCADE ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_prediction_station_target
ON load_prediction(station_id, target_time);

CREATE TABLE IF NOT EXISTS recharge_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    amount REAL NOT NULL CHECK(amount > 0),
    balance_before REAL NOT NULL DEFAULT 0 CHECK(balance_before >= 0),
    balance_after REAL NOT NULL DEFAULT 0 CHECK(balance_after >= 0),
    created_at TEXT NOT NULL,
    FOREIGN KEY(user_id) REFERENCES user(id)
        ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_recharge_user_created
ON recharge_log(user_id, created_at DESC);

CREATE TABLE IF NOT EXISTS charging_review (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    order_id INTEGER NOT NULL UNIQUE,
    user_id INTEGER NOT NULL,
    station_id INTEGER NOT NULL,
    charger_id INTEGER NOT NULL,
    environment_score INTEGER NOT NULL CHECK(environment_score BETWEEN 1 AND 5),
    queue_score INTEGER NOT NULL CHECK(queue_score BETWEEN 1 AND 5),
    equipment_score INTEGER NOT NULL CHECK(equipment_score BETWEEN 1 AND 5),
    parking_score INTEGER NOT NULL CHECK(parking_score BETWEEN 1 AND 5),
    overall_score REAL NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(order_id) REFERENCES charging_order(id)
        ON UPDATE CASCADE ON DELETE RESTRICT,
    FOREIGN KEY(user_id) REFERENCES user(id)
        ON UPDATE CASCADE ON DELETE RESTRICT,
    FOREIGN KEY(station_id) REFERENCES station(id)
        ON UPDATE CASCADE ON DELETE RESTRICT,
    FOREIGN KEY(charger_id) REFERENCES charger(id)
        ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_review_station_id ON charging_review(station_id);
CREATE INDEX IF NOT EXISTS idx_review_user_id ON charging_review(user_id);

CREATE TABLE IF NOT EXISTS user_preference (
    user_id INTEGER PRIMARY KEY,
    home_latitude REAL,
    home_longitude REAL,
    home_radius_km REAL NOT NULL DEFAULT 3.0 CHECK(home_radius_km > 0 AND home_radius_km <= 100),
    reminder_start_time TEXT NOT NULL DEFAULT '08:00',
    reminder_end_time TEXT NOT NULL DEFAULT '22:00',
    min_idle_chargers INTEGER NOT NULL DEFAULT 1 CHECK(min_idle_chargers >= 1),
    dnd_start_time TEXT NOT NULL DEFAULT '22:00',
    dnd_end_time TEXT NOT NULL DEFAULT '07:00',
    enabled INTEGER NOT NULL DEFAULT 1 CHECK(enabled IN (0, 1)),
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CHECK((home_latitude IS NULL AND home_longitude IS NULL)
          OR (home_latitude BETWEEN -90 AND 90 AND home_longitude BETWEEN -180 AND 180)),
    FOREIGN KEY(user_id) REFERENCES user(id) ON UPDATE CASCADE ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS user_favorite_station (
    user_id INTEGER NOT NULL,
    station_id INTEGER NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(user_id, station_id),
    FOREIGN KEY(user_id) REFERENCES user(id) ON UPDATE CASCADE ON DELETE CASCADE,
    FOREIGN KEY(station_id) REFERENCES station(id) ON UPDATE CASCADE ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_favorite_station_station_id
ON user_favorite_station(station_id);

CREATE TABLE IF NOT EXISTS user_preferred_charger_type (
    user_id INTEGER NOT NULL,
    charger_type INTEGER NOT NULL CHECK(charger_type IN (0, 1)),
    PRIMARY KEY(user_id, charger_type),
    FOREIGN KEY(user_id) REFERENCES user(id) ON UPDATE CASCADE ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS vehicle_profile (
    user_id INTEGER PRIMARY KEY,
    battery_capacity_kwh REAL NOT NULL DEFAULT 60
        CHECK(battery_capacity_kwh BETWEEN 1 AND 200),
    target_soc REAL NOT NULL DEFAULT 80
        CHECK(target_soc BETWEEN 10 AND 100),
    min_balance_reserve REAL NOT NULL DEFAULT 5
        CHECK(min_balance_reserve >= 0),
    usual_leave_time TEXT NOT NULL DEFAULT '18:00'
        CHECK(length(usual_leave_time) = 5),
    charge_mode INTEGER NOT NULL DEFAULT 0
        CHECK(charge_mode IN (0, 1, 2)),
    updated_at TEXT NOT NULL DEFAULT '',
    FOREIGN KEY(user_id) REFERENCES user(id)
        ON UPDATE CASCADE ON DELETE CASCADE
);
