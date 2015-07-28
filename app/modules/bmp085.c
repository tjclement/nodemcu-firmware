#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_stdlib.h"
#include "c_string.h"

static const uint32_t bmp085_i2c_id = 0;
static const uint8_t bmp085_i2c_addr = 0x77;

static struct {
    int16_t  AC1;
    int16_t  AC2;
    int16_t  AC3;
    uint16_t AC4;
    uint16_t AC5;
    uint16_t AC6;
    int16_t  B1;
    int16_t  B2;
    int16_t  MB;
    int16_t  MC;
    int16_t  MD;
} bmp085_data;

static uint8_t ICACHE_FLASH_ATTR r8u(uint32_t id, uint8_t reg) {
    uint8_t ret;

    platform_i2c_send_start(id);
    platform_i2c_send_address(id, bmp085_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(id, reg);
    platform_i2c_send_stop(id);
    platform_i2c_send_start(id);
    platform_i2c_send_address(id, bmp085_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    ret = platform_i2c_recv_byte(id, 1);
    platform_i2c_send_stop(id);
    return ret;
}

static uint16_t ICACHE_FLASH_ATTR r16u(uint32_t id, uint8_t reg) {
    uint8_t msb = r8u(id, reg);
    uint8_t lsb = r8u(id, reg + 1);
    return (msb << 8) | lsb;
}

static int16_t ICACHE_FLASH_ATTR r16(uint32_t id, uint8_t reg) {
    uint16_t ret = r16u(id, reg);
    return (int16_t) ret;
}

static int ICACHE_FLASH_ATTR bmp085_init(lua_State* L) {
    uint32_t sda;
    uint32_t scl;

    if (lua_isnumber(L, 1) && lua_isnumber(L, 2)) {
        sda = luaL_checkinteger(L, 1);
        scl = luaL_checkinteger(L, 2);
    } else {
        return luaL_error(L, "wrong arg range");
    }

    if (scl == 0 || sda == 0)
        return luaL_error(L, "no i2c for D0");

    platform_i2c_setup(bmp085_i2c_id, sda, scl, PLATFORM_I2C_SPEED_SLOW);

    bmp085_data.AC1 = r16(bmp085_i2c_id, 0xAA);
    bmp085_data.AC2 = r16(bmp085_i2c_id, 0xAC);
    bmp085_data.AC3 = r16(bmp085_i2c_id, 0xAE);
    bmp085_data.AC4 = r16u(bmp085_i2c_id, 0xB0);
    bmp085_data.AC5 = r16u(bmp085_i2c_id, 0xB2);
    bmp085_data.AC6 = r16u(bmp085_i2c_id, 0xB4);
    bmp085_data.B1  = r16(bmp085_i2c_id, 0xB6);
    bmp085_data.B2  = r16(bmp085_i2c_id, 0xB8);
    bmp085_data.MB  = r16(bmp085_i2c_id, 0xBA);
    bmp085_data.MC  = r16(bmp085_i2c_id, 0xBC);
    bmp085_data.MD  = r16(bmp085_i2c_id, 0xBE);
    
    c_printf("Yay! \n AC1:%hi\n  AC2:%hi\n  AC3:%hi\n  AC4:%hu\n  AC5:%hu\n  AC6:%hu\n",
            bmp085_data.AC1, 
            bmp085_data.AC2, 
            bmp085_data.AC3, 
            bmp085_data.AC4, 
            bmp085_data.AC5, 
            bmp085_data.AC6);

    return 1;
}

static int ICACHE_FLASH_ATTR bmp085_temperature(lua_State* L) {
    uint8 oss = 0;

    if (lua_isnumber(L, 1)) {
        oss = luaL_checkinteger(L, 1);
        if (oss > 3)  {
            oss = 3;
        }
    }

    

}
static int ICACHE_FLASH_ATTR bmp085_pressure(lua_State* L) {}
static int ICACHE_FLASH_ATTR bmp085_altitude(lua_State* L) {}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE bmp085_map[] =
{
    { LSTRKEY( "temperature" ), LFUNCVAL( bmp085_temperature )},
    { LSTRKEY( "pressure" ), LFUNCVAL( bmp085_pressure )},
    { LSTRKEY( "altitude" ), LFUNCVAL( bmp085_altitude )},
    { LSTRKEY( "init" ), LFUNCVAL( bmp085_init )},
    { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_bmp085(lua_State *L) {
    LREGISTER(L, "bmp085", bmp085_map);
    return 1;
}

