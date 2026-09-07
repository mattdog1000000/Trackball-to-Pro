# Trackball-to-Pro

Adapter from USB trackball to Switch Pro Controller via modified Waveshare RP2350-USB-A.

Created to have fair and fun multiplayer in the [Switch 2 Arcade Archives port of Armadillo Racing](https://www.arcadearchives.com/en/title/aca-420/).

## Required Modification to Waveshare RP2350-USB-A

Waveshare RP2350-USB-A needs to be modified for this project to function!
Waveshare RP2350-USB-A does not allow using the USB-A port to act as host for low-power devices be default.

To allow using the USB-A port as a host and never allow using it as a device ever again:
Remove R13 (either via desoldering or by just carefully scraping it off).
This is the resistor immediately adject to pin 6 and the rp2350, nearest pin 7.

See: https://qsantos.fr/2025/11/21/fixing-the-rp2350-usb-a-not-working-as-usb-host/

## Build Steps

```
git clone https://github.com/mattdog1000000/Trackball-to-Pro
cd Trackball-to-Pro
mkdir build
cd build
cmake ..
make
```

## Credits

- Originally based on [RP2350-USB-C](https://github.com/waveshareteam/RP2350-USB-C) 
- Switch Pro Controller functionality based on [PicoGamepadConverter](https://github.com/Loc15/PicoGamepadConverter) which referenced [GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE)
- fixed_point_16_16_sqrt() taken from [sqrt_fx16_16_to_fx16_16()](https://github.com/chmike/fpsqrt/blob/master/fpsqrt.c#L113) in [fpsqrt](https://github.com/chmike/fpsqrt)
