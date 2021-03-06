
#include "handmade.h"
#include "handmade_intrinsics.h"

static void drawRectangle(GameImageBuffer* buffer,
						  real32 realMinX, real32 realMinY,
						  real32 realMaxX, real32 realMaxY,
						  real32 R, real32 G, real32 B)
{
	// Color calculation
	uint32_t color = (roundReal32ToUInt32(R * 255.0f) << 16)
					 | (roundReal32ToUInt32(G * 255.0f) << 8)
					 | (roundReal32ToUInt32(B * 255.0f) << 0);

	// Round input params
	int32_t minX = roundReal32ToInt32(realMinX);
	int32_t minY = roundReal32ToInt32(realMinY);
	int32_t maxX = roundReal32ToInt32(realMaxX);
	int32_t maxY = roundReal32ToInt32(realMaxY);

	// If param values exceed actual bounds, clip to min and max values of the buffer
	if (minX < 0) 			   { minX = 0; }
	if (minY < 0) 			   { minY = 0; }
	if (maxX > buffer->Width)  { maxX = buffer->Width; }
	if (maxY > buffer->Height) { maxY = buffer->Height; }

	// Gets top left corner of player
	uint8_t* row = (uint8_t*)buffer->BitmapMemory + minX*buffer->bytesPerPixel + minY*buffer->Pitch;

	// Draw up to, but not including, the point (maxX, maxY)
	for (int y = minY; y < maxY; ++y) {
		uint32_t* pixel = (uint32_t*) row; // cast so that we can increment by pixels when coloring
		for (int x = minX; x < maxX; ++x) {
			*pixel++ = color;
		}
		row += buffer->Pitch;
	}
}

// static void RenderWeirdGradient(GameImageBuffer* buffer, int xOffset, int yOffset)
// {
// 	uint8_t* row = (uint8_t*) buffer->BitmapMemory;
// 	for (int y = 0; y < buffer->Height; ++y) {
//
// 		uint32_t* pixel = (uint32_t*) row;
// 		for (int x = 0; x < buffer->Width; ++x) {
// 			//   Memory:   BB GG RR XX
// 			//   Register: XX RR GG BB
// 			uint8_t blue = (uint8_t) (x + xOffset);
// 			uint8_t green = (uint8_t) (y + yOffset);
// 			uint8_t red;
// 			*pixel++ = ((green << 8) | (blue));
// 		}
//
// 		row += buffer->Pitch;
// 	}
// }

static void GameOutputSound(game_state* gameState, GameSoundBuffer* SoundBuffer, int toneHertz) {
	int16_t toneVolume = 3000;
	int wavePeriod = SoundBuffer->samplesPerSecond / toneHertz;
	int16_t* sampleOut = SoundBuffer->samples;

	// Loop through the first region to write to the buffer, stereo sound is encoded as pairs of 16bit values (left, right)
	for (int SampleIndex = 0; SampleIndex < SoundBuffer->sampleCount; ++SampleIndex) {
#if 0
		real32 sineValue = sinf(gameState->tSine);
		int16_t sampleValue = (int16_t) (sineValue * toneVolume);
#else
		int16_t sampleValue = 0;
#endif

#if 0
		*sampleOut++ = sampleValue;
		*sampleOut++ = sampleValue;
		gameState->tSine += 2.0f * PI * 1.0f / (real32) wavePeriod;
		if(gameState->tSine > 2.0f*PI) { gameState->tSine -= 2.0f*PI; }
#endif
	}
}

inline tilemap* getTileMap(world* world, int tileMapX, int tileMapY) {
	tilemap* tileMap = 0;
	if (tileMapX >= 0  &&  tileMapX < world->numTileMapsX  && // make sure the tile map coordinates are valid
		tileMapY >= 0  &&  tileMapY < world->numTileMapsY)
	{
		int pitch = tileMapY * world->numTileMapsX;
		tileMap = &(world->tileMaps[pitch + tileMapX]);
	}
	return tileMap;
}

inline uint32_t getTileValue(world* world, tilemap* tileMap, int tileX, int tileY) {
	assert(tileMap);
	assert (tileX >= 0  &&  tileX < world->numTilesX  && // make sure the player is actually on the tile map
		    tileY>= 0  &&  tileY < world->numTilesY)

	int pitch = tileY * world->numTilesX;
	uint32_t tileVal = tileMap->tiles[pitch + tileX];
	return tileVal;
}

