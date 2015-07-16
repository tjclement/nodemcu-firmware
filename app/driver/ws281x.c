#include "driver/ws281x.h"

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
void ICACHE_FLASH_ATTR ws281x_write(uint8_t pin, uint8_t *pixels, uint32_t length, uint8_t is_ws2812) {
  uint8_t *p, *end, pixel, mask;
  uint32_t t, t0h, t1h, ttot, c, start_time, pin_mask;

  pin_mask = 1 << pin_num[pin];
  p =  pixels;
  end =  p + length;
  pixel = *p++;
  mask = 0x80;
  start_time = 0;
  t0h  = (1000 * system_get_cpu_freq()) / 2857;  // 0.35us (spec=0.35 +- 0.15)
  t1h  = (1000 * system_get_cpu_freq()) / 1429;  // 0.75us (spec=0.70 +- 0.15)
  if (is_ws2812) {
    ttot = (1000 * system_get_cpu_freq()) / 800;  // 1.25us (must be >= 1.25 on ws2812)
  } else {
    ttot = (1000 * system_get_cpu_freq()) / 667;  // 1.50us (must be >= 1.50 on ws2811)
  }

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

