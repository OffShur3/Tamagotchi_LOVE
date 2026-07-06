/**
 * @file lv_conf.h
 * Configuración básica de LVGL para Waveshare ESP32-S3 LCD 1.47"
 */

#ifndef LV_CONF_H
#define LV_CONF_H

// Habilitar esta configuración (debe ser 1)
#define LV_CONF_SUPPRESS_DEFINE_CHECK 1

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   0      // 0 si los colores se ven bien, si están cambiados probá con 1
#define LV_COLOR_SCREEN_TRANSP   1

/*====================
   DISPLAY SETTINGS
 *====================*/
#define LV_HOR_RES_MAX     172
#define LV_VER_RES_MAX     320

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_SIZE        (48U * 1024U)   // 48 kB, suficiente para la UI del mockup
#define LV_MEMCPY_MEMCPY   1                // Usa la función memcpy estándar

/*====================
   TICK SETTINGS
 *====================*/
#define LV_TICK_CUSTOM     0                // Usaremos lv_tick_inc() manualmente en el loop
// Si quisieras tick automático con Arduino millis():
// #define LV_TICK_CUSTOM     1
// #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
// #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/*====================
   FONT USAGE
 *====================*/
#define LV_FONT_DEFAULT    &lv_font_montserrat_14  // Tamaño por defecto
#define LV_FONT_FMT_TXT_CMAP 0

/*====================
   DEBUG
 *====================*/
#define LV_USE_DEBUG       0

/*====================
   OTHERS
 *====================*/
#define LV_USE_LOG         0
#define LV_USE_ASSERT_NULL     0
#define LV_USE_ASSERT_MALLOC   0
#define LV_USE_PERF_MONITOR    0

#endif /* LV_CONF_H */
