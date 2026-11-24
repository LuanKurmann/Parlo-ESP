"""Tests fuer die Pruefungen in screenshot_test.py:  python3 -m unittest discover -s tools"""

import unittest

from PIL import Image

import screenshot_test as st


def black():
    return Image.new("RGB", (st.SIZE, st.SIZE), (0, 0, 0))


class OutsideCircleTest(unittest.TestCase):
    def test_black_image_is_fine(self):
        self.assertEqual(st.outside_circle(black()), 0)

    def test_content_in_the_middle_is_fine(self):
        image = black()
        for x in range(100, 366):
            for y in range(100, 366):
                image.putpixel((x, y), (255, 255, 255))
        self.assertEqual(st.outside_circle(image), 0)

    def test_pixel_in_the_corner_is_found(self):
        image = black()
        image.putpixel((3, 3), (255, 255, 255))
        self.assertEqual(st.outside_circle(image), 1)

    def test_pixel_just_inside_the_edge_but_outside_the_margin_is_found(self):
        image = black()
        image.putpixel((233, 1), (95, 224, 138))  # oben in der Mitte, 1 px vom Rand
        self.assertEqual(st.outside_circle(image), 1)
        image = black()
        image.putpixel((233, 5), (95, 224, 138))  # 5 px vom Rand: erlaubt
        self.assertEqual(st.outside_circle(image), 0)

    def test_nearly_black_pixels_do_not_count(self):
        image = black()
        image.putpixel((3, 3), (st.BLACK_LIMIT, st.BLACK_LIMIT, st.BLACK_LIMIT))
        self.assertEqual(st.outside_circle(image), 0)


class DifferentPixelsTest(unittest.TestCase):
    def test_identical_images(self):
        count, _ = st.different_pixels(black(), black())
        self.assertEqual(count, 0)

    def test_tiny_brightness_changes_are_tolerated(self):
        other = Image.new("RGB", (st.SIZE, st.SIZE), (st.MAX_CHANNEL_DIFF,) * 3)
        count, _ = st.different_pixels(black(), other)
        self.assertEqual(count, 0)

    def test_a_changed_area_is_counted(self):
        other = black()
        for x in range(200, 210):
            for y in range(200, 210):
                other.putpixel((x, y), (255, 255, 255))
        count, diff = st.different_pixels(black(), other)
        self.assertEqual(count, 100)
        self.assertEqual(diff.getpixel((205, 205)), 255)
        self.assertEqual(diff.getpixel((10, 10)), 0)

    def test_a_single_colour_channel_change_is_counted(self):
        other = black()
        other.putpixel((50, 50), (0, 0, 200))
        count, _ = st.different_pixels(black(), other)
        self.assertEqual(count, 1)


if __name__ == "__main__":
    unittest.main()
