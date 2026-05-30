import os
import re

def fix_blinker_colors(file_path):
    if not os.path.exists(file_path):
        print(f"File {file_path} not found.")
        return

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Locate the array block matching the data layout
    array_match = re.search(r'([^\n]*uint8_t\s+\w+\[\]\s*=\s*\{)([^}]+)\}', content)
    if not array_match:
        print(f"Error: Could not find data array in {file_path}")
        return

    heading_part = array_match.group(1)
    hex_data_str = array_match.group(2)
    
    tokens = re.findall(r'0x[0-9a-fA-F]+', hex_data_str)
    bytes_data = [int(t, 16) for t in tokens]

    # Process chunks of 3 bytes (Color_Byte_A, Color_Byte_B, Alpha_Byte)
    fixed_bytes = []
    for i in range(0, len(bytes_data), 3):
        if i + 2 < len(bytes_data):
            b0 = bytes_data[i]
            b1 = bytes_data[i+1]
            alpha = bytes_data[i+2]
            # Swap the two color bytes, leave the alpha channel untouched
            fixed_bytes.extend([b1, b0, alpha])
        else:
            fixed_bytes.extend(bytes_data[i:])

    # Format back to a clean C array structure
    formatted_lines = []
    current_line = []
    for b in fixed_bytes:
        current_line.append(f"0x{b:02x}")
        if len(current_line) == 12:
            formatted_lines.append(", ".join(current_line) + ",")
            current_line = []
    if current_line:
        formatted_lines.append(", ".join(current_line))

    new_array_content = "\n  " + "\n  ".join(formatted_lines) + "\n"
    new_array_string = heading_part + new_array_content + "}"

    content = content.replace(array_match.group(0), new_array_string)

    # Overwrite the file with the corrected version
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Successfully fixed color bytes for {file_path}")

# Run the fix over your files
target_files = ["left_blinker.c", "right_blinker.c"]
for target in target_files:
    fix_blinker_colors(target)