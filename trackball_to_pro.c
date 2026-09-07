// !!!IMPORTANT!!!
// Waveshare RP2350-USB-A needs to be modified for this project to function!
// Waveshare RP2350-USB-A does not allow using the USB-A port to act as host for low-power devices be default.
// To allow using the USB-A port as a host and never allow using it as a device ever again:
// Remove R13 (either via desoldering or by just carefully scraping it off).
// This is the resistor immediately adject to pin 6 and the rp2350, nearest pin 7.
// See https://qsantos.fr/2025/11/21/fixing-the-rp2350-usb-a-not-working-as-usb-host/

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

// Originally based on https://github.com/waveshareteam/RP2350-USB-C 
// Switch Pro Controller functionality based on https://github.com/Loc15/PicoGamepadConverter which references https://github.com/OpenStickCommunity/GP2040-CE
// fixed_point_16_16_sqrt() taken from sqrt_fx16_16_to_fx16_16() in https://github.com/chmike/fpsqrt

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "pico/bootrom.h"

#include "pio_usb.h"
#include "tusb.h"

#include "pico/status_led.h"

#include <stdint.h>

// Algorithm and code Author Christophe Meessen 1993. 
// Initially published in usenet comp.lang.c, Thu, 28 Jan 1993 08:35:23 GMT, 
// Subject: Fixed point sqrt ; by Meessen Christophe
//
// https://groups.google.com/forum/?hl=fr%05aacf5997b615c37&fromgroups#!topic/comp.lang.c/IpwKbw0MAxw/discussion
// Note: there was a bug in the published sqrtL2L routine. It is corrected in this implementation.
int32_t fixed_point_16_16_sqrt(int32_t v)
{
  uint32_t t, q, b, r;
  r = (int32_t)v; 
  q = 0;          
  b = 0x40000000UL;
  if (r < 0x40000200)
  {
    while (b != 0x40)
    {
      t = q + b;
      if (r >= t)
      {
        r -= t;
        q = t + b; // equivalent to q += 2*b
      }
      r <<= 1;
      b >>= 1;
    }
    q >>= 8;
    return q;
  }
  while (b > 0x40)
  {
    t = q + b;
    if (r >= t)
    {
      r -= t;
      q = t + b; // equivalent to q += 2*b
    }
    if ((r & 0x80000000) != 0)
    {
      q >>= 1;
      b >>= 1;
      r >>= 1;
      while (b > 0x20)
      {
        t = q + b;
        if (r >= t)
        {
          r -= t;
          q = t + b;
        }
        r <<= 1;
        b >>= 1;
      }
      q >>= 7;
      return q;
    }
    r <<= 1;
    b >>= 1;
  }
  q >>= 8;
  return q;
}

// Switch 

#define SWITCH_ENDPOINT_SIZE 64

// HAT report (4 bits)
#define SWITCH_HAT_UP        0x00
#define SWITCH_HAT_UPRIGHT   0x01
#define SWITCH_HAT_RIGHT     0x02
#define SWITCH_HAT_DOWNRIGHT 0x03
#define SWITCH_HAT_DOWN      0x04
#define SWITCH_HAT_DOWNLEFT  0x05
#define SWITCH_HAT_LEFT      0x06
#define SWITCH_HAT_UPLEFT    0x07
#define SWITCH_HAT_NOTHING   0x08

// Button report (16 bits)
#define SWITCH_MASK_Y       (1U <<  0)
#define SWITCH_MASK_B       (1U <<  1)
#define SWITCH_MASK_A       (1U <<  2)
#define SWITCH_MASK_X       (1U <<  3)
#define SWITCH_MASK_L       (1U <<  4)
#define SWITCH_MASK_R       (1U <<  5)
#define SWITCH_MASK_ZL      (1U <<  6)
#define SWITCH_MASK_ZR      (1U <<  7)
#define SWITCH_MASK_MINUS   (1U <<  8)
#define SWITCH_MASK_PLUS    (1U <<  9)
#define SWITCH_MASK_L3      (1U << 10)
#define SWITCH_MASK_R3      (1U << 11)
#define SWITCH_MASK_HOME    (1U << 12)
#define SWITCH_MASK_CAPTURE (1U << 13)

