#ifndef win32_handmade_h
#define win32_handmade_h

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

struct win32_SoundInfo {
	int samplesPerSecond;
	uint32_t runningSampleIndex;
	int bytesPerSample;
	DWORD secondaryBufferSize;
	DWORD safetyBytes;
	real32 tSine;
};

struct win32_DebugTimeMarker {
	DWORD outputPlayCursor;
	DWORD outputWriteCursor;
	DWORD outputLocation;
	DWORD outputByteCount;
	DWORD expectedFlipPlayCursor;
	
	DWORD flipPlayCursor;
	DWORD flipWriteCursor;
};

struct win32_GameCode {
	HMODULE gameCodeDLL;
	FILETIME DllLastWriteTime;
	game_update_and_render* updateAndRender;
	game_get_sound_samples* getSoundSamples;
	bool isValid;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

struct win32_ReplayBuffer {
	HANDLE fileHandle;
	HANDLE memoryMap;
	char fileName[WIN32_STATE_FILE_NAME_COUNT];
	void* memoryBlock;
};

// Holds our game memory, stuff for live looped code editing, exe file name
struct win32_state {
	win32_ReplayBuffer replayBuffers[4];
	
	uint64_t totalSize;
	void* gameMemoryBlock;
	
	HANDLE recordingHandle;
	int inputRecordingIndex;
	HANDLE playBackHandle;
	int inputPlayingIndex;
	
	char exeFileName[WIN32_STATE_FILE_NAME_COUNT];
	char* onePastLastSlash;
};

#endif
