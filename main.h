#pragma once

#pragma warning(push, 3)
// '_WIN32_WINNT_WIN10_TH2' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif
#pragma warning(disable: 4668)  
#include <windows.h>
#pragma warning(pop)

// Process Status API, e.g. GetProcessMemoryInfo
#include <psapi.h>

// Windows Multimedia library, we use it for timeBeginPeriod 
// to adjust the global system timer resolution.
#pragma comment(lib, "Winmm.lib")


// Imported from Ntdll.dll, this is for using the undocumented Windows API function NtQueryTimerResolution.
typedef LONG(NTAPI* _NtQueryTimerResolution) (OUT PULONG MinimumResolution, OUT PULONG MaximumResolution, OUT PULONG CurrentResolution);
 // test change

#define GAME_NAME "Game_B"

#define GAME_RES_WIDTH  384

#define GAME_RES_HEIGHT 240

#define GAME_BPP         32

// 8 bits for red, 8 bits for green, 8 bits for blue, and 8 bits for alpha. 
// 32 bits per pixel == 4 bytes per pixel.

#define GAME_DRAWING_AREA_MEMORY_SIZE (GAME_RES_WIDTH * GAME_RES_HEIGHT * (GAME_BPP / 8))   

// Every 120 frames/2 seconds we will calculate some statistics such as average FPS, CPU usage, etc.

#define CALCULATE_STATS_EVERY_X_FRAMES		120

// 16.67 milliseconds is 60 frames per second.

#define TARGET_MICROSECONDS_PER_FRAME		  16667ULL

#define TARGET_NANOSECONDS_PER_FRAME      (TARGET_MICROSECONDS_PER_FRAME * 1000)


#pragma warning(push)
#pragma warning(disable: 4820)  // 4-byte padding added automatically by the compiler but give warning.
typedef struct GAMEBITMAP
{
  BITMAPINFO BitmapInfo;  // 44 bytes
  
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

  float RawFPSAverage;

  float CookedFPSAverage;

  uint32_t MinimumTimerResolution;

  uint32_t MaximumTimerResolution;

  uint32_t CurrentTimerResolution;

  MONITORINFO MonitorInfo;  // TODO init. somewhere ! = { sizeof(MONITORINFO) };

  int32_t MonitorWidth;

  int32_t MonitorHeight;

  BOOL DisplayDebugInfo;

} GAMEPERFDATA;
#pragma warning(pop)


LRESULT CALLBACK MainWindowProc(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);

static DWORD DisplayErrorWithCode(char* ErrorString);

static DWORD CreateMainGameWindow(void);

static BOOL GameIsAlreadyRunning(void);

static void ProcessPlayerInput(void);

static void RenderFrameGraphics(int32_t ScreenX, int32_t ScreenY);
