#ifdef _WIN32
#pragma warning (disable : 4710 4711)  // silly release build code specified inlining conflicts with c++17 automatic inlining
#endif

#include <stdio.h>

#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN 
#pragma warning(push, 3)
#include <windows.h>
#pragma warning(pop)

#include <stdint.h>

#include "main.h"


// TODO, do we need all these globals?

HWND gGameWindow;

BOOL gGameIsRunning;

GAMEBITMAP gBackBuffer;

MONITORINFO gMonitorInfo = { sizeof(MONITORINFO) };

int32_t gMonitorWidth;

int32_t gMonitorHeight;


int WINAPI WinMain(_In_ HINSTANCE Instance, _In_opt_ HINSTANCE PrevInstance, _In_ LPSTR CmdLine, _In_ int CmdShow)
{
  UNREFERENCED_PARAMETER(Instance);

  UNREFERENCED_PARAMETER(PrevInstance);

  UNREFERENCED_PARAMETER(CmdLine);
  
  UNREFERENCED_PARAMETER(CmdShow);

  DWORD Result = ERROR_SUCCESS;

  if (GameIsAlreadyRunning())
  {
    MessageBoxA(NULL, "Another instance of this application is already running!", "Error!", MB_ICONEXCLAMATION | MB_OK);

    goto Exit;
  }

  if ((Result = CreateMainGameWindow()) != ERROR_SUCCESS)
  {
    goto Exit;
  }


  gBackBuffer.BitmapInfo.bmiHeader.biSize = sizeof(gBackBuffer);

  gBackBuffer.BitmapInfo.bmiHeader.biWidth = GAME_RES_WIDTH;

  gBackBuffer.BitmapInfo.bmiHeader.biHeight = GAME_RES_HEIGHT;

  gBackBuffer.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;

  gBackBuffer.BitmapInfo.bmiHeader.biCompression = BI_RGB;

  gBackBuffer.BitmapInfo.bmiHeader.biPlanes = 1;

  gBackBuffer.Memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  
  if (gBackBuffer.Memory == NULL)
  {
    MessageBoxA(NULL, "Failed to allocate memory for the drawing surface!", "Error!", MB_ICONEXCLAMATION | MB_OK);

    goto Exit;
  }

  memset(gBackBuffer.Memory, 0x7F, GAME_DRAWING_AREA_MEMORY_SIZE);

  MSG Message = { 0 };

  gGameIsRunning = TRUE;

  while (TRUE == gGameIsRunning)
  {
    while (PeekMessageA(&Message, gGameWindow, 0, 0, PM_REMOVE))
    {
      DispatchMessageA(&Message);
    }

    ProcessPlayerInput();

    RenderFrameGraphics();

    Sleep(1);  // TODO do i need this?
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

  if (!GetMonitorInfoA(MonitorFromWindow(gGameWindow, MONITOR_DEFAULTTOPRIMARY), &gMonitorInfo))
  {
    Result = ERROR_MONITOR_NO_DESCRIPTOR;
    
    goto Exit;
  }

  gMonitorWidth = gMonitorInfo.rcMonitor.right - gMonitorInfo.rcMonitor.left;

  gMonitorHeight = gMonitorInfo.rcMonitor.bottom - gMonitorInfo.rcMonitor.top;

  if (!SetWindowLongPtrA(gGameWindow, GWL_STYLE, (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_OVERLAPPEDWINDOW))
  {
    Result = GetLastError();

    goto Exit;
  }

  if (!SetWindowPos(gGameWindow, HWND_TOP /*HWND_TOPMOST*/,
    gMonitorInfo.rcMonitor.left, gMonitorInfo.rcMonitor.top, 
    gMonitorWidth, gMonitorHeight, SWP_FRAMECHANGED))
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
    0, 0, gMonitorWidth, gMonitorHeight, 
    0, 0, GAME_RES_WIDTH, GAME_RES_HEIGHT, 
    gBackBuffer.Memory, &gBackBuffer.BitmapInfo, 
    DIB_RGB_COLORS, SRCCOPY);

  ReleaseDC(gGameWindow, DeviceContext);
}
#pragma warning(pop)
