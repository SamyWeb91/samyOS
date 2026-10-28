archivos = ["paging.cpp", "usermode.asm", "ring3_test.cpp"]

with open("volcado.txt", "w") as out:
    for f in archivos:
        try:
            with open(f, "r", errors="ignore") as fh:
                contenido = fh.read()
        except FileNotFoundError:
            contenido = "[NO EXISTE]\n"
        out.write("************\n")
        out.write(f"ARCHIVO: {f}\n")
        out.write("************\n")
        out.write(contenido)
        out.write("\n************\n\n")

print("Listo: volcado.txt")
