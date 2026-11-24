#!/usr/bin/env python3
"""Wandelt eine GPX-Aufzeichnung (z. B. vom Handy) in eine NMEA-Datei um,
die der Simulator mit --replay abspielen kann.

    python3 tools/gpx_to_nmea.py runde.gpx runde.nmea

Die Punkte werden auf eine Messung pro Sekunde umgerechnet, so wie der
GPS-Empfaenger des Geraets sie liefert. Punkte ohne Zeitangabe gelten als
eine Sekunde auseinander.
"""

import sys
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta, timezone


def checksum(body):
    value = 0
    for char in body.encode("ascii"):
        value ^= char
    return "%02X" % value


def sentence(body):
    return "$%s*%s\r\n" % (body, checksum(body))


def coordinate(degrees, degree_digits, positive, negative):
    hemisphere = negative if degrees < 0 else positive
    total = round(abs(degrees) * 60 * 100000)  # hunderttausendstel Minuten
    whole, minute_part = divmod(total, 6000000)
    return "%0*d%02d.%05d,%s" % (degree_digits, whole, minute_part // 100000, minute_part % 100000, hemisphere)


def burst(when, lat, lon, elevation):
    time = when.strftime("%H%M%S.000")
    position = coordinate(lat, 2, "N", "S") + "," + coordinate(lon, 3, "E", "W")
    gga = "GNGGA,%s,%s,1,12,0.90,%.1f,M,0.0,M,," % (time, position, elevation)
    rmc = "GNRMC,%s,A,%s,0.00,0.00,%s,,,A,V" % (time, position, when.strftime("%d%m%y"))
    return sentence(gga) + sentence(rmc)


def read_points(gpx_text):
    """Liefert [(Zeit, Breite, Laenge, Hoehe)], sortiert nach Zeit."""
    root = ET.fromstring(gpx_text)
    points = []
    fallback = datetime(2026, 1, 1, 8, 0, 0, tzinfo=timezone.utc)
    for element in root.iter():
        if not element.tag.endswith("trkpt"):
            continue
        lat, lon = float(element.attrib["lat"]), float(element.attrib["lon"])
        if not (-90 <= lat <= 90 and -180 <= lon <= 180):
            raise ValueError("Koordinate ausserhalb des gueltigen Bereichs: %s, %s" % (lat, lon))
        when, elevation = None, 0.0
        for child in element:
            if child.tag.endswith("time") and child.text:
                when = datetime.fromisoformat(child.text.strip().replace("Z", "+00:00"))
                if when.tzinfo is None:
                    when = when.replace(tzinfo=timezone.utc)
                when = when.astimezone(timezone.utc)
            elif child.tag.endswith("ele") and child.text:
                elevation = float(child.text)
        if when is None:
            when = fallback + timedelta(seconds=len(points))
        points.append((when, lat, lon, elevation))
    points.sort(key=lambda p: p[0])
    return points


def resample(points):
    """Eine Position pro Sekunde, zwischen den Punkten geradlinig verbunden."""
    if not points:
        return []
    result = []
    index = 0
    when = points[0][0].replace(microsecond=0)
    end = points[-1][0]
    while when <= end:
        while index + 1 < len(points) and points[index + 1][0] <= when:
            index += 1
        t0, lat0, lon0, ele0 = points[index]
        if index + 1 < len(points) and when > t0:
            t1, lat1, lon1, ele1 = points[index + 1]
            f = (when - t0).total_seconds() / (t1 - t0).total_seconds()
            result.append((when, lat0 + (lat1 - lat0) * f, lon0 + (lon1 - lon0) * f, ele0 + (ele1 - ele0) * f))
        else:
            result.append((when, lat0, lon0, ele0))
        when += timedelta(seconds=1)
    return result


def convert(gpx_text):
    return "".join(burst(*point) for point in resample(read_points(gpx_text)))


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    with open(argv[1], encoding="utf-8") as source:
        nmea = convert(source.read())
    if not nmea:
        print("Keine Punkte (trkpt) in %s gefunden." % argv[1])
        return 1
    with open(argv[2], "w", encoding="ascii", newline="") as target:
        target.write(nmea)
    print("%d Sekunden nach %s geschrieben." % (nmea.count("GNGGA"), argv[2]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
