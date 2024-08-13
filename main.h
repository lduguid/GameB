#pragma once


#define GAME_NAME "Game_B"

#define GAME_RES_WIDTH  384

#define GAME_RES_HEIGHT 240

#define GAME_BPP         32

#define GAME_DRAWING_AREA_MEMORY_SIZE (GAME_RES_WIDTH * GAME_RES_HEIGHT * (GAME_BPP / 8))   // 8 bits for red, 8 bits for green, 8 bits for blue, and 8 bits for alpha. 32 bits per pixel == 4 bytes per pixel.

#define CALCULATE_AVG_FPS_EVERY_X_FRAMES  100


#pragma warning(push)
#pragma warning(disable: 4820)  // see padding NOTE below
typedef struct GAMEBITMAP
{
  BITMAPINFO BitmapInfo;  // 44 bytes
  
  //uint32_t pad;  // NOTE: explicit 4-bytes padding or else compiler does it which generates a warning

  void* Memory;           // 8 bytes (64-bit)
} GAMEBITMAP;             // 52 bytes total


typedef struct PIXEL32
{
  uint8_t Blue;

  uint8_t Green;

  uint8_t Red;

  uint8_t Alpha;

} PIXEL32;

typedef struct GAMEPERFDATA
{
  uint64_t TotalFramesRendered;

  uint32_t RawFramesPerSecondAverage;

  uint64_t CookedFramesPerSecondAverage;

  LARGE_INTEGER PerfFrequency;

  LARGE_INTEGER FrameStart;

  LARGE_INTEGER FrameEnd;

  LARGE_INTEGER ElapsedMicroSecondsPerFrame;

  MONITORINFO MonitorInfo;  // TODO init. somewhere ! = { sizeof(MONITORINFO) };

  int32_t MonitorWidth;

  int32_t MonitorHeight;

} GAMEPERFDATA;
#pragma warning(pop)


LRESULT CALLBACK MainWindowProc(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);

static DWORD DisplayErrorWithCode(char* ErrorString);

static DWORD CreateMainGameWindow(void);

static BOOL GameIsAlreadyRunning(void);

static void ProcessPlayerInput(void);

static void RenderFrameGraphics(void);
