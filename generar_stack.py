# generar_stack.py
import os

# Lista de archivos en el orden que pediste
archivos = [
    "kernel.c",
    "gfx.cpp",
    "paging.cpp",
    "wallpaper.cpp",
    "vbe.cpp",
    "isr.h"
]

# Nombre del archivo de salida
archivo_salida = "stack.log"

with open(archivo_salida, "w", encoding="utf-8") as outfile:
    for nombre in archivos:
        # Escribir el nombre del archivo como encabezado
        outfile.write(f"=== INICIO DE {nombre} ===\n")
        
        if os.path.exists(nombre):
            try:
                with open(nombre, "r", encoding="utf-8") as infile:
                    # Escribir todo el contenido del archivo
                    outfile.write(infile.read())
                    # Asegurar que haya un salto de línea al final si el archivo no lo tiene
                    outfile.write("\n")
            except Exception as e:
                outfile.write(f"[ERROR AL LEER EL ARCHIVO: {e}]\n")
        else:
            outfile.write(f"[ARCHIVO NO ENCONTRADO: {nombre}]\n")
            
        # Separador al final de cada archivo
        outfile.write("*********************\n")
        outfile.write(f"=== FIN DE {nombre} ===\n\n")

print(f"¡Listo! Se ha creado {archivo_salida} con el contenido de {len(archivos)} archivos.")
