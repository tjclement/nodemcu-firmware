#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"
#include "driver/ws281x.h"

// Lua: ws2811.write(pin, "string")
// Byte triples in the string are interpreted as R G B values.
//
// ws2811.write(4, string.char(0, 255, 0)) uses GPIO2 and sets the first LED green.
// ws2811.write(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// ws2811.write(4, string.char(255, 0, 0, 255, 255, 255)) first LED red, second LED white.
static int ICACHE_FLASH_ATTR ws2811_writergb(lua_State* L) {
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *buffer = luaL_checklstring(L, 2, &length);

  // Initialize the output pin
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 0);

  // Send the buffer
  os_intr_lock();
  ws2811_write(pin, (uint8_t*) buffer, length);
  os_intr_unlock();

  return 0;
}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE ws2811_map[] =
{
  { LSTRKEY( "write" ), LFUNCVAL( ws2811_writergb )},
  { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_ws2811(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  LREGISTER(L, "ws2811", ws2811_map);
  return 1;
}

