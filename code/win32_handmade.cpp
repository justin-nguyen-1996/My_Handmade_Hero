
// Includes
#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h> // TODO: temp include

// Defines
#define PI 3.14159265359f

// Typedefs
typedef float real32;
typedef double real64;

// Structs
struct win32_Buffer {
	BITMAPINFO BitmapInfo;
	void* BitmapMemory;
	int Width;
	int Height;
	int Pitch;
};

struct win32_WinDim {
	int Width;
	int Height;
};

struct win32_SoundInfo {
	int samplesPerSecond;
	int toneHertz;
	int16_t toneVolume;
	uint32_t runningSampleIndex;
	int bytesPerSample;
	int wavePeriod;
	int secondaryBufferSize;
	real32 tSine;
	int latencySampleCount;
};

// Globals
static bool Running;
static win32_Buffer GlobalBackBuffer;
static LPDIRECTSOUNDBUFFER SecondaryBuffer;
int xOffset = 0; int yOffset = 0; // TODO: temp vars
win32_SoundInfo soundInfo; // TODO: temp var

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

// Helper functions
win32_WinDim Win32_GetWinDim(HWND window) {
	win32_WinDim res;
   	RECT ClientRect;
    GetClientRect(window, &ClientRect);
	res.Width = ClientRect.right - ClientRect.left;
	res.Height = ClientRect.bottom - ClientRect.top;
	return res;
}

// Test code for displaying a gradient
static void
RenderWeirdGradient(win32_Buffer* buffer, int xOffset, int yOffset)
{
	uint8_t* row = (uint8_t*) buffer->BitmapMemory;
	for (int y = 0; y < buffer->Height; ++y) {

		uint32_t* pixel = (uint32_t*) row;
		for (int x = 0; x < buffer->Width; ++x) {
			//   Memory:   BB GG RR XX
			//   Register: XX RR GG BB
			uint8_t blue = (x + xOffset);
			uint8_t green = (y + yOffset);
			uint8_t red;
			*pixel++ = ((green << 8) | blue);
		}

		row += buffer->Pitch;
	}
}

// Manually load the XInput dll (helps with compatibility)
static void
Win32_LoadXInput() {
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll"); // load the .dll into our virtual address space, look for import libraries
	if (! XInputLibrary) { XInputLibrary = LoadLibraryA("xinput9_1_0.dll"); }
	if (! XInputLibrary) { XInputLibrary = LoadLibraryA("xinput1_3.dll"); }
	if (XInputLibrary) {
		XInputGetState = (x_input_get_state*) GetProcAddress(XInputLibrary, "XInputGetState"); // find the desired functions in the specified library
		if (! XInputGetState) { XInputGetState = XInputGetStateStub; }
		XInputSetState = (x_input_set_state*) GetProcAddress(XInputLibrary, "XInputSetState"); // Windows normally uses these addresses to patch up you code
		if (! XInputSetState) { XInputSetState = XInputSetStateStub; }
	}
}

// Initialize Direct Sound (DSound)
static void
Win32_InitDirectSound(HWND Window, int32_t SamplesPerSecond, int32_t BufferSize) {
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
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
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
				
			}
		}
	}
}

