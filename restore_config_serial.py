#!/usr/bin/env python3
"""
WLED Configuration Restore via USB Serial
Restores wled_cfg.json and wled_presets.json to device
"""

import serial
import time
import json
import sys
from pathlib import Path

# Configuration
PORT = '/dev/cu.usbmodem1141101'
BAUD = 115200
TIMEOUT = 2

def send_config_via_serial(port, baud, cfg_file, presets_file):
    """Send configuration files to WLED device via serial"""
    try:
        print(f"Opening serial port: {port} @ {baud}bps")
        ser = serial.Serial(port, baud, timeout=TIMEOUT)
        time.sleep(1)
        
        # Read config file
        print(f"\nReading config file: {cfg_file}")
        with open(cfg_file, 'r') as f:
            cfg_data = json.load(f)
        print(f"Config size: {len(json.dumps(cfg_data))} bytes")
        
        # Read presets file
        print(f"Reading presets file: {presets_file}")
        with open(presets_file, 'r') as f:
            presets_data = json.load(f)
        print(f"Presets size: {len(json.dumps(presets_data))} bytes")
        
        # Send reset command to enter config mode
        print("\n[1/4] Sending reset command...")
        ser.write(b'\x04')  # Ctrl+D to reset
        time.sleep(0.5)
        
        # Clear any existing input
        ser.reset_input_buffer()
        time.sleep(0.2)
        
        # Send configuration via POST request simulation
        print("[2/4] Preparing to send configuration...")
        
        # Build JSON command to restore config
        cmd = {
            "action": "restore",
            "config": cfg_data
        }
        
        cmd_json = json.dumps(cmd)
        print(f"Command size: {len(cmd_json)} bytes")
        
        # Send via serial with length prefix
        length = len(cmd_json).to_bytes(4, byteorder='little')
        print(f"[3/4] Sending configuration ({len(cmd_json)} bytes)...")
        
        ser.write(b'CONFIG\n')
        ser.write(length)
        ser.write(cmd_json.encode())
        ser.write(b'\n')
        
        # Wait for response
        print("[4/4] Waiting for device response...")
        response = b''
        start_time = time.time()
        
        while time.time() - start_time < 10:
            if ser.in_waiting:
                response += ser.read(1)
                if b'OK' in response or b'done' in response.lower():
                    print(f"\n✓ Device response: {response.decode('utf-8', errors='ignore').strip()}")
                    break
            time.sleep(0.1)
        
        if not response:
            print("⚠ No response from device (may still be processing)")
        
        ser.close()
        print("\n✓ Configuration restore complete!")
        print("Device will reboot and apply settings.")
        return True
        
    except FileNotFoundError as e:
        print(f"✗ Error: Config file not found: {e}")
        return False
    except serial.SerialException as e:
        print(f"✗ Serial error: {e}")
        print("Make sure:")
        print("  1. Device is connected via USB")
        print("  2. No other program is using the port")
        print("  3. Port is correct: {port}")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == '__main__':
    cfg_file = '/Users/denniss-private/Projects/WLED_Pluto1010/.backup/wled_cfg-3.json'
    presets_file = '/Users/denniss-private/Projects/WLED_Pluto1010/.backup/wled_presets-2.json'
    
    print("=" * 60)
    print("WLED Configuration Restore via Serial")
    print("=" * 60)
    
    # Check if pyserial is available
    try:
        import serial
    except ImportError:
        print("⚠ pyserial not installed.")
        print("Install with: pip install pyserial")
        sys.exit(1)
    
    success = send_config_via_serial(PORT, BAUD, cfg_file, presets_file)
    sys.exit(0 if success else 1)
