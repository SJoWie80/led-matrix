// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
//
// This code is public domain
// (but note, that the led-matrix library this depends on is GPL v2)

#include "led-matrix.h"
#include "threaded-canvas-manipulator.h"

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>

using std::min;
using std::max;

using namespace rgb_matrix;

// This is an example how to use the Canvas abstraction to map coordinates.
//
// This is a Canvas that delegates to some other Canvas (typically, the RGB
// matrix).
//
// Here, we want to address four 32x32 panels as one big 64x64 panel. Physically,
// we chain them together and do a 180 degree 'curve', somewhat like this:
// [>] [>]
//         v
// [<] [<]
class LargeSquare64x64Canvas : public Canvas {
public:
  // This class takes over ownership of the delegatee.
  LargeSquare64x64Canvas(Canvas *delegatee) : delegatee_(delegatee) {
    // Our assumptions of the underlying geometry:
    assert(delegatee->height() == 32);
    assert(delegatee->width() == 128);
  }
  virtual ~LargeSquare64x64Canvas() { delete delegatee_; }

  virtual void Clear() { delegatee_->Clear(); }
  virtual void Fill(uint8_t red, uint8_t green, uint8_t blue) {
    delegatee_->Fill(red, green, blue);
  }
  virtual int width() const { return 64; }
  virtual int height() const { return 64; }
  virtual void SetPixel(int x, int y,
                        uint8_t red, uint8_t green, uint8_t blue) {
    if (x < 0 || x >= width() || y < 0 || y >= height()) return;
    // We have up to column 64 one direction, then folding around. Lets map
    if (y > 31) {
      x = 127 - x;
      y = 63 - y;
    }
    delegatee_->SetPixel(x, y, red, green, blue);
  }

private:
Canvas *delegatee_;
};


class LargeSquare96x64Canvas : public Canvas {
public:
  // This class takes over ownership of the delegatee.
  LargeSquare96x64Canvas(Canvas *delegatee) : delegatee_(delegatee) {
    // Our assumptions of the underlying geometry:
    assert(delegatee->height() == 32);
    assert(delegatee->width() == 192);
  }
  virtual ~LargeSquare96x64Canvas() { delete delegatee_; }

  virtual void Clear() { delegatee_->Clear(); }
  virtual void Fill(uint8_t red, uint8_t green, uint8_t blue) {
    delegatee_->Fill(red, green, blue);
  }
  virtual int width() const { return 96; }
  virtual int height() const { return 64; }
  virtual void SetPixel(int x, int y,
                        uint8_t red, uint8_t green, uint8_t blue) {
    if (x < 0 || x >= width() || y < 0 || y >= height()) return;
    // We have up to column 64 one direction, then folding around. Lets map
    if (y > 31) {
      x = 191 - x;
      y = 63 - y;
    }
    delegatee_->SetPixel(x, y, red, green, blue);
  }



private:
  Canvas *delegatee_;
};

/*
 * The following are demo image generators. They all use the utility
 * class ThreadedCanvasManipulator to generate new frames.
 */

/* ***************************************************************************

   Conway's Game of Life Demo
   Dr. Scott M. Baker, smbaker@gmail.com

   This MatrixManipulator implements Conway's Game of Life.

   The board is randomly seeded. After that, the board is updated according to
   the following rules:

      1) A cell with less than two neighbors dies due to underpopulation
      2) A cell with exactly 2 or 3 neighbors continues to live
      3) A cell with more than three neighbors dies due to overpopulation
      4) An empty spot with exactly three live neighbors becomes live, due to
         reprorduction.

      (see http://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)

   Edges of the board wrap around to the opposing edge. For example, the 32nd
   column is identical to the 1st column.

   The board continues to be updated until a cycle is detected. The cycle will
   be displayed for 25 updates and then the board will be re-seeded.

   ***************************************************************************
*/

#define NUM_BOARDS 10

#define WIDTH 32
#define HEIGHT 32

#define XY(x,y) ((y)*HEIGHT+(x))

