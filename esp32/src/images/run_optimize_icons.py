import os
import re

def autocrop_lvgl_icon(file_path):
    if not os.path.exists(file_path):
        print(f"File {file_path} not found.")
        return

    print(f"Optimizing {file_path}...")
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Automatically find Width and Height at the bottom of the file
    w_match = re.search(r'\bw\s*=\s*(\d+)', content)
    h_match = re.search(r'\bh\s*=\s*(\d+)', content)
    
    if not w_match or not h_match:
        print(f"Error: Could not extract width/height dimensions from {file_path}.")
        return

    w = int(w_match.group(1))
    h = int(h_match.group(1))

    # 2. Extract the raw hexadecimal map data along with its full declaration header
    # Group 1 captures everything on the line up to the '{' (e.g., const ... uint8_t map[] = {)
    # Group 2 captures the raw hex tokens inside
    array_match = re.search(r'([^\n]*uint8_t\s+\w+\[\]\s*=\s*\{)([^}]+)\}', content)
    if not array_match:
        print(f"Error: Could not locate the pixel data array in {file_path}.")
        return

    heading_part = array_match.group(1)
    hex_data_str = array_match.group(2)
    
    tokens = re.findall(r'0x[0-9a-fA-F]+', hex_data_str)
    bytes_data = [int(t, 16) for t in tokens]

    # True Color Alpha = 3 bytes per pixel: [Alpha, RGB_low, RGB_high]
    pixel_size = 3
    expected_bytes = w * h * pixel_size
    if len(bytes_data) != expected_bytes:
        print(f"Error: Dimensions mismatch. Expected {expected_bytes} bytes, found {len(bytes_data)}.")
        return

    # 3. Parse bytes into a 2D grid of pixels
    grid = []
    idx = 0
    for r in range(h):
        row = []
        for c in range(w):
            row.append(bytes_data[idx:idx+pixel_size])
            idx += pixel_size
        grid.append(row)

    # 4. Scan the grid to find the boundary box of the actual icon graphic (Alpha > 0)
    min_r, max_r = h, -1
    min_c, max_c = w, -1

    for r in range(h):
        for c in range(w):
            alpha = grid[r][c][0] # The first byte of each pixel group is Alpha
            if alpha > 0:
                if r < min_r: min_r = r
                if r > max_r: max_r = r
                if c < min_c: min_c = c
                if c > max_c: max_c = c

    if max_r == -1:
        print(f"Skipping {file_path} (image is completely transparent).")
        return

    # 5. Crop grid down to content bounds
    new_w = max_c - min_c + 1
    new_h = max_r - min_r + 1

    cropped_grid = []
    for r in range(min_r, max_r + 1):
        cropped_grid.append(grid[r][min_c:max_c + 1])

    # Flatten back to a single list of bytes
    new_bytes = []
    for row in cropped_grid:
        for pixel in row:
            new_bytes.extend(pixel)

    # 6. Format the new byte list into structured C arrays
    formatted_lines = []
    current_line = []
    for b in new_bytes:
        current_line.append(f"0x{b:02x}")
        if len(current_line) == 12:
            formatted_lines.append(", ".join(current_line) + ",")
            current_line = []
    if current_line:
        formatted_lines.append(", ".join(current_line))

    new_array_content = "\n  " + "\n  ".join(formatted_lines) + "\n"
    new_array_string = heading_part + new_array_content + "}"

    # Swap out the old array block for the cropped array block
    content = content.replace(array_match.group(0), new_array_string)

    # 7. Dynamically update Width, Height, and Data Size in the bottom structure
    content = re.sub(r'(\bw\s*=\s*)\d+', r'\g<1>' + str(new_w), content)
    content = re.sub(r'(\bh\s*=\s*)\d+', r'\g<1>' + str(new_h), content)
    content = re.sub(r'(\bdata_size\s*=\s*)\d+\s*\*\s*\d+\s*\*\s*\d+', r'\g<1>' + f"{new_w} * {new_h} * {pixel_size}", content)
    content = re.sub(r'(\bdata_size\s*=\s*)\d+', r'\g<1>' + str(len(new_bytes)), content)

    # Output optimized version
    output_name = "fixed_" + os.path.basename(file_path)
    with open(output_name, 'w', encoding='utf-8') as f:
        f.write(content)

    saved_pct = (1 - (len(new_bytes) / len(bytes_data))) * 100
    print(f"Saved to {output_name}")
    print(f"  Dimensions: {w}x{h} -> {new_w}x{new_h}")
    print(f"  Size Reduced By: {saved_pct:.1f}%\n")

# Execute optimization on your file names
target_files = ["headlight.c", "left_blinker.c", "right_blinker.c"]
for target in target_files:
    autocrop_lvgl_icon(target)