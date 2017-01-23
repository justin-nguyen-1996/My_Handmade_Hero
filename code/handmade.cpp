
#include "handmade.h"

static void RenderWeirdGradient(GameImageBuffer* buffer, int xOffset, int yOffset)
{
	uint8_t* row = (uint8_t*) buffer->BitmapMemory;
	for (int y = 0; y < buffer->Height; ++y) {

		uint32_t* pixel = (uint32_t*) row;
		for (int x = 0; x < buffer->Width; ++x) {
			//   Memory:   BB GG RR XX
			//   Register: XX RR GG BB
			uint8_t blue = (uint8_t) (x + xOffset);
			uint8_t green = (uint8_t) (y + yOffset);
			uint8_t red;
			*pixel++ = ((green << 8) | blue);
		}

		row += buffer->Pitch;
	}
}

static void GameOutputSound(GameSoundBuffer* SoundBuffer, int toneHertz) {
	static real32 tSine;
	int16_t toneVolume = 3000;
	int wavePeriod = SoundBuffer->samplesPerSecond / toneHertz;
	int16_t* sampleOut = SoundBuffer->samples;

	// Loop through the first region to write to the buffer, stereo sound is encoded as pairs of 16bit values (left, right)
	for (int SampleIndex = 0; SampleIndex < SoundBuffer->sampleCount; ++SampleIndex) {
		real32 sineValue = sinf(tSine);
		int16_t sampleValue = (int16_t) (sineValue * toneVolume);
		*sampleOut++ = sampleValue;
		*sampleOut++ = sampleValue;
		tSine += 2.0f * PI * 1.0f / (real32) wavePeriod;
	}
}

static void gameUpdateAndRender(GameMemory* memory,
								GameInput* input, 
								GameImageBuffer* imageBuffer, 
								GameSoundBuffer* soundBuffer) 
{
	GameState* gameState = (GameState*) memory->permanentStorage;

	// Initialize the game state and memory
	if (! memory->isInit) {
		char* fileName = __FILE__; // TODO: temp
		DebugReadFile file = DEBUG_Platform_readEntireFile(fileName);
		if (file.contents) { 
			DEBUG_Platform_writeEntireFile("../data/test.out", file.contentSize, file.contents);
			DEBUG_Platform_freeFileMemory(file.contents); 
		}
		gameState->toneHertz = 256;
		memory->isInit = true;
	}
	
	// Handle game input
	for (int controllerIndex = 0; controllerIndex < arrayCount(input->controllers); ++controllerIndex) {
		GameControllerInput* controller = getController(input, controllerIndex);
		
		if (controller->isAnalog) {
			gameState->blueOffset += (int) (4.0f * controller->stickAverageX);
			gameState->toneHertz = 256 + (int) (128.0f * controller->stickAverageY);
		} else {
			if (controller->moveLeft.endedDown)       { gameState->blueOffset -= 1; } 
			else if (controller->moveRight.endedDown) { gameState->blueOffset += 1; }
		}

		if (controller->actionDown.endedDown) {
			gameState->greenOffset += 1;
		}	
	}
	
	GameOutputSound(soundBuffer, gameState->toneHertz);
	RenderWeirdGradient(imageBuffer, gameState->blueOffset, gameState->greenOffset);
}