class Conway : public RGBMatrixManipulator {
public:
  int *boards[NUM_BOARDS];
  int curBoardIndex;
  int liveR, liveG, liveB;

  Conway(RGBMatrix *m) : RGBMatrixManipulator(m) {
      liveR = 0;
      liveG = 0;
      liveB = 192;

      curBoardIndex = 0;

      for (int i=0; i<NUM_BOARDS; i++) {
          boards[i] = (int*) malloc(HEIGHT * WIDTH * sizeof(int));
          bzero(boards[i], HEIGHT * WIDTH * sizeof(int));
      }
  }

  void Run() {
      int cycleCount = 0;

      SeedBoard();
      while (running_) {
          int *curBoard = boards[curBoardIndex];
          for (int x=0; x<32; x++) {
              for (int y=0; y<32; y++) {
                  int live = curBoard[XY(x,y)];
                  matrix_->SetPixel(x,y, live * liveR, live * liveG, live * liveB);
              }
          }
          usleep(100*1000);
          UpdateBoard();

          if (cycleCount>0) {
              cycleCount = cycleCount - 1;
              if (cycleCount==0) {
                  SeedBoard();
              }
          } else {
              if (CheckCycle()) {
                  // once a cycle is detected, display it for a while before
                  // resetting the board.
                  cycleCount = 25;
              }
          }
      }
  }

  int GetBoard(int x, int y) {
      if (x<0) {
          x=x+32;
      }
      if (y<0) {
          y=y+32;
      }
      if (x>=32) {
          x=x-32;
      }
      if (y>=32) {
          y=y-32;
      }
      return boards[curBoardIndex][XY(x,y)];
  }

  void SetBoard(int x, int y, int j) {
      if (x<0) {
          x=x+32;
      }
      if (y<0) {
          y=y+32;
      }
      if (x>=32) {
          x=x-32;
      }
      if (y>=32) {
          y=y-32;
      }
      boards[curBoardIndex][XY(x,y)] = j;
  }

  void UpdateBoard() {
      int *newBoard;
      int newBoardIndex;

      newBoardIndex = curBoardIndex + 1;
      if (newBoardIndex >= NUM_BOARDS) {
          newBoardIndex = newBoardIndex - NUM_BOARDS;
      }
      newBoard = boards[newBoardIndex];

      for (int x=0; x<32; x++) {
          for (int y=0; y<32; y++) {
              int neighbors = 0;
              for (int nx=x-1; nx<=x+1; nx++) {
                  for (int ny=y-1; ny<=y+1; ny++) {
                      if ((nx!=x) || (ny!=y)) {
                          neighbors += GetBoard(nx,ny);
                      }
                  }
              }
              int live = GetBoard(x,y);
              newBoard[XY(x,y)] = 0;
              if ((live) && (neighbors>=2) && (neighbors<=3)) {
                  newBoard[XY(x,y)] = 1;
              } else if ((!live) && (neighbors==3)) {
                  newBoard[XY(x,y)] = 1;
              }
          }
      }

      curBoardIndex = newBoardIndex;
  }

  int CheckCycle() {
      for(int i=0; i<NUM_BOARDS; i++) {
          if (i==curBoardIndex) {
              continue;
          }
          if (memcmp(boards[curBoardIndex], boards[i], HEIGHT*WIDTH*sizeof(int)) == 0) {
              return 1;
          }
      }
      return 0;
  }


  void SeedBoard() {
      int *board = boards[curBoardIndex];
      for (int x=0; x<32; x++) {
          for (int y=0; y<32; y++) {
              if (random()%3==1) {
                  board[XY(x,y)] = 1;
              } else {
                  board[XY(x,y)] = 0;
              }
          }
      }
  }

};


