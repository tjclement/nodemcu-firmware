#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"

static inline uint32_t _getCycleCount(void) {
  uint32_t cycles;
  __asm__ __volatile__("rsr %0,ccount":"=a" (cycles));
  return cycles;
}

// This algorithm reads the cpu clock cycles to calculate the correct
// pulse widths. It works in both 80 and 160 MHz mode.
// The values for t0h, t1h, ttot have been tweaked and it doesn't get faster than this.
// The datasheet is confusing and one might think that a shorter pulse time can be achieved.
// The period has to be at least 1.25us, even if the datasheet says:
//   T0H: 0.35 (+- 0.15) + T0L: 0.8 (+- 0.15), which is 0.85<->1.45 us.
//   T1H: 0.70 (+- 0.15) + T1L: 0.6 (+- 0.15), which is 1.00<->1.60 us.
// Anything lower than 1.25us will glitch in the long run.
static void ICACHE_RAM_ATTR ws2812_write(uint8_t pin, uint8_t *pixels, uint32_t length) {
  uint8_t *p, *end, pixel, mask;
  uint32_t t, t0h, t1h, ttot, c, start_time, pin_mask;

  pin_mask = 1 << pin;
  p =  pixels;
  end =  p + length;
  pixel = *p++;
  mask = 0x80;
  start_time = 0;
  t0h  = (1000 * system_get_cpu_freq()) / 3333;  // 0.30us (spec=0.35 +- 0.15)
  t1h  = (1000 * system_get_cpu_freq()) / 1666;  // 0.60us (spec=0.70 +- 0.15)
  ttot = (1000 * system_get_cpu_freq()) /  800;  // 1.25us (MUST be >= 1.25)

  while (true) {
    if (pixel & mask) {
        t = t1h;
    } else {
        t = t0h;
    }
    while (((c = _getCycleCount()) - start_time) < ttot); // Wait for the previous bit to finish
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pin_mask);      // Set pin high
    start_time = c;                                       // Save the start time
    while (((c = _getCycleCount()) - start_time) < t);    // Wait for high time to finish
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask);      // Set pin low
    if (!(mask >>= 1)) {                                  // Next bit/byte
      if (p >= end) {
        break;
      }
      pixel= *p++;
      mask = 0x80;
    }
  }
}

// Lua: ws2812.writergb(pin, "string")
// Byte triples in the string are interpreted as R G B values and sent to the hardware as G R B.
// WARNING: this function scrambles the input buffer :
//    a = string.char(255,0,128)
//    ws212.writergb(3,a)
//    =a.byte()
//    (0,255,128)

// ws2812.writergb(4, string.char(255, 0, 0)) uses GPIO2 and sets the first LED red.
// ws2812.writergb(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// ws2812.writergb(4, string.char(0, 255, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR ws2812_writergb(lua_State* L)
{
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *rgb = luaL_checklstring(L, 2, &length);

  // dont modify lua-internal lstring - make a copy instead
  char *buffer = (char *)c_malloc(length);
  c_memcpy(buffer, rgb, length);

  // Ignore incomplete Byte triples at the end of buffer:
  length -= length % 3;

  // Rearrange R G B values to G R B order needed by WS2812 LEDs:
  size_t i;
  for (i = 0; i < length; i += 3) {
    const char r = buffer[i];
    const char g = buffer[i + 1];
    buffer[i] = g;
    buffer[i + 1] = r;
  }

  // Initialize the output pin and wait a bit
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 0);

  // Send the buffer
  os_intr_lock();
  ws2812_write(pin_num[pin], (uint8_t*) buffer, length);
  os_intr_unlock();

  c_free(buffer);

  return 0;
}

// Lua: ws2812.write(pin, "string")
// Byte triples in the string are interpreted as G R B values.
// This function does not corrupt your buffer.
//
// ws2812.write(4, string.char(0, 255, 0)) uses GPIO2 and sets the first LED red.
// ws2812.write(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// ws2812.write(4, string.char(255, 0, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR ws2812_writegrb(lua_State* L) {
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *buffer = luaL_checklstring(L, 2, &length);

  // Initialize the output pin
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 0);

  // Send the buffer
  os_intr_lock();
  ws2812_write(pin_num[pin], (uint8_t*) buffer, length);
  os_intr_unlock();

  return 0;
}






