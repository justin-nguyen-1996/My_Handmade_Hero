
#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

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

/*******************************************/

// resolving Xinput DLL ... magic unicorns

// 1) (X_INPUT_GET_STATE) is for our own use. Calling this fxn macro with param X will evaluate into a function with name X and params specified by get/set state.
// 2) (x_input_get_state) is now a type. A type that is a function of type (DWORD WINAPI) with params as specified by get/set state.
// 3) (XInputGetStateStub) is a function stub that will initialize our function pointer right off the bat.
// 4) (XInputGetState_) is now a function pointer to XInputGetStateStub. It is statically scoped to this file. This statement sets our function pointer to null.
// 5) Can still call function by its real name (XInputGetState) but now use our function pointer (XInputGetState_) instead.

// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex,  XINPUT_STATE* pState)            // 1)
typedef X_INPUT_GET_STATE(x_input_get_state);                                                          // 2)
X_INPUT_GET_STATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }                           // 3)
static x_input_get_state* XInputGetState_ = XInputGetStateStub;                                        // 4)
#define XInputGetState XInputGetState_                                                                 // 5)

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)     // 1)
typedef X_INPUT_SET_STATE(x_input_set_state);                                                          // 2)
X_INPUT_SET_STATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }                           // 3)
static x_input_set_state* XInputSetState_ = XInputSetStateStub;                                        // 4)
#define XInputSetState XInputSetState_                                                                 // 5)

/*******************************************/

/*******************************************/

// resolving DirectSound DLL ... magic unicorns
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

/*******************************************/
// globals
static bool Running;
static win32_Buffer GlobalBackBuffer;
static LPDIRECTSOUNDBUFFER SecondaryBuffer;

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
RenderWeirdGradient(win32_Buffer* buffer, int xOffset, int yOffset)
{
	int Width = buffer->Width;
	int Height = buffer->Height;
	int BytesPerPixel = buffer->BytesPerPixel;

	uint8_t* row = (uint8_t*) buffer->BitmapMemory;
	for (int y = 0; y < buffer->Height; ++y) {

		uint32_t* pixel = (uint32_t*) row;
		for (int x = 0; x < buffer->Width; ++x) {
			//   Memory:   BB GG RR XX
			//   Register: XX RR GG BB
			uint8_t blue = (x + xOffset);
			uint8_t green = (y + yOffset);
			uint8_t red;
			*pixel = ((green << 8) | blue);
			pixel += 1;
		}

		row += buffer->Pitch;
	}
}

// Manually load the dll (helps with compatibility)
static void
LoadXInput() {
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll"); // load the .dll into our virtual address space, look for import libraries
	if (! XInputLibrary) { HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll"); }
	if (XInputLibrary) {
		XInputGetState = (x_input_get_state*) GetProcAddress(XInputLibrary, "XInputGetState"); // find the desired functions in the specified library
		XInputSetState = (x_input_set_state*) GetProcAddress(XInputLibrary, "XInputSetState"); // Windows normally uses these addresses to patch up you code
																							   // by filling in memory addresses with pointers
	}
}

// Direct Sound (DSound)
static void
InitDirectSound(HWND Window, int32_t BufferSize, int32_t SamplesPerSecond) {
	// Load the library
	HMODULE SoundLibrary = LoadLibraryA("dsound.dll");

	if (SoundLibrary) {

		// Get a Direct Sound object
		direct_sound_create* DirectSoundCreate = (direct_sound_create*) GetProcAddress(SoundLibrary, "DirectSoundCreate");
		LPDIRECTSOUND DirectSound;

		if (DirectSoundCreate  &&  SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {

			// Set the buffers' format
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = WaveFormat.nChannels * WaveFormat.wBitsPerSample / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nBlockAlign * WaveFormat.nSamplesPerSec;
			WaveFormat.cbSize = 0;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {

				// Create a primary buffer --> used to get a handle to the actual sound card ... Windows is weird
				// 						   --> sets the mode of the sound card so we can play the format of sound we want
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0))) {
					
					if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
						
					}
				}
			}

			// Create a secondary buffer --> our actual sound buffer we will be writing from
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			
			if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &SecondaryBuffer, 0))) {
				OutputDebugStringA("hi");
			}
		}
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
	buffer->BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

