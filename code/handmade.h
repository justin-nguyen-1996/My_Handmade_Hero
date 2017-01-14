#ifndef HANDMADE_H

struct gameBuffer {
	void* BitmapMemory;
	int Width;
	int Height;
	int Pitch;
};

// Services the game provides to the platform layer
static void gameUpdateAndRender(gameBuffer* buffer, int xOffset, int yOffset);

// Services the game provides to the platform layer

#define HANDMADE_H
#endif
