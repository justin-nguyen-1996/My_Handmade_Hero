#ifndef HANDMADE_H
#define HANDMADE_H

#include "handmade_platform.h"

// HANDMADE_SLOW function macros
#if HANDMADE_SLOW
	#define   assert(expression)  if (! (expression)) { *(int*) 0 = 0; }
#else
	#define   assert(expression)
#endif

// Defines
#define PI 3.14159265359f

// Function macros
#define   arrayCount(array)   (sizeof(array) / sizeof(array[0]))
#define   Kilobytes(val)      (val * 1024LL)
#define   Megabytes(val)      (Kilobytes(val) * 1024LL)
#define   Gigabytes(val)      (Megabytes(val) * 1024LL)
#define   Terabytes(val)      (Gigabytes(val) * 1024LL)

typedef struct tilemap {
	uint32_t* tiles;
} tilemap;

typedef struct world {
	// constant values for the tile maps
	real32 upperLeftX;
	real32 upperLeftY;
	real32 tileWidth;
	real32 tileHeight;
	
	int32_t sizeX;
	int32_t sizeY;
	tilemap* tileMaps;
} world;

typedef struct GameState {
	int32_t playerTileMapX;
	int32_t playerTileMapY;
	real32 playerX;
	real32 playerY;
} GameState;

/******************************************************************/
/******** Services the platform provides to the game layer ********/
/******************************************************************/

// Helper functions
inline uint32_t safeTruncateUInt64(uint64_t value) {
	assert(value <= 0xFFFFFFFF);
	uint32_t res = (uint32_t) value;
	return res;
}

inline GameControllerInput* getController(GameInput* input, int controllerIndex) {
	assert(controllerIndex < arrayCount(input->controllers));
	GameControllerInput* res = &input->controllers[controllerIndex];
	return res;
}

#endif