// Have Windows output our buffer and scale it as appropriate
static void
DisplayDib(win32_Buffer* buffer,
	   	   HDC DeviceContext,
		   int WinWidth, int WinHeight)
{
	StretchDIBits(DeviceContext,
				  0, 0, WinWidth, WinHeight,
				  0, 0, buffer->Width, buffer->Height,
				  buffer->BitmapMemory,
				  &buffer->BitmapInfo,
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

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:       { uint32_t VKCode = wParam; // virtual key code
							   bool WasDown = ((lParam & (1 << 30)) != 0);
							   bool IsDown = ((lParam & (1 << 31)) == 0);
							   if (WasDown != IsDown) {
							       if (VKCode == 'W') {  }
							       else if (VKCode == 'A') { }
							       else if (VKCode == 'S') { }
							       else if (VKCode == 'D') { }
							       else if (VKCode == 'Q') { }
							       else if (VKCode == 'E') { }
							       else if (VKCode == VK_UP) { }
							       else if (VKCode == VK_LEFT) { }
							       else if (VKCode == VK_DOWN) { }
							       else if (VKCode == VK_RIGHT) { }
							       else if (VKCode == VK_ESCAPE) { }
							       else if (VKCode == VK_SPACE) { } }
							   bool AltKeyDown = lParam & (1 << 29);
							   if ((VKCode == VK_F4)  &&  AltKeyDown) { Running = false; }
							 } break;

		case WM_PAINT:       { PAINTSTRUCT Paint;
							   HDC DeviceContext = BeginPaint(hwnd, &Paint);
							   int X = Paint.rcPaint.left;
							   int Y = Paint.rcPaint.top;
							   int W = Paint.rcPaint.right - Paint.rcPaint.left;
							   int H = Paint.rcPaint.bottom - Paint.rcPaint.top;
							   win32_WinDim Dimension = GetWinDim(hwnd);
							   DisplayDib(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height); // display our buffer
							   EndPaint(hwnd, &Paint);
							 } break;

		default:               // OutputDebugStringA("DEFAULT\n");
							   res = DefWindowProc(hwnd, Msg, wParam, lParam); break;
	}

	return res;
}

// WinMain
int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Load XInput .dll
	LoadXInput();

	// Set up WindowClass
	WNDCLASSA WindowClass = {};
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
			int xOffset = 0; int yOffset = 0; // TODO: temp variables
			HDC DeviceContext = GetDC(WindowHandle);

			// TODO: temp vars for direct sound test
			int samplesPerSecond = 48000;
			int toneHertz = 256;
			int runningSampleIndex = 0;
			int bytesPerSample = sizeof(int16_t) * 2;
			int secondaryBufferSize = samplesPerSecond * bytesPerSample;
			int squareWavePeriod = samplesPerSecond / toneHertz;
			int halfSquareWavePeriod = squareWavePeriod / 2;

			// Initialize direct sound
			InitDirectSound(WindowHandle, 48000, 48000 * sizeof(int16_t) * 2); // samples per second = 48000
			SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

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
				for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex) {
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
					} else { // controller not plugged in
// 						TODO: handle this case
					}
				}

				// TODO: some temp stuff for displaying our gradient
				win32_WinDim Dimension = GetWinDim(WindowHandle);
				RenderWeirdGradient(&GlobalBackBuffer, xOffset, yOffset);
				DisplayDib(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
				++xOffset; ++yOffset;

				// Direct Sound output test
				DWORD playCursor; DWORD writeCursor;
				if (SUCCEEDED(SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))) { // get positions of play and write cursors
					
					DWORD bytesToWrite;
					DWORD byteToLock = (runningSampleIndex * bytesPerSample) % secondaryBufferSize; // convert samples to bytes and wrap
					
					// Get the number of bytes to write into the sound buffer
					if (byteToLock > playCursor) { bytesToWrite = (secondaryBufferSize - byteToLock) + (playCursor); }
					else                         { bytesToWrite = (playCursor - byteToLock); }
					
					// Need two regions because the region up to the play cursor could be in two chunks (we're using a circular buffer)
					VOID* region1; DWORD region1Size;
					VOID* region2; DWORD region2Size;
					
					if (SUCCEEDED(SecondaryBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
					
						int region1SampleCount = region1Size / bytesPerSample;
						int16_t* sampleOut = (int16_t*) region1;
						
						// Loop through the first region
						for (DWORD SampleIndex = 0; SampleIndex < region1SampleCount; ++SampleIndex) {
							int16_t sampleValue = ((runningSampleIndex / halfSquareWavePeriod) % 2) ? 16000 : -16000;
							*sampleOut = sampleValue; sampleOut += 1; // write out the left value
							*sampleOut = sampleValue; sampleOut += 1; // write out the right value
							runningSampleIndex += 1;
						}
						
						sampleOut = (int16_t*) region2;
						int region2SampleCount = region2Size / bytesPerSample;
						
						// Loop through the second region
						for (DWORD SampleIndex = 0; SampleIndex < region2SampleCount; ++SampleIndex) {
							int16_t sampleValue = ((runningSampleIndex / halfSquareWavePeriod) % 2) ? 16000 : -16000;
							*sampleOut = sampleValue; sampleOut += 1; // write out the left value
							*sampleOut = sampleValue; sampleOut += 1; // write out the right value
							runningSampleIndex += 1;
						}
					}
				}
			}
		}
	}

	return 0;
}

