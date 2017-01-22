#ifndef HANDMADE_H
#define HANDMADE_H

// Function macros
#define   arrayCount(array)   (sizeof(array) / sizeof(array[0]))
#define   Kilobytes(val)      (val * 1024LL)
#define   Megabytes(val)      (Kilobytes(val) * 1024LL)
#define   Gigabytes(val)      (Megabytes(val) * 1024LL)
#define   Terabytes(val)      (Gigabytes(val) * 1024LL)

struct GameImageBuffer {
	void* BitmapMemory;
	int Width;
	int Height;
	int Pitch;
};

struct GameSoundBuffer {
	int samplesPerSecond;
	int sampleCount;
	int16_t* samples;
};

struct GameButtonState {
	int halfTransitionCount;
	bool endedDown;
};

struct GameControllerInput {

	bool isAnalog;

	real32 startX; real32 endX;
	real32 startY; real32 endY;
	real32 minX; real32 maxX;
	real32 minY; real32 maxY;

	union {
		GameButtonState buttons[6];
		struct {
			GameButtonState up;
			GameButtonState down;
			GameButtonState right;
			GameButtonState left;
			GameButtonState lShoulder;
			GameButtonState rShoulder;
		};
	};
};

struct GameInput {
	GameControllerInput controllers[4];
};

struct GameState {
	int toneHertz;
	int blueOffset;
	int greenOffset;
};

struct GameMemory {
	bool isInit;
	uint64_t permanentStorageSize;
	void* permanentStorage;
	uint64_t transientStorageSize;
	void* transientStorage;
};

struct DebugReadFile {
	uint32_t contentSize;
	void* contents;
};

/*  Notes for macro defines
	 
	HANDMADE_INTERNAL:
		0 - build for public release
		1 - build for developers only

	HANDMADE_SLOW:
		0 - no slow code allowed
		1 - slow code allowed
 */

#if HANDMADE_INTERNAL
	static DebugReadFile DEBUG_Platform_readEntireFile(char* fileName);
	static void DEBUG_Platform_freeFileMemory(void* memory);
	static bool DEBUG_Platform_writeEntireFile(char* fileName, uint32_t memorySize, void* memory);
#endif

#if HANDMADE_SLOW
	#define   assert(expression)  if (! (expression)) { *(int*) 0 = 0; }
#else
	#define   assert(expression)
#endif


// Services the game provides to the platform layer
static void gameUpdateAndRender(GameMemory*      memory,
								GameInput*       input,
								GameImageBuffer* imageBuffer,
								GameSoundBuffer* soundBuffer);

// Services the game provides to the platform layer

// Helper functions
inline uint32_t safeTruncateUInt64(uint64_t value) {
	assert(value <= 0xFFFFFFFF);
	uint32_t res = (uint32_t) value; 
	return res;
}

#endif
