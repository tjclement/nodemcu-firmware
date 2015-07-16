#ifndef WS281X_APP_H
#define WS281X_APP_H

#include "platform.h"
#include "user_interface.h"

void ICACHE_RAM_ATTR ws281x_write(uint8_t pin, uint8_t *pixels, uint32_t length, uint8_t is_ws2812);

#define ws2811_write(pin, pixels, length) ws281x_write((pin), (pixels), (length), 0)
#define ws2812_write(pin, pixels, length) ws281x_write((pin), (pixels), (length), 1)

#endif

