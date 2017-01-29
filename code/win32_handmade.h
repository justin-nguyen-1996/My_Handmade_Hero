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
	real32 tSine;
	int latencySampleCount;
};

struct win32_DebugTimeMarker {
	DWORD playCursor;
	DWORD writeCursor;
};

#endif
