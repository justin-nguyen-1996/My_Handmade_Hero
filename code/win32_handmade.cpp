
#include <windows.h>

int CALLBACK WinMain( HINSTANCE hInstance,
					  HINSTANCE hPrevInstance,
					  LPSTR     lpCmdLine,
					  int       nCmdShow)
{
	MessageBox(0, "This is Handmade Hero", "Hero",
			   MB_OK | MB_ICONWARNING);
	return 0;
}

