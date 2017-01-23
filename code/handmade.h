#ifndef HANDMADE_H
#define HANDMADE_H

/*  Notes for macro defines

	HANDMADE_INTERNAL:
		0 - build for public release
		1 - build for developers only

	HANDMADE_SLOW:
		0 - no slow code allowed
		1 - slow code allowed
 */

// HANDMADE_INTERNAL function macros
#if HANDMADE_INTERNAL
	static DebugReadFile DEBUG_Platform_readEntireFile(char* fileName);
	static void DEBUG_Platform_freeFileMemory(void* memory);
	static bool DEBUG_Platform_writeEntireFile(char* fileName, uint32_t memorySize, void* memory);
#endif

// HANDMADE_SLOW function macros
#if HANDMADE_SLOW
	#define   assert(expression)  if (! (expression)) { *(int*) 0 = 0; }
#else
	#define   assert(expression)
#endif

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

	bool isConnected;
	bool isAnalog;
	real32 stickAverageX;
	real32 stickAverageY;

	union {
		GameButtonState buttons[10];
		struct {
			GameButtonState moveUp;
			GameButtonState moveDown;
			GameButtonState moveRight;
			GameButtonState moveLeft;
			
			GameButtonState actionUp;
			GameButtonState actionDown;
			GameButtonState actionRight;
			GameButtonState actionLeft;
			
			GameButtonState lShoulder;
			GameButtonState rShoulder;

			GameButtonState back;
			GameButtonState start;
		};
	};
};

struct GameInput {
	GameControllerInput controllers[5];
};

inline GameControllerInput* getController(GameInput* input, int controllerIndex) {
	assert(controllerIndex < arrayCount(input->controllers));
	GameControllerInput* res = &input->controllers[controllerIndex];
	return res;
}

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
