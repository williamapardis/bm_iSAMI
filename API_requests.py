import requests
import pandas as pd
from datetime import datetime
import struct

def fetch_sofar_data(token, spotter_id):
    url = f"https://api.sofarocean.com/api/sensor-data"
    params = {
        "token": token,
        "spotterId": spotter_id
    }
    
    response = requests.get(url, params=params)
    return response.json()

def parse_hex_value(hex_value):
    # Convert hex string to float values using struct
    value1 = struct.unpack('f', bytes.fromhex(hex_value[0:8]))[0]
    value2 = struct.unpack('f', bytes.fromhex(hex_value[8:16]))[0]
    return value1, value2

def parse_sensor_data(data):
    # Create DataFrame from response
    df = pd.DataFrame(data['data'])
    
    # Convert timestamp strings to datetime objects
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    
    # Parse hex values into separate columns
    df[['pH','temperature']] = df.apply(
        lambda row: pd.Series(parse_hex_value(row['value'])), 
        axis=1
    )
    
    # Structure the data
    structured_data = {
        'spotter_id': data['spotterId'],
        'status': data['status'],
        'measurements': df.to_dict('records')
    }
    
    return structured_data

# Example usage
token = "9c2ee45af58ad0ad302ab5c347323c"
spotter_id = "SPOT-31562C"

raw_data = fetch_sofar_data(token, spotter_id)
structured_data = parse_sensor_data(raw_data)

# Access the structured data
print(f"Spotter ID: {structured_data['spotter_id']}")
print(f"Status: {structured_data['status']}")
print(f"Number of measurements: {len(structured_data['measurements'])}")

# Print individual measurements
for measurement in structured_data['measurements']:
    print(f"\nTimestamp: {measurement['timestamp']}")
    print(f"Location: ({measurement['latitude']}, {measurement['longitude']})")
    print(f"pH: {measurement['pH']:.3f}")
    print(f"Temperature: {measurement['temperature']:.3f}°C")