// Switch analog sticks only report 8 bits
#define SWITCH_JOYSTICK_MIN 0x00
#define SWITCH_JOYSTICK_MID 0x80
#define SWITCH_JOYSTICK_MAX 0xFF

typedef struct __attribute((packed, aligned(1)))
{
  uint16_t buttons;
  uint8_t hat;
  uint8_t lx;
  uint8_t ly;
  uint8_t rx;
  uint8_t ry;
  uint8_t vendor;
} SwitchReport;

static const uint8_t switch_report_descriptor[] =
{
  0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
  0x09, 0x05,        // Usage (Game Pad)
  0xA1, 0x01,        // Collection (Application)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x35, 0x00,        //   Physical Minimum (0)
  0x45, 0x01,        //   Physical Maximum (1)
  0x75, 0x01,        //   Report Size (1)
  0x95, 0x10,        //   Report Count (16)
  0x05, 0x09,        //   Usage Page (Button)
  0x19, 0x01,        //   Usage Minimum (0x01)
  0x29, 0x10,        //   Usage Maximum (0x10)
  0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
  0x25, 0x07,        //   Logical Maximum (7)
  0x46, 0x3B, 0x01,  //   Physical Maximum (315)
  0x75, 0x04,        //   Report Size (4)
  0x95, 0x01,        //   Report Count (1)
  0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
  0x09, 0x39,        //   Usage (Hat switch)
  0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
  0x65, 0x00,        //   Unit (None)
  0x95, 0x01,        //   Report Count (1)
  0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
  0x26, 0xFF, 0x00,  //   Logical Maximum (255)
  0x46, 0xFF, 0x00,  //   Physical Maximum (255)
  0x09, 0x30,        //   Usage (X)
  0x09, 0x31,        //   Usage (Y)
  0x09, 0x32,        //   Usage (Z)
  0x09, 0x35,        //   Usage (Rz)
  0x75, 0x08,        //   Report Size (8)
  0x95, 0x04,        //   Report Count (4)
  0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
  0x09, 0x20,        //   Usage (0x20)
  0x95, 0x01,        //   Report Count (1)
  0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  0x0A, 0x21, 0x26,  //   Usage (0x2621)
  0x95, 0x08,        //   Report Count (8)
  0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
  0xC0,              // End Collection
};

void set_color_led(uint32_t grb)
{
  //https://github.com/raspberrypi/pico-sdk/issues/2630
  colored_status_led_set_state(false);
  sleep_us(100);
  colored_status_led_set_on_with_color(grb);
}

void core1_main()
{
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
  tuh_init(1);
  while (true)
  {
    tuh_task();
  }
}

int main(void)
{
  set_sys_clock_khz(120000, true);

  status_led_init();
  colored_status_led_set_on_with_color(0);

  stdio_init_all();

  multicore_reset_core1();
  multicore_launch_core1(core1_main);

  tud_init(0);
  while (true)
  {
    tud_task();
  }

  return 0;
}

static SwitchReport switchReport =
{
  .buttons = 0,
  .hat = SWITCH_HAT_NOTHING,
  .lx = SWITCH_JOYSTICK_MID,
  .ly = SWITCH_JOYSTICK_MID,
  .rx = SWITCH_JOYSTICK_MID,
  .ry = SWITCH_JOYSTICK_MID,
  .vendor = 0,
};

uint32_t start_ms = 0;
static void sendReportData()
{
  const uint32_t INTERVAL_MS = 1;
  if (to_ms_since_boot(get_absolute_time()) - start_ms < INTERVAL_MS)
  {
    return;
  }
  start_ms += INTERVAL_MS;

  if (tud_suspended())
  {
    tud_remote_wakeup();
  }

  if (tud_hid_ready())
  {
    tud_hid_report(0, &switchReport, sizeof(switchReport));
  }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  (void)desc_report;
  (void)desc_len;

  if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_MOUSE)
  {
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    printf("mounted mouse vid: %04d, pid: %04d\n", vid, pid);
    set_color_led(0x000001);

    tuh_hid_receive_report(dev_addr, instance);
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  (void)dev_addr;
  (void)instance;
}

