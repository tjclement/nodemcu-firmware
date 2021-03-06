// Module for RTC user memory access

#include "lauxlib.h"
#include "rtc/rtcaccess.h"

static int rtcmem_read32 (lua_State *L)
{
  int idx = luaL_checknumber (L, 1);
  int n = 1;
  if (lua_isnumber (L, 2))
    n = lua_tonumber (L, 2);

  if (!lua_checkstack (L, n))
    return 0;

  int ret  = 0;
  while (n > 0 && idx >= 0 && idx < RTC_USER_MEM_NUM_DWORDS)
  {
    lua_pushinteger (L, rtc_mem_read (idx++));
    --n;
    ++ret;
  }
  return ret;
}


static int rtcmem_write32 (lua_State *L)
{
  int idx = luaL_checknumber (L, 1);
  int n = lua_gettop (L) - 1;
  luaL_argcheck (
    L, idx + n <= RTC_USER_MEM_NUM_DWORDS, 1, "RTC mem would overrun");
  int src = 2;
  while (n-- > 0)
  {
    rtc_mem_write (idx++, lua_tonumber (L, src++));
  }
  return 0;
}


// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE rtcmem_map[] =
{
  { LSTRKEY("read32"),  LFUNCVAL(rtcmem_read32)  },
  { LSTRKEY("write32"), LFUNCVAL(rtcmem_write32) },
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_rtcmem (lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else
  luaL_register (L, AUXLIB_RTCMEM, rtcmem_map);
  return 1;
#endif
}
