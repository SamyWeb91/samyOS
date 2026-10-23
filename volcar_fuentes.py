#!/usr/bin/env python3
"""
Volca el contenido de varios archivos a un único .txt, separados por '********'.
Los archivos se buscan en el mismo directorio donde está este script.
"""

import os
import sys

# Archivos a volcar, en el orden pedido
ARCHIVOS = [
    "tss.cpp",
    "tss.asm",
    "ring3_test.cpp",
    "usermode.asm",
    "gdt.cpp",
    "gdt.asm",
    "paging.cpp",
    "paging.asm",
]

SALIDA = "volcado.txt"
SEPARADOR = "********"


def main() -> int:
    # Directorio del script (funciona también si se ejecuta como .py o congelado)
    base_dir = os.path.dirname(os.path.abspath(__file__))
    out_path = os.path.join(base_dir, SALIDA)

    encontrados = 0
    faltantes = []

    with open(out_path, "w", encoding="utf-8", errors="replace") as out:
        for i, nombre in enumerate(ARCHIVOS):
            ruta = os.path.join(base_dir, nombre)

            if i > 0:
                out.write("\n" + SEPARADOR + "\n\n")

            out.write(f"===== {nombre} =====\n\n")

            if not os.path.isfile(ruta):
                out.write(f"[ARCHIVO NO ENCONTRADO: {ruta}]\n")
                faltantes.append(nombre)
                continue

            try:
                with open(ruta, "r", encoding="utf-8", errors="replace") as f:
                    contenido = f.read()
                out.write(contenido)
                if not contenido.endswith("\n"):
                    out.write("\n")
                encontrados += 1
            except OSError as e:
                out.write(f"[ERROR AL LEER {nombre}: {e}]\n")
                faltantes.append(nombre)

    print(f"Volcado generado: {out_path}")
    print(f"Archivos incluidos: {encontrados}/{len(ARCHIVOS)}")
    if faltantes:
        print("No encontrados / con error:")
        for f in faltantes:
            print(f"  - {f}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
