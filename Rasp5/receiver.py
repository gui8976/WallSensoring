import json
import time
import paho.mqtt.client as mqtt

# --- Configuration ---
BROKER_IP   = "127.0.0.1"  # "localhost" if the broker script runs on the same Pi 5
BROKER_PORT = 1883
MQTT_TOPIC  = "esp32/sensors"

# --- In-Memory Storage Array ---
sensor_data_history = []


def on_connect(client, userdata, flags, rc, properties=None):
    """Callback triggered when the client connects to the broker."""
    if rc == 0:
        print("[Pi 5] Connected to MQTT Broker successfully!")
        client.subscribe(MQTT_TOPIC)
        print(f"[Pi 5] Subscribed to topic: '{MQTT_TOPIC}'\n")
    else:
        print(f"[Pi 5] Connection failed with result code {rc}")


def on_message(client, userdata, msg):
    """Callback triggered whenever a message arrives from an ESP32."""
    try:
        # 1. Decode raw byte payload into a UTF-8 string
        raw_payload = msg.payload.decode("utf-8")

        # 2. Parse the JSON string into a Python dictionary
        decoded_data = json.loads(raw_payload)

        # 3. Store the dictionary in our array (list)
        sensor_data_history.append(decoded_data)

        # 4. Print live confirmation and total array size
        print("--------------------------------------------------")
        print(f"Received from ESP32 : {decoded_data}")
        print(f"Extracted Temperature Sensor : {decoded_data.get('temperatureSensor_val')}")
        print(f"Extracted Humidity Sensor : {decoded_data.get('humiditySensor_val')}")
        print(f"Extracted Extensometer Sensor : {decoded_data.get('extensometerSensor_val')}")
        print(f"Total entries stored in array: {len(sensor_data_history)}")

    except json.JSONDecodeError:
        print("[Error] Failed to parse JSON. Raw message:", msg.payload)
    except Exception as e:
        print(f"[Error] Exception occurred: {e}")


# --- Setup Paho MQTT Client ---
# Using MQTTv5 API / Callback API v2 compatible structure
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="RaspberryPi5_Subscriber")
client.on_connect = on_connect
client.on_message = on_message

print("[Pi 5] Connecting to MQTT broker...")
client.connect(BROKER_IP, BROKER_PORT, keepalive=60)

# Start network loop in non-blocking mode
client.loop_start()

# Keep script alive
try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("\n[Pi 5] Stopping subscriber...")
    client.loop_stop()
    client.disconnect()