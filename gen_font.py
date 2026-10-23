# guardar como gen_font.py
import re

# Lee el archivo font8x16.c (el original)
with open('font8x16.c', 'r') as f:
    content = f.read()

# Extrae todos los valores hex { 0x.., 0x.., ... } con regex
hex_values = re.findall(r'0x([0-9A-Fa-f]{2})', content)

# Toma los primeros 256*16 bytes (4096 bytes)
font_data = bytearray([int(x, 16) for x in hex_values[:4096]])

# Si faltan datos, rellena con ceros
if len(font_data) < 4096:
    font_data.extend([0] * (4096 - len(font_data)))

with open('font_ext.bin', 'wb') as f:
    f.write(font_data)

print("font_ext.bin generado con", len(font_data), "bytes")
