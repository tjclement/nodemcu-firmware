#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"
#include "driver/ws281x.h"

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

  // Initialize the output pin
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 0);

  // Send the buffer
  os_intr_lock();
  ws2812_write(pin, (uint8_t*) buffer, length);
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
  ws2812_write(pin, (uint8_t*) buffer, length);
  os_intr_unlock();

  return 0;
}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE ws2812_map[] =
{
  { LSTRKEY( "writergb" ), LFUNCVAL( ws2812_writergb )},
  { LSTRKEY( "write" ), LFUNCVAL( ws2812_writegrb )},
  { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  LREGISTER(L, "ws2812", ws2812_map);
  return 1;
}
