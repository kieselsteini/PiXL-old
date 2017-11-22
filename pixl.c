////////////////////////////////////////////////////////////////////////////////
//
// PiXL - a tiny Lua pixel/chiptune engine
//
// MIT License
//
// Copyright(c) 2017 Sebastian Steinhauer
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

#include "SDL.h"
#include "lua53.h"
#include "lz4.h"

// SDL2 pragmas (MS VS C++)
#ifdef _WIN32
#pragma comment(lib, "SDL2.lib")
#pragma comment(lib, "SDL2main.lib")
#endif // _WIN32

// network includes
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#define INVALID_SOCKET -1
#define closesocket(s) close(s)
#endif // _WIN32



////////////////////////////////////////////////////////////////////////////////
//
//  Configuration
//
////////////////////////////////////////////////////////////////////////////////

// maximum and default screen resolution
#define PX_SCREEN_MAX_WIDTH   1024
#define PX_SCREEN_MAX_HEIGHT  1024
#define PX_SCREEN_WIDTH       256
#define PX_SCREEN_HEIGHT      240

// Window title
#define PX_WINDOW_TITLE       "PiXL Window"
#define PX_WINDOW_PADDING     32

// Audio settings
#define PX_AUDIO_CHANNELS     8
#define PX_AUDIO_FREQUENCY    44100
#define PX_AUDIO_NOISE        1024

// Frame time
#define PX_FPS                30
#define PX_FPS_TICKS          (1000 / PX_FPS)

// Number of controllers
#define PX_NUM_CONTROLLERS    8

// Version and Author
#define PX_AUTHOR             "Sebastian Steinhauer <s.steinhauer@yahoo.de>"
#define PX_VERSION            530



////////////////////////////////////////////////////////////////////////////////
//
//  Global Variables
//
////////////////////////////////////////////////////////////////////////////////

// SDL stuff
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;
SDL_AudioDeviceID audio_device = 0;

// Screen
Uint8 screen[PX_SCREEN_MAX_WIDTH][PX_SCREEN_MAX_HEIGHT];
SDL_Point translation;
int screen_width, screen_height;

// Audio
enum {
  PX_WAVEFORM_SILENCE,
  PX_WAVEFORM_PULSE_12, PX_WAVEFORM_PULSE_25, PX_WAVEFORM_PULSE_50,
  PX_WAVEFORM_SAWTOOTH, PX_WAVEFORM_TRIANLGE,
  PX_WAVEFORM_NOISE
};

typedef struct AudioChannel {
  // MML source and current pointer
  const char *source;
  const char *in;
  int looping;
  // current waveform settings
  float t;
  float frequency;
  int waveform;
  int duration;
  int silence;
  // current parse settings
  int tempo;
  int octave;
  int default_length;
} AudioChannel;

AudioChannel channels[PX_AUDIO_CHANNELS];
Sint8 audio_noise[PX_AUDIO_NOISE];
float mixing_frequency;

// Input
enum {
  PX_BUTTON_A, PX_BUTTON_B, PX_BUTTON_X, PX_BUTTON_Y,
  PX_BUTTON_LEFT, PX_BUTTON_RIGHT, PX_BUTTON_UP, PX_BUTTON_DOWN,
  PX_BUTTON_START,
  PX_BUTTON_LAST
};

typedef struct Input {
  // state
  Uint16      down, pressed;
  SDL_Point   mouse;
} Input;

Input inputs[PX_NUM_CONTROLLERS];

// assorted stuff
int running;
int fullscreen;
Uint32 seed;
int margc;
char **margv;

// UDP networking
int socket_fd;



////////////////////////////////////////////////////////////////////////////////
//
//  Static data
//
////////////////////////////////////////////////////////////////////////////////

// map string to number values (string color mapping)
static const Uint8 sprite_color_map[128] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
  2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10,
  11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0
};

// default color palette
static const SDL_Color colors[16] = {
  { 0x00, 0x00, 0x00, 0xFF }, // 00 black
  { 0x1D, 0x2B, 0x53, 0xFF }, // 01 dark-blue
  { 0x7E, 0x25, 0x53, 0xFF }, // 02 dark-purple
  { 0x00, 0x87, 0x51, 0xFF }, // 03 dark-green
  { 0xAB, 0x52, 0x36, 0xFF }, // 04 brown
  { 0x5F, 0x57, 0x4F, 0xFF }, // 05 dark-gray
  { 0xC2, 0xC3, 0xC7, 0xFF }, // 06 light-gray
  { 0xFF, 0xF1, 0xE8, 0xFF }, // 07 white
  { 0xFF, 0x00, 0x4D, 0xFF }, // 08 red
  { 0xFF, 0xA3, 0x00, 0xFF }, // 09 orange
  { 0xFF, 0xEC, 0x27, 0xFF }, // 10 yellow
  { 0x00, 0xE4, 0x36, 0xFF }, // 11 green
  { 0x29, 0xAD, 0xFF, 0xFF }, // 12 blue
  { 0x83, 0x76, 0x9C, 0xFF }, // 13 indigo
  { 0xFF, 0x77, 0xA8, 0xFF }, // 14 pink
  { 0xFF, 0xCC, 0xAA, 0xFF }  // 15 peach
};

