// LVGL-Konfiguration fuer Geraet und Simulator.
// Alles, was hier nicht steht, nimmt LVGL aus seinen Standardwerten.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// Speicher fuer LVGL-Objekte. Der Simulator braucht mehr, weil ein Screenshot
// das ganze Bild auf einmal im Speicher haelt.
#ifdef GOLF_SIMULATOR
#define LV_MEM_SIZE (8 * 1024U * 1024U)
#else
#define LV_MEM_SIZE (128 * 1024U)
#endif

// Die App nutzt eigene Schriften (lib/ui/src/ui/fonts). Von den eingebauten
// bleibt nur die Standardschrift als Rueckfall.
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#ifdef GOLF_SIMULATOR
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#define LV_USE_SNAPSHOT 1 // fuer Screenshots
#endif

#endif // LV_CONF_H
