#!/usr/bin/env python3
"""Vergleicht jede Ansicht des Simulators mit ihrem Referenzbild.

    python3 tools/screenshot_test.py            # pruefen
    python3 tools/screenshot_test.py --update   # Referenzbilder neu erzeugen

Voraussetzung: der Simulator ist gebaut (pio run -e simulator) und Pillow ist
installiert (pip install pillow).

Geprueft wird pro Bild:
  1. Es stimmt mit dem Referenzbild in test/screens/golden/ ueberein.
  2. Nichts wird ausserhalb des runden Displays gezeichnet, und alles haelt
     einen kleinen Abstand zum Rand.
Bei Abweichungen landen das aktuelle Bild und ein Differenzbild in
screenshot_failures/.
"""

import json
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIMULATOR = os.path.join(ROOT, ".pio", "build", "simulator", "program")
SCREENS = os.path.join(ROOT, "test", "screens", "screens.json")
GOLDEN_DIR = os.path.join(ROOT, "test", "screens", "golden")
FAILURE_DIR = os.path.join(ROOT, "screenshot_failures")

SIZE = 466
SAFE_RADIUS = SIZE / 2 - 3      # so nah darf Inhalt an den runden Rand
BLACK_LIMIT = 10                # Helligkeit, bis zu der ein Pixel als schwarz gilt
MAX_CHANNEL_DIFF = 12           # so stark darf ein Pixel vom Referenzbild abweichen
MAX_DIFFERENT_PIXELS = 20       # so viele Pixel duerfen staerker abweichen


def capture(screen, path):
    commands = screen["do"]
    commands = (commands + "," if commands else "") + "rawshot:" + path
    result = subprocess.run(
        [SIMULATOR, "--headless"] + screen.get("args", []) + ["--do", commands],
        capture_output=True, text=True, timeout=120,
    )
    if result.returncode != 0:
        raise RuntimeError("Simulator meldet Fehler: " + result.stderr.strip())
    return Image.open(path).convert("RGB")


def outside_circle(image):
    """Anzahl nicht schwarzer Pixel ausserhalb des sicheren Kreises."""
    pixels = image.load()
    center = SIZE / 2
    count = 0
    for y in range(SIZE):
        for x in range(SIZE):
            dx, dy = x + 0.5 - center, y + 0.5 - center
            if dx * dx + dy * dy > SAFE_RADIUS * SAFE_RADIUS and max(pixels[x, y]) > BLACK_LIMIT:
                count += 1
    return count


def different_pixels(actual, golden):
    """Anzahl Pixel, die staerker als erlaubt abweichen, und das Differenzbild."""
    diff = ImageChops.difference(actual, golden)
    strong = diff.point(lambda value: 255 if value > MAX_CHANNEL_DIFF else 0).convert("L")
    count = strong.histogram()[255]
    return count, strong


def round_view(image):
    """Bild so, wie man es auf dem Geraet sieht (Ecken in Gehaeusefarbe)."""
    view = image.copy()
    pixels = view.load()
    center = SIZE / 2
    for y in range(SIZE):
        for x in range(SIZE):
            dx, dy = x + 0.5 - center, y + 0.5 - center
            if dx * dx + dy * dy > center * center:
                pixels[x, y] = (0x23, 0x28, 0x2A)
    return view


def main(argv):
    update = "--update" in argv
    if not os.path.exists(SIMULATOR):
        print("Simulator nicht gefunden. Zuerst bauen: pio run -e simulator")
        return 2
    with open(SCREENS, encoding="utf-8") as source:
        screens = json.load(source)["screens"]
    names = [screen["name"] for screen in screens]
    if len(names) != len(set(names)):
        print("Doppelte Namen in screens.json")
        return 2

    os.makedirs(GOLDEN_DIR, exist_ok=True)
    failures = []
    with tempfile.TemporaryDirectory() as work:
        for screen in screens:
            name = screen["name"]
            golden_path = os.path.join(GOLDEN_DIR, name + ".png")
            problems = []
            try:
                actual = capture(screen, os.path.join(work, name + ".bmp"))
            except Exception as error:  # Absturz oder unbekannter Befehl
                failures.append((name, [str(error)]))
                print("FEHLER  %s: %s" % (name, error))
                continue

            if actual.size != (SIZE, SIZE):
                problems.append("falsche Groesse %s" % (actual.size,))
            outside = outside_circle(actual)
            if outside:
                problems.append("%d Pixel ausserhalb des runden Displays" % outside)

            diff_image = None
            if update:
                actual.save(golden_path, optimize=True)
            elif not os.path.exists(golden_path):
                problems.append("kein Referenzbild (mit --update erzeugen)")
            else:
                golden = Image.open(golden_path).convert("RGB")
                count, diff_image = different_pixels(actual, golden)
                if count > MAX_DIFFERENT_PIXELS:
                    problems.append("%d Pixel weichen vom Referenzbild ab" % count)

            if problems:
                failures.append((name, problems))
                os.makedirs(FAILURE_DIR, exist_ok=True)
                round_view(actual).save(os.path.join(FAILURE_DIR, name + "_ist.png"))
                if diff_image is not None:
                    diff_image.save(os.path.join(FAILURE_DIR, name + "_differenz.png"))
                print("FEHLER  %s: %s" % (name, "; ".join(problems)))
            else:
                print("ok      %s" % name)

    # Referenzbilder ohne Eintrag sind vergessene Altlasten.
    stale = sorted(set(f[:-4] for f in os.listdir(GOLDEN_DIR) if f.endswith(".png")) - set(names))
    for name in stale:
        if update:
            os.remove(os.path.join(GOLDEN_DIR, name + ".png"))
        else:
            failures.append((name, ["Referenzbild ohne Eintrag in screens.json"]))
            print("FEHLER  %s: Referenzbild ohne Eintrag in screens.json" % name)

    print()
    if failures:
        print("%d von %d Bildern fehlerhaft. Details in %s" % (len(failures), len(screens), FAILURE_DIR))
        return 1
    print("%d Bilder %s." % (len(screens), "neu erzeugt" if update else "stimmen"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