// Simple generator that pulses through RGB and White.
class ColorPulseGenerator : public ThreadedCanvasManipulator {
public:
  ColorPulseGenerator(Canvas *m) : ThreadedCanvasManipulator(m) {}
  void Run() {
    uint32_t continuum = 0;
    while (running()) {
      usleep(5 * 1000);
      continuum += 1;
      continuum %= 3 * 255;
      int r = 0, g = 0, b = 0;
      if (continuum <= 255) {
        int c = continuum;
        b = 255 - c;
        r = c;
      } else if (continuum > 255 && continuum <= 511) {
        int c = continuum - 256;
        r = 255 - c;
        g = c;
      } else {
        int c = continuum - 512;
        g = 255 - c;
        b = c;
      }
      canvas()->Fill(r, g, b);
    }
  }
};

class SimpleSquare : public ThreadedCanvasManipulator {
public:
  SimpleSquare(Canvas *m) : ThreadedCanvasManipulator(m) {}
  void Run() {
    const int width = canvas()->width();
    const int height = canvas()->height();
    // Diagonal
    for (int x = 0; x < width; ++x) {
      canvas()->SetPixel(x, x, 255, 255, 255);           // white
      canvas()->SetPixel(height -1 - x, x, 255, 0, 255); // magenta
    }
    for (int x = 0; x < width; ++x) {
      canvas()->SetPixel(x, 0, 255, 0, 0);              // top line: red
      canvas()->SetPixel(x, height - 1, 255, 255, 0);   // bottom line: yellow
    }
    for (int y = 0; y < height; ++y) {
      canvas()->SetPixel(0, y, 0, 0, 255);              // left line: blue
      canvas()->SetPixel(width - 1, y, 0, 255, 0);      // right line: green
    }
  }
};

class GrayScaleBlock : public ThreadedCanvasManipulator {
public:
  GrayScaleBlock(Canvas *m) : ThreadedCanvasManipulator(m) {}
  void Run() {
    const int sub_blocks = 16;
    const int width = canvas()->width();
    const int height = canvas()->height();
    const int x_step = max(1, width / sub_blocks);
    const int y_step = max(1, height / sub_blocks);
    uint8_t count = 0;
    while (running()) {
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          int c = sub_blocks * (y / y_step) + x / x_step;
          switch (count % 4) {
          case 0: canvas()->SetPixel(x, y, c, c, c); break;
          case 1: canvas()->SetPixel(x, y, c, 0, 0); break;
          case 2: canvas()->SetPixel(x, y, 0, c, 0); break;
          case 3: canvas()->SetPixel(x, y, 0, 0, c); break;
          }
        }
      }
      count++;
      sleep(2);
    }
  }
};

// Simple class that generates a rotating block on the screen.
class RotatingBlockGenerator : public ThreadedCanvasManipulator {
public:
  RotatingBlockGenerator(Canvas *m) : ThreadedCanvasManipulator(m) {}

  uint8_t scale_col(int val, int lo, int hi) {
    if (val < lo) return 0;
    if (val > hi) return 255;
    return 255 * (val - lo) / (hi - lo);
  }

  void Run() {
    const int cent_x = canvas()->width() / 2;
    const int cent_y = canvas()->height() / 2;

    // The square to rotate (inner square + black frame) needs to cover the
    // whole area, even if diagnoal. Thus, when rotating, the outer pixels from
    // the previous frame are cleared.
    const int rotate_square = min(canvas()->width(), canvas()->height()) * 1.41;
    const int min_rotate = cent_x - rotate_square / 2;
    const int max_rotate = cent_x + rotate_square / 2;

    // The square to display is within the visible area.
    const int display_square = min(canvas()->width(), canvas()->height()) * 0.7;
    const int min_display = cent_x - display_square / 2;
    const int max_display = cent_x + display_square / 2;

    const float deg_to_rad = 2 * 3.14159265 / 360;
    int rotation = 0;
    while (running()) {
      ++rotation;
      usleep(15 * 1000);
      rotation %= 360;
      for (int x = min_rotate; x < max_rotate; ++x) {
        for (int y = min_rotate; y < max_rotate; ++y) {
          float rot_x, rot_y;
          Rotate(x - cent_x, y - cent_x,
                 deg_to_rad * rotation, &rot_x, &rot_y);
          if (x >= min_display && x < max_display &&
              y >= min_display && y < max_display) { // within display square
            canvas()->SetPixel(rot_x + cent_x, rot_y + cent_y,
                               scale_col(x, min_display, max_display),
                               255 - scale_col(y, min_display, max_display),
                               scale_col(y, min_display, max_display));
          } else {
            // black frame.
            canvas()->SetPixel(rot_x + cent_x, rot_y + cent_y, 0, 0, 0);
          }
        }
      }
    }
  }

private:
  void Rotate(int x, int y, float angle,
              float *new_x, float *new_y) {
    *new_x = x * cosf(angle) - y * sinf(angle);
    *new_y = x * sinf(angle) + y * cosf(angle);
  }
};

