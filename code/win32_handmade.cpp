
#include <windows.h>
#include <stdint.h>
#include <xinput.h>

// structs
struct win32_Buffer {
	BITMAPINFO BitmapInfo;
	void* BitmapMemory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

struct win32_WinDim {
	int Width;
	int Height;
};

/* resolving Xinput DLL ... magic unicorns */

// 1) (X_INPUT_GET_STATE) is for our own use. Calling this fxn macro with param X will evaluate into a function with name X and params specified by get/set state.
// 2) (x_input_get_state) is now a type. A type that is a function of type (DWORD WINAPI) with params as specified by get/set state.
// 3) (XInputGetStateStub) is a function stub that will initialize our function pointer right off the bat.
// 4) (XInputGetState_) is now a function pointer to XInputGetStateStub. It is statically scoped to this file. This statement sets our function pointer to null.
// 5) Can still call function by its real name (XInputGetState) but now use our function pointer (XInputGetState_) instead.
 
// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex,  XINPUT_STATE* pState)            // 1)
typedef X_INPUT_GET_STATE(x_input_get_state);                                                          // 2)
X_INPUT_GET_STATE(XInputGetStateStub) { return 0; }                                                    // 3)
static x_input_get_state* XInputGetState_ = XInputGetStateStub;                                        // 4)
#define XInputGetState XInputGetState_                                                                 // 5)

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)     // 1)
typedef X_INPUT_SET_STATE(x_input_set_state);                                                          // 2)
X_INPUT_SET_STATE(XInputSetStateStub) { return 0; }                                                    // 3)
static x_input_set_state* XInputSetState_ = XInputSetStateStub;                                        // 4)
#define XInputSetState XInputSetState_                                                                 // 5)

/*******************************************/

// globals
static bool Running;
static win32_Buffer GlobalBackBuffer;

// helper functions
static win32_WinDim GetWinDim(HWND window) {
	win32_WinDim res;
   	RECT ClientRect;
    GetClientRect(window, &ClientRect);
	res.Width = ClientRect.right - ClientRect.left;
	res.Height = ClientRect.bottom - ClientRect.top;
	return res;
}

static void
RenderWeirdGradient(win32_Buffer buffer, int xOffset, int yOffset)
{
	int Width = buffer.Width;
	int Height = buffer.Height;
	int BytesPerPixel = buffer.BytesPerPixel;

	uint8_t* row = (uint8_t*) buffer.BitmapMemory;
	for (int y = 0; y < buffer.Height; ++y) {

		uint32_t* pixel = (uint32_t*) row;
		for (int x = 0; x < buffer.Width; ++x) {
			//   Memory:   BB GG RR XX
			//   Register: XX RR GG BB
			uint8_t blue = (x + xOffset);
			uint8_t green = (y + yOffset);
			uint8_t red;
			*pixel = ((green << 8) | blue);
			pixel += 1;
		}

		row += buffer.Pitch;
	}
}