const int16_t MAX_MOUSE_MAGNITUDE = 0x10;
const int16_t MOUSE_TO_STICK_SCALE = (SWITCH_JOYSTICK_MID / MAX_MOUSE_MAGNITUDE);
static uint8_t convert_mouse_to_stick(int8_t input)
{
  int16_t output = (input * MOUSE_TO_STICK_SCALE) + SWITCH_JOYSTICK_MID;

  if (output > SWITCH_JOYSTICK_MAX)
  {
    return SWITCH_JOYSTICK_MAX;
  }
  if (output < SWITCH_JOYSTICK_MIN)
  {
    return SWITCH_JOYSTICK_MIN;
  }

  return (uint8_t)output;
}

static void process_mouse_report(uint8_t dev_addr, hid_mouse_report_t const * report)
{
  (void) dev_addr;

  uint16_t switch_buttons = 0;
  if (report->buttons & 1)
  {
    switch_buttons |= SWITCH_MASK_A;
  }
  if (report->buttons & 2)
  {
    switch_buttons |= SWITCH_MASK_L;
  }
  if (report->buttons & 4)
  {
    switch_buttons |= SWITCH_MASK_PLUS;
  }
  if (report->buttons & 8)
  {
    switch_buttons |= SWITCH_MASK_B;
  }
  if (report->buttons & 16)
  {
    switch_buttons |= SWITCH_MASK_R;
  }
  switchReport.buttons = switch_buttons;

  const int32_t FIXED_POINT_24_8_MAX_MOUSE_MAGNITUDE = MAX_MOUSE_MAGNITUDE << 8;
  const int32_t FIXED_POINT_16_16_MAX_MOUSE_MAGNITUDE_SQUARED = FIXED_POINT_24_8_MAX_MOUSE_MAGNITUDE * FIXED_POINT_24_8_MAX_MOUSE_MAGNITUDE;
  const int32_t fixed_point_24_8_x = ((int32_t)report->x) << 8;
  const int32_t fixed_point_24_8_y = ((int32_t)report->y) << 8;
  const int32_t fixed_point_16_16_x_squared = fixed_point_24_8_x * fixed_point_24_8_x;
  const int32_t fixed_point_16_16_y_squared = fixed_point_24_8_y * fixed_point_24_8_y;
  const int32_t fixed_point_16_16_length_squared = fixed_point_16_16_x_squared + fixed_point_16_16_y_squared;
  if (fixed_point_16_16_length_squared > FIXED_POINT_16_16_MAX_MOUSE_MAGNITUDE_SQUARED)
  {
    const int32_t fixed_point_16_16_length = fixed_point_16_16_sqrt(fixed_point_16_16_length_squared);
    const int8_t scaled_x = (MAX_MOUSE_MAGNITUDE * (fixed_point_24_8_x << 8)) / fixed_point_16_16_length;
    const int8_t scaled_y = (MAX_MOUSE_MAGNITUDE * (fixed_point_24_8_y << 8)) / fixed_point_16_16_length;

    printf("%d %d -> %d %d\n", report->x, report->y, scaled_x, scaled_y);

    switchReport.lx = convert_mouse_to_stick(scaled_x);
    switchReport.ly = convert_mouse_to_stick(scaled_y);
  }
  else
  {
    switchReport.lx = convert_mouse_to_stick(report->x);
    switchReport.ly = convert_mouse_to_stick(report->y);
  }

  // Only sending report data when input is received.
  // This means that unless there is digital input, the stick will never return to (0,0)
  // For Armadillo Racing... this does not matter.
  sendReportData();
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) len;
  
  if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_MOUSE)
  {
    process_mouse_report(dev_addr, (hid_mouse_report_t const*) report );
    tuh_hid_receive_report(dev_addr, instance);
  }
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
  (void) itf;

  return switch_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) reqlen;

  uint8_t report_size = sizeof(SwitchReport);

  memcpy(buffer, &switchReport, report_size);

  return report_size;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}