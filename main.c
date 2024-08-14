#ifdef _WIN32
#pragma warning (disable : 4710 4711)  // silly release build code specified inlining conflicts with c++17 automatic inlining
#endif

#include <stdio.h>

#include <stdlib.h>

//#define WIN32_LEAN_AND_MEAN 
//#pragma warning(push, 3)
//#include <windows.h>
//#pragma warning(pop)

#include <stdint.h>

#include "main.h"

#include <time.h>


// TODO, do we need all these globals here?

_NtQueryTimerResolution NtQueryTimerResolution = NULL;


HWND gGameWindow;

BOOL gGameIsRunning;

GAMEBITMAP gBackBuffer;

GAMEPERFDATA gPerformanceData;


int WINAPI WinMain(_In_ HINSTANCE Instance, _In_opt_ HINSTANCE PrevInstance, _In_ LPSTR CmdLine, _In_ int CmdShow)
{
  UNREFERENCED_PARAMETER(Instance);

  UNREFERENCED_PARAMETER(PrevInstance);

  UNREFERENCED_PARAMETER(CmdLine);
  
  UNREFERENCED_PARAMETER(CmdShow);

  DWORD Result = ERROR_SUCCESS;

  // The time in microseconds that each frame starts

  int64_t FrameStart = 0;
  
  // The time in microseconds that each frame ends. 
  // This variable is reused to measure both "raw" and "cooked" frame times.
  
  int64_t FrameEnd = 0;
  
  // The elapsed time in microseconds that each frame ends. 
  // This variable is reused to measure both "raw" and "cooked" frame times.

  int64_t ElapsedMicroseconds = 0;

  // We accumulate the amount of time taken to render a "raw" frame and then
  // calculate the average every x frames, i.e., we divide it by x number of frames rendered.

  int64_t ElapsedMicrosecondsAccumulatorRaw = 0;

  // We accumulate the amount of time taken to render a "cooked" frame and then
  // calculate the average every x frames, i.e., we divide it by x number of frames rendered.
  // This should hopefully be a steady 60fps. A "cooked" frame is a frame after we have 
  // waited/slept for a calculated amount of time before starting the next frame.

  int64_t ElapsedMicrosecondsAccumulatorCooked = 0;


  if (GameIsAlreadyRunning())
  {
    MessageBoxA(NULL, "Another instance of this application is already running!", "Error!", MB_ICONEXCLAMATION | MB_OK);

    goto Exit;
  }

  // We need the undocumented Windows API function NtQueryTimerResolution to get the resolution of the global system timer.
  // A higher resolution timer will show a lower number, because if your clock can tick every e.g. 0.5ms, that is a higher 
  // resolution than a timer that can only tick every 1.0ms.
  
  // VS2022 seems to require a strip of type (FARPROC) with a cast to (void*) first, then desired function type, 
  // in this case (_NtQueryTimerResolution)
  
  if ((NtQueryTimerResolution = (_NtQueryTimerResolution)(void*) GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryTimerResolution")) == NULL)
  {
    MessageBoxA(NULL, "Couldn't find the NtQueryTimerResolution function in ntdll.dll!", "Error!", MB_ICONERROR | MB_OK);

    goto Exit;
  }

  NtQueryTimerResolution(
    (PULONG) &gPerformanceData.MinimumTimerResolution, 
    (PULONG) &gPerformanceData.MaximumTimerResolution, 
    (PULONG) &gPerformanceData.CurrentTimerResolution);

  SYSTEM_INFO SystemInfo;

  GetSystemInfo(&SystemInfo);

  // Initialize the variable that is used to calculate average CPU usage of this process.

  int64_t PreviousSystemTime = 0;

  GetSystemTimeAsFileTime((FILETIME*)&PreviousSystemTime);

  // The timeBeginPeriod function controls a global system-wide timer. So
  // increasing the clock resolution here will affect all processes running on this
  // entire system. It will increase context switching, the rate at which timers
  // fire across the entire system, etc. Due to this, Microsoft generally discourages 
  // the use of timeBeginPeriod completely. However, we need it, because without 
  // 1ms timer resolution, we simply cannot maintain a reliable 60 frames per second.
  // Also, we don't need to worry about calling timeEndPeriod, because Windows will
  // automatically cancel our requested timer resolution once this process exits.

  if (timeBeginPeriod(1) == TIMERR_NOCANDO)
  {
    MessageBoxA(NULL, "Failed to set global timer resolution!", "Error!", MB_ICONERROR | MB_OK);

    goto Exit;
  }

  // Increase process and thread priority to minimize the chances of another thread on the system
  // preempting us when we need to run and causing a stutter in our frame rate. (Though it can still happen.)
  // Windows is not a real-time OS and you cannot guarantee timings or deadlines, but you can get close.  
  
  if (SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS) == 0)
  {
    MessageBoxA(NULL, "Failed to set process priority!", "Error!", MB_ICONERROR | MB_OK);

    goto Exit;
  }

  if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0)
  {
    MessageBoxA(NULL, "Failed to set thread priority!", "Error!", MB_ICONERROR | MB_OK);

    goto Exit;
  }

  if ((Result = CreateMainGameWindow()) != ERROR_SUCCESS)
  {
    goto Exit;
  }

  int64_t PerfFrequency = 0;

  QueryPerformanceFrequency((LARGE_INTEGER*) &PerfFrequency);
  
  gBackBuffer.BitmapInfo.bmiHeader.biSize = sizeof(gBackBuffer.BitmapInfo.bmiHeader);

  gBackBuffer.BitmapInfo.bmiHeader.biWidth = GAME_RES_WIDTH;

  gBackBuffer.BitmapInfo.bmiHeader.biHeight = GAME_RES_HEIGHT;

  gBackBuffer.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;

  gBackBuffer.BitmapInfo.bmiHeader.biCompression = BI_RGB;

  gBackBuffer.BitmapInfo.bmiHeader.biPlanes = 1;

  // NOTE: Corressponding VirtualFree(...) not required due to this memory being required 
  // for the lifetime of the process.  Once we exit, windows will clean it up automatically.

  gBackBuffer.Memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  
  if (gBackBuffer.Memory == NULL)
  {
    MessageBoxA(NULL, "Failed to allocate memory for the drawing surface!", "Error!", MB_ICONEXCLAMATION | MB_OK);

    goto Exit;
  }

  memset(gBackBuffer.Memory, 0x7F, GAME_DRAWING_AREA_MEMORY_SIZE);

  MSG Message = { 0 };


  // This is the main game loop. Setting gGameIsRunning to FALSE at any point will cause
  // the game to exit immediately. The loop has two important functions: ProcessPlayerInput
  // and RenderFrameGraphics. The loop will execute these two duties as quickly as possible,
  // but will then sleep for a few milliseconds using a precise timing mechanism in order 
  // to achieve a smooth 60 frames per second. We also calculate some performance statistics
  // every 2 seconds or 120 frames.    

  gGameIsRunning = TRUE;

  while (TRUE == gGameIsRunning)
  {
    QueryPerformanceCounter((LARGE_INTEGER*) &FrameStart);

    while (PeekMessageA(&Message, gGameWindow, 0, 0, PM_REMOVE))
    {
      DispatchMessageA(&Message);
    }

    ProcessPlayerInput();

    RenderFrameGraphics();

    QueryPerformanceCounter((LARGE_INTEGER*) &FrameEnd);

    ElapsedMicroseconds = FrameEnd - FrameStart;

    ElapsedMicroseconds *= 1000000;

    ElapsedMicroseconds /= PerfFrequency;

    gPerformanceData.TotalFramesRendered++;

    ElapsedMicrosecondsAccumulatorRaw += ElapsedMicroseconds;

    while (ElapsedMicroseconds < TARGET_MICROSECONDS_PER_FRAME)
    {
      ElapsedMicroseconds = FrameEnd - FrameStart;

      ElapsedMicroseconds *= 1000000;

      ElapsedMicroseconds /= PerfFrequency;

      QueryPerformanceCounter((LARGE_INTEGER*) &FrameEnd);

      // If we are less than 75% of the way through the current frame, then rest.
      // Sleep(1) is only anywhere near 1 millisecond if we have previously set the global
      // system timer resolution to 1ms or below using timeBeginPeriod.

      if (ElapsedMicroseconds < (TARGET_MICROSECONDS_PER_FRAME * 0.75f))
      {
        Sleep(1);
      }
    }

    ElapsedMicrosecondsAccumulatorCooked += ElapsedMicroseconds;

    if ((gPerformanceData.TotalFramesRendered % CALCULATE_STATS_EVERY_X_FRAMES) == 0) 
    {
      //int64_t AverageMicroSecondsPerFrameRaw = ElapsedMicrosecondsAccumulatorRaw / CALCULATE_STATS_EVERY_X_FRAMES;

      //int64_t AverageMicroSecondsPerFrameCooked = ElapsedMicrosecondsAccumulatorCooked / CALCULATE_STATS_EVERY_X_FRAMES;

      gPerformanceData.RawFPSAverage = 1.0f / (((float)ElapsedMicrosecondsAccumulatorRaw / CALCULATE_STATS_EVERY_X_FRAMES) * 0.000001f);

      gPerformanceData.CookedFPSAverage = 1.0f / (((float)ElapsedMicrosecondsAccumulatorCooked / CALCULATE_STATS_EVERY_X_FRAMES) * 0.000001f);

      char str[256] = { 0 };

      _snprintf_s(str, _countof(str), _TRUNCATE,
        "Avg FPS Cooked: %.01f\tAvg FPS Raw: %.01f\n",
        gPerformanceData.CookedFPSAverage, gPerformanceData.RawFPSAverage);
      
      OutputDebugStringA(str);

      ElapsedMicrosecondsAccumulatorRaw = 0;

      ElapsedMicrosecondsAccumulatorCooked = 0;
    }
  }

Exit: 
  return Result;
}


