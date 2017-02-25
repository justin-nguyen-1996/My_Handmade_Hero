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

 // HANDMADE_SLOW function macros
#if HANDMADE_SLOW
	 #define   assert(expression)  if (! (expression)) { *(int*) 0 = 0; }
#else
	 #define   assert(expression)
#endif

// Includes
#include <math.h>
#include <stdint.h>

// Defines
#define PI 3.14159265359f

// Typedefs
typedef float real32;
typedef double real64;

// Function macros
#define   arrayCount(array)   (sizeof(array) / sizeof(array[0]))
#define   Kilobytes(val)      (val * 1024LL)
#define   Megabytes(val)      (Kilobytes(val) * 1024LL)
#define   Gigabytes(val)      (Megabytes(val) * 1024LL)
#define   Terabytes(val)      (Gigabytes(val) * 1024LL)

#if HANDMADE_INTERNAL

	struct DebugReadFile {
		uint32_t contentSize;
		void* contents;
	};

	struct ThreadContext {
		int placeHolder;
	};

	#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(ThreadContext* threadContext, void* memory)
	typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

	#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DebugReadFile name(ThreadContext* threadContext, char* fileName)
	typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);
	
	#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name(ThreadContext* threadContext, char* fileName, uint32_t memorySize, void* memory)
	typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
	
#endif

struct GameImageBuffer {
	void* BitmapMemory;
	int Width;
	int Height;
	int Pitch;
	int bytesPerPixel;
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
		GameButtonState buttons[12];
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
	GameButtonState mouseButtons[5];
	int32_t mouseX; 
	int32_t mouseY; 
	int32_t mouseZ;
	GameControllerInput controllers[5];

	real32 secondsToAdvanceOverUpdate;
};

inline GameControllerInput* getController(GameInput* input, int controllerIndex) {
	assert(controllerIndex < arrayCount(input->controllers));
	GameControllerInput* res = &input->controllers[controllerIndex];
	return res;
}

struct GameState {
	
};

struct GameMemory {
	bool isInit;
	
	uint64_t permanentStorageSize;
	void* permanentStorage;
	uint64_t transientStorageSize;
	void* transientStorage;

	debug_platform_free_file_memory* DEBUG_Platform_FreeFileMemory;
	debug_platform_read_entire_file* DEBUG_Platform_ReadEntireFile;
	debug_platform_write_entire_file* DEBUG_Platform_WriteEntireFile;
};

/******************************************************************/
/******** Services the game provides to the platform layer ********/
/******************************************************************/

#define GAME_UPDATE_AND_RENDER(name) void name(ThreadContext* threadContext, GameMemory* memory, GameInput* input, GameImageBuffer* imageBuffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(ThreadContext* threadContext, GameMemory* memory, GameSoundBuffer* soundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

/******************************************************************/
/******** Services the platform provides to the game layer ********/
/******************************************************************/

// Helper functions
inline uint32_t safeTruncateUInt64(uint64_t value) {
	assert(value <= 0xFFFFFFFF);
	uint32_t res = (uint32_t) value;
	return res;
}

#endif
