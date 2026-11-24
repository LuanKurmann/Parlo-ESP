"""Tests fuer gpx_to_nmea.py:  python3 -m unittest discover -s tools"""

import os
import re
import unittest

import gpx_to_nmea

HERE = os.path.dirname(os.path.abspath(__file__))


def parse_coordinate(text, hemisphere, degree_digits):
    value = int(text[:degree_digits]) + float(text[degree_digits:]) / 60.0
    return -value if hemisphere in "SW" else value


class GpxToNmeaTest(unittest.TestCase):
    def setUp(self):
        with open(os.path.join(HERE, "testdata", "spaziergang.gpx"), encoding="utf-8") as source:
            self.nmea = gpx_to_nmea.convert(source.read())
        self.lines = self.nmea.split("\r\n")[:-1]

    def test_known_checksum(self):
        self.assertEqual(
            gpx_to_nmea.sentence("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"),
            "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n",
        )

    def test_one_measurement_per_second(self):
        gga = [line for line in self.lines if line.startswith("$GNGGA")]
        rmc = [line for line in self.lines if line.startswith("$GNRMC")]
        self.assertEqual(len(gga), 271)  # 08:00:00 bis 08:04:30
        self.assertEqual(len(rmc), 271)
        self.assertTrue(gga[0].startswith("$GNGGA,080000.000,"))
        self.assertTrue(gga[-1].startswith("$GNGGA,080430.000,"))
        self.assertIn(",241126,", rmc[0])

    def test_every_line_has_a_valid_checksum(self):
        for line in self.lines:
            match = re.fullmatch(r"\$(.*)\*([0-9A-F]{2})", line)
            self.assertIsNotNone(match, line)
            self.assertEqual(gpx_to_nmea.checksum(match.group(1)), match.group(2))

    def test_positions_are_interpolated(self):
        gga = [line.split(",") for line in self.lines if line.startswith("$GNGGA")]
        start = parse_coordinate(gga[0][2], gga[0][3], 2)
        self.assertAlmostEqual(start, 47.3769, places=6)
        self.assertAlmostEqual(parse_coordinate(gga[0][4], gga[0][5], 3), 8.5417, places=6)
        # Nach 10 s Stehen 140 s Gehen: bei 80 s ist die Haelfte des Wegs geschafft.
        halfway = parse_coordinate(gga[80][2], gga[80][3], 2)
        self.assertAlmostEqual(halfway, (47.3769 + 47.378744) / 2, places=6)
        self.assertAlmostEqual(parse_coordinate(gga[-1][2], gga[-1][3], 2), 47.379931, places=6)

    def test_southern_and_western_hemisphere(self):
        self.assertEqual(gpx_to_nmea.coordinate(-33.8915, 2, "N", "S"), "3353.49000,S")
        self.assertEqual(gpx_to_nmea.coordinate(-2.8025, 3, "E", "W"), "00248.15000,W")

    def test_minutes_never_round_up_to_sixty(self):
        self.assertEqual(gpx_to_nmea.coordinate(47.99999999, 2, "N", "S"), "4800.00000,N")

    def test_points_without_time_are_one_second_apart(self):
        gpx = '<gpx><trk><trkseg><trkpt lat="47.0" lon="8.0"/><trkpt lat="47.00001" lon="8.0"/><trkpt lat="47.00002" lon="8.0"/></trkseg></trk></gpx>'
        self.assertEqual(gpx_to_nmea.convert(gpx).count("$GNGGA"), 3)

    def test_empty_and_invalid_files(self):
        self.assertEqual(gpx_to_nmea.convert("<gpx></gpx>"), "")
        with self.assertRaises(ValueError):
            gpx_to_nmea.convert('<gpx><trkpt lat="95.0" lon="8.0"/></gpx>')


if __name__ == "__main__":
    unittest.main()
