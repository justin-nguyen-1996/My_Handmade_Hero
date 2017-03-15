#ifndef HANDMADE_H
#define HANDMADE_H

#include "handmade_platform.h"

// #define HANDMADE_SLOW 1
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

typedef struct canonical_world_pos {
	int32_t tileMapX; 
	int32_t tileMapY; 
	int32_t tileX;
	int32_t tileY;

	// tile-relative x & y
	real32 x; 
	real32 y;
} canonical_world_pos;

typedef struct raw_world_pos {
	int32_t tileMapX; 
	int32_t tileMapY; 
 
	// tilemap-relative x & y
	real32 x; 
	real32 y;
} raw_world_pos; 

// Holds info on fixed tile map sizes and holds all of the tile maps
typedef struct world {
	// fixed tile map sizes
	real32 upperLeftX;
	real32 upperLeftY;
	real32 tileWidth;
	real32 tileHeight;
	int32_t numTilesX;
	int32_t numTilesY;
	
	// number of tile maps
	int32_t numTileMapsX;
	int32_t numTileMapsY;

	// all of the tile maps
	tilemap* tileMaps;
	
} world;

// Holds info on where the player is (which tile map and which tile)
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