static bool isTileMapPointEmpty(world* world, tilemap* tileMap, int32_t testTileX, int32_t testTileY) {

	bool isEmpty = false;

	if (tileMap) {

		if (testTileX >= 0  &&  testTileX < world->numTilesX  && // make sure the player is actually on the tile map
			testTileY >= 0  &&  testTileY < world->numTilesY)
		{
			uint32_t tileMapValue = getTileValue(world, tileMap, testTileX, testTileY);
			isEmpty = (tileMapValue == 0); // empty points are considered to have a values of 0 in our tile map
		}
	}

	return isEmpty;
}

inline void recanonicalizeCoord(world* world, uint32_t* tile, real32* tileRel) {
	int32_t offset = floorReal32ToInt32(*tileRel / world->tileSideInMeters); // if (greater than 1) --> moved off the tile to the right, vice-versa
	*tile += offset;
	*tileRel -= offset * world->tileSideInMeters;
	assert(*tileRel <= world->tileSideInMeters);
	assert(*tileRel >= 0);
}

inline world_pos recanonicalizePosition(world* world, world_pos pos) {
	world_pos res = pos;
	recanonicalizeCoord(world, &res.tileX, &res.tileRelX);
	recanonicalizeCoord(world, &res.tileY, &res.tileRelY);
	return res;
}

static bool isWorldPointEmpty(world* world, world_pos* canPos) {
	bool isEmpty = false;
	tilemap* tileMap = getTileMap(world, canPos->tileX, canPos->tileY);
	isEmpty = isTileMapPointEmpty(world, tileMap, canPos->tileX, canPos->tileY);
	return isEmpty;
}

#define TILE_MAP_SIZE_X 17

#define TILE_MAP_SIZE_Y 9

