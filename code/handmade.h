#ifndef HANDMADE_H

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

// Services the game provides to the platform layer
static void gameUpdateAndRender(GameImageBuffer* imageBuffer, int xOffset, int yOffset, GameSoundBuffer* soundBuffer);

// Services the game provides to the platform layer

#define HANDMADE_H
#endif