class ImageScroller : public ThreadedCanvasManipulator {
public:
  // Scroll image with "scroll_jumps" pixels every "scroll_ms" milliseconds.
  // If "scroll_ms" is negative, don't do any scrolling.
  ImageScroller(Canvas *m, int scroll_jumps, int scroll_ms = 30)
    : ThreadedCanvasManipulator(m), scroll_jumps_(scroll_jumps),
      scroll_ms_(scroll_ms),
      horizontal_position_(0) {
  }

  virtual ~ImageScroller() {
    Stop();
    WaitStopped();   // only now it is safe to delete our instance variables.
  }

  // _very_ simplified. Can only read binary P6 PPM. Expects newlines in headers
  // Not really robust. Use at your own risk :)
  // This allows reload of an image while things are running, e.g. you can
  // life-update the content.
  bool LoadPPM(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) return false;
    char header_buf[256];
    const char *line = ReadLine(f, header_buf, sizeof(header_buf));
#define EXIT_WITH_MSG(m) { fprintf(stderr, "%s: %s |%s", filename, m, line); \
      fclose(f); return false; }
    if (sscanf(line, "P6 ") == EOF)
      EXIT_WITH_MSG("Can only handle P6 as PPM type.");
    line = ReadLine(f, header_buf, sizeof(header_buf));
    int new_width, new_height;
    if (!line || sscanf(line, "%d %d ", &new_width, &new_height) != 2)
      EXIT_WITH_MSG("Width/height expected");
    int value;
    line = ReadLine(f, header_buf, sizeof(header_buf));
    if (!line || sscanf(line, "%d ", &value) != 1 || value != 255)
      EXIT_WITH_MSG("Only 255 for maxval allowed.");
    const size_t pixel_count = new_width * new_height;
    Pixel *new_image = new Pixel [ pixel_count ];
    assert(sizeof(Pixel) == 3);   // we make that assumption.
    if (fread(new_image, sizeof(Pixel), pixel_count, f) != pixel_count) {
      line = "";
      EXIT_WITH_MSG("Not enough pixels read.");
    }
#undef EXIT_WITH_MSG
    fclose(f);
    fprintf(stderr, "Read image '%s' with %dx%d\n", filename,
            new_width, new_height);
    horizontal_position_ = 0;
    MutexLock l(&mutex_new_image_);
    new_image_.Delete();  // in case we reload faster than is picked up
    new_image_.image = new_image;
    new_image_.width = new_width;
    new_image_.height = new_height;
    return true;
  }

  void Run() {
    const int screen_height = canvas()->height();
    const int screen_width = canvas()->width();
    while (running()) {
      {
        MutexLock l(&mutex_new_image_);
        if (new_image_.IsValid()) {
          current_image_.Delete();
          current_image_ = new_image_;
          new_image_.Reset();
        }
      }
      if (!current_image_.IsValid()) {
        usleep(100 * 1000);
        continue;
      }
      for (int x = 0; x < screen_width; ++x) {
        for (int y = 0; y < screen_height; ++y) {
          const Pixel &p = current_image_.getPixel(
                     (horizontal_position_ + x) % current_image_.width, y);
          canvas()->SetPixel(x, y, p.red, p.green, p.blue);
        }
      }
      horizontal_position_ += scroll_jumps_;
      if (horizontal_position_ < 0) horizontal_position_ = current_image_.width;
      if (scroll_ms_ <= 0) {
        // No scrolling. We don't need the image anymore.
        current_image_.Delete();
      } else {
        usleep(scroll_ms_ * 1000);
      }
    }
  }

