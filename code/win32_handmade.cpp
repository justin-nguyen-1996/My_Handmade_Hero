
#include <windows.h>
#include <stdint.h>

// globals
static bool Running;
static BITMAPINFO BitmapInfo;
static void* BitmapMemory;

static void
RenderWeirdGradient(int Width, int Height, int BytesPerPixel, int xOffset, int yOffset) 
{
	uint8_t* row = (uint8_t*) BitmapMemory;
	for (int y = 0; y < Height; ++y) {
		
		uint32_t* pixel = (uint32_t*) row;
		for (int x = 0; x < Width; ++x) {
			//   Memory:   BB GG RR XX
			//   Register: XX RR GG BB
			uint8_t blue = (x + xOffset);
			uint8_t green = (y + yOffset);
			uint8_t red;
			*pixel = ((green << 8) | blue);
			pixel += 1;
		}

		row += Width * BytesPerPixel;
	}
}

// Create the buffer that we will have Windows display for us
// Device Independent Bitmap
static void
ResizeDibSection(int Width, int Height)
{
	// Free our old DIB section if we ask for a new one
	if (BitmapMemory) { VirtualFree(BitmapMemory, 0, MEM_RELEASE); }
	
	// setup the bit map info header
	BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
	BitmapInfo.bmiHeader.biWidth = Width;
	BitmapInfo.bmiHeader.biHeight = -Height;
	BitmapInfo.bmiHeader.biPlanes = 1;
	BitmapInfo.bmiHeader.biBitCount = 32;
	BitmapInfo.bmiHeader.biCompression = BI_RGB;

	// Allocate memory for our DIB section
	int BytesPerPixel = 4;
	int BitmapMemorySize = (Width * Height) * (BytesPerPixel);
	BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

// Have Windows output our buffer and scale it as appropriate
static void
DisplayDib(HDC DeviceContext, RECT* WinRect, int X, int Y, int W, int H)
{
	StretchDIBits(DeviceContext,
		   		  /*X, Y, W, H,
				  X, Y, W, H,*/
				  0, 0, WinRect->right - WinRect->left, WinRect->bottom - WinRect->top,
				  0, 0, WinRect->right - WinRect->left, WinRect->bottom - WinRect->top,
				  BitmapMemory,
				  &BitmapInfo,
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
		case WM_SIZE:        { RECT ClientRect;
							   GetClientRect(hwnd, &ClientRect);
							   ResizeDibSection(ClientRect.right - ClientRect.left, // resize our DIB section
									  			ClientRect.bottom - ClientRect.top);
							   break;
							 }
		case WM_CLOSE:         Running = false; break;
		case WM_DESTROY:       Running = false; break;
		case WM_ACTIVATEAPP:   OutputDebugStringA("WM_ACTIVATEAPP\n"); break;

		case WM_PAINT:       { PAINTSTRUCT Paint;
							   HDC DeviceContext = BeginPaint(hwnd, &Paint);
							   int X = Paint.rcPaint.left;
							   int Y = Paint.rcPaint.top;
							   int W = Paint.rcPaint.right - Paint.rcPaint.left;
							   int H = Paint.rcPaint.bottom - Paint.rcPaint.top;
							   RECT ClientRect;
							   GetClientRect(hwnd, &ClientRect);
							   DisplayDib(DeviceContext, &ClientRect, X, Y, W, H); // have Windows output our buffer
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
	WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
	WindowClass.lpfnWndProc = WindowProc;
	WindowClass.hInstance = hInstance;
// 	WindowClass.hIcon = ;
	WindowClass.lpszClassName = "Handmade Hero Window Class";

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
				int tempW = 4096; int tempH = 4096; HDC DeviceContext = GetDC(WindowHandle); RECT ClientRect; GetClientRect(WindowHandle, &ClientRect);
				int w = ClientRect.right - ClientRect.left; int h = ClientRect.bottom - ClientRect.top;
				RenderWeirdGradient(w, h, 4, xOffset, yOffset);
				DisplayDib(DeviceContext, &ClientRect, 0, 0, w, h); // have Windows output our buffer
				ReleaseDC(WindowHandle, DeviceContext);
				++xOffset;
			}
		}
	}

	return 0;
}

