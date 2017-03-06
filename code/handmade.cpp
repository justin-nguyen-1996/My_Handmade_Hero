
#include "handmade.h"

inline static int32_t roundReal32ToUInt32(real32 val) {
	uint32_t res = (uint32_t) (val + 0.5f);
	return res;
}

inline static int32_t roundReal32ToInt32(real32 val) {
	int32_t res = (int32_t) (val + 0.5f);
	return res;
}

inline static int32_t truncateReal32ToInt32(real32 val) {
	return (int32_t)(val);
}

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

static void GameOutputSound(GameState* gameState, GameSoundBuffer* SoundBuffer, int toneHertz) {
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

inline uint32_t getTileValue(tilemap* tileMap, int x, int y) {
	int pitch = y * tileMap->sizeX;
	uint32_t tileVal = tileMap->tiles[pitch + x];
	return tileVal;
}

static bool isTileMapPointEmpty(tilemap* tileMap, real32 testX, real32 testY) {
	// Map new location to a tile
	int playerTileX = truncateReal32ToInt32((testX - tileMap->upperLeftX) / tileMap->tileWidth);
	int playerTileY = truncateReal32ToInt32((testY - tileMap->upperLeftY) / tileMap->tileHeight);
	
	bool isEmpty = false;
	if (playerTileX >= 0  &&  playerTileX < tileMap->sizeX  && // make sure the player is actually on the tile map
		playerTileY >= 0  &&  playerTileY < tileMap->sizeY) 
	{
		uint32_t tileMapValue = getTileValue(tileMap, playerTileX, playerTileY);
		isEmpty = (tileMapValue == 0); // empty points are considered to have a values of 0 in our tile map
	}
	
	return isEmpty;
}

#define TILE_MAP_SIZE_X 17
#define TILE_MAP_SIZE_Y 9

// static void gameUpdateAndRender(ThreadContext* threadContext, GameMemory* memory, GameInput* input, GameImageBuffer* imageBuffer)
extern "C" GAME_UPDATE_AND_RENDER(gameUpdateAndRender) {
	
	GameState* gameState = (GameState*) memory->permanentStorage;

	// Initialize the game state and memory
	if (! memory->isInit) { 
		gameState->playerX = 150;
		gameState->playerY = 90;
		memory->isInit = true; 
	}
	
	// Tile map
	uint32_t tiles0[TILE_MAP_SIZE_Y][TILE_MAP_SIZE_X] = 
	{
		 { 1, 1, 1, 1,     1, 1, 1, 1,     0,     1, 1, 1, 1,     1, 1, 1, 1 },
		 { 1, 0, 1, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 0, 0, 1 },
		 { 1, 0, 0, 1,     1, 1, 1, 1,     0,     0, 1, 0, 0,     0, 0, 1, 1 },
		 { 1, 1, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 0, 0, 1 },
		 { 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 0, 0, 1 },
		 { 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 1, 1, 1 },
		 { 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 0, 0, 1 },
		 { 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 0, 0, 1 },
		 { 1, 1, 1, 1,     1, 1, 1, 1,     0,     1, 1, 1, 1,     1, 1, 1, 1 },
	};
	
	uint32_t tiles1[TILE_MAP_SIZE_Y][TILE_MAP_SIZE_X] = 
	{
		{ 1, 1, 1, 1,     1, 1, 1, 1,     0,     1, 1, 1, 1,     1, 1, 1, 1 },
		{ 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 0, 0, 0,     0, 0, 0, 1 },
		{ 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 0, 0, 0,     0, 0, 0, 1 },
		{ 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 0, 0, 0,     0, 0, 0, 1 },
		{ 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 0, 0, 0,     0, 0, 0, 1 },
		{ 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 0, 0, 0,     0, 0, 0, 1 },
		{ 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 0, 0, 0,     0, 0, 0, 1 },
		{ 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 0, 0, 0,     0, 0, 0, 1 },
		{ 1, 1, 1, 1,     1, 1, 1, 1,     0,     1, 1, 1, 1,     1, 1, 1, 1 },
	};

	// Our array of tile maps
	tilemap tileMaps[2];
	
	// Tile Map [0]
	tileMaps[0].sizeX = TILE_MAP_SIZE_X;
	tileMaps[0].sizeY = TILE_MAP_SIZE_Y;
	tileMaps[0].upperLeftX = -30;
	tileMaps[0].upperLeftY = 0;
	tileMaps[0].tileWidth = 50;
	tileMaps[0].tileHeight = 50;
	tileMaps[0].tiles = (uint32_t*) tiles0;
	
	// Tile Map [1]
	tileMaps[1] = tileMaps[0];
	tileMaps[1].tiles = (uint32_t*) tiles1;

	// Current tile map
	tilemap* tileMap = &tileMaps[0];
	
	// Obtain player dimensions
	real32 playerWidth = 0.75f * tileMap->tileWidth;
	real32 playerHeight = tileMap->tileHeight;
	
	// Handle game input
	for (int controllerIndex = 0; controllerIndex < arrayCount(input->controllers); ++controllerIndex) {
		
		GameControllerInput* controller = getController(input, controllerIndex);
		if (controller->isAnalog) { // controller input
			
		} else { // keyboard input

			// Figure out which direction the player moved
			real32 deltaPlayerX = 0.0f;
			real32 deltaPlayerY = 0.0f;
			if (controller->moveUp.endedDown) { deltaPlayerY = -1.0f; }
			if (controller->moveDown.endedDown) { deltaPlayerY = 1.0f; }
			if (controller->moveRight.endedDown) { deltaPlayerX = 1.0f; }
			if (controller->moveLeft.endedDown) { deltaPlayerX = -1.0f; }

			// Scale the delta
			deltaPlayerX *= 20.0f;
			deltaPlayerY *= 20.0f;

			// Put the player's new coordinates (old coordinates + input) in a temp value for testing
			real32 newPlayerX = gameState->playerX + input->deltaTimeForFrame * deltaPlayerX;
			real32 newPlayerY = gameState->playerY + input->deltaTimeForFrame * deltaPlayerY;

			// Only change actual location of the player if given valid coordinates
			if ((isTileMapPointEmpty(tileMap, newPlayerX - 0.5f*playerWidth, newPlayerY)) &&
			    (isTileMapPointEmpty(tileMap, newPlayerX + 0.5f*playerWidth, newPlayerY)) &&
			    (isTileMapPointEmpty(tileMap, newPlayerX, newPlayerY)))
			{
				gameState->playerX = newPlayerX;
				gameState->playerY = newPlayerY;
			}
		}
	}

	// White background
    drawRectangle(imageBuffer, 0.0f, 0.0f, (real32)imageBuffer->Width, (real32)imageBuffer->Height, 1.0f, 1.0f, 1.0f);
	
	// Display the tile map
	for (int y = 0; y < 9; ++y) {
		for (int x = 0; x < 17; ++x) {
			int tileIndex = getTileValue(tileMap, x, y);
			real32 minX = tileMap->upperLeftX + (tileMap->tileWidth * (real32)x);
			real32 minY = tileMap->upperLeftY + (tileMap->tileHeight * (real32)y);
			real32 maxX = minX + tileMap->tileWidth;
			real32 maxY = minY + tileMap->tileHeight;
			real32 tempColor = 0.5f;
			if (tileIndex == 1) { tempColor = 1.0f; }
			drawRectangle(imageBuffer, minX, minY, maxX, maxY, tempColor, tempColor, tempColor);
		}
	}

	// Obtain player coordinates 
	real32 playerLeft = gameState->playerX - (playerWidth * 0.5f);
	real32 playerTop = gameState->playerY - playerHeight;
	
	// Obtain player color
	real32 playerR = 1.0;
	real32 playerG = 1.0;
	real32 playerB = 0.0;

	// Draw the player
	drawRectangle(imageBuffer, playerLeft, playerTop, playerLeft + playerWidth, playerTop + playerHeight, playerR, playerG, playerB); 
}

extern "C" GAME_GET_SOUND_SAMPLES(gameGetSoundSamples) {
	GameState* gameState = (GameState*) memory->permanentStorage;
	GameOutputSound(gameState, soundBuffer, 400);
}

