#if !defined(HANDMADE_H)

/*
	NOTE:

	HANDMADE_INTERNAL:
	 0 - Build for public release
	 1 - Build for dev only

	HANDMADE_SLOW:
	 0 - No slow code allowed
	 1 - Slow code can be used
*/

#if HANDMADE_SLOW
	#define Assert(Expression) if (!(Expression)) { *(int*)0 = 0; }
#else
	#define Assert(Expression)
#endif

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes(Value) * 1024)
#define Terabytes(Value) (Gigabytes(Value) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof(Array[0]))
// TODO: Swap, min, max, ... macros?

inline uint32
SafeTruncateUInt64(uint64 Value)
{
	// TODO: Defines for max values (UInt32Max)
	Assert(Value <= 0xFFFFFFFF);
	uint32 Result = (uint32)Value;
	return Result;
}



/*
NOTE: Services that the platform layer provides to the game
*/

#if HANDMADE_INTERNAL
	// IMPORTANT: These are not for doing anything in the shipping game -
	// they are blocking and write doesn't protect against lost data
	struct debug_read_file_result
	{
		uint32 ContentsSize;
		void* Contents;
	};
	internal debug_read_file_result DEBUGPlatformReadEntireFile(char* Filename);
	internal bool32 DEBUGPlatformWriteEntireFile(char* Filename, uint32 MemorySize, void* Memory);
	internal void DEBUGPlatformFreeFileMemory(void* Memory);
#endif



/*
NOTE: Services that the game provices to the platform layer
*/
struct game_offscreen_buffer
{
	void* Memory;
	int Width;
	int Height;
	int Pitch;
};

struct game_sound_output_buffer
{
	int SamplesPerSecond;
	int SampleCount;
	int16* Samples;
};

struct game_button_state
{
	int HalfTransitionCount;
	bool32 EndedDown;
};

struct game_controller_input
{
	bool32 IsAnalog;

	real32 StartX;
	real32 StartY;

	real32 MinX;
	real32 MinY;

	real32 MaxX;
	real32 MaxY;

	real32 EndX;
	real32 EndY;

	union
	{
		game_button_state Buttons[6];
		struct
		{
			game_button_state Up;
			game_button_state Down;
			game_button_state Left;
			game_button_state Right;
			game_button_state LeftShoulder;
			game_button_state RightShoulder;
		};
	};
};

struct game_input
{
	// TODO: Insert clock value here
	game_controller_input Controllers[4];
};

struct game_memory
{
	bool32 IsInitialized;
	uint64 PermanentStorageSize;
	void* PermanentStorage; // NOTE: Required to be cleared to zero at startup

	uint64 TransientStorageSize;
	void* TransientStorage; // NOTE: Required to be cleared to zero at startup
};


internal void
GameUpdateAndRender(game_memory* Memory, game_input* Input, game_offscreen_buffer* Buffer,
					game_sound_output_buffer* SoundBuffer);


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

struct game_state
{
	int ToneHz;
	int GreenOffset;
	int BlueOffset;
};



#define HANDMADE_H
#endif