LRESULT CALLBACK MainWindowProc(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam)    
{
  LRESULT Result = 0;

  switch (Message)
  {

    case WM_CLOSE:
      gGameIsRunning = FALSE;

      DestroyWindow(WindowHandle);

      break;
  
    case WM_QUIT:
      gGameIsRunning = FALSE;

      break;

    case WM_DESTROY:
    
      PostQuitMessage(0);

      break;

    default:

      Result = DefWindowProcA(WindowHandle, Message, WParam, LParam);
  }

  return Result;
}


static DWORD DisplayErrorWithCode(char* ErrorString)
{

  DWORD ErrorCode = GetLastError();

  char ErrorMsg[512] = { 0 };

  sprintf_s(ErrorMsg, 512, "%s. The reported error code is %lu", ErrorString, ErrorCode);

  MessageBoxA(NULL, ErrorMsg, "Error", MB_ICONEXCLAMATION | MB_OK);

  return ErrorCode;
}


static DWORD CreateMainGameWindow(void)
{
  DWORD Result = ERROR_SUCCESS;

  WNDCLASSA WindowClass = { 0 };  // TODO Ex WIN32 structures and functions? 

  WindowClass.style = 0;

  WindowClass.lpfnWndProc = (WNDPROC)MainWindowProc;
  
  WindowClass.cbClsExtra = 0;
  
  WindowClass.cbWndExtra = 0;
  
  WindowClass.hInstance = GetModuleHandleA(NULL);                     // Instance;
  
  WindowClass.hIcon = LoadIconA((HINSTANCE)NULL, IDI_APPLICATION);
  
  WindowClass.hCursor = LoadCursorA((HINSTANCE)NULL, IDC_ARROW);
  
  WindowClass.hbrBackground = CreateSolidBrush(RGB(255, 0, 255)); // GetStockObject(WHITE_BRUSH);
  
  WindowClass.lpszMenuName = NULL;
  
  WindowClass.lpszClassName = GAME_NAME"_WINDOWCLASS";

  // SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);  // NOTE in embedded manifest.
  
  if (!RegisterClassA(&WindowClass))
  {
    Result = DisplayErrorWithCode("Window Registration has failed");
    
    goto Exit;
  }

  if (!(gGameWindow = CreateWindowA(WindowClass.lpszClassName, GAME_NAME, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, (HWND)NULL, (HMENU)NULL, GetModuleHandleA(NULL) /*Instance*/, (LPVOID)NULL))) 
  {
    Result = DisplayErrorWithCode("Main Window Creation has failed");
    
    goto Exit;
  }

  gPerformanceData.MonitorInfo.cbSize = sizeof(MONITORINFO);

  if (!GetMonitorInfoA(MonitorFromWindow(gGameWindow, MONITOR_DEFAULTTOPRIMARY), &gPerformanceData.MonitorInfo))
  {
    Result = ERROR_MONITOR_NO_DESCRIPTOR;
    
    goto Exit;
  }

  gPerformanceData.MonitorWidth = gPerformanceData.MonitorInfo.rcMonitor.right - gPerformanceData.MonitorInfo.rcMonitor.left;

  gPerformanceData.MonitorHeight = gPerformanceData.MonitorInfo.rcMonitor.bottom - gPerformanceData.MonitorInfo.rcMonitor.top;

  if (!SetWindowLongPtrA(gGameWindow, GWL_STYLE, (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_OVERLAPPEDWINDOW))
  {
    Result = GetLastError();

    goto Exit;
  }

  if (!SetWindowPos(gGameWindow, HWND_TOP /*HWND_TOPMOST*/,
    gPerformanceData.MonitorInfo.rcMonitor.left, gPerformanceData.MonitorInfo.rcMonitor.top,
    gPerformanceData.MonitorWidth, gPerformanceData.MonitorHeight, SWP_FRAMECHANGED))
  {
    Result = GetLastError();

    goto Exit;
  }

Exit:
  return Result;
}


static BOOL GameIsAlreadyRunning(void)
{
  BOOL Result = FALSE;

  CreateMutexA(NULL, FALSE, "_Mutex");

  if (GetLastError() == ERROR_ALREADY_EXISTS)
  {
    Result = TRUE;
  }

  return Result;
}


static void ProcessPlayerInput(void)
{
  int16_t EscapeKeyIsDown = GetAsyncKeyState(VK_ESCAPE);

  if (EscapeKeyIsDown)
  {
    SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
  }
}

#pragma warning(push)
#pragma warning( disable : 5045)  // QSpectre crap.
static void RenderFrameGraphics(void)
{

  //memset(gBackBuffer.Memory, 0xff, GAME_RES_WIDTH);  // 1 pixel bottom left corner.

  PIXEL32 Pixel = { 0 };

  Pixel.Blue = 0xff;
  
  Pixel.Green = 0;
  
  Pixel.Red = 0;
  
  Pixel.Alpha = 0xff;

  for (int i = 0; i < GAME_RES_WIDTH * 5; i++)
  {
    Pixel.Blue = 0xff;

    // NOTE: No compiler warning about memcpy_s Vs. memcpy. general consensus is that
    // memcpy_s no safer then memcpy.

    memcpy((PIXEL32*)(gBackBuffer.Memory) + i, &Pixel, sizeof(PIXEL32));
    
    Pixel.Blue = 0xef;
    
    memcpy((PIXEL32*)(gBackBuffer.Memory) + (GAME_RES_WIDTH * 5 * 1) + i, &Pixel, sizeof(PIXEL32));
    
    Pixel.Blue = 0xdf;
    
    memcpy((PIXEL32*)(gBackBuffer.Memory) + (GAME_RES_WIDTH * 5 * 2) + i, &Pixel, sizeof(PIXEL32));
    
    Pixel.Blue = 0xcf;
    
    memcpy((PIXEL32*)(gBackBuffer.Memory) + (GAME_RES_WIDTH * 5 * 3) + i, &Pixel, sizeof(PIXEL32));
    
    Pixel.Blue = 0xbf;
    
    memcpy((PIXEL32*)(gBackBuffer.Memory) + (GAME_RES_WIDTH * 5 * 4) + i, &Pixel, sizeof(PIXEL32));
    
    Pixel.Blue = 0xaf;
    
    memcpy((PIXEL32*)(gBackBuffer.Memory) + (GAME_RES_WIDTH * 5 * 5) + i, &Pixel, sizeof(PIXEL32));
  }

  HDC DeviceContext = GetDC(gGameWindow);

  StretchDIBits(DeviceContext, 
    0, 0, gPerformanceData.MonitorWidth, gPerformanceData.MonitorHeight,
    0, 0, GAME_RES_WIDTH, GAME_RES_HEIGHT, 
    gBackBuffer.Memory, &gBackBuffer.BitmapInfo, 
    DIB_RGB_COLORS, SRCCOPY);

  ReleaseDC(gGameWindow, DeviceContext);
}
#pragma warning(pop)
