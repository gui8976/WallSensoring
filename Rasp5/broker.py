import asyncio
import logging
from amqtt.broker import Broker
from amqtt.client import MQTTClient

logging.basicConfig(level=logging.INFO)

config = {
    'listeners': {
        'default': {
            'type': 'tcp',
            'bind': '0.0.0.0:1883',
        },
    },
    'sys_interval': 10,
    'auth': {
        'allow-anonymous': True,
        'plugins': ['auth_anonymous'],
    },
}

async def listen_for_messages():
    await asyncio.sleep(1)  # Allow broker time to start
    client = MQTTClient()
    await client.connect('mqtt://127.0.0.1:1883/')
    # '#' subscribes to all topics
    await client.subscribe([('#', 0)])
    print("[Listener] Subscribed to all topics. Waiting for incoming data...\n")
    
    while True:
        message = await client.deliver_message()
        packet = message.publish_packet
        topic = packet.variable_header.topic_name
        payload = packet.payload.data.decode('utf-8', errors='ignore')
        print(f">>> [MQTT MESSAGE] Topic: '{topic}' | Payload: {payload}")

async def main():
    broker = Broker(config)
    await broker.start()
    print("MQTT Broker running on port 1883...")
    
    # Start the subscriber task in the background
    asyncio.create_task(listen_for_messages())
    
    while True:
        await asyncio.sleep(3600)

if __name__ == '__main__':
    asyncio.run(main())