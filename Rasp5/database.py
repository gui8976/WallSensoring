import json
import time
import sqlite3
import paho.mqtt.client as mqtt

# --- MQTT Configuration ---
BROKER_IP   = "127.0.0.1"
BROKER_PORT = 1883
MQTT_TOPIC  = "esp32/sensorReadings"
SAVE_INTERVAL = 2.0  # seconds between writes, per device

# --- SQLite Configuration ---
DB_PATH = "sensor_data.db"

# Single connection, shared across callbacks (safe here since paho runs
# on one background thread, and we only touch the DB from on_message)
db_conn = sqlite3.connect(DB_PATH, check_same_thread=False)
db_cursor = db_conn.cursor()

known_tables = {}        # {table_name: set(existing_column_names)}
buffered_rows = {}       # {device_id: [ {col: value, ...}, ... ]}
last_saved_at = {}       # {device_id: last flush timestamp}

# Any key in the payload other than these is treated as a sensor reading.
# This is what lets each message carry just one sensor, or two (e.g. the
# combined temperature+humidity sensor), in any combination.
RESERVED_KEYS = {"device_id", "timestamp_ms"}


def safe_table_name(device_id):
    """Sanitize device_id into a valid SQLite table name."""
    return "device_" + "".join(c if c.isalnum() else "_" for c in device_id)


def safe_column_name(key):
    """Sanitize a sensor key into a valid SQLite column name."""
    return "".join(c if c.isalnum() else "_" for c in key)


def ensure_table(table_name):
    """Creates the device's table if it doesn't exist yet, and loads its
    current column set so we know what's already there."""
    if table_name in known_tables:
        return
    db_cursor.execute(f"""
        CREATE TABLE IF NOT EXISTS "{table_name}" (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp_ms INTEGER,
            received_at REAL DEFAULT (strftime('%s','now'))
        )
    """)
    db_conn.commit()
    db_cursor.execute(f'PRAGMA table_info("{table_name}")')
    existing_cols = {row[1] for row in db_cursor.fetchall()}
    known_tables[table_name] = existing_cols


def ensure_columns(table_name, keys):
    """Adds any sensor columns that haven't been seen yet for this device."""
    existing = known_tables[table_name]
    for key in keys:
        col = safe_column_name(key)
        if col not in existing:
            try:
                db_cursor.execute(f'ALTER TABLE "{table_name}" ADD COLUMN "{col}" REAL')
                db_conn.commit()
                existing.add(col)
                print(f"[SQLite] Added new column '{col}' to table '{table_name}'")
            except sqlite3.OperationalError as e:
                # Could happen if two new sensor names race each other; safe to ignore.
                print(f"[Warn] Could not add column '{col}' to '{table_name}': {e}")


def flush_device(device_id, table_name):
    """Writes all buffered rows for a device to its table.
    Rows may have different sets of keys (different sensors reported at
    different moments), so we build the column list as the union of keys
    across the buffered rows and let missing values fall back to NULL."""
    rows = buffered_rows.get(device_id, [])
    if not rows:
        return
    try:
        all_keys = set()
        for r in rows:
            all_keys.update(r.keys())
        all_keys.discard("timestamp_ms")
        col_names = ["timestamp_ms"] + sorted(all_keys)

        col_list = ",".join(f'"{c}"' for c in col_names)
        placeholders = ",".join(["?"] * len(col_names))
        values = [tuple(r.get(c) for c in col_names) for r in rows]

        db_cursor.executemany(
            f'INSERT INTO "{table_name}" ({col_list}) VALUES ({placeholders})',
            values
        )
        db_conn.commit()
        print(f"[SQLite] Wrote {len(rows)} row(s) to table '{table_name}'")
        buffered_rows[device_id] = []
    except Exception as e:
        print(f"[Error] Failed to write to SQLite for {device_id}: {e}")


def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("[Pi 5] Connected to MQTT Broker successfully!")
        client.subscribe(MQTT_TOPIC)
        print(f"[Pi 5] Subscribed to topic: '{MQTT_TOPIC}'\n")
    else:
        print(f"[Pi 5] Connection failed with result code {rc}")


def on_message(client, userdata, msg):
    try:
        raw_payload = msg.payload.decode("utf-8")
        decoded_data = json.loads(raw_payload)

        device_id = decoded_data.get("device_id", "unknown_device")
        table_name = safe_table_name(device_id)
        ensure_table(table_name)

        # Whatever sensor keys are present in THIS message (could be 1, 2, or more)
        sensor_keys = [k for k in decoded_data.keys() if k not in RESERVED_KEYS]
        if not sensor_keys:
            print(f"[Warn] Message from {device_id} had no sensor fields, skipping.")
            return

        ensure_columns(table_name, sensor_keys)

        row = {safe_column_name(k): decoded_data.get(k) for k in sensor_keys}
        row["timestamp_ms"] = decoded_data.get("timestamp_ms")

        buffered_rows.setdefault(device_id, [])
        buffered_rows[device_id].append(row)

        print("--------------------------------------------------")
        print(f"Received from {device_id}: {decoded_data}")
        print(f"Buffered rows for {device_id}: {len(buffered_rows[device_id])}")

        now = time.time()
        if now - last_saved_at.get(device_id, 0) >= SAVE_INTERVAL:
            flush_device(device_id, table_name)
            last_saved_at[device_id] = now

    except json.JSONDecodeError:
        print("[Error] Failed to parse JSON. Raw message:", msg.payload)
    except Exception as e:
        print(f"[Error] Exception occurred: {e}")


client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="RaspberryPi5_Subscriber")

client.on_connect = on_connect
client.on_message = on_message

print("[Pi 5] Connecting to MQTT broker...")
client.connect(BROKER_IP, BROKER_PORT, keepalive=60)
client.loop_start()

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("\n[Pi 5] Stopping subscriber...")
    for device_id, table_name in [(d, safe_table_name(d)) for d in buffered_rows]:
        flush_device(device_id, table_name)
    client.loop_stop()
    client.disconnect()
    db_conn.close()
