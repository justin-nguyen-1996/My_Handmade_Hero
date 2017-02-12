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
	int latencySampleCount;
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

struct win32_state {
	int inputRecordingIndex;
	int inputPlayingIndex;
};

#endif