private:
  struct Pixel {
    Pixel() : red(0), green(0), blue(0){}
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  };

  struct Image {
    Image() : width(-1), height(-1), image(NULL) {}
    ~Image() { Delete(); }
    void Delete() { delete [] image; Reset(); }
    void Reset() { image = NULL; width = -1; height = -1; }
    inline bool IsValid() { return image && height > 0 && width > 0; }
    const Pixel &getPixel(int x, int y) {
      static Pixel black;
      if (x < 0 || x >= width || y < 0 || y >= height) return black;
      return image[x + width * y];
    }

    int width;
    int height;
    Pixel *image;
  };

  // Read line, skip comments.
  char *ReadLine(FILE *f, char *buffer, size_t len) {
    char *result;
    do {
      result = fgets(buffer, len, f);
    } while (result != NULL && result[0] == '#');
    return result;
  }

  const int scroll_jumps_;
  const int scroll_ms_;

  // Current image is only manipulated in our thread.
  Image current_image_;

  // New image can be loaded from another thread, then taken over in main thread.
  Mutex mutex_new_image_;
  Image new_image_;

  int32_t horizontal_position_;
};

static int usage(const char *progname) {
  fprintf(stderr, "usage: %s <options> -D <demo-nr> [optional parameter]\n",
          progname);
  fprintf(stderr, "Options:\n"
          "\t-r <rows>     : Display rows. 16 for 16x32, 32 for 32x32. "
          "Default: 32\n"
          "\t-c <chained>  : Daisy-chained boards. Default: 1.\n"
          "\t-L            : 'Large' display, composed out of 4 times 32x32\n"
          "\t-V            : 'Verry Large' display, composed out of 6 times 32x32\n"
          "\t-m <ms>       : Scroll speed 0 for disable\n"
          "\t-p <pwm-bits> : Bits used for PWM. Something between 1..11\n"
          "\t-l            : Don't do luminance correction (CIE1931)\n"
          "\t-D <demo-nr>  : Always needs to be set\n"
          "\t-d            : run as daemon. Use this when starting in\n"
          "\t                /etc/init.d, but also when running without\n"
          "\t                terminal (e.g. cron).\n"
          "\t-t <seconds>  : Run for these number of seconds, then exit.\n"
          "\t       (if neither -d nor -t are supplied, waits for <RETURN>)\n");
  fprintf(stderr, "Demos, choosen with -D\n");
  fprintf(stderr, "\t0  - some rotating square\n"
          "\t1  - forward scrolling an image (-m <scroll-ms>)\n"
          "\t2  - backward scrolling an image (-m <scroll-ms>)\n"
          "\t3  - test image: a square\n"
          "\t4  - Pulsing color\n"
          "\t5  - Grayscale Block\n");
  fprintf(stderr, "Example:\n\t%s -t 10 -D 1 runtext.ppm\n"
          "Scrolls the runtext for 10 seconds\n", progname);
  return 1;
}

