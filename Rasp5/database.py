import json
import time
import sqlite3
import paho.mqtt.client as mqtt

# --- MQTT Configuration ---
BROKER_IP   = "127.0.0.1"
BROKER_PORT = 1883
MQTT_TOPIC  = "esp32/sensors"
SAVE_INTERVAL = 2.0  # seconds between writes, per device

# --- SQLite Configuration ---
DB_PATH = "sensor_data.db"

# Single connection, shared across callbacks (safe here since paho runs
# on one background thread, and we only touch the DB from on_message)
db_conn = sqlite3.connect(DB_PATH, check_same_thread=False)
db_cursor = db_conn.cursor()

known_tables = set()     # tables already created this run
buffered_rows = {}       # {device_id: [(temp, hum, ext, ts_ms), ...]}
last_saved_at = {}       # {device_id: last flush timestamp}


def safe_table_name(device_id):
    """Sanitize device_id into a valid SQLite table name."""
    return "device_" + "".join(c if c.isalnum() else "_" for c in device_id)


def ensure_table(table_name):
    """Creates the device's table if it doesn't exist yet."""
    if table_name in known_tables:
        return
    db_cursor.execute(f"""
        CREATE TABLE IF NOT EXISTS "{table_name}" (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            temperatureSensor_val INTEGER,
            humiditySensor_val INTEGER,
            extensometerSensor_val INTEGER,
            timestamp_ms INTEGER,
            received_at REAL DEFAULT (strftime('%s','now'))
        )
    """)
    db_conn.commit()
    known_tables.add(table_name)


def flush_device(device_id, table_name):
    """Writes all buffered rows for a device to its table."""
    rows = buffered_rows.get(device_id, [])
    if not rows:
        return
    try:
        db_cursor.executemany(
            f'INSERT INTO "{table_name}" '
            f'(temperatureSensor_val, humiditySensor_val, extensometerSensor_val, timestamp_ms) '
            f'VALUES (?, ?, ?, ?)',
            rows
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

        row = (
            decoded_data.get("temperatureSensor_val"),
            decoded_data.get("humiditySensor_val"),
            decoded_data.get("extensometerSensor_val"),
            decoded_data.get("timestamp_ms"),
        )

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