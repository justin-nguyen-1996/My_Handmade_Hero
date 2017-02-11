
// Defines
#define PI 3.14159265359f

// Includes
#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <malloc.h> // TODO: temp include
#include "handmade.h"
#include "win32_handmade.h"

// Globals
static bool Running;
static win32_Buffer GlobalBackBuffer;
static LPDIRECTSOUNDBUFFER SecondaryBuffer;
static int64_t PerformanceFreq;

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
            BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
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

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUG_Platform_FreeFileMemory) {
    if (memory) { VirtualFree(memory, 0, MEM_RELEASE); }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUG_Platform_ReadEntireFile) {

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
                    DEBUG_Platform_FreeFileMemory(result.contents);
                    result.contents = 0;
                }
            }
        }

        CloseHandle(fileHandle);
    }

    return result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUG_Platform_WriteEntireFile) {

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
    buffer->BytesPerPixel = BytesPerPixel;

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
    buffer->Pitch = Width * BytesPerPixel;
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

inline real32 Win32_getSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
    real32 res = (real32)(end.QuadPart - start.QuadPart) / (real32)PerformanceFreq;
    return res;
}

inline LARGE_INTEGER Win32_getWallClock() {
    LARGE_INTEGER res;
    QueryPerformanceCounter(&res);
    return res;
}

static void Win32_debugDrawVertical(win32_Buffer* backBuffer, int x, int top, int bottom, uint32_t color) {
    if (top <= 0) { top = 0; }
    if (bottom > backBuffer->Height) { bottom = backBuffer->Height; }
    if (x >= 0  &&  x < backBuffer->Width) {
        uint8_t* pixel = (uint8_t*) backBuffer->BitmapMemory +
            x * backBuffer->BytesPerPixel +
            top * backBuffer->Pitch;
        for (int y = 0; y < bottom; ++y) {
            *(uint32_t*)pixel = color;
            pixel += backBuffer->Pitch;
        }
    }
}

inline void Win32_drawSoundBufferMarker(win32_Buffer* backBuffer,
        win32_SoundInfo* soundInfo,
        real32 c, int padX, int top, int bottom, DWORD value, uint32_t color)
{
    real32 xReal32 = (c * (real32)value);
    int x = padX + (int)xReal32;
    Win32_debugDrawVertical(backBuffer, x, top, bottom, color);
}

static void Win32_debugSyncDisplay(win32_Buffer* globalBackBuffer,
        int markerCount,
        win32_DebugTimeMarker* markers,
        int currentMarkerIndex,
        win32_SoundInfo* soundInfo,
        real32 targetSecondsPerFrame)
{
    int padX = 16;
    int padY = 16;
    int lineHeight = 64;

    real32 c = ((real32)globalBackBuffer->Width - 2*padX) / (real32)soundInfo->secondaryBufferSize;

    for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex) {

        win32_DebugTimeMarker* thisMarker = &markers[markerIndex];
        assert(thisMarker->outputPlayCursor < soundInfo->secondaryBufferSize);
        assert(thisMarker->outputWriteCursor < soundInfo->secondaryBufferSize);
        assert(thisMarker->flipPlayCursor < soundInfo->secondaryBufferSize);
        assert(thisMarker->flipWriteCursor < soundInfo->secondaryBufferSize);

        DWORD playColor = 0xFFFFFFFF;
        DWORD writeColor = 0xFFFF0000;
        DWORD expectedFlipColor = 0xFFFFFF00;

        int top = padY;
        int bottom = padY + lineHeight;

        if (markerIndex == currentMarkerIndex) {
            top += lineHeight + padY;
            bottom += lineHeight + padY;

            Win32_drawSoundBufferMarker(globalBackBuffer, soundInfo, c, padX, top, bottom, thisMarker->outputPlayCursor, playColor);
            Win32_drawSoundBufferMarker(globalBackBuffer, soundInfo, c, padX, top, bottom, thisMarker->outputWriteCursor, writeColor);
            Win32_drawSoundBufferMarker(globalBackBuffer, soundInfo, c, padX, top, bottom, thisMarker->expectedFlipPlayCursor, expectedFlipColor);

            top += lineHeight + padY;
            bottom += lineHeight + padY;

            Win32_drawSoundBufferMarker(globalBackBuffer, soundInfo, c, padX, top, bottom, thisMarker->outputLocation, playColor);
            Win32_drawSoundBufferMarker(globalBackBuffer, soundInfo, c, padX, top, bottom, thisMarker->outputLocation + thisMarker->outputByteCount, writeColor);

            top += lineHeight + padY;
            bottom += lineHeight + padY;
        }

        Win32_drawSoundBufferMarker(globalBackBuffer, soundInfo, c, padX, top, bottom, thisMarker->flipPlayCursor, playColor);
        Win32_drawSoundBufferMarker(globalBackBuffer, soundInfo, c, padX, top, bottom, thisMarker->flipWriteCursor, writeColor);
    }
}

