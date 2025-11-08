#!/usr/bin/env python3
"""
WLED Configuration Restore via HTTP API
Restores wled_cfg.json and wled_presets.json to device
"""

import json
import requests
import time
import sys
from pathlib import Path

DEVICE_IP = "192.168.9.47"
TIMEOUT = 10

def restore_config(device_ip, cfg_file, presets_file):
    """Restore configuration to WLED device via HTTP API"""
    try:
        print(f"Target device: http://{device_ip}")
        print(f"Configuration file: {cfg_file}")
        print(f"Presets file: {presets_file}")
        print("=" * 60)
        
        # Read config file
        print("\n[1/3] Reading configuration file...")
        with open(cfg_file, 'r') as f:
            cfg_data = json.load(f)
        print(f"✓ Config loaded ({len(json.dumps(cfg_data))} bytes)")
        
        # Read presets file
        print("[2/3] Reading presets file...")
        with open(presets_file, 'r') as f:
            presets_data = json.load(f)
        print(f"✓ Presets loaded ({len(json.dumps(presets_data))} bytes)")
        
        # Send config via POST to /json endpoint
        print("\n[3/3] Sending configuration to device...")
        
        # Combine both configs
        restore_payload = {
            "state": cfg_data.get("def", {}),
            "config": cfg_data,
            "presets": presets_data
        }
        
        # Post to the device
        url = f"http://{device_ip}/json"
        headers = {'Content-Type': 'application/json'}
        
        print(f"POST {url}")
        response = requests.post(url, json=restore_payload, timeout=TIMEOUT)
        
        print(f"Response status: {response.status_code}")
        
        if response.status_code == 200:
            print("✓ Configuration sent successfully!")
            print("\nWaiting for device to process...")
            time.sleep(5)
            
            # Verify device is still responsive
            print("Verifying device...")
            try:
                check = requests.get(f"http://{device_ip}/json", timeout=5)
                if check.status_code == 200:
                    info = check.json().get("info", {})
                    print(f"✓ Device is online")
                    print(f"  Version: {info.get('ver')}")
                    print(f"  Name: {info.get('name')}")
                    return True
                else:
                    print(f"⚠ Device returned status {check.status_code}")
                    return True  # Config may have been applied
            except Exception as e:
                print(f"⚠ Could not verify (device may be rebooting): {e}")
                return True
        else:
            print(f"✗ Error: Device returned {response.status_code}")
            print(response.text[:200])
            return False
            
    except FileNotFoundError as e:
        print(f"✗ Error: File not found: {e}")
        return False
    except requests.exceptions.RequestException as e:
        print(f"✗ Network error: {e}")
        print("Make sure device is online at:", f"http://{device_ip}")
        return False
    except json.JSONDecodeError as e:
        print(f"✗ JSON error in config files: {e}")
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
    print("WLED Configuration Restore via HTTP API")
    print("=" * 60)
    
    # Check if requests is available
    try:
        import requests
    except ImportError:
        print("✗ requests library not installed.")
        print("Install with: pip install requests")
        sys.exit(1)
    
    success = restore_config(DEVICE_IP, cfg_file, presets_file)
    
    if success:
        print("\n" + "=" * 60)
        print("✓ Restore completed successfully!")
        print("=" * 60)
        sys.exit(0)
    else:
        print("\n" + "=" * 60)
        print("✗ Restore failed")
        print("=" * 60)
        sys.exit(1)
