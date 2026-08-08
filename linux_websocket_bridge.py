import json
import time
import mmap
import struct
import sys
import os
import websocket

SHM_FILE = "/dev/shm/goldmine_tick_shm"
SHM_SIZE = 64
STRUCT_FORMAT_PAYLOAD = "<QddII" # 32 bytes
STRUCT_FORMAT_SEQ = "<Q" # 8 bytes

sequence_id = 0
mapfile = None

def on_message(ws, message):
    global sequence_id, mapfile
    try:
        data = json.loads(message)
        if "b" in data and "a" in data:
            bid = float(data["b"])
            ask = float(data["a"])
            timestamp_ms = int(time.time() * 1000)
            volume = 1 
            sequence_id += 1

            # Write payload to offset 8 FIRST (magic header 0x474F4C44)
            payload = struct.pack(STRUCT_FORMAT_PAYLOAD, timestamp_ms, bid, ask, volume, 0x474F4C44)
            mapfile.seek(8)
            mapfile.write(payload)
            
            # Write Sequence ID to offset 0 LAST
            mapfile.seek(0)
            mapfile.write(struct.pack(STRUCT_FORMAT_SEQ, sequence_id))
            
            print(f"[Binance -> SHM] PAXG/USDT | Bid: {bid:.3f} | Ask: {ask:.3f} | Seq: {sequence_id}")
    except Exception as e:
        print(f"Error parsing message: {e}")

def on_error(ws, error):
    print(f"WebSocket Error: {error}")

def on_close(ws, close_status_code, close_msg):
    print("WebSocket connection closed. Reconnecting...")

def on_open(ws):
    print("[*] Binance WebSocket connected. Streaming PAXG/USDT (Gold Equivalent)...")

def run_ws():
    WS_URL = "wss://stream.binance.com:9443/ws/btcusdt@bookTicker"
    while True:
        ws = websocket.WebSocketApp(WS_URL,
                                  on_open=on_open,
                                  on_message=on_message,
                                  on_error=on_error,
                                  on_close=on_close)
        ws.run_forever()
        time.sleep(2)

def main():
    global mapfile
    
    # 1. Create the physical file if C++ hasn't yet
    if not os.path.exists(SHM_FILE):
        with open(SHM_FILE, "wb") as f:
            f.write(b'\x00' * SHM_SIZE)
            
    # 2. Map directly to the OS filesystem bypassing IPC libraries
    try:
        f = open(SHM_FILE, "r+b")
        mapfile = mmap.mmap(f.fileno(), SHM_SIZE, mmap.MAP_SHARED, mmap.PROT_WRITE)
    except Exception as e:
        print(f"Failed to map OS file: {e}")
        sys.exit(1)

    print(f"[*] Directly mapped to raw filesystem ({SHM_FILE}).")
    run_ws()

if __name__ == "__main__":
    main()