static void ICACHE_RAM_ATTR ws2812_writedual(
    uint8_t pin_a, uint8_t pin_b, uint8_t *pixels, uint32_t num_bytes) {
  uint8_t *p1, *p2, *end, pix_a, pix_b, mask;
  uint32_t t, t0h, t1h, t01h, ttot, c, start_time, pin_mask_a, pin_mask_b, bits;

  pin_mask_a = 1 << pin_a;
  pin_mask_b = 1 << pin_b;
  p1 = pixels;
  p2 = pixels + num_bytes / 2;
  end = p1 + num_bytes / 2;
  pix_a = *p1++;
  pix_b = *p2++;
  mask = 0x80;
  start_time = _getCycleCount();
  t0h  = (1000 * system_get_cpu_freq()) / 2857;  // 0.35us (spec=0.35 +- 0.15)
  t1h  = (1000 * system_get_cpu_freq()) / 1428;  // 0.70us (spec=0.70 +- 0.15)
  ttot = (1000 * system_get_cpu_freq()) /  800;  // 1.25us (MUST be >= 1.25)

  t01h = t1h + t0h; // Time to wait when having different bits

  while (true) {

    while (((c = _getCycleCount()) - start_time) < ttot); // Wait for the previous bit to finish

    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pin_mask_a); // Set pin_a high
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pin_mask_b); // Set pin_b high

    start_time = c;

    if (pix_a & mask) {
        if (pix_b & mask) {
            // 11;
            while (((c = _getCycleCount()) - start_time) < t1h);  // Wait high duration
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask_a);    // Set pin_a low
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask_b);    // Set pin_b low
        } else {
            // 10;
            while (((c = _getCycleCount()) - start_time) < t0h);  // Wait high duration
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask_b);    // Set pin_b low
            while (((c = _getCycleCount()) - start_time) < t01h); // Wait remaining time
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask_a);    // Set pin_a low
        }
    } else {
        if (pix_b & mask) {
            // 01;
            while (((c = _getCycleCount()) - start_time) < t0h);  // Wait high duration
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask_a);    // Set pin_a low
            while (((c = _getCycleCount()) - start_time) < t01h); // Wait remaining time
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask_b);    // Set pin_b low
        } else {
            // 00;
            while (((c = _getCycleCount()) - start_time) < t0h);  // Wait high duration
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask_a);    // Set pin_a low
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask_b);    // Set pin_b low
        }
    }

    if (!(mask >>= 1)) {
      if (p1 >= end) {
        break;
      }
      pix_a  = *p1++;
      pix_b  = *p2++;
      mask = 0x80;
    }
  }
}


static int ICACHE_FLASH_ATTR ws2812_writedual_lua(lua_State* L) {
  const uint8_t pin_a = luaL_checkinteger(L, 1);
  const uint8_t pin_b = luaL_checkinteger(L, 2);
  size_t length;
  const char *buffer = luaL_checklstring(L, 3, &length);

  // Initialize the output pins
  platform_gpio_mode(pin_a, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin_a, 0);
  platform_gpio_mode(pin_b, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin_b, 0);

  // Sleep a bit in order to let the GPIO pins settle.
  // Not happy about this but it's needed.
  os_delay_us(10);

  // Send the buffer
  os_intr_lock();
  ws2812_writedual(pin_num[pin_a], pin_num[pin_b], (uint8_t*) buffer, length);
  os_intr_unlock();
}


#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE ws2812_map[] =
{
  { LSTRKEY( "writergb" ), LFUNCVAL( ws2812_writergb )},
  { LSTRKEY( "write" ), LFUNCVAL( ws2812_writegrb )},
  { LSTRKEY( "writedual" ), LFUNCVAL( ws2812_writedual_lua )},
  { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  LREGISTER(L, "ws2812", ws2812_map);
  return 1;
}
