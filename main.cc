// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include "thread.h"
#include "led-matrix.h"

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gif_lib.h>

#include <algorithm>

using std::min;
using std::max;

// Base-class for a Thread that does something with a matrix.
class RGBMatrixManipulator : public Thread {
public:
  RGBMatrixManipulator(RGBMatrix *m) : running_(true), matrix_(m) {}
  virtual ~RGBMatrixManipulator() { running_ = false; }

  // Run() implementation needs to check running_ regularly.

protected:
  volatile bool running_;  // TODO: use mutex, but this is good enough for now.
  RGBMatrix *const matrix_;
};

// Pump pixels to screen. Needs to be high priority real-time because jitter
// here will make the PWM uneven.
class DisplayUpdater : public RGBMatrixManipulator {
public:
  DisplayUpdater(RGBMatrix *m) : RGBMatrixManipulator(m) {}

  void Run() {
    while (running_) {
      matrix_->UpdateScreen();
    }
  }
};

// -- The following are demo image generators.

// Simple generator that pulses through RGB and White.
class ColorPulseGenerator : public RGBMatrixManipulator {
public:
  ColorPulseGenerator(RGBMatrix *m) : RGBMatrixManipulator(m) {}
  void Run() {
    const int width = matrix_->width();
    const int height = matrix_->height();
    uint32_t count = 0;
    while (running_) {
      sleep(2);
      ++count;
      int color = count % 6;
      int value = 0xff;
      int r, g, b;
      switch (color) {
      case 0: r = value; g = b = 0; break;
      case 1: r = g = value; b = 0; break;
      case 2: g = value; r = b = 0; break;
      case 3: g = b = value; r = 0; break;
      case 4: b = value; r = g = 0; break;
      default: r = g = b = value; break;
      }
      for (int x = 0; x < width; ++x)
        for (int y = 0; y < height; ++y)
          matrix_->SetPixel(x, y, r, g, b);
    }
  }
};

class SimpleSquare : public RGBMatrixManipulator {
public:
  SimpleSquare(RGBMatrix *m) : RGBMatrixManipulator(m) {}
  void Run() {
    const int width = matrix_->width();
    const int height = matrix_->height();
    // Diagonaly
    for (int x = 0; x < width; ++x) {
        matrix_->SetPixel(x, x, 255, 255, 255);
        matrix_->SetPixel(height -1 - x, x, 255, 0, 255);
    }
    for (int x = 0; x < width; ++x) {
      matrix_->SetPixel(x, 0, 255, 0, 0);
      matrix_->SetPixel(x, height - 1, 255, 255, 0);
    }
    for (int y = 0; y < height; ++y) {
      matrix_->SetPixel(0, y, 0, 0, 255);
      matrix_->SetPixel(width - 1, y, 0, 255, 0);
    }
  }
};

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

// amount of history to keep for detecting a cycle
#define NUM_BOARDS 1024

// width and height of board
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

// Simple class that generates a rotating block on the screen.
class RotatingBlockGenerator : public RGBMatrixManipulator {
public:
  RotatingBlockGenerator(RGBMatrix *m) : RGBMatrixManipulator(m) {}

  uint8_t scale_col(int val, int lo, int hi) {
    if (val < lo) return 0;
    if (val > hi) return 255;
    return 255 * (val - lo) / (hi - lo);
  }