// basic ASCII font (bit encoded)
static const Uint8 font8x8[128][8] = {
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0000 (nul)
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0001
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0002
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0003
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0004
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0005
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0006
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0007
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0008
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0009
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000A
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000B
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000C
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000D
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000E
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000F
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0010
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0011
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0012
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0013
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0014
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0015
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0016
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0017
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0018
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0019
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001A
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001B
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001C
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001D
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001E
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001F
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0020 (space)
  { 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00 },   // U+0021 (!)
  { 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0022 (")
  { 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00 },   // U+0023 (#)
  { 0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00 },   // U+0024 ($)
  { 0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00 },   // U+0025 (%)
  { 0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00 },   // U+0026 (&)
  { 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0027 (')
  { 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00 },   // U+0028 (()
  { 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00 },   // U+0029 ())
  { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00 },   // U+002A (*)
  { 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00 },   // U+002B (+)
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06 },   // U+002C (,)
  { 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00 },   // U+002D (-)
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00 },   // U+002E (.)
  { 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00 },   // U+002F (/)
  { 0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00 },   // U+0030 (0)
  { 0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00 },   // U+0031 (1)
  { 0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00 },   // U+0032 (2)
  { 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00 },   // U+0033 (3)
  { 0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00 },   // U+0034 (4)
  { 0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00 },   // U+0035 (5)
  { 0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00 },   // U+0036 (6)
  { 0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00 },   // U+0037 (7)
  { 0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00 },   // U+0038 (8)
  { 0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00 },   // U+0039 (9)
  { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00 },   // U+003A (:)
  { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06 },   // U+003B (//)
  { 0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00 },   // U+003C (<)
  { 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00 },   // U+003D (=)
  { 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00 },   // U+003E (>)
  { 0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00 },   // U+003F (?)
  { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00 },   // U+0040 (@)
  { 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00 },   // U+0041 (A)
  { 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00 },   // U+0042 (B)
  { 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00 },   // U+0043 (C)
  { 0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00 },   // U+0044 (D)
  { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00 },   // U+0045 (E)
  { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00 },   // U+0046 (F)
  { 0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00 },   // U+0047 (G)
  { 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00 },   // U+0048 (H)
  { 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0049 (I)
  { 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00 },   // U+004A (J)
  { 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00 },   // U+004B (K)
  { 0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00 },   // U+004C (L)
  { 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00 },   // U+004D (M)
  { 0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00 },   // U+004E (N)
  { 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00 },   // U+004F (O)
  { 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00 },   // U+0050 (P)
  { 0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00 },   // U+0051 (Q)
  { 0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00 },   // U+0052 (R)
  { 0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00 },   // U+0053 (S)
  { 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0054 (T)
  { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00 },   // U+0055 (U)
  { 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00 },   // U+0056 (V)
  { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00 },   // U+0057 (W)
  { 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00 },   // U+0058 (X)
  { 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0059 (Y)
  { 0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00 },   // U+005A (Z)
  { 0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00 },   // U+005B ([)
  { 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00 },   // U+005C (\)
  { 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00 },   // U+005D (])
  { 0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00 },   // U+005E (^)
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },   // U+005F (_)
  { 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0060 (`)
  { 0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00 },   // U+0061 (a)
  { 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00 },   // U+0062 (b)
  { 0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00 },   // U+0063 (c)
  { 0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00 },   // U+0064 (d)
  { 0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00 },   // U+0065 (e)
  { 0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00 },   // U+0066 (f)
  { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F },   // U+0067 (g)
  { 0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00 },   // U+0068 (h)
  { 0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0069 (i)
  { 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E },   // U+006A (j)
  { 0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00 },   // U+006B (k)
  { 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+006C (l)
  { 0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00 },   // U+006D (m)
  { 0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00 },   // U+006E (n)
  { 0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00 },   // U+006F (o)
  { 0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F },   // U+0070 (p)
  { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78 },   // U+0071 (q)
  { 0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00 },   // U+0072 (r)
  { 0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00 },   // U+0073 (s)
  { 0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00 },   // U+0074 (t)
  { 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00 },   // U+0075 (u)
  { 0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00 },   // U+0076 (v)
  { 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00 },   // U+0077 (w)
  { 0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00 },   // U+0078 (x)
  { 0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F },   // U+0079 (y)
  { 0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00 },   // U+007A (z)
  { 0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00 },   // U+007B ({)
  { 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00 },   // U+007C (|)
  { 0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00 },   // U+007D (})
  { 0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+007E (~)
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }    // U+007F
};



////////////////////////////////////////////////////////////////////////////////
//
//  Randomizer + some helper routines
//
////////////////////////////////////////////////////////////////////////////////

static Uint32 px_rand() {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed;
}

static void px_randomseed(Uint32 s) {
  int i;
  seed = s ? s : 47; // prevent 0 as seed
  for (i = 0; i < 1024; ++i) (void)px_rand();
}

static int px_check_parm(const char *name) {
  int i;
  for (i = 1; i < margc; ++i) if (!SDL_strcmp(name, margv[i])) return i;
  return 0;
}

static const char *px_check_arg(const char *name) {
  int i = px_check_parm(name);
  if (i && (i + 1) < margc) return margv[i + 1];
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
//
//  Video Drawing Primitives
//
////////////////////////////////////////////////////////////////////////////////

#define swap(T, a, b) do { T _tmp_ = a; a = b; b = _tmp_; } while(0)

static void _pixel(Uint8 color, int x0, int y0) {
  int x = x0 + translation.x;
  int y = y0 + translation.y;
  if (x >= 0 && x < screen_width && y >= 0 && y < screen_height) screen[x][y] = color;
}

static int f_clear(lua_State *L) {
  Uint8 color = (Uint8)luaL_optinteger(L, 1, 0);
  SDL_memset(screen, color, sizeof(screen));
  return 0;
}

static int f_point(lua_State *L) {
  Uint8 color = (Uint8)luaL_checkinteger(L, 1);
  int x0 = (int)luaL_checknumber(L, 2);
  int y0 = (int)luaL_checknumber(L, 3);
  _pixel(color, x0, y0);
  return 0;
}

static int f_fill(lua_State *L) {
  Uint8 color = (Uint8)luaL_checkinteger(L, 1);
  int x0 = (int)luaL_checknumber(L, 2);
  int y0 = (int)luaL_checknumber(L, 3);
  int x1 = (int)luaL_checknumber(L, 4);
  int y1 = (int)luaL_checknumber(L, 5);
  int x, y;
  if (x0 > x1) swap(int, x0, x1);
  if (y0 > y1) swap(int, y0, y1);
  for (y = y0; y <= y1; ++y) {
    for (x = x0; x <= x1; ++x) {
      _pixel(color, x, y);
    }
  }
  return 0;
}

static int f_rect(lua_State *L) {
  Uint8 color = (Uint8)luaL_checkinteger(L, 1);
  int x0 = (int)luaL_checknumber(L, 2);
  int y0 = (int)luaL_checknumber(L, 3);
  int x1 = (int)luaL_checknumber(L, 4);
  int y1 = (int)luaL_checknumber(L, 5);
  int x, y;
  if (x0 > x1) swap(int, x0, x1);
  if (y0 > y1) swap(int, y0, y1);
  for (x = x0; x <= x1; ++x) {
    _pixel(color, x, y0);
    _pixel(color, x, y1);
  }
  for (y = y0; y <= y1; ++y) {
    _pixel(color, x0, y);
    _pixel(color, x1, y);
  }
  return 0;
}

static int f_line(lua_State *L) {
  Uint8 color = (Uint8)luaL_checkinteger(L, 1);
  int x0 = (int)luaL_checknumber(L, 2);
  int y0 = (int)luaL_checknumber(L, 3);
  int x1 = (int)luaL_checknumber(L, 4);
  int y1 = (int)luaL_checknumber(L, 5);
  int dx = SDL_abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = SDL_abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = (dx > dy ? dx : -dy) / 2, e2;
  for (;;) {
    _pixel(color, x0, y0);
    if (x0 == x1 && y0 == y1) break;
    e2 = err;
    if (e2 > -dx) { err -= dy; x0 += sx; }
    if (e2 < dy) { err += dx; y0 += sy; }
  }
  return 0;
}

static int f_circle(lua_State *L) {
  Uint8 color = (Uint8)luaL_checkinteger(L, 1);
  int x0 = (int)luaL_checknumber(L, 2);
  int y0 = (int)luaL_checknumber(L, 3);
  int radius = (int)luaL_checknumber(L, 4);
  int fill = lua_toboolean(L, 5);
  int r0sq = fill ? 0 : (radius - 1) * (radius - 1);
  int r1sq = radius * radius;
  int x, y, dx, dy, dist;
  for (y = y0 - radius; y <= y0 + radius; ++y) {
    dy = y0 - y; dy *= dy;
    for (x = x0 - radius; x <= x0 + radius; ++x) {
      dx = x0 - x; dx *= dx;
      dist = dx + dy;
      if ((dist >= r0sq) && (dist < r1sq)) _pixel(color, x, y);
    }
  }
  return 0;
}

static int f_translate(lua_State *L) {
  if (lua_gettop(L) == 2) {
    int x = (int)luaL_checknumber(L, 1);
    int y = (int)luaL_checknumber(L, 2);
    translation.x = x;
    translation.y = y;
  }
  lua_pushinteger(L, translation.x);
  lua_pushinteger(L, translation.y);
  return 2;
}



////////////////////////////////////////////////////////////////////////////////
//
//  Highlevel drawing routines
//
////////////////////////////////////////////////////////////////////////////////

static int f_sprite(lua_State *L) {
  size_t length;
  Uint8 color;
  int x, y, w, h;
  int x0 = (int)luaL_checknumber(L, 1);
  int y0 = (int)luaL_checknumber(L, 2);
  const char *data = luaL_checklstring(L, 3, &length);
  int transparent = (int)luaL_optinteger(L, 4, -1);
  switch (length) {
  case 64: w = h = 8; break;
  case 256: w = h = 16; break;
  case 1024: w = h = 32; break;
  case 384: w = 16; h = 24; break;
  default: return luaL_argerror(L, 3, "invalid sprite data length");
  }
  for (y = y0; y < y0 + h; ++y) {
    for (x = x0; x < x0 + w; ++x) {
      color = sprite_color_map[(*data++) & 127];
      if (color != transparent) _pixel(color, x, y);
    }
  }
  return 0;
}

static int f_print(lua_State *L) {
  int x, y;
  Uint8 glyph;
  Uint8 color = (Uint8)luaL_checkinteger(L, 1);
  int x0 = (int)luaL_checknumber(L, 2);
  int y0 = (int)luaL_checknumber(L, 3);
  const char *str = luaL_checkstring(L, 4);
  for (; *str; ++str, x0 += 8) {
    for (y = 0; y < 8; ++y) {
      glyph = font8x8[*str & 127][y];
      for (x = 0; x < 8; ++x) {
        if (glyph & (1 << x)) _pixel(color, x0 + x, y0 + y);
      }
    }
  }
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
//
//  Audio Commands
//
////////////////////////////////////////////////////////////////////////////////

static void mml_reset_channel(AudioChannel *channel);

static int f_play(lua_State *L) {
  AudioChannel *channel;
  int i = (int)luaL_checkinteger(L, 1);
  const char *str = luaL_checkstring(L, 2);
  luaL_argcheck(L, i >= 0 && i < PX_AUDIO_CHANNELS, 1, "invalid channel");
  if (audio_device) {
    SDL_LockAudioDevice(audio_device);
    channel = &channels[i];
    mml_reset_channel(channel);
    channel->source = channel->in = SDL_strdup(str);
    channel->looping = lua_toboolean(L, 3);
    SDL_UnlockAudioDevice(audio_device);
  }
  return 0;
}

static int f_stop(lua_State *L) {
  int i = (int)luaL_checkinteger(L, 1);
  luaL_argcheck(L, i >= 0 && i < PX_AUDIO_CHANNELS, 1, "invalid channel");
  if (audio_device) {
    SDL_LockAudioDevice(audio_device);
    mml_reset_channel(&channels[i]);
    SDL_UnlockAudioDevice(audio_device);
  }
  return 0;
}

static int f_pause(lua_State *L) {
  int pause = lua_toboolean(L, 1);
  if (audio_device) SDL_PauseAudioDevice(audio_device, pause);
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
//
//  Input routines
//
////////////////////////////////////////////////////////////////////////////////

static int _check_button(lua_State *L) {
  static const char *options[] = { "A", "B", "X", "Y", "LEFT", "RIGHT", "UP", "DOWN", "START", NULL };
  return 1 << luaL_checkoption(L, 1, NULL, options);
}

static int _check_controller(lua_State *L) {
  int controller = (int)luaL_optinteger(L, 2, 0);
  luaL_argcheck(L, controller >= 0 && controller < PX_NUM_CONTROLLERS, 2, "invalid controller");
  return controller;
}

static int f_btn(lua_State *L) {
  lua_pushboolean(L, inputs[_check_controller(L)].down & _check_button(L));
  return 1;
}

static int f_btnp(lua_State *L) {
  lua_pushboolean(L, inputs[_check_controller(L)].pressed & _check_button(L));
  return 1;
}

static int f_mouse(lua_State *L) {
  Input *input = &inputs[_check_controller(L)];
  lua_pushinteger(L, input->mouse.x + translation.x);
  lua_pushinteger(L, input->mouse.y + translation.y);
  return 2;
}



////////////////////////////////////////////////////////////////////////////////
//
//  Misc routines
//
////////////////////////////////////////////////////////////////////////////////

static int f_clipboard(lua_State *L) {
  if (lua_gettop(L) > 0) {
    const char *text = luaL_checkstring(L, 1);
    if (SDL_SetClipboardText(text)) luaL_error(L, "SDL_SetClipboardText() failed: %s", SDL_GetError());
    lua_pushstring(L, text);
    return 1;
  }
  else {
    lua_pushstring(L, SDL_GetClipboardText());
    return 1;
  }
}

static int f_randomseed(lua_State *L) {
  if (lua_gettop(L) > 0) {
    Uint32 x = (Uint32)luaL_checkinteger(L, 1);
    px_randomseed(x);
  }
  lua_pushinteger(L, (lua_Integer)seed);
  return 1;
}

static int f_random(lua_State *L) {
  lua_Integer low, up;
  double r = (double)(px_rand() % 100000) / 100000.0;
  switch (lua_gettop(L)) {
  case 0: lua_pushnumber(L, (lua_Number)r); return 1;
  case 1: low = 1; up = luaL_checkinteger(L, 1); break;
  case 2: low = luaL_checkinteger(L, 1); up = luaL_checkinteger(L, 2); break;
  default: return luaL_error(L, "wrong number of arguments");
  }
  luaL_argcheck(L, low <= up, 1, "interval is empty");
  luaL_argcheck(L, low >= 0 || up <= LUA_MAXINTEGER + low, 1, "interval too large");
  r *= (double)(up - low) + 1.0;
  lua_pushinteger(L, (lua_Integer)r + low);
  return 1;
}

static int f_quit(lua_State *L) {
  (void)L;
  running = SDL_FALSE;
  return 0;
}

static int f_title(lua_State *L) {
  const char *title = luaL_checkstring(L, 1);
  SDL_SetWindowTitle(window, title);
  return 0;
}

static int f_time(lua_State *L) {
  Uint32 ticks = SDL_GetTicks();
  lua_pushnumber(L, (lua_Number)ticks / 1000.0);
  return 1;
}

static void px_create_texture(lua_State *L, int width, int height);

static int f_resolution(lua_State *L) {
  int width = (int)luaL_checkinteger(L, 1);
  int height = (int)luaL_checkinteger(L, 2);
  luaL_argcheck(L, width > 0 && width < PX_SCREEN_MAX_WIDTH, 1, "invalid width value");
  luaL_argcheck(L, height > 0 && height < PX_SCREEN_MAX_HEIGHT, 2, "invalid height value");
  px_create_texture(L, width, height);
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
//
//  Compression routines
//
////////////////////////////////////////////////////////////////////////////////

static int f_compress(lua_State *L) {
  size_t source_size;
  luaL_Buffer buffer;
  const char *source = luaL_checklstring(L, 1, &source_size);
  int dest_size = LZ4_compressBound((int)source_size);
  char *dest = luaL_buffinitsize(L, &buffer, dest_size);
  dest_size = LZ4_compress_default(source, dest, (int)source_size, dest_size);
  if (!dest_size) luaL_error(L, "compression failed");
  luaL_pushresultsize(&buffer, dest_size);
  return 1;
}

static int f_decompress(lua_State *L) {
  size_t source_size;
  luaL_Buffer buffer;
  const char *source = luaL_checklstring(L, 1, &source_size);
  int dest_size = (int)luaL_optinteger(L, 2, 64 * 1024);
  char *dest = luaL_buffinitsize(L, &buffer, dest_size);
  dest_size = LZ4_decompress_safe(source, dest, (int)source_size, dest_size);
  if (!dest_size) luaL_error(L, "decompression failed");
  luaL_pushresultsize(&buffer, dest_size);
  return 1;
}


////////////////////////////////////////////////////////////////////////////////
//
//  Networking routines
//
////////////////////////////////////////////////////////////////////////////////

static void net_create_socket(lua_State *L);

static int f_bind(lua_State *L) {
  struct sockaddr_in sin;
  Uint16 port = (Uint16)luaL_checkinteger(L, 1);
  // prepare addr
  SDL_zero(sin);
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  // bind socket
  net_create_socket(L);
  if (bind(socket_fd, (struct sockaddr*)&sin, sizeof(sin))) luaL_error(L, "Cannot bind on port %d", port);
  return 0;
}

static int f_unbind(lua_State *L) {
  (void)L;
  if (socket_fd != INVALID_SOCKET) {
    closesocket(socket_fd);
    socket_fd = INVALID_SOCKET;
  }
  return 0;
}

static int f_recv(lua_State *L) {
  struct sockaddr_in sin;
  int datalen = 1024 * 4;
  socklen_t sinlen;
  luaL_Buffer buffer;
  char *data;
  struct timeval tv;
  fd_set set;
  // check socket for data
  net_create_socket(L);
  FD_ZERO(&set); FD_SET(socket_fd, &set);
  tv.tv_sec = 0; tv.tv_usec = 0;
  if (select(socket_fd + 1, &set, NULL, NULL, &tv) <= 0) return 0;
  if (!FD_ISSET(socket_fd, &set)) return 0;
  // reset addr
  SDL_zero(sin);
  sin.sin_family = AF_INET;
  sin.sin_port = 0;
  sin.sin_addr.s_addr = INADDR_ANY;
  sinlen = sizeof(sin);
  // receive
  data = luaL_buffinitsize(L, &buffer, datalen);
  datalen = recvfrom(socket_fd, data, datalen, 0, (struct sockaddr*)&sin, &sinlen);
  if (datalen < 0) return 0;
  luaL_pushresultsize(&buffer, datalen);
  lua_pushinteger(L, ntohl(sin.sin_addr.s_addr));
  lua_pushinteger(L, ntohs(sin.sin_port));
  return 3;
}

static int f_send(lua_State *L) {
  struct sockaddr_in sin;
  size_t datalen;
  int sent;
  const char *data = luaL_checklstring(L, 1, &datalen);
  Uint32 host = (Uint32)luaL_checkinteger(L, 2);
  Uint16 port = (Uint16)luaL_checkinteger(L, 3);
  // prepare addr
  SDL_zero(sin);
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = htonl(host);
  // sned
  net_create_socket(L);
  sent = sendto(socket_fd, data, (int)datalen, 0, (struct sockaddr*)&sin, sizeof(sin));
  lua_pushboolean(L, sent == (int)datalen);
  return 1;
}

static int f_resolve(lua_State *L) {
  struct addrinfo hints, *result;
  const char *hostname = luaL_checkstring(L, 1);
  SDL_zero(hints);
  hints.ai_family = AF_INET;
  if (getaddrinfo(hostname, NULL, &hints, &result) == 0) {
    if (result) {
      lua_pushinteger(L, ntohl(((struct sockaddr_in*)(result->ai_addr))->sin_addr.s_addr));
      freeaddrinfo(result);
      return 1;
    }
  }
  lua_pushnil(L);
  lua_pushfstring(L, "cannot resolve " LUA_QS, hostname);
  return 2;
}



////////////////////////////////////////////////////////////////////////////////
//
//  Lua Api Mapping
//
////////////////////////////////////////////////////////////////////////////////

static const luaL_Reg px_functions[] = {
  // video primitives
  {"clear", f_clear},
  {"point", f_point},
  {"fill", f_fill},
  {"rect", f_rect},
  {"line", f_line},
  {"circle", f_circle},
  {"translate", f_translate},
  // highlevel video
  {"sprite", f_sprite},
  {"print", f_print},
  // audio calls
  {"play", f_play},
  {"stop", f_stop},
  {"pause", f_pause},
  // input functions
  {"btn", f_btn},
  {"btnp", f_btnp},
  {"mouse", f_mouse},
  // misc functions
  {"clipboard", f_clipboard},
  {"randomseed", f_randomseed},
  {"random", f_random},
  {"quit", f_quit},
  {"title", f_title},
  {"time", f_time},
  {"resolution", f_resolution},
  // compression stuff
  {"compress", f_compress},
  {"decompress", f_decompress},
  // network
  {"bind", f_bind},
  {"unbind", f_unbind},
  {"recv", f_recv},
  {"send", f_send},
  {"resolve", f_resolve},
  {NULL, NULL}
};

static int px_lua_open(lua_State *L) {
  luaL_newlib(L, px_functions);
  lua_pushstring(L, PX_AUTHOR); lua_setfield(L, -2, "_author");
  lua_pushinteger(L, PX_VERSION); lua_setfield(L, -2, "_version");
  return 1;
}



////////////////////////////////////////////////////////////////////////////////
//
//  Audio Mixing (MML parsing)
//
////////////////////////////////////////////////////////////////////////////////

static void mml_reset_channel(AudioChannel *channel) {
  if (channel->source) SDL_free((void*)channel->source);
  SDL_zerop(channel);
  channel->tempo = 140;
  channel->octave = 3;
  channel->waveform = PX_WAVEFORM_PULSE_50;
  channel->default_length = 4;
}

static void mml_skip_spaces(AudioChannel *channel) {
  while (SDL_isspace(*channel->in)) ++channel->in;
}

static int mml_is_next(AudioChannel *channel, char ch) {
  mml_skip_spaces(channel);
  if (*channel->in == ch) { channel->in++; return 1; }
  return 0;
}

static int mml_parse_number(AudioChannel *channel) {
  int value = 0;
  mml_skip_spaces(channel);
  while (SDL_isdigit(*channel->in)) {
    value *= 10;
    value += *channel->in++ - '0';
  }
  return value;
}

static void mml_parse_duration(AudioChannel *channel) {
  float duration = (float)mml_parse_number(channel);
  if (duration == 0.0f) duration = (float)channel->default_length;
  duration = mixing_frequency / ((float)channel->tempo * 0.25f / 60.0f) * (1.0f / duration);
  if (mml_is_next(channel, '.')) duration *= 1.5f;
  channel->duration = (int)duration;
  if (mml_is_next(channel, '&')) channel->silence = 0;
  else channel->silence = (int)(duration * (1.0f / 8.0f));
}

static void mml_parse_note(AudioChannel *channel, int key) {
  if (mml_is_next(channel, '#')) ++key;
  else if (mml_is_next(channel, '+')) ++key;
  else if (mml_is_next(channel, '-')) --key;
  key += (channel->octave - 1) * 12;
  channel->frequency = (float)SDL_pow(2.0, ((double)key - 49.0) / 12.0) * 440.0f;
  channel->t = 0.0f;
  mml_parse_duration(channel);
}

static void mml_parse_next(AudioChannel *channel) {
  for (;;) {
    if (!channel->in) return;
    mml_skip_spaces(channel);
    switch (*channel->in++) {
    case 0:
      if (channel->looping) channel->in = channel->source;
      else mml_reset_channel(channel);
      break;
    case 'T': case 't':
      channel->tempo = mml_parse_number(channel);
      break;
    case 'L': case 'l':
      channel->default_length = mml_parse_number(channel);
      break;
    case 'O': case 'o':
      channel->octave = mml_parse_number(channel);
      break;
    case '<':
      --channel->octave;
      break;
    case '>':
      ++channel->octave;
      break;
    case 'R': case 'r': case 'P': case 'p':
      mml_parse_duration(channel);
      channel->silence = channel->duration;
      return;
    case 'C': case 'c':
      mml_parse_note(channel, 4);
      return;
    case 'D': case 'd':
      mml_parse_note(channel, 6);
      return;
    case 'E': case 'e':
      mml_parse_note(channel, 8);
      return;
    case 'F': case 'f':
      mml_parse_note(channel, 9);
      return;
    case 'G': case 'g':
      mml_parse_note(channel, 11);
      return;
    case 'A': case 'a':
      mml_parse_note(channel, 13);
      return;
    case 'B': case 'b':
      mml_parse_note(channel, 15);
      return;
    case 'W': case 'w':
      switch (*channel->in++) {
      case '1': channel->waveform = PX_WAVEFORM_PULSE_12; break;
      case '2': channel->waveform = PX_WAVEFORM_PULSE_25; break;
      case '5': channel->waveform = PX_WAVEFORM_PULSE_50; break;
      case 'T': case 't': channel->waveform = PX_WAVEFORM_TRIANLGE; break;
      case 'S': case 's': channel->waveform = PX_WAVEFORM_SAWTOOTH; break;
      case 'N': case 'n': channel->waveform = PX_WAVEFORM_NOISE; break;
      }
      break;
    }
  }
}

static void px_audio_mixer_callback(void *userdata, Uint8 *stream, int len) {
  Sint8 *out, value;
  AudioChannel *channel;
  float v;
  int i, j, waveform;

  (void)userdata;
  for (i = 0, out = (Sint8*)stream; i < len; ++i) {
    value = 0;
    for (j = 0; j < PX_AUDIO_CHANNELS; ++j) {
      channel = &channels[j];
      if (--channel->duration < 0) mml_parse_next(channel);
      if (channel->duration > channel->silence) waveform = channel->waveform;
      else waveform = PX_WAVEFORM_SILENCE;
      channel->t += 1.0f / mixing_frequency * channel->frequency;
      channel->t -= (int)channel->t;
      switch (waveform) {
      case PX_WAVEFORM_PULSE_12: value += channel->t <= 0.125f ? 4 : -4; break;
      case PX_WAVEFORM_PULSE_25: value += channel->t <= 0.25f ? 4 : -4; break;
      case PX_WAVEFORM_PULSE_50: value += channel->t <= 0.5f ? 4 : -4; break;
      case PX_WAVEFORM_SAWTOOTH: value += (Sint8)((-1.0f + channel->t * 2.0f) * 4.0f); break;
      case PX_WAVEFORM_TRIANLGE:
        if (channel->t < 0.25f) v = channel->t * 4.0f;
        else if (channel->t < 0.75f) v = 1.0f - ((channel->t - 0.25f) * 4.0f);
        else v = -1.0f + ((channel->t) - 0.75f) * 4.0f;
        value += (Sint8)(v * 8.0f);
        break;
      case PX_WAVEFORM_NOISE: value += audio_noise[(int)(channel->t * (float)PX_AUDIO_NOISE)]; break;
      }
    }
    *out++ = value;
  }
}



////////////////////////////////////////////////////////////////////////////////
//
//  Network helper routines
//
////////////////////////////////////////////////////////////////////////////////

static void net_initialize(lua_State *L) {
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa)) luaL_error(L, "Windows sockets could not start");
#else
  (void)L;
#endif // _WIN32
  socket_fd = INVALID_SOCKET;
}

static void net_shutdown() {
  if (socket_fd != INVALID_SOCKET) closesocket(socket_fd);
#ifdef _WIN32
  WSACleanup();
#endif // _WIN32
}

static void net_create_socket(lua_State *L) {
  if (socket_fd == INVALID_SOCKET) {
    socket_fd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd == INVALID_SOCKET) luaL_error(L, "Cannot create UDP socket");
  }
}



////////////////////////////////////////////////////////////////////////////////
//
//  Main Loop and Events
//
////////////////////////////////////////////////////////////////////////////////

static void px_open_controllers(lua_State *L) {
  int i;
  SDL_GameController *controller;
  for (i = 0; i < SDL_NumJoysticks(); ++i) {
    if (SDL_IsGameController(i)) {
      controller = SDL_GameControllerOpen(i);
      if (!controller) luaL_error(L, "SDL_GameControllerOpen() failed: %s", SDL_GetError());
    }
  }
}

static void px_set_button(int player, int button, int down) {
  if (player >= 0 && player < PX_NUM_CONTROLLERS) {
    button = 1 << button;
    if (down) {
      inputs[player].down |= button;
      inputs[player].pressed |= button;
    }
    else {
      inputs[player].down &= ~button;
    }
  }
}

static void px_handle_keys(const SDL_Event *ev) {
  int button;
  // handle special keys
  if (ev->type == SDL_KEYDOWN) {
    switch (ev->key.keysym.sym) {
    case SDLK_ESCAPE:
      running = SDL_FALSE;
      return;
    case SDLK_F12:
      fullscreen = !fullscreen;
      SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
      if (!fullscreen) SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
      return;
    }
  }
  // handle normal keys
  switch (ev->key.keysym.sym) {
  case SDLK_UP: button = PX_BUTTON_UP; break;
  case SDLK_DOWN: button = PX_BUTTON_DOWN; break;
  case SDLK_LEFT: button = PX_BUTTON_LEFT; break;
  case SDLK_RIGHT: button = PX_BUTTON_RIGHT; break;
  case SDLK_y: case SDLK_z: button = PX_BUTTON_A; break;
  case SDLK_x: button = PX_BUTTON_B; break;
  case SDLK_a: button = PX_BUTTON_X; break;
  case SDLK_s: button = PX_BUTTON_Y; break;
  case SDLK_SPACE: case SDLK_RETURN: button = PX_BUTTON_START; break;
  default: return;
  }
  px_set_button(0, button, ev->type == SDL_KEYDOWN);
}

static void px_handle_mouse(const SDL_Event *ev) {
  int button;
  switch (ev->button.button) {
  case SDL_BUTTON_LEFT: button = PX_BUTTON_A; break;
  case SDL_BUTTON_RIGHT: button = PX_BUTTON_B; break;
  default: return;
  }
  px_set_button(0, button, ev->type == SDL_MOUSEBUTTONDOWN);
}

static void px_handle_controller(const SDL_Event *ev) {
  int button;
  switch (ev->cbutton.button) {
  case SDL_CONTROLLER_BUTTON_A: button = PX_BUTTON_A; break;
  case SDL_CONTROLLER_BUTTON_B: button = PX_BUTTON_B; break;
  case SDL_CONTROLLER_BUTTON_X: button = PX_BUTTON_X; break;
  case SDL_CONTROLLER_BUTTON_Y: button = PX_BUTTON_Y; break;
  case SDL_CONTROLLER_BUTTON_START: button = PX_BUTTON_START; break;
  case SDL_CONTROLLER_BUTTON_DPAD_UP: button = PX_BUTTON_UP; break;
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN: button = PX_BUTTON_DOWN; break;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT: button = PX_BUTTON_LEFT; break;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: button = PX_BUTTON_RIGHT; break;
  default: return;
  }
  px_set_button(ev->cbutton.which, button, ev->type == SDL_CONTROLLERBUTTONDOWN);
}

static void px_render_screen(lua_State *L) {
  const SDL_Color *color;
  Uint8 *pixels, *p;
  int x, y, pitch;

  // check if we really have a texture
  if (texture == NULL) {
    if (SDL_SetRenderDrawColor(renderer, 64, 16, 16, 255)) luaL_error(L, "SDL_SetRenderDrawColor() failed: %s", SDL_GetError());
    if (SDL_RenderClear(renderer)) luaL_error(L, "SDL_RenderClear() failed: %s", SDL_GetError());
    return;
  }

  // update texture
  if (SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch)) luaL_error(L, "SDL_LockTexture() failed: %s", SDL_GetError());
  for (y = 0; y < screen_height; ++y) {
    p = pixels + pitch * y;
    for (x = 0; x < screen_width; ++x) {
      color = &colors[screen[x][y] & 15];
      *p++ = 255; *p++ = color->b; *p++ = color->g; *p++ = color->r;
    }
  }
  SDL_UnlockTexture(texture);

  // render everything
  if (SDL_SetRenderDrawColor(renderer, 16, 16, 16, 255)) luaL_error(L, "SDL_SetRenderDrawColor() failed: %s", SDL_GetError());
  if (SDL_RenderClear(renderer)) luaL_error(L, "SDL_RenderClear() failed: %s", SDL_GetError());
  if (SDL_RenderCopy(renderer, texture, NULL, NULL)) luaL_error(L, "SDL_RenderCopy() failed: %s", SDL_GetError());
  SDL_RenderPresent(renderer);
}

static void px_run_main_loop(lua_State *L) {
  int i;
  SDL_Event ev;
  Uint32 last_tick, current_tick, delta_ticks;

  // init callback
  if (lua_getglobal(L, "init") == LUA_TFUNCTION) lua_call(L, 0, 0);
  else lua_pop(L, 1);

  // loop
  last_tick = SDL_GetTicks(); delta_ticks = 0;
  while (running) {
    // fetch events
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
      case SDL_QUIT: running = SDL_FALSE; break;
      case SDL_KEYDOWN: case SDL_KEYUP: px_handle_keys(&ev); break;
      case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: px_handle_mouse(&ev); break;
      case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP: px_handle_controller(&ev); break;
      case SDL_CONTROLLERDEVICEADDED: case SDL_CONTROLLERDEVICEREMOVED: px_open_controllers(L); break;
      case SDL_MOUSEMOTION: inputs[0].mouse.x = ev.motion.x; inputs[0].mouse.y = ev.motion.y; break;
      }
    }
    // update callback
    current_tick = SDL_GetTicks();
    delta_ticks += current_tick - last_tick;
    last_tick = current_tick;
    for (; delta_ticks >= PX_FPS_TICKS; delta_ticks -= PX_FPS_TICKS) {
      // do update call
      if (lua_getglobal(L, "update") == LUA_TFUNCTION) lua_call(L, 0, 0);
      else lua_pop(L, 1);
      // reset input
      for (i = 0; i < PX_NUM_CONTROLLERS; ++i) inputs[i].pressed = 0;
    }
    // render stuff
    px_render_screen(L);
  }
}



////////////////////////////////////////////////////////////////////////////////
//
//  Init & Shutdown
//
////////////////////////////////////////////////////////////////////////////////

static void px_create_texture(lua_State *L, int width, int height) {
  SDL_DisplayMode display_mode;

  // create new texture
  if (texture) SDL_DestroyTexture(texture);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!texture) luaL_error(L, "SDL_CreateTexture() failed: %s", SDL_GetError());
  if (SDL_RenderSetLogicalSize(renderer, width, height)) luaL_error(L, "SDL_RenderSetLogicalSize() failed: %s", SDL_GetError());
  screen_width = width; screen_height = height;

  // determine the best window size and center it
  if (SDL_GetDesktopDisplayMode(0, &display_mode)) luaL_error(L, "SDL_GetDesktopDisplayMode() failed: %s", SDL_GetError());
  width = (display_mode.w - PX_WINDOW_PADDING) / screen_width;
  height = (display_mode.h - PX_WINDOW_PADDING) / screen_height;
  if (width < height) {
    height = screen_height * width;
    width = screen_width * width;
  } 
  else {
    width = screen_width * height;
    height = screen_height * height;
  }
  SDL_SetWindowSize(window, width, height);
  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

static int px_lua_init(lua_State *L) {
  SDL_AudioSpec want, have;
  int i, flags;
  const char *str;

  // setup some hints
  str = px_check_arg("-video");
  if (str) SDL_SetHint(SDL_HINT_RENDER_DRIVER, str);

  // check for fullscreen
  if (px_check_parm("-window")) { fullscreen = SDL_FALSE; flags = SDL_WINDOW_RESIZABLE; }
  else { fullscreen = SDL_TRUE; flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP; }

  // init network
  net_initialize(L);

  // init SDL
  if (SDL_Init(SDL_INIT_EVERYTHING)) luaL_error(L, "SDL_Init() failed: %s", SDL_GetError());

  // create window + texture
  window = SDL_CreateWindow(PX_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, PX_SCREEN_WIDTH, PX_SCREEN_HEIGHT, flags);
  if (!window) luaL_error(L, "SDL_CreateWindow() failed: %s", SDL_GetError());
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) luaL_error(L, "SDL_CreateRenderer() failed: %s", SDL_GetError());
  px_create_texture(L, PX_SCREEN_WIDTH, PX_SCREEN_HEIGHT);
  SDL_ShowCursor(0);

  // audio init
  if (!px_check_parm("-nosound")) {
    SDL_zero(want); SDL_zero(have);
    want.callback = px_audio_mixer_callback;
    want.channels = 1;
    want.format = AUDIO_S8;
    want.freq = PX_AUDIO_FREQUENCY;
    want.samples = 1024 * 4;
    str = px_check_arg("-audio");
    audio_device = SDL_OpenAudioDevice(str, SDL_FALSE, &want, &have, 0);
    if (have.format != AUDIO_S8) luaL_error(L, "SDL_OpenAudioDevice() didn't provide AUDIO_S8 format");
    if (have.channels != 1) luaL_error(L, "SDL_OpenAudioDevice() didn't provide a mono channel");
    if (!audio_device) luaL_error(L, "SDL_OpenAudioDevice() failed: %s", SDL_GetError());
    mixing_frequency = (float)have.freq;
    SDL_PauseAudioDevice(audio_device, SDL_FALSE);
  }

  // init some stuff
  px_randomseed(4096); for (i = 0; i < PX_AUDIO_NOISE; ++i) audio_noise[i] = px_rand() % 8 - 4;
  for (i = 0; i < PX_AUDIO_CHANNELS; ++i) SDL_zerop(&channels[i]);
  running = SDL_TRUE;
  SDL_zero(inputs); SDL_zero(translation);
  px_open_controllers(L);
  px_randomseed(47 * 1024); // reset prng

  // load the Lua script
  str = px_check_arg("-file");
  if (luaL_loadfile(L, str ? str : "game.lua")) lua_error(L);
  lua_call(L, 0, 0);

  // run main loop
  px_run_main_loop(L);
  return 0;
}

static void px_shutdown() {
  if (audio_device) SDL_CloseAudioDevice(audio_device);
  if (texture) SDL_DestroyTexture(texture);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
  SDL_Quit();
  net_shutdown();
}

static void px_register_args(lua_State *L, int argc, char **argv) {
  int i;
  lua_newtable(L);
  for (i = 0; i < argc; ++i) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i);
  }
  lua_setglobal(L, "arg");
}

int main(int argc, char **argv) {
  lua_State *L = luaL_newstate();
  margc = argc; margv = argv;
  px_register_args(L, argc, argv);
  luaL_openlibs(L);
  luaL_requiref(L, "pixl", px_lua_open, 1);
  lua_getglobal(L, "debug"); lua_getfield(L, -1, "traceback"); lua_remove(L, -2);
  lua_pushcfunction(L, px_lua_init);
  if (lua_pcall(L, 0, 0, -2) != LUA_OK) {
    const char *message = luaL_gsub(L, lua_tostring(L, -1), "\t", "  ");
    if (audio_device) SDL_PauseAudioDevice(audio_device, SDL_TRUE);
    #ifndef _WIN32
    fprintf(stderr, "=[ PiXL Panic ]=\n%s\n", message);
    #endif // _WIN32
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "PiXL Panic", message, window);
  }
  lua_close(L);
  px_shutdown();
  return 0;
}
