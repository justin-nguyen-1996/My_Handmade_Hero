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

struct GameButtonState {
	int halfTransitionCount;
	bool endedDown;
};

struct GameControllerInput {
	
	bool isAnalog;
	
	real32 startX; real32 endX;
	real32 startY; real32 endY;
	real32 minX; real32 maxX;
	real32 minY; real32 maxY;
	
	union {
		GameButtonState buttons[6];
		struct {
			GameButtonState up;
			GameButtonState down;
			GameButtonState right;
			GameButtonState left;
			GameButtonState leftShoulder;
			GameButtonState rightShoulder;
		};
	};
};

struct GameInput {
	GameControllerInput controllers[4];
};

// Services the game provides to the platform layer
static void gameUpdateAndRender(GameInput*       input,
								GameImageBuffer* imageBuffer,
								GameSoundBuffer* soundBuffer);

// Services the game provides to the platform layer

#define HANDMADE_H
#endif