  void Run() {
    const int cent_x = matrix_->width() / 2;
    const int cent_y = matrix_->height() / 2;

    // The square to rotate (inner square + black frame) needs to cover the
    // whole area, even if diagnoal.
    const int rotate_square = min(matrix_->width(), matrix_->height()) * 1.41;
    const int min_rotate = cent_x - rotate_square / 2;
    const int max_rotate = cent_x + rotate_square / 2;

    // The square to display is within the visible area.
    const int display_square = min(matrix_->width(), matrix_->height()) * 0.7;
    const int min_display = cent_x - display_square / 2;
    const int max_display = cent_x + display_square / 2;

    const float deg_to_rad = 2 * 3.14159265 / 360;
    int rotation = 0;
    while (running_) {
      ++rotation;
      usleep(15 * 1000);
      rotation %= 360;
      for (int x = min_rotate; x < max_rotate; ++x) {
        for (int y = min_rotate; y < max_rotate; ++y) {
          float disp_x, disp_y;
          Rotate(x - cent_x, y - cent_y,
                 deg_to_rad * rotation, &disp_x, &disp_y);
          if (x >= min_display && x < max_display &&
              y >= min_display && y < max_display) { // within display square
            matrix_->SetPixel(disp_x + cent_x, disp_y + cent_y,
                              scale_col(x, min_display, max_display),
                              255 - scale_col(y, min_display, max_display),
                              scale_col(y, min_display, max_display));
          } else {
            // black frame.
            matrix_->SetPixel(disp_x + cent_x, disp_y + cent_y, 0, 0, 0);
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

/* cropping animated gif:
       convert pacman.gif -coalesce -repage 0x0 -crop 107x107+108+89 +repage pacman107x107.gif
   resizing:
       convert pacman107x107.gif -filter point -resize 32x32 pacman32x32a.gif
*/

class GifAnimator: public RGBMatrixManipulator {
public:
  char *fn;
  GifFileType *gif;
  int rotation;
  int frameDelay;
  int xOff;
  int yOff;
  char *transColor;
  int transR, transG, transB;

  GifAnimator(RGBMatrix *m, char *fn, int aFrameDelay=30000, int aXOff=0, int aYOff=0, int aRotation=0, char *aTransColor=NULL) : RGBMatrixManipulator(m) {
      LoadGif(fn);
      rotation = 0;
      frameDelay = aFrameDelay;
      xOff = aXOff;
      yOff = aYOff;
      rotation = aRotation;

      transColor = aTransColor;
      if (transColor != NULL) {
          int tc = (int) strtol(transColor, NULL, 16);
          transR = (tc>>16);
          transG = (tc>>8) & 0xFF;
          transB = tc & 0xFF;
      }
  }

  int LoadGif(char *fn) {
    int result;

    gif = DGifOpenFileName(fn);
    if (gif==NULL) {
        fprintf(stderr, "failed to open %s\n", fn);
        exit(-1);
    }

    result = DGifSlurp(gif);
    if (result != GIF_OK) {
        fprintf(stderr, "failed to read gif\n");
        exit(-1);
    }

    return 1;
  }

  void CheckJoystick() {
      IoBits b;

      b.raw = matrix_->GetInput();

      if (!b.bits.joy_u) {
         rotation=2;
      } else if (!b.bits.joy_d) {
         rotation=0;
      } else if (!b.bits.joy_r) {
         rotation=3;
      } else if (!b.bits.joy_l) {
         rotation=1;
      }

      //fprintf(stdout, "%d %d %d %d %d\n", b.bits.joy_u, b.bits.joy_d, b.bits.joy_l, b.bits.joy_r, b.bits.joy_c);
  }

  void DisplaySavedImage(SavedImage *img) {
      int transparentIndex = -1;

      if (transColor != NULL) {
          for (int i=0; i<img->ExtensionBlockCount; i++) {
              ExtensionBlock *eb = &img->ExtensionBlocks[i];
              if (eb->Function==0xF9) {
                  transparentIndex = eb->Bytes[3];
              }
          }
      }

      for (int y=0; y<matrix_->height(); y++) {
          for (int x=0; x<matrix_->width(); x++) {
              unsigned char index;
              int px, py;
              GifColorType c;

              if (rotation == 0) {
                  px=x;
                  py=y;
              } else if (rotation == 1) {
                  px=y;
                  py=matrix_->width()-x-1;
              } else if (rotation == 2) {
                  px=matrix_->width()-x-1;
                  py=matrix_->height()-y-1;
              } else {
                  px=matrix_->height()-y-1;
                  py=x;
              }

              index = img->RasterBits[(py+yOff)*gif->SWidth+(px+xOff)];

              if (index == transparentIndex) {
                 matrix_->SetPixel(x,y,transR,transG,transB);
              } else {
                  if (img->ImageDesc.ColorMap!=NULL) {
                      c = img->ImageDesc.ColorMap->Colors[index];
                  } else {
                      c = gif->SColorMap->Colors[index];
                  }

                  matrix_->SetPixel(x, y, c.Red, c.Green, c.Blue);
              }
          }
      }
  }

  void Run() {
    int imageIndex = 0;
    while (running_) {
        CheckJoystick();
        DisplaySavedImage(&gif->SavedImages[imageIndex]);
        usleep(frameDelay);
        imageIndex ++;
        if (imageIndex >= gif->ImageCount) {
            imageIndex = 0;
        }
    }
  }
};

class ImageScroller : public RGBMatrixManipulator {
public:
  ImageScroller(RGBMatrix *m, int scroll_jumps)
    : RGBMatrixManipulator(m), scroll_jumps_(scroll_jumps),
      image_(NULL), horizontal_position_(0) {
  }

  // _very_ simplified. Can only read binary P6 PPM. Expects newlines in headers
  // Not really robust. Use at your own risk :)
  bool LoadPPM(const char *filename) {
    if (image_) {
      delete [] image_;
      image_ = NULL;
    }
    FILE *f = fopen(filename, "r");
    if (f == NULL) return false;
    char header_buf[256];
    const char *line = ReadLine(f, header_buf, sizeof(header_buf));
#define EXIT_WITH_MSG(m) { fprintf(stderr, "%s: %s |%s", filename, m, line); \
      fclose(f); return false; }
    if (sscanf(line, "P6 ") == EOF)
      EXIT_WITH_MSG("Can only handle P6 as PPM type.");
    line = ReadLine(f, header_buf, sizeof(header_buf));
    if (!line || sscanf(line, "%d %d ", &width_, &height_) != 2)
      EXIT_WITH_MSG("Width/height expected");
    int value;
    line = ReadLine(f, header_buf, sizeof(header_buf));
    if (!line || sscanf(line, "%d ", &value) != 1 || value != 255)
      EXIT_WITH_MSG("Only 255 for maxval allowed.");
    const size_t pixel_count = width_ * height_;
    image_ = new Pixel [ pixel_count ];
    assert(sizeof(Pixel) == 3);   // we make that assumption.
    if (fread(image_, sizeof(Pixel), pixel_count, f) != pixel_count) {
      line = "";
      EXIT_WITH_MSG("Not enough pixels read.");
    }
#undef EXIT_WITH_MSG
    fclose(f);
    fprintf(stderr, "Read image '%s' with %dx%d\n", filename, width_, height_);
    horizontal_position_ = 0;
    return true;
  }

  void Run() {
    const int screen_height = matrix_->height();
    const int screen_width = matrix_->width();
    while (running_) {
      if (image_ == NULL) {
        usleep(100 * 1000);
        continue;
      }
      usleep(30 * 1000);
      for (int x = 0; x < screen_width; ++x) {
        for (int y = 0; y < screen_height; ++y) {
          const Pixel &p = getPixel((horizontal_position_ + x) % width_, y);
          matrix_->SetPixel(x, y, p.red, p.green, p.blue);
        }
      }
      horizontal_position_ += scroll_jumps_;
      if (horizontal_position_ < 0) horizontal_position_ = width_;
    }
  }

private:
  struct Pixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  };

  // Read line, skip comments.
  char *ReadLine(FILE *f, char *buffer, size_t len) {
    char *result;
    do {
      result = fgets(buffer, len, f);
    } while (result != NULL && result[0] == '#');
    return result;
  }

  const Pixel &getPixel(int x, int y) {
    static Pixel dummy;
    if (x < 0 || x > width_ || y < 0 || y > height_) return dummy;
    return image_[x + width_ * y];
  }

  const int scroll_jumps_;
  int width_;
  int height_;
  Pixel *image_;
  int32_t horizontal_position_;
};

static int usage(const char *progname) {
  fprintf(stderr, "usage: %s <options> -D <demo-nr> [optional parameter]\n",
          progname);
  fprintf(stderr, "Options:\n"
          "\t-D <demo-nr>  : Always needs to be set\n"
          "\t-d            : run as daemon. Use this when starting in\n"
          "\t                /etc/init.d, but also when running without\n"
          "\t                terminal.\n"
          "\t-t <seconds>  : Run for these number of seconds, then exit\n"
          "\t       (if neither -d nor -t are supplied, waits for <RETURN>)\n"
          "\t-x <n>        : GifAnimator, x offset\n"
          "\t-y <n>        : GifAnimator, y offset\n"
          "\t-f <n>	   : GifAnimator, frame delay in milliseconds\n"
          "\t-r [0-3]      : GifAnimator, rotate 90 degree intervals\n"
          "\t-c #rrggbb    : GifAnimator, set transparent color\n");
  fprintf(stderr, "Demos, choosen with -D\n");
  fprintf(stderr, "\t0  - some rotating square\n"
          "\t1  - forward scrolling an image\n"
          "\t2  - backward scrolling an image\n"
          "\t3  - test image: a square\n"
          "\t4  - Pulsing color\n"
          "\t5  - Conway's game of life\n"
          "\t6  - Display animated gif\n");
  fprintf(stderr, "Example:\n\t%s -t 10 -D 1 runtext.ppm\n"
          "Scrolls the runtext for 10 seconds\n", progname);
  return 1;
}

class Controller : public Thread {
protected:
  int running_;
  RGBMatrix *matrix_;
  RGBMatrixManipulator *image_gen_;
  IoBits lastBits;
  int image_gen_index_;
  int image_gen_count_;

public:
  Controller(RGBMatrix *aMat) {
      matrix_ = aMat;
      running_ = true;
      image_gen_ = NULL;
      image_gen_index_ = 0;
      image_gen_count_ = 5;
  }

  ~Controller() {
      running_ = false;
  }

  void LaunchImageGenerator(int index) {
      switch (index) {
          case 0:
              image_gen_ = new GifAnimator(matrix_, (char*) "gifs/pacman32x32.gif", 30000, 0, 0, 3, NULL);
              break;

          case 1:
              image_gen_ = new GifAnimator(matrix_, (char*) "gifs/mario32x32.gif", 90000, 0, 0, 3, (char*) "#000000");
              break;

          case 2:
              image_gen_ = new GifAnimator(matrix_, (char*) "gifs/pbj33x35b.gif", 120000, 0, 3, 3, NULL);
              break;

          case 3:
              image_gen_ = new GifAnimator(matrix_, (char*) "gifs/pumpkin32x32.gif", 120000, 0, 0, 3, NULL);
              break;

          case 4:
              image_gen_ = new Conway(matrix_);
              break;
      }

      image_gen_ -> Start();
  }

  void NextImageGenerator() {
      image_gen_index_ = image_gen_index_ + 1;
      if (image_gen_index_ >= image_gen_count_) {
          image_gen_index_ = 0;
      }
      LaunchImageGenerator(image_gen_index_);
  }

  void sw1_up() {
      matrix_ -> DecrementBrightness();
  }

  void sw2_up() {
      if (image_gen_) {
          delete image_gen_;
          image_gen_ = NULL;

          NextImageGenerator();
      }
  }

  void Run() {
    LaunchImageGenerator(image_gen_index_);
    while (running_) {
      IoBits b;

      b.raw = matrix_->GetInput();

      if ((!b.bits.sw1) && (lastBits.bits.sw1)) {
          sw1_up();
      }
      if ((!b.bits.sw2) && (lastBits.bits.sw2)) {
          sw2_up();
      }

      lastBits.raw = b.raw;

      usleep(10 * 1000);
    }
    if (image_gen_ != NULL) {
      delete image_gen_;
      image_gen_ = NULL;
    }
  }
};

int main(int argc, char *argv[]) {
  bool as_daemon = false;
  int runtime_seconds = -1;
  int frameDelay = 30000;
  int demo = -1;
  int xOff = 0;
  int yOff = 0;
  int rotation =0;
  char *transColor = NULL;
  const char *demo_parameter = NULL;
  class Controller *controller = NULL;

  int opt;
  while ((opt = getopt(argc, argv, "D:t:df:x:y:r:c:")) != -1) {
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

    case 'f':
      frameDelay = atoi(optarg)*1000;
      break;

    case 'x':
      xOff = atoi(optarg);
      break;

    case 'y':
      yOff = atoi(optarg);
      break;

    case 'r':
      rotation = atoi(optarg);
      break;

    case 'c':
      transColor = optarg;
      break;

    default: /* '?' */
      return usage(argv[0]);
    }
  }

  if (optind < argc) {
    demo_parameter = argv[optind];
  }

  if (demo < 0) {
    fprintf(stderr, "Expect required option -D <demo>\n");
    return usage(argv[0]);
  }

  if (getuid() != 0) {
    fprintf(stderr, "Must run as root to be able to access /dev/mem\n"
            "Prepend 'sudo' to the command:\n\tsudo %s ...\n",
            argv[0]);
    return 1;
  }

  if (as_daemon) {
    if (fork() != 0)
      return 0;
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }

  GPIO io;
  if (!io.Init())
    return 1;

  // The matrix, our 'frame buffer'.
  RGBMatrix m(&io);

  // The RGBMatrixManipulator objects are filling
  // the matrix continuously.
  RGBMatrixManipulator *image_gen = NULL;
  switch (demo) {
  case 0:
    image_gen = new RotatingBlockGenerator(&m);
    break;

  case 1:
  case 2:
    if (demo_parameter) {
      ImageScroller *scroller = new ImageScroller(&m, demo == 1 ? 1 : -1);
      if (!scroller->LoadPPM(demo_parameter))
        return 1;
      image_gen = scroller;
    } else {
      fprintf(stderr, "Demo %d Requires PPM image as parameter\n", demo);
      return 1;
    }
    break;

  case 3:
    image_gen = new SimpleSquare(&m);
    break;

  case 4:
    image_gen = new ColorPulseGenerator(&m);
    break;

  case 5:
    image_gen = new Conway(&m);
    break;

  case 6:
    if (demo_parameter) {
      GifAnimator *animator = new GifAnimator(&m, (char*) demo_parameter, frameDelay, xOff, yOff, rotation, transColor);
      image_gen = animator;
    } else {
      fprintf(stderr, "Demo %d Requires FID image as parameter\n", demo);
      return 1;
    }
    break;

  case 7:
    controller = new Controller(&m);
    break;
  }

  if ((image_gen == NULL) && (controller == NULL)) {
    return usage(argv[0]);
  }

  // the DisplayUpdater continuously pushes the matrix
  // content to the display.
  RGBMatrixManipulator *updater = new DisplayUpdater(&m);
  updater->Start(10);   // high priority

  if (controller != NULL) {
      controller->Start();
  } else {
      image_gen->Start();
  }

  if (as_daemon) {
    sleep(runtime_seconds > 0 ? runtime_seconds : INT_MAX);
  } else if (runtime_seconds > 0) {
    sleep(runtime_seconds);
  } else {
    // Things are set up. Just wait for <RETURN> to be pressed.
    printf("Press <RETURN> to exit and reset LEDs\n");
    getchar();
  }

  // Stopping threads and wait for them to join.
  if (controller != NULL) {
      delete controller;
  } else {
      delete image_gen;
  }
  delete updater;

  // Final thing before exit: clear screen and update once, so that
  // we don't have random pixels burn
  m.ClearScreen();
  m.UpdateScreen();

  return 0;
}