// Create the buffer that we will have Windows display for us
// Device Independent Bitmap
static void
ResizeDibSection(win32_Buffer* buffer, int Width, int Height)
{
	// Free our old DIB section if we ask for a new one
	if (buffer->BitmapMemory) { VirtualFree(buffer->BitmapMemory, 0, MEM_RELEASE); }

	// Setup the buffer
	buffer->Width = Width; buffer->Height = Height;
	buffer->BytesPerPixel = 4;
	buffer->Pitch = buffer->Width * buffer->BytesPerPixel;

	// Setup the bit map info header
	buffer->BitmapInfo.bmiHeader.biSize = sizeof(buffer->BitmapInfo.bmiHeader);
	buffer->BitmapInfo.bmiHeader.biWidth = buffer->Width;
	buffer->BitmapInfo.bmiHeader.biHeight = -1 * buffer->Height;
	buffer->BitmapInfo.bmiHeader.biPlanes = 1;
	buffer->BitmapInfo.bmiHeader.biBitCount = 32;
	buffer->BitmapInfo.bmiHeader.biCompression = BI_RGB;

	// Allocate memory for our DIB section
	int BitmapMemorySize = (buffer->Width * buffer->Height) * (buffer->BytesPerPixel);
	buffer->BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

// Have Windows output our buffer and scale it as appropriate
static void
DisplayDib(HDC DeviceContext, 
		   int WinWidth, int WinHeight,
		   win32_Buffer buffer,
		   int X, int Y, int W, int H)
{
	StretchDIBits(DeviceContext,
		   		  /*X, Y, W, H,
				  X, Y, W, H,*/
				  0, 0, WinWidth, WinHeight,
				  0, 0, buffer.Width, buffer.Height,
				  buffer.BitmapMemory,
				  &buffer.BitmapInfo,
				  DIB_RGB_COLORS,
				  SRCCOPY);
}

// Window Callback
LRESULT CALLBACK
WindowProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	LRESULT res = 0;

	// Parse Message
	switch (Msg) {
		case WM_SIZE:          break;
		case WM_CLOSE:         Running = false; break;
		case WM_DESTROY:       Running = false; break;
		case WM_ACTIVATEAPP:   OutputDebugStringA("WM_ACTIVATEAPP\n"); break;

		case WM_PAINT:       { PAINTSTRUCT Paint;
							   HDC DeviceContext = BeginPaint(hwnd, &Paint);
							   int X = Paint.rcPaint.left;
							   int Y = Paint.rcPaint.top;
							   int W = Paint.rcPaint.right - Paint.rcPaint.left;
							   int H = Paint.rcPaint.bottom - Paint.rcPaint.top;
							   win32_WinDim Dimension = GetWinDim(hwnd);
							   DisplayDib(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer, X, Y, W, H); // display our buffer
							   EndPaint(hwnd, &Paint);
							   break;
							 }

		default:               // OutputDebugStringA("DEFAULT\n");
							   res = DefWindowProc(hwnd, Msg, wParam, lParam); break;
	}

	return res;
}

// WinMain
int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Set up WindowClass
	WNDCLASS WindowClass = {};
	WindowClass.style = CS_VREDRAW | CS_HREDRAW;
	WindowClass.lpfnWndProc = WindowProc;
	WindowClass.hInstance = hInstance;
// 	WindowClass.hIcon = ;
	WindowClass.lpszClassName = "Handmade Hero Window Class";

	// Set the size of our buffer
   	ResizeDibSection(&GlobalBackBuffer, 1280, 720);

	// Register the window and create the window
	if (RegisterClass(&WindowClass)) {
		HWND WindowHandle = CreateWindowExA( 0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
										 	 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
		if (WindowHandle) {
			Running = true; 
			int xOffset = 0; int yOffset = 0; 
			HDC DeviceContext = GetDC(WindowHandle); 
 
			// Message Loop
			while (Running) {

				// Message queue --> pull out one at a time --> translate & dispatch --> parse in Window Callback procedure
				MSG Message; 
				while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
					if (Message.message == WM_QUIT) { Running = false; }
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}

				// Poll for XInput
				for (DWORD ControllerIndex = 0; 
					 ControllerIndex < XUSER_MAX_COUNT; 
					 ++ControllerIndex) 
				{
					XINPUT_STATE ControllerState;
					if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) { // controller plugged in
						XINPUT_GAMEPAD* pad = &ControllerState.Gamepad;
						bool Up =            (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool Down =          (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool Left =          (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool Right =         (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool Start =         (pad->wButtons & XINPUT_GAMEPAD_START);
						bool Back =          (pad->wButtons & XINPUT_GAMEPAD_BACK);
						bool LeftShoulder =  (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						bool RightShoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool AButton =       (pad->wButtons & XINPUT_GAMEPAD_A);
						bool BButton =       (pad->wButtons & XINPUT_GAMEPAD_B);
						bool XButton =       (pad->wButtons & XINPUT_GAMEPAD_X);
						bool YButton =       (pad->wButtons & XINPUT_GAMEPAD_Y);

						int16_t StickX = pad->sThumbLX;
						int16_t StickY = pad->sThumbLY;
					}
				   	else { // controller not plugged in
// 						TODO: handle this case
					}
				}
				
				// TODO: some temp stuff for displaying our gradient
				win32_WinDim Dimension = GetWinDim(WindowHandle);
				RenderWeirdGradient(GlobalBackBuffer, xOffset, yOffset);
				DisplayDib(DeviceContext, Dimension.Width, Dimension.Height, // have Windows output our buffer
						   GlobalBackBuffer, 0, 0, 
						   Dimension.Width, Dimension.Height); 
				++xOffset; ++yOffset;
			}
		}
	}

	return 0;
}