inline FILETIME Win32_getLastWriteTime(char* fileName) {
    FILETIME lastWriteTime = {};
    WIN32_FIND_DATA findData;
    HANDLE fileHandle = FindFirstFileA(fileName, &findData);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        lastWriteTime = findData.ftLastWriteTime;
        FindClose(fileHandle);
    }
    return lastWriteTime;
}

static win32_GameCode Win32_loadGameCode(char* sourceDLLName) {

    char* tempDLLName = "handmade_temp.dll";

    win32_GameCode res = {};
    CopyFile(sourceDLLName, tempDLLName, FALSE);
    res.gameCodeDLL = LoadLibraryA(tempDLLName);

    res.DllLastWriteTime = Win32_getLastWriteTime(sourceDLLName);


    if (res.gameCodeDLL) {
        res.updateAndRender = (game_update_and_render*) GetProcAddress(res.gameCodeDLL, "gameUpdateAndRender");
        res.getSoundSamples = (game_get_sound_samples*) GetProcAddress(res.gameCodeDLL, "gameGetSoundSamples");
        res.isValid = (res.updateAndRender && res.getSoundSamples);
    }

    if (! res.isValid) {
        res.updateAndRender = GameUpdateAndRenderStub;
        res.getSoundSamples = GameGetSoundSamplesStub;
    }

    return res;
}

static void Win32_unloadGameCode(win32_GameCode* gameCode) {
    if (gameCode->gameCodeDLL) { FreeLibrary(gameCode->gameCodeDLL); gameCode->gameCodeDLL = 0; }
    gameCode->isValid = false;
    gameCode->updateAndRender = GameUpdateAndRenderStub;
    gameCode->getSoundSamples = GameGetSoundSamplesStub;
}

// WinMain
int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Some basic setup

    // Get the system's performance frequency for profiling purposes
    LARGE_INTEGER PerformanceFreqRes;
    QueryPerformanceFrequency(&PerformanceFreqRes);
    PerformanceFreq = PerformanceFreqRes.QuadPart;

    // Set Windows scheduler granularity to one millisecond so that our sleep can be more granular
    UINT desiredSchedulerMS = 1;
    bool sleepIsGranular = (timeBeginPeriod(desiredSchedulerMS) == TIMERR_NOERROR);

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

    // Set the monitor refresh rate TODO: shouldn't have to manually hardcode this
