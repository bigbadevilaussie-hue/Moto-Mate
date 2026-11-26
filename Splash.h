#pragma once

#include <LovyanGFX.hpp>

// Landscape 160 x 128 renderer. Does not initialise hardware, manage power,
// delay, read sensors or change operating state. All dynamic text is white.
enum class SplashMode : uint8_t {
  Initialising,
  Maintenance,
  BleUpload,
  LogUpload,
  FirmwareUpdate,
  RiderReady,
  BatteryCheck,
  BenchAwake
};

// Returns false if dimensions or text fit checks fail.
// detail is optional caller-supplied status (e.g. OTA progress).
// Battery readings are supplied by the caller; invalid readings display "--".
bool drawSplash(lgfx::LGFXBase& target, SplashMode mode,
                const char* detail = nullptr,
                float voltage = NAN, float temperatureC = NAN);
