
#include "handmade.h"

// Test code for displaying a gradient
static void
RenderWeirdGradient(GameImageBuffer* buffer, int xOffset, int yOffset)
{
	uint8_t* row = (uint8_t*) buffer->BitmapMemory;
	for (int y = 0; y < buffer->Height; ++y) {

		uint32_t* pixel = (uint32_t*) row;
		for (int x = 0; x < buffer->Width; ++x) {
			//   Memory:   BB GG RR XX
			//   Register: XX RR GG BB
			uint8_t blue = (x + xOffset);
			uint8_t green = (y + yOffset);
			uint8_t red;
			*pixel++ = ((green << 8) | blue);
		}

		row += buffer->Pitch;
	}
}

static void GameOutputSound(GameSoundBuffer* SoundBuffer) {
	static real32 tSine;
	int16_t toneVolume = 3000;
	int toneHertz = 256;
	int wavePeriod = SoundBuffer->samplesPerSecond / toneHertz;
	int16_t* sampleOut = SoundBuffer->samples;

	// Loop through the first region to write to the buffer, stereo sound is encoded as pairs of 16bit values (left, right)
	for (DWORD SampleIndex = 0; SampleIndex < SoundBuffer->sampleCount; ++SampleIndex) {
		real32 sineValue = sinf(tSine);
		int16_t sampleValue = (int16_t) (sineValue * toneVolume);
		*sampleOut++ = sampleValue;
		*sampleOut++ = sampleValue;
		tSine += 2.0f * PI * 1.0f / (real32) wavePeriod;
	}
}

static void gameUpdateAndRender(GameImageBuffer* imageBuffer, int xOffset, int yOffset, GameSoundBuffer* soundBuffer) {
	GameOutputSound(soundBuffer);
	RenderWeirdGradient(imageBuffer, xOffset, yOffset);
}
