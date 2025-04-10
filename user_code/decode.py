import struct

def hex_to_struct(hex_data, struct_description):
    # Convert the hex data to bytes.
    byte_data = bytes.fromhex(hex_data.strip())

    # Create the format string for struct.unpack based on the struct description.
    # '>' is used for big endian (most significant byte is stored in the smallest address).
    # '<' is used for little endian (least significant byte is stored in the smallest address).
    format_string = '<'
    for data_type, _ in struct_description:
        if data_type == 'uint16_t':
            format_string += 'H'  # H is the format code for ushort in struct.
        elif data_type == 'double':
            format_string += 'd'  # d is the format code for double in struct.
        elif data_type == 'float':
            format_string += 'f'  # f is the format code for double in struct.
        else:
            raise ValueError(f"Unsupported data type: {data_type}")

    # Check the total size of the struct.
    expected_size = struct.calcsize(format_string)
    if len(byte_data) != expected_size:
        raise ValueError(f"Expected {expected_size} bytes, but got {len(byte_data)} bytes")

    # Unpack the data.
    values = struct.unpack(format_string, byte_data)

    # Convert the values to a dictionary based on the struct description.
    result = {name: value for (_, name), value in zip(struct_description, values)}

    return result

struct_description = [
    ('float', 'pH_final'),
    ('float', 'temp_final'),
]

struct_size_bytes = 8
hex_data = """04 9e c9 40 7f 84 27 42"""
sensor_names = ["pH_final", "temp_final"]

for i in range(len(sensor_names)):
    hex_data = hex_data.replace(" ", "").replace("\n", "")
    struct_data = hex_data[i*struct_size_bytes*2:(i+1)*struct_size_bytes*2]
    print(sensor_names[i])
    print(hex_to_struct(struct_data, struct_description))

# ---
# =>
# temp
# {'sample_count': 15, 'mean': 23.61246109008789, 'stdev': 0.03030121698975563}
# hum
# {'sample_count': 15, 'mean': 51.35880661010742, 'stdev': 1.1128873825073242}