#define monitorRefreshHz       60
#define gameUpdateHz           (monitorRefreshHz / 2)
    real32 targetSecondsPerFrame = 1.0f / (real32) gameUpdateHz;

    // Register the window and create the window
    if (RegisterClassA(&WindowClass)) {
        HWND WindowHandle = CreateWindowExA( 0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
        if (WindowHandle) {

            Running = true;
            HDC DeviceContext = GetDC(WindowHandle);

            // sound vars
            win32_SoundInfo soundInfo = {};
            soundInfo.samplesPerSecond = 48000;
            soundInfo.bytesPerSample = sizeof(int16_t) * 2;
            soundInfo.safetyBytes = (soundInfo.samplesPerSecond * soundInfo.bytesPerSample / gameUpdateHz) / 3; // bytes per frame
            soundInfo.latencySampleCount = 3 * (soundInfo.samplesPerSecond / gameUpdateHz); // samples per frame
            soundInfo.secondaryBufferSize = soundInfo.samplesPerSecond * soundInfo.bytesPerSample;

            // Initialize direct sound
            Win32_InitDirectSound(WindowHandle, soundInfo.samplesPerSecond, soundInfo.secondaryBufferSize);
            Win32_ClearBuffer(&soundInfo);
            SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            int16_t* samples =
                (int16_t*) VirtualAlloc(0, soundInfo.secondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID baseAddress = (LPVOID) Terabytes((uint64_t) 2);
#else
            LPVOID baseAddress = 0;
#endif

            // Initialize our game memory
            GameMemory gameMemory = {};
            gameMemory.permanentStorageSize = Megabytes(64);
            gameMemory.transientStorageSize = Gigabytes(1);
            gameMemory.DEBUG_Platform_FreeFileMemory = DEBUG_Platform_FreeFileMemory;
            gameMemory.DEBUG_Platform_ReadEntireFile = DEBUG_Platform_ReadEntireFile;
            gameMemory.DEBUG_Platform_WriteEntireFile = DEBUG_Platform_WriteEntireFile;
            uint64_t totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
            gameMemory.permanentStorage = VirtualAlloc(baseAddress, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            gameMemory.transientStorage = ((uint8_t*) (gameMemory.permanentStorage)) + gameMemory.permanentStorageSize; // cast in order to increment by bytes

            if (samples  &&  gameMemory.permanentStorage  &&  gameMemory.transientStorage) {

                /* temp stuff */

                // TODO: some temp stuff for our input
                GameInput input[2] = {};
                GameInput* newInput = &input[0];
                GameInput* oldInput = &input[1];

                // some timing stuff
                LARGE_INTEGER beginCounter = Win32_getWallClock();
                LARGE_INTEGER flipWallClock = Win32_getWallClock();

                // TODO: temp vars
                int debugTimeMarkerIndex = 0;
                win32_DebugTimeMarker debugTimeMarkers[gameUpdateHz / 2] = {};

                uint64_t beginCycleCount = __rdtsc();

                DWORD audioLatencyBytes = 0;
                real32 audioLatencySeconds = 0;
                bool soundIsValid = false;

                // Load the game code so we can grab function addresses
                char* sourceDLLName = "handmade.dll";
                win32_GameCode game = Win32_loadGameCode(sourceDLLName);
                uint32_t loadCounter = 0;

                while (Running) {
                    
                    // Instantaneous live code editing by checking if the file changed
                    FILETIME newDLLWriteTime = Win32_getLastWriteTime(sourceDLLName);
                    if (CompareFileTime(&newDLLWriteTime, &game.DllLastWriteTime) != 0) {
                        Win32_unloadGameCode(&game);
                        game = Win32_loadGameCode(sourceDLLName);
                        loadCounter = 0;
                    }

                    /* Keyboard Input */
                    // Zero the keyboard controller input
                    GameControllerInput* oldKeyboardController = getController(oldInput, 0);
                    GameControllerInput* newKeyboardController = getController(newInput, 0);
                    *newKeyboardController = {};
                    newKeyboardController->isConnected = true;

                    // For each button, set the new button state to the old button state
                    for (int buttonIndex = 0; buttonIndex < arrayCount(newKeyboardController->buttons); ++buttonIndex) {
                        newKeyboardController->buttons[buttonIndex].endedDown
                            = oldKeyboardController->buttons[buttonIndex].endedDown;
                    }

                    // Process keyboard messages
                    Win32_ProcessPendingMessages(newKeyboardController);

                    /* Gamepad Input */
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

                    // TODO: some temp stuff for displaying our gradient
                    GameImageBuffer imageBuffer = {};
                    imageBuffer.BitmapMemory = GlobalBackBuffer.BitmapMemory;
                    imageBuffer.Width = GlobalBackBuffer.Width;
                    imageBuffer.Height = GlobalBackBuffer.Height;
                    imageBuffer.Pitch = GlobalBackBuffer.Pitch;

                    // Call our game platform
                    game.updateAndRender(&gameMemory, newInput, &imageBuffer);

                    LARGE_INTEGER audioWallClock = Win32_getWallClock();
                    real32 fromBeginToAudioSeconds = Win32_getSecondsElapsed(flipWallClock, audioWallClock);

                    /*
                     * Note on audio latency:
                     *  --> Not caused by size of the buffer
                     *  --> Caused by how far ahead of the play cursor you write
                     *  --> Sound latency is the amount that will cause the frame's audio to coincide with the frame's image
                     *  --> This latency is often difficult to ascertain due to unspecified bounds and crappy equipment latency
                     */

                    DWORD playCursor;
                    DWORD writeCursor;
                    if (SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK) {

                        /* sound output calculation
                         *
                         * Safety amount = (in units of number of samples), dependent on game loop's update variability
                         * Look at play cursor and forecast future location of play cursor on the next frame boundary.
                         *
                         * If write cursor is before that by our safety amount (low latency)
                         * 		--> target fill position is that frame boundary plus one frame
                         * Else if write cursor is after that by our safety amount (high latency)
                         * 		--> write one frame's worth of audio plus safety amount of samples (dependent on variability of frame computation)
                         */

                        if (! soundIsValid) {
                            soundInfo.runningSampleIndex = writeCursor / soundInfo.bytesPerSample;
                            soundIsValid = true;
                        }

                        // Get the byte to lock, convert samples to bytes and wrap
                        DWORD byteToLock = (soundInfo.runningSampleIndex * soundInfo.bytesPerSample) % soundInfo.secondaryBufferSize;

                        // Some needed calculations and expected results
                        DWORD expectedSoundBytesPerFrame = soundInfo.samplesPerSecond * soundInfo.bytesPerSample / gameUpdateHz;
                        DWORD expectedFrameBoundaryByte = playCursor + expectedSoundBytesPerFrame;
                        real32 secondsLeftUntilFlip = targetSecondsPerFrame - fromBeginToAudioSeconds;
                        DWORD expectedBytesUntilFlip = (DWORD) ((secondsLeftUntilFlip / targetSecondsPerFrame) * (real32)expectedSoundBytesPerFrame);

                        // Get the safe write cursor (to account for jitter)
                        DWORD safeWriteCursor = writeCursor;
                        if (safeWriteCursor < playCursor) { safeWriteCursor += soundInfo.secondaryBufferSize; }
                        assert(safeWriteCursor >= playCursor);
                        safeWriteCursor += soundInfo.safetyBytes;

                        // Get the target cursor based on audio card's latency
                        DWORD targetCursor = 0;
                        bool audioCardIsLowLatency = (safeWriteCursor < expectedFrameBoundaryByte);
                        if (audioCardIsLowLatency) { targetCursor = (expectedFrameBoundaryByte + expectedSoundBytesPerFrame); }
                        else                       { targetCursor = (writeCursor + expectedSoundBytesPerFrame + soundInfo.safetyBytes); }
                        targetCursor = targetCursor % (soundInfo.secondaryBufferSize);

                        // Get the number of bytes to write into the sound buffer, how much to write
                        DWORD bytesToWrite = 0;
                        if (byteToLock > targetCursor) { bytesToWrite = soundInfo.secondaryBufferSize - byteToLock + targetCursor; }
                        else                           { bytesToWrite = targetCursor - byteToLock; }

                        GameSoundBuffer soundBuffer = {};
                        soundBuffer.samplesPerSecond = soundInfo.samplesPerSecond;
                        soundBuffer.sampleCount = bytesToWrite / soundInfo.bytesPerSample;
                        soundBuffer.samples = samples;
                        game.getSoundSamples(&gameMemory, &soundBuffer);

#if HANDMADE_INTERNAL
                        win32_DebugTimeMarker* marker = &debugTimeMarkers[debugTimeMarkerIndex];
                        marker->outputPlayCursor = playCursor;
                        marker->outputWriteCursor = writeCursor;
                        marker->outputLocation = byteToLock;
                        marker->outputByteCount = bytesToWrite;
                        marker->expectedFlipPlayCursor = expectedFrameBoundaryByte;

                        DWORD unwrappedWriteCursor = writeCursor;
                        if (unwrappedWriteCursor < playCursor) { unwrappedWriteCursor += soundInfo.secondaryBufferSize; }
                        audioLatencyBytes = unwrappedWriteCursor - playCursor;
                        audioLatencySeconds = (real32)audioLatencyBytes / (real32)soundInfo.bytesPerSample / (real32)soundInfo.samplesPerSecond;

                        // 						char TextBuffer[256];
                        //                         _snprintf_s(TextBuffer, sizeof(TextBuffer),
                        //                                     "LPC:%u BTL:%u TC:%u BTW:%u - PC:%u WC:%u\n",
                        //                                     LastPlayCursor, ByteToLock, TargetCursor, BytesToWrite,
                        //                                     PlayCursor, WriteCursor);
                        //                         OutputDebugStringA(TextBuffer);
#endif
                        Win32_FillSoundBuffer(&soundInfo, byteToLock, bytesToWrite, &soundBuffer);
                    } else {
                        soundIsValid = false;
                    }

                    /* timing stuff to adjust our frame rate */

                    // Calculate the timing differences
                    LARGE_INTEGER workCounter = Win32_getWallClock();
                    real32 secondsElapsedForWork = Win32_getSecondsElapsed(beginCounter, workCounter);
                    real32 secondsElapsedForFrame = secondsElapsedForWork;

                    // If time per frame is not enough (didn't hit our target seconds per frame) then sleep until it does hit our target
                    if (secondsElapsedForFrame < targetSecondsPerFrame) {

                        if (sleepIsGranular) {
                            DWORD sleepMilliSecs = (DWORD) (1000.0f * (targetSecondsPerFrame - secondsElapsedForFrame));
                            if (sleepMilliSecs > 0) { Sleep(sleepMilliSecs); }
                        }

                        real32 testSecondsElapsedForFrame = Win32_getSecondsElapsed(beginCounter, Win32_getWallClock());
                        // 						if(testSecondsElapsedForFrame < targetSecondsPerFrame);

                        while (secondsElapsedForFrame < targetSecondsPerFrame) {
                            secondsElapsedForFrame = Win32_getSecondsElapsed(beginCounter, Win32_getWallClock());
                        }
                    }

                    flipWallClock = Win32_getWallClock();

                    // Use the Windows platform to display our buffer
                    win32_WinDim Dimension = Win32_GetWinDim(WindowHandle);

#if HANDMADE_INTERNAL
                    Win32_debugSyncDisplay(&GlobalBackBuffer, arrayCount(debugTimeMarkers), debugTimeMarkers,
                            debugTimeMarkerIndex - 1, &soundInfo, targetSecondsPerFrame);
#endif

                    Win32_DisplayBuffer(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);

#if HANDMADE_INTERNAL
                    if (SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK) {
                        assert(debugTimeMarkerIndex < arrayCount(debugTimeMarkers));
                        win32_DebugTimeMarker* marker = &debugTimeMarkers[debugTimeMarkerIndex];
                        marker->flipPlayCursor = playCursor;
                        marker->flipWriteCursor = writeCursor;
                    }
#endif

                    /* end of loop stuff - swapping old/new inputs, resetting counters, ending performance timing stuff */

                    // Swap the old and new inputs
                    GameInput* tempInput = oldInput;
                    oldInput = newInput;
                    newInput = tempInput;

                    // End our performance query
                    uint64_t endCycleCount = __rdtsc();
                    beginCycleCount = endCycleCount;
                    uint64_t cyclesElapsed = endCycleCount - beginCycleCount;

                    // Some debug stuff for timing
                    real64 FPS = 0.0f;
                    real64 MCPF = ((real64)cyclesElapsed / (1000.0f * 1000.0f));
                    char FPSBuffer[256];
                    //                     _snprintf_s(FPSBuffer, sizeof(FPSBuffer), "%.02fms/f,  %.02ff/s,  %.02fmc/f\n", msPerFrame, FPS, MCPF);
                    OutputDebugStringA(FPSBuffer);

#if HANDMADE_INTERNAL
                    ++debugTimeMarkerIndex;
                    if (debugTimeMarkerIndex == arrayCount(debugTimeMarkers)) { debugTimeMarkerIndex = 0; }
#endif

                }

                // End of game loop
            }
        }
    }

    return 0;
}

