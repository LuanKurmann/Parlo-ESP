# Bettet die Web-Oberflaeche (Ordner web/) als C-Datei in die Firmware ein.
# Laeuft automatisch vor dem Bau der Firmware (siehe platformio.ini).
import os

Import("env")  # noqa: F821  (von PlatformIO bereitgestellt)

ROOT = env["PROJECT_DIR"]  # noqa: F821
WEB = os.path.join(ROOT, "web")
OUT = os.path.join(ROOT, "src", "device", "web_files.h")
TYPES = {".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".css": "text/css; charset=utf-8"}

lines = [
    "// Automatisch erzeugt von scripts/embed_web.py. Nicht von Hand aendern.",
    "#pragma once",
    "",
    "#include <cstddef>",
    "#include <cstdint>",
    "",
    "struct WebFile {",
    "    const char* path;",
    "    const char* content_type;",
    "    const uint8_t* data;",
    "    size_t size;",
    "};",
    "",
]
entries = []
for index, name in enumerate(sorted(os.listdir(WEB))):
    extension = os.path.splitext(name)[1]
    if extension not in TYPES:
        continue  # Tests und anderes gehoeren nicht aufs Geraet
    with open(os.path.join(WEB, name), "rb") as source:
        data = source.read()
    body = ",".join(str(byte) for byte in data)
    lines.append("static const uint8_t kWebData%d[] = {%s};" % (index, body))
    entries.append('    {"/%s", "%s", kWebData%d, %d},' % (name, TYPES[extension], index, len(data)))
lines += ["", "static const WebFile kWebFiles[] = {"] + entries + ["};", "static const size_t kWebFileCount = %d;" % len(entries), ""]

content = "\n".join(lines)
if not os.path.exists(OUT) or open(OUT).read() != content:
    with open(OUT, "w") as target:
        target.write(content)