int main(int argc, char *argv[]) {
  bool as_daemon = false;
  int runtime_seconds = -1;
  int demo = -1;
  int rows = 32;
  int chain = 1;
  int scroll_ms = 30;
  int pwm_bits = -1;
  bool large_display = false;
  bool verry_large_display = false;
  bool do_luminance_correct = true;

  const char *demo_parameter = NULL;

  int opt;
  while ((opt = getopt(argc, argv, "dlD:t:r:p:c:m:L:V")) != -1) {
    switch (opt) {
    case 'D':
      demo = atoi(optarg);
      break;

    case 'd':
      as_daemon = true;
      break;

    case 't':
      runtime_seconds = atoi(optarg);
      break;

    case 'r':
      rows = atoi(optarg);
      break;

    case 'c':
      chain = atoi(optarg);
      break;

    case 'm':
      scroll_ms = atoi(optarg);
      break;

    case 'p':
      pwm_bits = atoi(optarg);
      break;

    case 'l':
      do_luminance_correct = !do_luminance_correct;
      break;

    case 'L':
      // The 'large' display assumes a chain of four displays with 32x32
      chain = 4;
      rows = 32;
      large_display = true;
      break;
      
    case 'V':
      // The 'large' display assumes a chain of six displays with 32x32
      chain = 6;
      rows = 32;
      verry_large_display = true;
      break;
      
    default: /* '?' */
      return usage(argv[0]);
    }
  }

  if (optind < argc) {
    demo_parameter = argv[optind];
  }

  if (demo < 0) {
    fprintf(stderr, "Expected required option -D <demo>\n");
    return usage(argv[0]);
  }

  if (getuid() != 0) {
    fprintf(stderr, "Must run as root to be able to access /dev/mem\n"
            "Prepend 'sudo' to the command:\n\tsudo %s ...\n", argv[0]);
    return 1;
  }

  if (rows != 16 && rows != 32) {
    fprintf(stderr, "Rows can either be 16 or 32\n");
    return 1;
  }

  if (chain < 1) {
    fprintf(stderr, "Chain outside usable range\n");
    return 1;
  }
  if (chain > 8) {
    fprintf(stderr, "That is a long chain. Expect some flicker.\n");
  }

  // Initialize GPIO pins. This might fail when we don't have permissions.
  GPIO io;
  if (!io.Init())
    return 1;

  // Start daemon before we start any threads.
  if (as_daemon) {
    if (fork() != 0)
      return 0;
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }

  // The matrix, our 'frame buffer' and display updater.
  RGBMatrix *matrix = new RGBMatrix(&io, rows, chain);
  matrix->set_luminance_correct(do_luminance_correct);
  if (pwm_bits >= 0 && !matrix->SetPWMBits(pwm_bits)) {
    fprintf(stderr, "Invalid range of pwm-bits\n");
    return 1;
  }

  Canvas *canvas = matrix;

  if (large_display) {
    // Mapping the coordinates of a 32x128 display mapped to a square of 64x64
    canvas = new LargeSquare64x64Canvas(canvas);
  }

  if (verry_large_display) {
    // Mapping the coordinates of a 32x128 display mapped to a square of 64x64
    canvas = new LargeSquare96x64Canvas(canvas);
  }
  // The ThreadedCanvasManipulator objects are filling
  // the matrix continuously.
  ThreadedCanvasManipulator *image_gen = NULL;
  switch (demo) {
  case 0:
    image_gen = new RotatingBlockGenerator(canvas);
    break;

  case 1:
  case 2:
    if (demo_parameter) {
      ImageScroller *scroller = new ImageScroller(canvas,
                                                  demo == 1 ? 1 : -1,
                                                  scroll_ms);
      if (!scroller->LoadPPM(demo_parameter))
        return 1;
      image_gen = scroller;
    } else {
      fprintf(stderr, "Demo %d Requires PPM image as parameter\n", demo);
      return 1;
    }
    break;

  case 3:
    image_gen = new SimpleSquare(canvas);
    break;

  case 4:
    image_gen = new ColorPulseGenerator(canvas);
    break;

  case 5:
    image_gen = new GrayScaleBlock(canvas);
    break;
  }

  case 6:
    image_gen = new Conway(&m);
    break;
  }

  if (image_gen == NULL)
    return usage(argv[0]);

  // Image generating demo is crated. Now start the thread.
  image_gen->Start();

  // Now, the image genreation runs in the background. We can do arbitrary
  // things here in parallel. In this demo, we're essentially just
  // waiting for one of the conditions to exit.
  if (as_daemon) {
    sleep(runtime_seconds > 0 ? runtime_seconds : INT_MAX);
  } else if (runtime_seconds > 0) {
    sleep(runtime_seconds);
  } else {
    // Things are set up. Just wait for <RETURN> to be pressed.
    printf("Press <RETURN> to exit and reset LEDs\n");
    getchar();
  }

  // Stop image generating thread.
  delete image_gen;
  delete canvas;

  return 0;
}
