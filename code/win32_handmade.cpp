
#include <windows.h>

LRESULT CALLBACK 
WindowProc( HWND   hwnd,
			UINT   Msg,
			WPARAM wParam,
			LPARAM lParam)
{
	LRESULT res = 0;

	switch (Msg) {
		case WM_SIZE:          OutputDebugStringA("WM_SIZE\n"); break;
		case WM_CLOSE:         OutputDebugStringA("WM_CLOSE\n"); break;
		case WM_DESTROY:       OutputDebugStringA("WM_DESTROY\n"); break; 
		case WM_ACTIVATEAPP:   OutputDebugStringA("WM_ACTIVATEAPP\n"); break;
							   
		case WM_PAINT:       { PAINTSTRUCT Paint;
							   HDC DeviceContext = BeginPaint(hwnd, &Paint); 

							   static DWORD Operation = WHITENESS;

							   PatBlt(DeviceContext, 
									  Paint.rcPaint.left, 
									  Paint.rcPaint.top, 
									  Paint.rcPaint.right - Paint.rcPaint.left,
									  Paint.rcPaint.bottom - Paint.rcPaint.top,
	 							      WHITENESS);

							   if (Operation == WHITENESS) { Operation = BLACKNESS; }
							   else { Operation = WHITENESS; }

							   EndPaint(hwnd, &Paint); 
							   break;
							 }
							   
		default:               // OutputDebugStringA("DEFAULT\n");
							   res = DefWindowProc(hwnd, Msg, wParam, lParam); break;
	}

	return res;
}

int CALLBACK 
WinMain(HINSTANCE hInstance,
		HINSTANCE hPrevInstance,
		LPSTR     lpCmdLine,
		int       nCmdShow)
{
	OutputDebugStringA("hi");
	WNDCLASS WindowClass = {};
	WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
	WindowClass.lpfnWndProc = WindowProc;
	WindowClass.hInstance = hInstance;
// 	WindowClass.hIcon = ;
	WindowClass.lpszClassName = "Handmade Hero Window Class";
	
	if (RegisterClass(&WindowClass)) {
		HWND WindowHandle = CreateWindowEx( 0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
											CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
		if (WindowHandle) {
			for (;;) {
				MSG Message;
				BOOL MessageResult = GetMessage(&Message, 0, 0, 0);
				if (MessageResult > 0) {
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				} else { 
					break; 
				}
			}
		}
	}
	
	return 0;
}

