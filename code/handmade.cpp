
#include "handmade.h"

static int32_t roundReal32ToUInt32(real32 val) {
	uint32_t res = (uint32_t) (val + 0.5f);
	return res;
}

static int32_t roundReal32ToInt32(real32 val) {
	int32_t res = (int32_t) (val + 0.5f);
	return res;
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
	if (maxY > buffer->Height) { maxY = buffer->Width; }
	
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

extern "C" GAME_UPDATE_AND_RENDER(gameUpdateAndRender) {
	
	GameState* gameState = (GameState*) memory->permanentStorage;

	// Initialize the game state and memory
	if (! memory->isInit) { memory->isInit = true; }
	
	// Handle game input
	for (int controllerIndex = 0; controllerIndex < arrayCount(input->controllers); ++controllerIndex) {
		
		GameControllerInput* controller = getController(input, controllerIndex);
		if (controller->isAnalog) { // controller input
			
		} else { // keyboard input
			
		}
	}

	// Tile map
	uint32_t tileMap[9][16] = 
	{
	     { 0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0 },
	     { 0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0 },
	     { 0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0 },
	     { 0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0 },
	     { 0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0 },
	     { 0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0 },
	     { 0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0 },
	     { 0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0 },
	     { 0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0 },
	}
	
	// Draw some temp rectangles
	drawRectangle(imageBuffer, 0, 0, (real32)imageBuffer->Width, (real32)imageBuffer->Height, 0.5f, 1.0f, 0.5f);
	drawRectangle(imageBuffer, 10.0f, 10.0f, 40.0f, 40.0f, 1.0, 0.0, 0.0);
}

extern "C" GAME_GET_SOUND_SAMPLES(gameGetSoundSamples) {
	GameState* gameState = (GameState*) memory->permanentStorage;
	GameOutputSound(gameState, soundBuffer, 400);
}

