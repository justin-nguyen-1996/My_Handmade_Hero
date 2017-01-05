
#include <windows.h>
#include <stdint.h>

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
		HWND WindowHandle = CreateWindowEx( 0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
											CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
		if (WindowHandle) {
			// Message Loop
			// Message queue --> pull out one at a time --> translate & dispatch --> parse in Window Callback procedure
			Running = true; MSG Message; int xOffset = 0; int yOffset = 0;
			while (Running) {

				while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
					if (Message.message == WM_QUIT) { Running = false; }
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}
				
				// TODO: some temp stuff for displaying our gradient
			    HDC DeviceContext = GetDC(WindowHandle); 
				win32_WinDim Dimension = GetWinDim(WindowHandle);
				RenderWeirdGradient(GlobalBackBuffer, xOffset, yOffset);
				DisplayDib(DeviceContext, Dimension.Width, Dimension.Height, // have Windows output our buffer
						   GlobalBackBuffer, 0, 0, 
						   Dimension.Width, Dimension.Height); 
				ReleaseDC(WindowHandle, DeviceContext);
				++xOffset; ++yOffset;
			}
		}
	}

	return 0;
}

