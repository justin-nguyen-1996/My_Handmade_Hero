
#include "handmade.h"

// Test code for displaying a gradient
static void
RenderWeirdGradient(gameBuffer* buffer, int xOffset, int yOffset)
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

static void gameUpdateAndRender(gameBuffer* buffer, int xOffset, int yOffset) {
	RenderWeirdGradient(buffer, xOffset, yOffset);
}
