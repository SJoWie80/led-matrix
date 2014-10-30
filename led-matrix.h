// -*- c++ -*-
// Controlling a 32x32 RGB matrix via GPIO.

#ifndef RPI_RGBMATRIX_H
#define RPI_RGBMATRIX_H

#include <stdint.h>
#include "gpio.h"

  union IoBits {
    struct {
      unsigned int unused1 : 2;  // 0..1
      unsigned int output_enable : 1;  // 2
      unsigned int clock  : 1;   // 3
      unsigned int strobe : 1;   // 4
      unsigned int joy_u : 1;
      unsigned int joy_d : 1;
      unsigned int row : 4;  // 7..10
      unsigned int sw1 : 1;
      unsigned int joy_l : 1; // 12
      unsigned int joy_r : 1; // 13
      unsigned int unused3b : 3;  // 14..16
      unsigned int r1 : 1;   // 17
      unsigned int g1 : 1;   // 18
      unsigned int joy_c : 1; // 19
      unsigned int unused4 : 2; // 20..21
      unsigned int b1 : 1;   // 22
      unsigned int r2 : 1;   // 23
      unsigned int g2 : 1;   // 24
      unsigned int b2 : 1;   // 25
      unsigned int unused5: 1; // 26
      unsigned int sw2: 1;   // 27
    } bits;
    uint32_t raw;
    IoBits() : raw(0) {}
  };

class RGBMatrix {
 public:
  RGBMatrix(GPIO *io);
  void ClearScreen();
  void FillScreen(uint8_t red, uint8_t green, uint8_t blue);

  // Here the set-up  [>] [>]
  //                         v
  //                  [<] [<]   ... so column 65..127 are backwards.
  int width() const { return 32; }
  int height() const { return 32; }
  void SetPixel(uint8_t x, uint8_t y,
                uint8_t red, uint8_t green, uint8_t blue);

   int GetInput(void) { return io_->Read(); }

  // Updates the screen once. Call this in a continous loop in some realtime
  // thread.
  void UpdateScreen();

  void IncrementBrightness() { brightness++; if (brightness>10) { brightness = 1; } }
  void DecrementBrightness() { brightness--; if (brightness<=0) { brightness = 10; } }

private:
  GPIO *const io_;
  int brightness;

  enum {
    kDoubleRows = 16,     // Physical constant of the used board.
    kChainedBoards = 1,   // Number of boards that are daisy-chained.
    kColumns = kChainedBoards * 32,
    kPWMBits = 4          // maximum PWM resolution.
  };

  // A double row represents row n and n+16. The physical layout of the
  // 32x32 RGB is two sub-panels with 32 columns and 16 rows.
  struct DoubleRow {
    IoBits column[kColumns];  // only color bits are set
  };
  struct Screen {
    DoubleRow row[kDoubleRows];
  };

  Screen bitplane_[kPWMBits];
};

#endif  // RPI_RGBMATRIX_H
