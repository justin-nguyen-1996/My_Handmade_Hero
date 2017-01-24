
// Typedefs
typedef float real32;
typedef double real64;

// Defines
#define PI 3.14159265359f

// Includes
#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h> // TODO: temp include
#include <malloc.h> // TODO: temp include
#include "handmade.h"
#include "handmade.cpp"

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
	uint32_t runningSampleIndex;
	int bytesPerSample;
	int secondaryBufferSize;
	real32 tSine;
	int latencySampleCount;
};

// Globals
static bool Running;
static win32_Buffer GlobalBackBuffer;
static LPDIRECTSOUNDBUFFER SecondaryBuffer;

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

// Manually load the XInput dll (helps with compatibility)
static void Win32_LoadXInput() {
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

// Process button input
static void Win32_ProcessButton(DWORD XInputButtonState, GameButtonState* oldState, DWORD buttonBit, GameButtonState* newState) {
	newState->endedDown = ((XInputButtonState & buttonBit) == buttonBit);
	newState->halfTransitionCount = (oldState->endedDown != newState->endedDown) ? 1 : 0;
}

// Process keyboard input
static void Win32_ProcessKeyboard(GameButtonState* newState, bool isDown) {
	assert(newState->endedDown != isDown); // this method is only supposed to be called if the keyboard state changed
	newState->endedDown = isDown;
	newState->halfTransitionCount += 1;
}

static real32 Win32_ProcessXInputStickPos(SHORT val, SHORT deadZoneThreshold) {
	real32 res = 0;
	if (val < -deadZoneThreshold)     { res = (real32) val / 32768.0f; }
	else if (val > deadZoneThreshold) { res = (real32) val / 32767.0f; }
	return res;
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
Win32_FillSoundBuffer(win32_SoundInfo* soundInfo, DWORD byteToLock, DWORD bytesToWrite, GameSoundBuffer* soundBuffer) {
	// Need two regions because the region up to the play cursor could be in two chunks (we're using a circular buffer)
	VOID* region1; DWORD region1Size;
	VOID* region2; DWORD region2Size;

	// Need to lock the buffer to have DirectSound let us write into the buffer
	if (SUCCEEDED(SecondaryBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {

		DWORD region1SampleCount = region1Size / soundInfo->bytesPerSample;
		int16_t* destSample = (int16_t*) region1;
		int16_t* srcSample = soundBuffer->samples;

		// Loop through the first region to write to the buffer, stereo sound is encoded as pairs of 16bit values (left, right)
		for (DWORD SampleIndex = 0; SampleIndex < region1SampleCount; ++SampleIndex) {
			*destSample++ = *srcSample++;
			*destSample++ = *srcSample++;
			++soundInfo->runningSampleIndex;
		}

		DWORD region2SampleCount = region2Size / soundInfo->bytesPerSample;
		destSample = (int16_t*) region2;

		// Loop through the second region to write to the buffer, stereo sound is encoded as pairs of 16bit values (left, right)
		for (DWORD SampleIndex = 0; SampleIndex < region2SampleCount; ++SampleIndex) {
			*destSample++ = *srcSample++;
			*destSample++ = *srcSample++;
			++soundInfo->runningSampleIndex;
		}

		// Unlock the buffer --> done writing so continue playing again
		SecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
	}
}

// Clear the sound buffer
static void
Win32_ClearBuffer(win32_SoundInfo* soundBuffer) {

	// Need two regions because the region up to the play cursor could be in two chunks (we're using a circular buffer)
	VOID* region1; DWORD region1Size;
	VOID* region2; DWORD region2Size;

	if (SUCCEEDED(SecondaryBuffer->Lock(0, soundBuffer->secondaryBufferSize, &region1, &region1Size, &region2, &region2Size, 0))) {

		// Loop through the first region to write to the buffer, stereo sound is encoded as pairs of 16bit values (left, right)
		uint8_t* destSample = (uint8_t*) region1;
		for (DWORD ByteIndex = 0; ByteIndex < region1Size; ++ByteIndex) { *destSample++ = 0; }

		// Loop through the second region to write to the buffer, stereo sound is encoded as pairs of 16bit values (left, right)
		destSample = (uint8_t*) region2;
		for (DWORD ByteIndex = 0; ByteIndex < region2Size; ++ByteIndex) { *destSample++ = 0; }

		// Unlock the buffer --> done writing so continue playing again
		SecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
	}
}

static DebugReadFile DEBUG_Platform_readEntireFile(char* fileName) {

	DebugReadFile result = {}; LARGE_INTEGER fileSize;
	HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

	if (fileHandle != INVALID_HANDLE_VALUE) {

		if (GetFileSizeEx(fileHandle, &fileSize)) {

			uint32_t fileSize32 = safeTruncateUInt64(fileSize.QuadPart);
			result.contents = VirtualAlloc(0, fileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

			if (result.contents) {

				DWORD bytesRead;
				if (ReadFile(fileHandle, result.contents, fileSize32, &bytesRead, 0)   &&   fileSize32 == bytesRead) {
					result.contentSize = fileSize32;
				} else {
					DEBUG_Platform_freeFileMemory(result.contents);
					result.contents = 0;
				}
			}
		}

		CloseHandle(fileHandle);
	}

	return result;
}

static void DEBUG_Platform_freeFileMemory(void* memory) {
	if (memory) { VirtualFree(memory, 0, MEM_RELEASE); }
}

static bool DEBUG_Platform_writeEntireFile(char* fileName, uint32_t memorySize, void* memory) {

	bool result = false; LARGE_INTEGER fileSize;
	HANDLE fileHandle = CreateFileA(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

	if (fileHandle != INVALID_HANDLE_VALUE) {

		DWORD bytesWritten;
		if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0)) {
			result = (bytesWritten == memorySize);
		}

		CloseHandle(fileHandle);
	}

	return result;
}

// Create the buffer that we will have Windows display for us
// Device Independent Bitmap
static void Win32_ResizeDibSection(win32_Buffer* buffer, int Width, int Height)
{
	// Free our old DIB section if we ask for a new one
	if (buffer->BitmapMemory) { VirtualFree(buffer->BitmapMemory, 0, MEM_RELEASE); }

	// Setup the image buffer
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
static void Win32_DisplayBuffer(win32_Buffer* buffer, HDC DeviceContext, int WinWidth, int WinHeight)
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
LRESULT CALLBACK Win32_WindowProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
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
		case WM_KEYUP:         break;

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

static void Win32_ProcessPendingMessages(GameControllerInput* keyboardController) {
	// Message queue --> pull out one at a time --> translate & dispatch --> parse in Window Callback procedure
	MSG Message;
	while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
		switch(Message.message) {
			case WM_QUIT: Running = false; break;
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP: { uint32_t VKCode = (uint32_t) Message.wParam; // virtual key code
							 bool WasDown = ((Message.lParam & (1 << 30)) != 0);
							 bool IsDown = ((Message.lParam & (1 << 31)) == 0);
							 if (WasDown != IsDown) { // only process keyboard inputs if the state of the keyboard changed
								 if (VKCode == 'W')            { Win32_ProcessKeyboard(&keyboardController->moveUp,          IsDown); }
								 else if (VKCode == 'A')       { Win32_ProcessKeyboard(&keyboardController->moveLeft,        IsDown); }
								 else if (VKCode == 'S')       { Win32_ProcessKeyboard(&keyboardController->moveDown,        IsDown); }
								 else if (VKCode == 'D')       { Win32_ProcessKeyboard(&keyboardController->moveRight,       IsDown); }
								 else if (VKCode == 'Q')       { Win32_ProcessKeyboard(&keyboardController->lShoulder,       IsDown); }
								 else if (VKCode == 'E')       { Win32_ProcessKeyboard(&keyboardController->rShoulder,       IsDown); }
								 else if (VKCode == VK_UP)     { Win32_ProcessKeyboard(&keyboardController->actionUp,        IsDown); }
								 else if (VKCode == VK_LEFT)   { Win32_ProcessKeyboard(&keyboardController->actionLeft,      IsDown); }
								 else if (VKCode == VK_DOWN)   { Win32_ProcessKeyboard(&keyboardController->actionDown,      IsDown); }
								 else if (VKCode == VK_RIGHT)  { Win32_ProcessKeyboard(&keyboardController->actionRight,     IsDown); }
								 else if (VKCode == VK_ESCAPE) { Win32_ProcessKeyboard(&keyboardController->start,           IsDown); }
								 else if (VKCode == VK_SPACE)  { Win32_ProcessKeyboard(&keyboardController->back,            IsDown); }
							 }

							 bool AltKeyDown = Message.lParam & (1 << 29);
							 if ((VKCode == VK_F4)  &&  AltKeyDown) { Running = false; }
						   }

			default: { TranslateMessage(&Message);
					   DispatchMessageA(&Message); // calls the Window Callback procedure Win32_WindowProc, required by Windows
					   break;
					 }
		}
	}
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
			win32_SoundInfo soundInfo = {};
			soundInfo.samplesPerSecond = 48000;
			soundInfo.latencySampleCount = soundInfo.samplesPerSecond / 15;
			soundInfo.bytesPerSample = sizeof(int16_t) * 2;
			soundInfo.secondaryBufferSize = soundInfo.samplesPerSecond * soundInfo.bytesPerSample;

			// Initialize direct sound
			Win32_InitDirectSound(WindowHandle, soundInfo.samplesPerSecond, soundInfo.secondaryBufferSize);
			Win32_ClearBuffer(&soundInfo);
			SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
			int16_t* samples = (int16_t*) VirtualAlloc(0, soundInfo.secondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
			LPVOID baseAddress = (LPVOID) Terabytes((uint64_t) 2);
#else
			LPVOID baseAddress = 0;
#endif

			// Initialize our game memory
			GameMemory gameMemory = {};
			gameMemory.permanentStorageSize = Megabytes(64);
			gameMemory.transientStorageSize = Gigabytes(1);
			uint64_t totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
			gameMemory.permanentStorage = VirtualAlloc(baseAddress, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			gameMemory.transientStorage = ((uint8_t*) (gameMemory.permanentStorage)) + gameMemory.permanentStorageSize; // cast in order to increment by bytes

			if (samples  &&  gameMemory.permanentStorage  &&  gameMemory.transientStorage) {

				// Start our performance query
				LARGE_INTEGER beginCounter;
				QueryPerformanceCounter(&beginCounter);
				uint64_t beginCycleCount = __rdtsc();

				// TODO: some temp stuff for our input
				GameInput input[2] = {};
				GameInput* newInput = &input[0];
				GameInput* oldInput = &input[1];

				// Message Loop
				while (Running) {

					// Keyboard Input
					// Zero the keyboard controller input
					GameControllerInput* oldKeyboardController = getController(oldInput, 0);
					GameControllerInput* newKeyboardController = getController(newInput, 0);
					*newKeyboardController = {};
					newKeyboardController->isConnected = true;

					// For each button, set the new button state to the old button state
					for (int buttonIndex = 0; buttonIndex < arrayCount(newKeyboardController->buttons); ++buttonIndex) {
						newKeyboardController->buttons[buttonIndex].endedDown = oldKeyboardController->buttons[buttonIndex].endedDown;
					}
					
					// Process keyboard messages
					Win32_ProcessPendingMessages(newKeyboardController);

					// Gamepad Input
					// In case they add more than four controllers some day (five controllers including keyboard)
					DWORD maxControllerCount = XUSER_MAX_COUNT;
					if (maxControllerCount > arrayCount(newInput->controllers) - 1) { maxControllerCount = arrayCount(newInput->controllers) - 1; }

					// Poll for XInput
					for (DWORD ControllerIndex = 0; ControllerIndex < maxControllerCount; ++ControllerIndex) {

						DWORD ourControllerIndex = ControllerIndex + 1;
						GameControllerInput* oldController = getController(oldInput, ourControllerIndex);
						GameControllerInput* newController = getController(newInput, ourControllerIndex);

						XINPUT_STATE ControllerState;
						if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) { // controller plugged in

							XINPUT_GAMEPAD* pad = &ControllerState.Gamepad;
							newController->isConnected = true;
							
							// Normalize the x & y stick
							newController->stickAverageX = Win32_ProcessXInputStickPos(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
							newController->stickAverageY = Win32_ProcessXInputStickPos(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

							// See if we're using a game controller (as opposed to the keyboard)
							if (newController->stickAverageX   ||   newController->stickAverageY) { newController->isAnalog = true; }

							// Process the DPAD
							if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)         { newController->stickAverageY = 1.0f;  newController->isAnalog = false; }
							else if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  { newController->stickAverageY = -1.0f; newController->isAnalog = false; }
							else if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  { newController->stickAverageY = -1.0f; newController->isAnalog = false; }
							else if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) { newController->stickAverageY = 1.0f;  newController->isAnalog = false; }

							// Process each of the buttons
							real32 threshold = 0.5f;
							Win32_ProcessButton( (newController->stickAverageX   <   -threshold) ? 1 : 0, 
												 &oldController->moveLeft, 1, 
												 &newController->moveLeft);
							Win32_ProcessButton( (newController->stickAverageX   >   threshold) ? 1 : 0,
												 &oldController->moveRight, 1, 
												 &newController->moveRight);
							Win32_ProcessButton( (newController->stickAverageY   <   -threshold) ? 1 : 0, 
												 &oldController->moveDown, 1,
												 &newController->moveDown);
							Win32_ProcessButton( (newController->stickAverageY   >   threshold) ? 1 : 0, 
												 &oldController->moveUp, 1,
												 &newController->moveUp);
							
							Win32_ProcessButton(pad->wButtons, &oldController->actionDown,         XINPUT_GAMEPAD_A,              &newController->actionDown);
							Win32_ProcessButton(pad->wButtons, &oldController->actionRight,         XINPUT_GAMEPAD_B,              &newController->actionRight);
							Win32_ProcessButton(pad->wButtons, &oldController->actionLeft,          XINPUT_GAMEPAD_X,              &newController->actionLeft);
							Win32_ProcessButton(pad->wButtons, &oldController->actionUp,            XINPUT_GAMEPAD_Y,              &newController->actionUp);
							Win32_ProcessButton(pad->wButtons, &oldController->lShoulder,     XINPUT_GAMEPAD_LEFT_SHOULDER,  &newController->lShoulder);
							Win32_ProcessButton(pad->wButtons, &oldController->rShoulder,     XINPUT_GAMEPAD_RIGHT_SHOULDER, &newController->rShoulder);
							Win32_ProcessButton(pad->wButtons, &oldController->start,     XINPUT_GAMEPAD_START, &newController->start);
							Win32_ProcessButton(pad->wButtons, &oldController->back,     XINPUT_GAMEPAD_BACK, &newController->back);
							
						} else { // controller not plugged in
							newController->isConnected = false;
						}
					}

					// TODO: some temp stuff for Direct Sound output test
					DWORD playCursor = 0; DWORD writeCursor = 0;
					DWORD byteToLock = 0; DWORD targetCursor = 0; DWORD bytesToWrite = 0;
					bool soundIsValid = false;

					if (SUCCEEDED(SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))) { // Get locs of play and write cursors, where to start writing
						// Get the byte to lock, convert samples to bytes and wrap
						byteToLock = (soundInfo.runningSampleIndex * soundInfo.bytesPerSample) % soundInfo.secondaryBufferSize;

						// Get the target cursor position
						targetCursor = (playCursor + (soundInfo.latencySampleCount * soundInfo.bytesPerSample))   %   soundInfo.secondaryBufferSize;

						// Get the number of bytes to write into the sound buffer, how much to write
						if (byteToLock > targetCursor) { bytesToWrite = soundInfo.secondaryBufferSize - byteToLock + targetCursor; }
						else                           { bytesToWrite = targetCursor - byteToLock; }

						soundIsValid = true;
					}

					// TODO: some temp stuff for our sound
					GameSoundBuffer SoundBuffer = {};
					SoundBuffer.samplesPerSecond = soundInfo.samplesPerSecond;
					SoundBuffer.sampleCount = bytesToWrite / soundInfo.bytesPerSample;
					SoundBuffer.samples = samples;

					// TODO: some temp stuff for displaying our gradient
					GameImageBuffer ImageBuffer = {};
					ImageBuffer.BitmapMemory = GlobalBackBuffer.BitmapMemory;
					ImageBuffer.Width = GlobalBackBuffer.Width;
					ImageBuffer.Height = GlobalBackBuffer.Height;
					ImageBuffer.Pitch = GlobalBackBuffer.Pitch;

					gameUpdateAndRender(&gameMemory, newInput, &ImageBuffer, &SoundBuffer);
					win32_WinDim Dimension = Win32_GetWinDim(WindowHandle);
					Win32_DisplayBuffer(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);

					/*
					 * Note on audio latency:
					 *  --> Not caused by size of the buffer
					 *  --> Caused by how far ahead of the play cursor you write
					 *  --> Sound latency is the amount that will cause the frame's audio to coincide with the frame's image
					 *  --> This latency is often difficult to ascertain due to unspecified bounds and crappy equipment latency
					 */

					if (soundIsValid) {
						// Fill the sound buffer with data
						Win32_FillSoundBuffer(&soundInfo, byteToLock, bytesToWrite, &SoundBuffer);
					}

					// End our performance query
					uint64_t endCycleCount = __rdtsc();
					LARGE_INTEGER endCounter;
					QueryPerformanceCounter(&endCounter);

					// Calculate the timing differences and output as debug info
					uint64_t cycleDiff = endCycleCount - beginCycleCount;
					int64_t counterDiff = endCounter.QuadPart - beginCounter.QuadPart;
					real64 msPerFrame = (1000.0f * (real64)counterDiff) / (real64) PerformanceFreq;
					real64 FPS = (real64) PerformanceFreq / (real64) counterDiff;
					real64 MegaCyclesPerFrame = (real64) cycleDiff / (1000 * 1000);

					// Reset the counters
					beginCounter = endCounter;
					beginCycleCount = endCycleCount;

					// Swap the old and new inputs
					GameInput* tempInput = oldInput;
					oldInput = newInput;
					newInput = tempInput;
				}
			}
		}
	}

	return 0;
}