// Fill the sound buffer
static void
Win32_FillSoundBuffer(win32_SoundInfo* soundInfo, DWORD byteToLock, DWORD bytesToWrite) {
	// Need two regions because the region up to the play cursor could be in two chunks (we're using a circular buffer)
	VOID* region1; DWORD region1Size;
	VOID* region2; DWORD region2Size;

	// Need to lock the buffer to have DirectSound let us write into the buffer
	if (SUCCEEDED(SecondaryBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {

		int16_t* sampleOut = (int16_t*) region1;
		int region1SampleCount = region1Size / soundInfo->bytesPerSample;

		// Loop through the first region to write to the buffer, stereo sound is encoded as pairs of 16bit values (left, right)
		for (DWORD SampleIndex = 0; SampleIndex < region1SampleCount; ++SampleIndex) {
			real32 sineValue = sinf(soundInfo->tSine);
			int16_t sampleValue = (int16_t) (sineValue * soundInfo->toneVolume);
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			soundInfo->tSine += 2.0f * PI * 1.0f / (real32) soundInfo->wavePeriod;
			++soundInfo->runningSampleIndex;
		}

		sampleOut = (int16_t*) region2;
		int region2SampleCount = region2Size / soundInfo->bytesPerSample;

		// Loop through the second region to write to the buffer, stereo sound is encoded as pairs of 16bit values (left, right)
		for (DWORD SampleIndex = 0; SampleIndex < region2SampleCount; ++SampleIndex) {
			real32 sineValue = sinf(soundInfo->tSine);
			int16_t sampleValue = (int16_t) (sineValue * soundInfo->toneVolume);
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			soundInfo->tSine += 2.0f * PI * 1.0f / (real32) soundInfo->wavePeriod;
			++soundInfo->runningSampleIndex;
		}

		// Unlock the buffer --> done writing so continue playing again
		SecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
	}
}

// Create the buffer that we will have Windows display for us
// Device Independent Bitmap
static void
Win32_ResizeDibSection(win32_Buffer* buffer, int Width, int Height)
{
	// Free our old DIB section if we ask for a new one
	if (buffer->BitmapMemory) { VirtualFree(buffer->BitmapMemory, 0, MEM_RELEASE); }

	// Setup the buffer
	int BytesPerPixel = 4;
	buffer->Width = Width; buffer->Height = Height;
	buffer->Pitch = Width * BytesPerPixel;

	// Setup the bit map info header
	buffer->BitmapInfo.bmiHeader.biSize = sizeof(buffer->BitmapInfo.bmiHeader);
	buffer->BitmapInfo.bmiHeader.biWidth = buffer->Width;
	buffer->BitmapInfo.bmiHeader.biHeight = -1 * buffer->Height;
	buffer->BitmapInfo.bmiHeader.biPlanes = 1;
	buffer->BitmapInfo.bmiHeader.biBitCount = 32;
	buffer->BitmapInfo.bmiHeader.biCompression = BI_RGB;

	// Allocate memory for our DIB section
	int BitmapMemorySize = (buffer->Width * buffer->Height) * (BytesPerPixel);
	buffer->BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

// Have Windows display our buffer and scale it as appropriate
static void
Win32_DisplayBuffer(win32_Buffer* buffer,
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

// Window Callback Procedure
LRESULT CALLBACK
Win32_WindowProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	LRESULT res = 0;

	// Parse Message
	switch (Msg) {
		case WM_SIZE:          break;
		case WM_CLOSE:         Running = false; break;
		case WM_DESTROY:       Running = false; break;
		case WM_ACTIVATEAPP:   break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:       { uint32_t VKCode = wParam; // virtual key code
							   bool WasDown = ((lParam & (1 << 30)) != 0);
							   bool IsDown = ((lParam & (1 << 31)) == 0);
// 							   if (WasDown != IsDown) {
							       if (VKCode == 'W') { yOffset += 100; }
							       else if (VKCode == 'A') { xOffset += 100; }
							       else if (VKCode == 'S') { }
							       else if (VKCode == 'D') { }
							       else if (VKCode == 'Q') { }
							       else if (VKCode == 'E') { }
							       else if (VKCode == VK_UP) { soundInfo.toneHertz += 30;
								  							   soundInfo.wavePeriod = soundInfo.samplesPerSecond/soundInfo.toneHertz; }
							       else if (VKCode == VK_LEFT) { }
							       else if (VKCode == VK_DOWN) { }
							       else if (VKCode == VK_RIGHT) { }
							       else if (VKCode == VK_ESCAPE) { }
							       else if (VKCode == VK_SPACE) { }
// 	 							 }
							   bool AltKeyDown = lParam & (1 << 29);
							   if ((VKCode == VK_F4)  &&  AltKeyDown) { Running = false; }
							 } break;

		case WM_PAINT:       { PAINTSTRUCT Paint;
							   HDC DeviceContext = BeginPaint(hwnd, &Paint);
							   win32_WinDim Dimension = Win32_GetWinDim(hwnd);
							   Win32_DisplayBuffer(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height); // display our buffer
							   EndPaint(hwnd, &Paint);
							 } break;

		default:               res = DefWindowProcA(hwnd, Msg, wParam, lParam); break;
	}

	return res;
}

// WinMain
int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Some basic setup
	// Get the system's performance frequency for profiling purposes
	LARGE_INTEGER PerformanceFreqRes;
	QueryPerformanceFrequency(&PerformanceFreqRes);
	int64_t PerformanceFreq = PerformanceFreqRes.QuadPart;
	
	// Load XInput .dll
	Win32_LoadXInput();

	// Set up WindowClass
	WNDCLASSA WindowClass = {};
	WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
	WindowClass.lpfnWndProc = Win32_WindowProc;
	WindowClass.hInstance = hInstance;
// 	WindowClass.hIcon = ;
	WindowClass.lpszClassName = "Handmade Hero Window Class";
	
	// Set the size of our buffer
	Win32_ResizeDibSection(&GlobalBackBuffer, 1280, 720);

	// Register the window and create the window
	if (RegisterClassA(&WindowClass)) {
		HWND WindowHandle = CreateWindowExA( 0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
										 	 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
		if (WindowHandle) {
			Running = true;
			HDC DeviceContext = GetDC(WindowHandle);

			// TODO: temp sound vars
			soundInfo = {};
			soundInfo.samplesPerSecond = 48000;
			soundInfo.toneHertz = 256;
			soundInfo.toneVolume = 3000;
			soundInfo.runningSampleIndex = 0;
			soundInfo.latencySampleCount = soundInfo.samplesPerSecond / 15;
			soundInfo.bytesPerSample = sizeof(int16_t) * 2;
			soundInfo.wavePeriod = soundInfo.samplesPerSecond / soundInfo.toneHertz;
			soundInfo.secondaryBufferSize = soundInfo.samplesPerSecond * soundInfo.bytesPerSample;

			// Initialize direct sound
			Win32_InitDirectSound(WindowHandle, soundInfo.samplesPerSecond, soundInfo.secondaryBufferSize);
			Win32_FillSoundBuffer(&soundInfo, 0, soundInfo.latencySampleCount * soundInfo.bytesPerSample);
			SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			// Start our performance query
			LARGE_INTEGER beginCounter;
			QueryPerformanceCounter(&beginCounter);
			uint64_t beginCycleCount = __rdtsc();
			
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
						xOffset += StickX >> 12;
						yOffset += StickY >> 12;
					} else { // controller not plugged in
// 						TODO: handle this case
					}
				}

				
				// TODO: some temp stuff for displaying our gradient
				win32_WinDim Dimension = Win32_GetWinDim(WindowHandle);
				RenderWeirdGradient(&GlobalBackBuffer, xOffset, yOffset);
				Win32_DisplayBuffer(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);

				/*
				 * Note on audio latency:
				 *  --> Not caused by size of the buffer
				 *  --> Caused by how far ahead of the play cursor you write
				 *  --> Sound latency is the amount that will cause the frame's audio to coincide with the frame's image
				 *  --> This latency is often difficult to ascertain due to unspecified bounds and crappy equipment latency
				 */

				// TODO: some temp stuff for Direct Sound output test

				DWORD playCursor; DWORD writeCursor; DWORD bytesToWrite;
			   	if (SUCCEEDED(SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))) { // Get locs of play and write cursors, where to start writing

					// Get the byte to lock, convert samples to bytes and wrap
					DWORD byteToLock = (soundInfo.runningSampleIndex * soundInfo.bytesPerSample) % soundInfo.secondaryBufferSize;

					// Get the target cursor position
					DWORD targetCursor = (playCursor + (soundInfo.latencySampleCount * soundInfo.bytesPerSample))   %   soundInfo.secondaryBufferSize;

					// Get the number of bytes to write into the sound buffer, how much to write
					if (byteToLock > targetCursor) { bytesToWrite = soundInfo.secondaryBufferSize - byteToLock + targetCursor; }
					else                           { bytesToWrite = targetCursor - byteToLock; }

					// Fill the sound buffer with data
					Win32_FillSoundBuffer(&soundInfo, byteToLock, bytesToWrite);
				}
				
				// End our performance query
				uint64_t endCycleCount = __rdtsc();
				LARGE_INTEGER endCounter;
				QueryPerformanceCounter(&endCounter);
				
				// Calculate the timing differences and output as debug info
				int64_t cycleDiff = endCycleCount - beginCycleCount;
				int64_t counterDiff = endCounter.QuadPart - beginCounter.QuadPart;
				int32_t msPerFrame = (1000 * counterDiff) / PerformanceFreq; // (num counter ticks) divided by (num ticks per sec) --> total secs that passed
				int32_t FPS = PerformanceFreq / counterDiff;
				int32_t MegaCyclesPerFrame = (int32_t) cycleDiff / (1000 * 1000);
				char buffer[256]; wsprintf(buffer, "Ms/frame: %d fps: %d cycles passed: %d\n", msPerFrame, FPS, MegaCyclesPerFrame); OutputDebugStringA(buffer);

				// Reset the counters
				beginCounter = endCounter;
				beginCycleCount = endCycleCount;
			}
		}
	}

	return 0;
}

