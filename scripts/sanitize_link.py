# Die Sanitizer muessen auch beim Linken aktiv sein, nicht nur beim Kompilieren.
Import("env")

env.Append(LINKFLAGS=["-fsanitize=address,undefined"])
