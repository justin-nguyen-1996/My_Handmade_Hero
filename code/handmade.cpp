
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

struct tilemap {
	int sizeX;
	int sizeY;
	real32 upperLeftX;
	real32 upperLeftY;
	real32 tileWidth;
	real32 tileHeight;
	uint32_t* tiles;
};

static bool isTileMapPointEmpty(tilemap* tileMap, real32 testX, real32 testY) {
	// Map new location to a tile
	int playerTileX = truncateReal32ToInt32((testX - tileMap->upperLeftX) / tileMap->tileWidth);
	int playerTileY = truncateReal32ToInt32((testY - tileMap->upperLeftY) / tileMap->tileHeight);
	
	bool isEmpty = false;
	if (playerTileX >= 0  &&  playerTileX < tileMap->sizeX  &&
		playerTileY >= 0  &&  playerTileY < tileMap->sizeY) 
	{
		int pitch = playerTileY*tileMap->sizeX;
		uint32_t tileMapValue = tileMap->tiles[pitch][playerTileX];
		isEmpty = (tileMapValue == 0);
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
	uint32_t tileMap[TILE_MAP_SIZE_Y][TILE_MAP_SIZE_X] = 
	{
		 { 1, 0, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     1, 0, 0, 1 },
		 { 1, 0, 1, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 0, 0, 0 },
		 { 1, 0, 0, 1,     1, 1, 1, 1,     0,     0, 1, 0, 0,     0, 0, 1, 0 },
		 { 1, 1, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 0, 0, 1 },
		 { 0, 0, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 0, 0, 0 },
		 { 0, 0, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 1, 1, 0 },
		 { 0, 0, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 0, 0, 0 },
		 { 0, 0, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 0,     0, 0, 0, 0 },
		 { 0, 0, 0, 0,     0, 0, 0, 0,     0,     0, 1, 0, 1,     0, 1, 0, 0 },
	};

	// Some tile map vars
	real32 upperLeftX = -30; real32 upperLeftY = 0;
	real32 tileWidth = 50; real32 tileHeight = 50;
	
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

			// Check if the player is in a valid tile location (not out of bounds and not on another object)
			real32 newPlayerX = gameState->playerX + input->deltaTimeForFrame * deltaPlayerX;
			real32 newPlayerY = gameState->playerY + input->deltaTimeForFrame * deltaPlayerY;
			bool isValid = isTileMapPointEmpty(tileMap, newPlayerX, newPlayerY);

			// Only change actual location of the player if given valid coordinates
			if (isValid) {
				gameState->playerX = newPlayerX;
				gameState->playerY = newPlayerY;
			}
		}
	}

	// White background
    drawRectangle(imageBuffer, 0.0f, 0.0f, (real32)imageBuffer->Width, (real32)imageBuffer->Height, 1.0f, 1.0f, 1.0f);
	
	// Display the tile map
	for (int row = 0; row < 9; ++row) {
		for (int col = 0; col < 17; ++col) {
			int tileIndex = tileMap[row][col];
			real32 minX = upperLeftX + (tileWidth * (real32)col);
			real32 minY = upperLeftY + (tileHeight * (real32)row);
			real32 maxX = minX + tileWidth;
			real32 maxY = minY + tileHeight;
			real32 tempColor = 0.5f;
			if (tileIndex == 1) { tempColor = 1.0f; }
			drawRectangle(imageBuffer, minX, minY, maxX, maxY, tempColor, tempColor, tempColor);
		}
	}

	// Obtain player coordinates 
	real32 playerWidth = 0.75f * tileWidth;
	real32 playerHeight = tileHeight;
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

