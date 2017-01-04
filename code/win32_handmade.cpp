
#include <windows.h>

// globals
static bool Running;
static BITMAPINFO BitmapInfo;
static void* BitmapMemory;
static HBITMAP BitmapHandle;
static HDC BitmapDeviceContext;

// Create the buffer that we will have Windows display for us
// Device Independent Bitmap
static void
ResizeDibSection(int Width, int Height)
{
	// TODO: check memory freeing (either free first or after)
 
	// Free old DIB section before creating a new one, Else create the device context
	if (BitmapHandle)          { DeleteObject(BitmapHandle); }
	if (! BitmapDeviceContext) { BitmapDeviceContext = CreateCompatibleDC(0); }
 
	// setup the bit map info header
	BITMAPINFOHEADER bmiHeader = BitmapInfo.bmiHeader;
	bmiHeader.biSize = sizeof(bmiHeader);
	bmiHeader.biWidth = Width;
	bmiHeader.biHeight = Height;
	bmiHeader.biPlanes = 1;
	bmiHeader.biBitCount = 32;
	bmiHeader.biCompression = BI_RGB;

	BitmapHandle = CreateDIBSection(
			BitmapDeviceContext, &BitmapInfo,
			DIB_RGB_COLORS,
		   	&BitmapMemory,
			0, 0);
}

// Have Windows output our buffer and scale it as appropriate
static void
Win32_UpdateWindow(HDC DeviceContext, int X, int Y, int W, int H)
{
	StretchDIBits(DeviceContext,
		   		  X, Y, W, H,
				  X, Y, W, H,
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
							   Win32_UpdateWindow(DeviceContext, X, Y, W, H); // have Windows output our buffer
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
			Running = true;
			while (Running) {
				MSG Message;
				BOOL MessageResult = GetMessageA(&Message, 0, 0, 0);
				if (MessageResult > 0) {
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				} else {
					break;
				}
			}
		}
	}

	return 0;
}

