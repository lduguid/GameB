#pragma once


#define GAME_NAME "Game_B"

#define GAME_RES_WIDTH  384

#define GAME_RES_HEIGHT 240

#define GAME_BPP         32

#define GAME_DRAWING_AREA_MEMORY_SIZE (GAME_RES_WIDTH * GAME_RES_HEIGHT * (GAME_BPP / 8))


typedef struct GAMEBITMAP
{
  BITMAPINFO BitmapInfo;
  
  uint32_t pad;  // NOTE: explicit 4-bytes padding or else compiler does it which generates a warning

  void* Memory;
} GAMEBITMAP;

typedef struct PIXEL32
{
  uint8_t Blue;

  uint8_t Green;

  uint8_t Red;

  uint8_t Alpha;

} PIXEL32;


LRESULT CALLBACK MainWindowProc(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);

static DWORD DisplayErrorWithCode(char* ErrorString);

static DWORD CreateMainGameWindow(void);

static BOOL GameIsAlreadyRunning(void);

static void ProcessPlayerInput(void);

static void RenderFrameGraphics(void);
