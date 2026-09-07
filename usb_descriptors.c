/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *                    sekigon-gonnoc
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

#include "tusb.h"

static const uint8_t switch_device_descriptor[] =
{
  0x12,        // bLength
  0x01,        // bDescriptorType (Device)
  0x00, 0x02,  // bcdUSB 2.00
  0x00,        // bDeviceClass (Use class information in the Interface Descriptors)
  0x00,        // bDeviceSubClass
  0x00,        // bDeviceProtocol
  0x40,        // bMaxPacketSize0 64
  0x0D, 0x0F,  // idVendor 0x0F0D
  0x92, 0x00,  // idProduct 0x92
  0x00, 0x01,  // bcdDevice 2.00
  0x01,        // iManufacturer (String Index)
  0x02,        // iProduct (String Index)
  0x00,        // iSerialNumber (String Index)
  0x01,        // bNumConfigurations 1
};

uint8_t const * tud_descriptor_device_cb(void)
{
  return switch_device_descriptor;
}

static const uint8_t switch_configuration_descriptor[] =
{
  0x09,        // bLength
  0x02,        // bDescriptorType (Configuration)
  0x29, 0x00,  // wTotalLength 41
  0x01,        // bNumInterfaces 1
  0x01,        // bConfigurationValue
  0x00,        // iConfiguration (String Index)
  0x80,        // bmAttributes
  0xFA,        // bMaxPower 500mA

  0x09,        // bLength
  0x04,        // bDescriptorType (Interface)
  0x00,        // bInterfaceNumber 0
  0x00,        // bAlternateSetting
  0x02,        // bNumEndpoints 2
  0x03,        // bInterfaceClass
  0x00,        // bInterfaceSubClass
  0x00,        // bInterfaceProtocol
  0x00,        // iInterface (String Index)

  0x09,        // bLength
  0x21,        // bDescriptorType (HID)
  0x11, 0x01,  // bcdHID 1.11
  0x00,        // bCountryCode
  0x01,        // bNumDescriptors
  0x22,        // bDescriptorType[0] (HID)
  0x56, 0x00,  // wDescriptorLength[0] 86

  0x07,        // bLength
  0x05,        // bDescriptorType (Endpoint)
  0x02,        // bEndpointAddress (OUT/H2D)
  0x03,        // bmAttributes (Interrupt)
  0x40, 0x00,  // wMaxPacketSize 64
  0x01,        // bInterval 1 (unit depends on device speed)

  0x07,        // bLength
  0x05,        // bDescriptorType (Endpoint)
  0x81,        // bEndpointAddress (IN/D2H)
  0x03,        // bmAttributes (Interrupt)
  0x40, 0x00,  // wMaxPacketSize 64
  0x01,        // bInterval 1 (unit depends on device speed)
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index;

  return switch_configuration_descriptor;
}

static const uint8_t switch_string_language[]     = { 0x09, 0x04 };
static const uint8_t switch_string_manufacturer[] = "MATTDOG1000000";
static const uint8_t switch_string_product[]      = "TRACKBALL TO PRO";
static const uint8_t switch_string_version[]      = "0.1";

static const uint8_t *switch_string_descriptors[] =
{
  switch_string_language,
  switch_string_manufacturer,
  switch_string_product,
  switch_string_version
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  if (index >= 4)
  {
    return NULL;
  }

  unsigned int chr_count = 0;

  const uint8_t *p_string_descriptor[4];

  memcpy(p_string_descriptor, switch_string_descriptors, (sizeof(const uint8_t *) * 4));

  if (index == 0)
  {
    memcpy(&_desc_str[1], p_string_descriptor[0], 2);
    chr_count = 1;
  }
  else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if (index >= sizeof(p_string_descriptor) / sizeof(p_string_descriptor[0]))
    {
      return NULL;
    }

    const uint8_t* str = p_string_descriptor[index];

    // Cap at max char
    chr_count = (uint8_t) strlen((const char*)str);
    if (chr_count > (TU_ARRAY_SIZE(_desc_str) - 1))
    {
      chr_count = TU_ARRAY_SIZE(_desc_str) - 1;
    }

    // Convert ASCII string into UTF-16
    for (unsigned int i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8 ) | (2*chr_count + 2));

  return _desc_str;
}
