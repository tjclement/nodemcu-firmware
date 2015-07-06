#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_stdlib.h"
#include "c_string.h"

/**
 * Code is based on https://github.com/CHERTS/esp8266-devkit/blob/master/Espressif/examples/EspLightNode/user/ws2801.c
 * and provides a similar api as the ws2812 module.
 * The current implementation allows the caller to use
 * any combination of GPIO0, GPIO2, GPIO4, GPIO5 as clock and data.
 * Also supports HSPI (Hardware-SPI) which reduces CPU load a lot
 * and supports much higher bandwidth.
 */

#define PIN_CLK_DEFAULT         0
#define PIN_DATA_DEFAULT        2
#define WS2801_USE_HSPI         -1

static u32 ws2801_bit_clk;
static u32 ws2801_bit_data;
static u8  ws2801_hspi_mode;

static __inline__ void ws2801_byte(u8 n) {
    u8 bitmask;
    for (bitmask = 0x80; bitmask !=0 ; bitmask >>= 1) {
        if (n & bitmask) {
            GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, ws2801_bit_data);
        } else {
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, ws2801_bit_data);
        }
        GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, ws2801_bit_clk);
        GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, ws2801_bit_clk);
    }
}

static void ws2801_strip(u8 const * data, u16 len) {
    while (len--) {
        ws2801_byte(*(data++));
    }
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, ws2801_bit_data);
}

static void enable_pin_mux(pin) {
    // The API only supports setting PERIPHS_IO_MUX on GPIO 0, 2, 4, 5
    switch (pin) {
        case 0:
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
            break;
        case 2:
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
            break;
        case 4:
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
            break;
        case 5:
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
            break;
        default:
            break;
    }
}

static u32 ws2801_init_gpio(lua_State* L, u32 pin_clk, u32 pin_data) {
    ws2801_hspi_mode = 0;
    ws2801_bit_clk = 1 << pin_clk;
    ws2801_bit_data = 1 << pin_data;

    // Needed?
    os_delay_us(10);

    //Set GPIO pins to output mode
    enable_pin_mux(pin_clk);
    enable_pin_mux(pin_data);

    //Set both GPIOs low low
    gpio_output_set(0, ws2801_bit_clk | ws2801_bit_data, ws2801_bit_clk | ws2801_bit_data, 0);

    // Needed?
    os_delay_us(10);

    return 0;
}

static void ws2801_strip_hspi(u8 const * data, u16 len) {
    while (len--) {
        platform_spi_send_recv(1, *(data++));
    }
}

static u32 ws2801_init_hspi(lua_State* L) {
    // do HSPI stuff
    ws2801_hspi_mode = 1;

    // TODO: clock seems to be broken in app/driver/spi.c
    return platform_spi_setup(1, PLATFORM_SPI_MASTER, PLATFORM_SPI_CPOL_HIGH,
              PLATFORM_SPI_CPHA_HIGH, PLATFORM_SPI_DATABITS_8, 0);
}

/* Lua: ws2801.init(pin_clk, pin_data) bit-baning mode
 * Lua: ws2801.init(ws2801.USE_HSPI) HSPI-mode
 * Sets up the GPIO pins or HSPI initialization.
 * 
 * ws2801.init(0, 2) uses GPIO0 as clock and GPIO2 as data.
 * This is the default behavior.
 * ws2801.init(ws2801.USE_HSPI) uses HSPI instead.
 * This is faster but limited to specific pins.
 */
static int ws2801_init_lua(lua_State* L) {
    u32 res;

    if (lua_isnumber(L, 1)) {
        if (lua_isnumber(L, 2)) {
            u32 pin_clk, pin_data;
            pin_clk = luaL_checkinteger(L, 1);
            pin_data = luaL_checkinteger(L, 2);
            res = ws2801_init_gpio(L, pin_clk, pin_data);
        } else if (luaL_checkinteger(L, 1) == WS2801_USE_HSPI) {
            res = ws2801_init_hspi(L);
        } else {
            res = luaL_error(L, "wrong arg range");
        }
    } else {
        res = ws2801_init_gpio(L, PIN_CLK_DEFAULT, PIN_DATA_DEFAULT); 
    }

    lua_pushinteger(L, res);
    return 1;
}

/* Lua: ws2801.write("string")
 * Byte triples in the string are interpreted as R G B values.
 * This function does not corrupt your buffer.
 *
 * ws2801.write(string.char(255, 0, 0)) sets the first LED red.
 * ws2801.write(string.char(0, 0, 255):rep(10)) sets ten LEDs blue.
 */
static int ICACHE_FLASH_ATTR ws2801_writergb(lua_State* L) {
    size_t length;
    const char *buffer = luaL_checklstring(L, 1, &length);

    if (ws2801_hspi_mode) {
        ws2801_strip_hspi(buffer, length);
    } else {
        os_delay_us(10);
        os_intr_lock();
        ws2801_strip(buffer, length);
        os_intr_unlock();
    }

    lua_pushinteger(L, length);

    return 1;
}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE ws2801_map[] =
{
    { LSTRKEY("write"),    LFUNCVAL(ws2801_writergb)},
    { LSTRKEY("init"),     LFUNCVAL(ws2801_init_lua)},
    { LSTRKEY("USE_HSPI"), LNUMVAL(WS2801_USE_HSPI) },
    { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_ws2801(lua_State *L) {
    LREGISTER(L, "ws2801", ws2801_map);
    MOD_REG_NUMBER(L, "USE_HSPI", WS2801_USE_HSPI);
    return 1;
}