// static void gameUpdateAndRender(ThreadContext* threadContext, GameMemory* memory, GameInput* input, GameImageBuffer* imageBuffer)
extern "C" GAME_UPDATE_AND_RENDER(gameUpdateAndRender) {

	// Tile map
	uint32_t tiles00[TILE_MAP_SIZE_Y][TILE_MAP_SIZE_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
        {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0},
        {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1},
        {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
    };

    uint32_t tiles01[TILE_MAP_SIZE_Y][TILE_MAP_SIZE_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
    };

    uint32_t tiles10[TILE_MAP_SIZE_Y][TILE_MAP_SIZE_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
    };

	uint32_t tiles11[TILE_MAP_SIZE_Y][TILE_MAP_SIZE_X] =
    {
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
    };

	// Our array of tile maps
	tilemap tileMaps[2][2];

	// Pointing our tilemap structs to our array of numbers above
	tileMaps[0][0].tiles = (uint32_t*) tiles00;
	tileMaps[0][1].tiles = (uint32_t*) tiles10;
	tileMaps[1][0].tiles = (uint32_t*) tiles01;
	tileMaps[1][1].tiles = (uint32_t*) tiles11;

	// World struct
	world world;
	world.tileSideInPixels = 60;
	world.tileSideInMeters = 1.4f;
	world.metersToPixels = world.tileSideInPixels / world.tileSideInMeters;
	world.numTileMapsX = 2;
	world.numTileMapsY = 2;
	world.numTilesX = TILE_MAP_SIZE_X;
	world.numTilesY = TILE_MAP_SIZE_Y;
	world.lowerLeftX = -(real32)world.tileSideInPixels / 2;
	world.lowerLeftY = (real32)imageBuffer->Height;
	world.tileMaps = (tilemap*) tileMaps;

	// Initialize the game state and memory
	game_state* gameState = (game_state*) memory->permanentStorage;
	if (! memory->isInit) {
		gameState->playerPos.tileMapX = 0;
		gameState->playerPos.tileMapY = 0;
		gameState->playerPos.tileX = 3;
		gameState->playerPos.tileY = 3;
		gameState->playerPos.tileRelX = 5.0f;
		gameState->playerPos.tileRelY = 5.0f;
		memory->isInit = true;
	}

	// Current tile map
	tilemap* tileMap = getTileMap(&world, gameState->playerPos.tileMapX, gameState->playerPos.tileMapY);
	assert(tileMap);

	// Obtain player dimensions
	real32 playerHeight = 1.4f;
	real32 playerWidth = 0.75f * playerHeight;

	// Handle game input
	for (int controllerIndex = 0; controllerIndex < arrayCount(input->controllers); ++controllerIndex) {

		GameControllerInput* controller = getController(input, controllerIndex);
		if (controller->isAnalog) { // controller input

		} else { // keyboard input

			// Figure out which direction the player moved
			real32 deltaPlayerX = 0.0f;
			real32 deltaPlayerY = 0.0f;
			if (controller->moveUp.endedDown)    { deltaPlayerY =  1.0f; }
			if (controller->moveDown.endedDown)  { deltaPlayerY = -1.0f; }
			if (controller->moveRight.endedDown) { deltaPlayerX =  1.0f; }
			if (controller->moveLeft.endedDown)  { deltaPlayerX = -1.0f; }

			// Scale the delta
			deltaPlayerX *= 1.0f;
			deltaPlayerY *= 1.0f;

			// Re-adjust player's position
			world_pos newPlayerPos = gameState->playerPos; // middle
				newPlayerPos.tileRelX += input->deltaTimeForFrame * deltaPlayerX;
				newPlayerPos.tileRelY += input->deltaTimeForFrame * deltaPlayerY;
				newPlayerPos = recanonicalizePosition(&world, newPlayerPos);
			world_pos playerLeftPos = newPlayerPos;        // left
				playerLeftPos.tileRelX -= 0.5f*playerWidth; 
				playerLeftPos = recanonicalizePosition(&world, playerLeftPos);
			world_pos playerRightPos = newPlayerPos;       // right
				playerRightPos.tileRelX += 0.5f*playerWidth; 
				playerRightPos = recanonicalizePosition(&world, playerRightPos);

			// Only change actual location of the player if given valid coordinates
			if ((isWorldPointEmpty(&world, &newPlayerPos)) &&
				(isWorldPointEmpty(&world, &playerLeftPos)) &&
				(isWorldPointEmpty(&world, &playerRightPos)))
			{
				gameState->playerPos = newPlayerPos;
			}
		}
	}

	// White background
    drawRectangle(imageBuffer, 0.0f, 0.0f, (real32)imageBuffer->Width, (real32)imageBuffer->Height, 1.0f, 1.0f, 1.0f);

	// Display the tile map (get tile ID, get rectangle bounds, call drawRect)
	for (int y = 0; y < 9; ++y) {
		for (int x = 0; x < 17; ++x) {
			int tileIndex = getTileValue(&world, tileMap, x, y);
			real32 minX = world.lowerLeftX + (world.tileSideInPixels * (real32)x);
			real32 minY = world.lowerLeftY - (world.tileSideInPixels * (real32)y);
			real32 maxX = minX + world.tileSideInPixels;
			real32 maxY = minY - world.tileSideInPixels;
			real32 tempColor = 0.5f;
			if (tileIndex == 1) { tempColor = 1.0f; }
			if (y == gameState->playerPos.tileY  &&  x == gameState->playerPos.tileX) { tempColor = 0.0f; }
			drawRectangle(imageBuffer, minX, maxY, maxX, minY, tempColor, tempColor, tempColor);
		}
	}

	// Obtain player coordinates
	real32 playerLeft = world.lowerLeftX + (gameState->playerPos.tileX*world.tileSideInPixels)
						+ (world.metersToPixels*gameState->playerPos.tileRelX) - (playerWidth*0.5f*world.metersToPixels);
	real32 playerTop = world.lowerLeftY - (gameState->playerPos.tileY*world.tileSideInPixels)
						- (world.metersToPixels*gameState->playerPos.tileRelY) - (playerHeight*world.metersToPixels); 

	// Obtain player color
	real32 playerR = 1.0;
	real32 playerG = 1.0;
	real32 playerB = 0.0;

	// Draw the player
	drawRectangle(imageBuffer, 
				  playerLeft, 
				  playerTop, 
				  playerLeft + playerWidth*world.metersToPixels, 
				  playerTop + playerHeight*world.metersToPixels, 
				  playerR, playerG, playerB);
}

extern "C" GAME_GET_SOUND_SAMPLES(gameGetSoundSamples) {
	game_state* gameState = (game_state*) memory->permanentStorage;
	GameOutputSound(gameState, soundBuffer, 400);
}

