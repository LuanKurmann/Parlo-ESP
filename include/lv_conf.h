// LVGL-Konfiguration fuer Geraet und Simulator.
// Alles, was hier nicht steht, nimmt LVGL aus seinen Standardwerten.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// Speicher fuer LVGL-Objekte
#define LV_MEM_SIZE (128 * 1024U)

// Schriften (die grosse Distanzzahl bekommt spaeter eine eigene Schrift)
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20

#ifdef GOLF_SIMULATOR
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#endif

#endif // LV_CONF_H
