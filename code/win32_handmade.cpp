#include <dsound.h>
#include <malloc.h>
#include <stdio.h>
#include <windows.h>
#include <xinput.h>

#include "handmade.h"
#include "win32_handmade.h"

// TODO: This is a global variable for now
global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;


// NOTE: Support for XInputGet(Set)State
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return (ERROR_DEVICE_NOT_CONNECTED); }
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return (ERROR_DEVICE_NOT_CONNECTED); }
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name)                                  \
	HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, \
						LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);


DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
	if (Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}


DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
	debug_read_file_result Result = {};

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0,  OPEN_EXISTING, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		if (GetFileSizeEx(FileHandle, &FileSize))
		{
			uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
			Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if (Result.Contents)
			{
				DWORD BytesRead;
				if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
					(FileSize32 == BytesRead))
				{
					// NOTE: File read successfully
					Result.ContentsSize = FileSize32;
				}
				else
				{
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
				}
			}
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// TODO: Logging
	}

	return(Result);
}


DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
	bool32 Result = false;

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;
		if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
		{
			// NOTE: File read successfully
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			// TODO: Logging
		}

		CloseHandle(FileHandle);
	}
	else
	{
		// TODO: Logging
	}

	return(Result);
}


struct win32_game_code
{
	HMODULE GameCodeDLL;
	game_update_and_render* UpdateAndRender;
	game_get_sound_samples* GetSoundSamples;

	bool32 IsValid;
};

internal win32_game_code
Win32LoadGameCode()
{
	win32_game_code Result = {};

	// TODO: Need to get the proper path here!

	CopyFile("handmade.dll", "handmade_temp.dll", FALSE);
	Result.GameCodeDLL = LoadLibraryA("handmade_temp.dll");
	if (Result.GameCodeDLL)
	{
		Result.UpdateAndRender = (game_update_and_render*)GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
		Result.GetSoundSamples = (game_get_sound_samples*)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");

		Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
	}

	if (!Result.IsValid)
	{
		Result.UpdateAndRender = GameUpdateAndRenderStub;
		Result.GetSoundSamples = GameGetSoundSamplesStub;
	}

	return (Result);
}


internal void
Win32UnloadGameCode(win32_game_code* GameCode)
{
	if (GameCode->GameCodeDLL)
	{
		FreeLibrary(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}

	GameCode->IsValid = false;
	GameCode->UpdateAndRender = GameUpdateAndRenderStub;
	GameCode->GetSoundSamples = GameGetSoundSamplesStub;
}

internal void
Win32LoadXInput(void)
{
	// TODO: Test on Win 8
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (!XInputLibrary)
	{
		// TODO: Diagnostic
		XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	}
	if (!XInputLibrary)
	{
		// TODO: Diagnostic
		XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}

	if (XInputLibrary)
	{
		XInputGetState =
			(x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
		if (!XInputGetState)
		{
			XInputGetState = XInputGetStateStub;
		}
		XInputSetState =
			(x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
		if (!XInputSetState)
		{
			XInputSetState = XInputSetStateStub;
		}
	}
	else
	{
		// Diagnostic
	}
}


internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	// NOTE: Load the lib
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

	if (DSoundLibrary)
	{
		direct_sound_create* DirectSoundCreate =(direct_sound_create*)
			GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// Setting primary buffer
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
					}
					else
					{
						// TODO: Diagnostic
					}
				}
			}
			else
			{
				// TODO: Diagnostic
			}

			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			if (SUCCEEDED(DirectSound->CreateSoundBuffer(
					&BufferDescription, &GlobalSecondaryBuffer, 0)))
			{
			}
		}
		else
		{
			// TODO: Diagnostic
		}
	}
}


internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return (Result);
}


internal void
Win32ResizeDIBSection(win32_offscreen_buffer* Buffer, int Width, int Height)
{

	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;

	int BytesPerPixel = 4;
	Buffer->BytesPerPixel = BytesPerPixel;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height; // top-down bitmap, not bottom-up
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (Buffer->Width * Buffer->Height) * BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	Buffer->Pitch = Width * BytesPerPixel;
}


internal void
Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth,
						   int WindowHeight, win32_offscreen_buffer* Buffer)
{
	StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0,
				  Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info,
				  DIB_RGB_COLORS, SRCCOPY);
}


internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	LRESULT Result = 0;

	switch (Message)
	{
	case WM_DESTROY:
	{
		// TODO: Handle this as an error - recreate window?
		GlobalRunning = false;
	}
	break;

	case WM_CLOSE:
	{
		// TODO: Handle with a message to user?
		GlobalRunning = false;
	}
	break;

	case WM_ACTIVATEAPP:
	{
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	}
	break;

	case WM_PAINT:
	{
		PAINTSTRUCT Paint;
		HDC DeviceContext = BeginPaint(Window, &Paint);

		win32_window_dimension Dimension = Win32GetWindowDimension(Window);
		Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
								   &GlobalBackbuffer);

		EndPaint(Window, &Paint);
	}
	break;

	default:
	{
		Result = DefWindowProcA(Window, Message, WParam, LParam);
	}
	break;
	}

	return (Result);
}


internal void
Win32ClearBuffer(win32_sound_output* SoundOutput)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;

	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
											  &Region1, &Region1Size, &Region2,
											  &Region2Size, 0)))
	{
		uint8 *DestSample = (uint8 *)Region1;
		for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ByteIndex++)
		{
			*DestSample++ = 0;
		}

		DestSample = (uint8 *)Region2;
		for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ByteIndex++)
		{
			*DestSample++ = 0;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}


internal void
Win32FillSoundBuffer(win32_sound_output* SoundOutput,
								   DWORD ByteToLock, DWORD BytesToWrite,
								   game_sound_output_buffer* SourceBuffer)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;

	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1,
											  &Region1Size, &Region2,
											  &Region2Size, 0)))
	{
		int16* DestSample = (int16 *)Region1;
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		int16* SourceSample = SourceBuffer->Samples;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount;
			 SampleIndex++)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		DestSample = (int16 *)Region2;
		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount;
			 SampleIndex++)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}


internal void
Win32ProcessKeyboardMessage(game_button_state* NewState, bool32 IsDown)
{
	Assert(NewState->EndedDown != IsDown);
	NewState->EndedDown = IsDown;
	++NewState->HalfTransitionCount;
}


internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, DWORD ButtonBit,
								game_button_state* OldState,
								game_button_state* NewState)
{
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount =
		(OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadzoneThreshold)
{
	real32 Result = 0;
	if (Value < -DeadzoneThreshold)
	{
		Result = (real32)((Value + DeadzoneThreshold) / (32768.0f - DeadzoneThreshold));
	}
	else if (Value > DeadzoneThreshold)
	{
		Result = (real32)((Value - DeadzoneThreshold) / (32767.0f - DeadzoneThreshold));
	}

	return(Result);
}


internal void
Win32ProcessPendingMessages(game_controller_input* KeyboardController)
{
    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch(Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKCode = (uint32)Message.wParam;
                bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);
                if(WasDown != IsDown)
                {
                    if(VKCode == 'W')
                    {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if(VKCode == 'A')
                    {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if(VKCode == 'S')
                    {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if(VKCode == 'D')
                    {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    }
                    else if(VKCode == 'Q')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                    }
                    else if(VKCode == 'E')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    }
                    else if(VKCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    }
                    else if(VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                    }
                    else if(VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    }
                    else if(VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    }
                    else if(VKCode == VK_ESCAPE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
                    else if(VKCode == VK_SPACE)
                    {
						Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                    }
#if HANDMADE_INTERNAL
					else if (VKCode == 'P')
					{
						if (IsDown)
							GlobalPause = !GlobalPause;
					}
#endif
                }

                bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                if((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GlobalRunning = false;
                }
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }
    }
}

inline LARGE_INTEGER
Win32GetWallClock()
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return(Result);
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	real32 Result = ((real32)(End.QuadPart - Start.QuadPart)) / ((real32)GlobalPerfCountFrequency);
	return(Result);
}

internal void
Win32DebugDrawVertical(win32_offscreen_buffer* Backbuffer,
					   int X, int Top, int Bottom, uint32 Color)
{
	if (Top <= 0)
	{
		Top = 0;
	}

	if (Bottom > Backbuffer->Height)
	{
		Bottom = Backbuffer->Height;
	}

	if (X >= 0 && X < Backbuffer->Width)
		{
		uint8* Pixel = (uint8*)Backbuffer->Memory + X * Backbuffer->BytesPerPixel +
						Top * Backbuffer->Pitch;
		for (int y = Top; y < Bottom; ++y)
		{
			*(uint32*)Pixel = Color;
			Pixel += Backbuffer->Pitch;
		}
	}
}

inline void
Win32DrawSoundBufferMarker(win32_offscreen_buffer* Backbuffer, win32_sound_output* SoundOutput,
						   real32 C, int PadX, int Top, int Bottom, DWORD Value, uint32 Color)
{
	real32 XReal32 = C * (real32)Value;
	int X = PadX + (int)XReal32;
	Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);
}


internal void
Win32DebugSyncDisplay(win32_offscreen_buffer* Backbuffer, int MarkerCount,
					  win32_debug_time_marker* Markers, win32_sound_output* SoundOutput,
					  real32 TargetSecondsPerFrame, int CurrentMarkerIndex)
{
	int PadX = 16;
	int PadY = 16;

	int LineHeight = 64;

	real32 C = (real32)(Backbuffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
	for (int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
	{
		win32_debug_time_marker* ThisMarker = &Markers[MarkerIndex];

		DWORD PlayColor = 0xFFFFFFFF;
		DWORD WriteColor = 0xFFFF0000;

		int Top = PadY;
		int Bottom = PadY + LineHeight;
		if (MarkerIndex == CurrentMarkerIndex)
		{
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;

			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);

			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;

			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);

			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;
		}

		Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
		Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
	}
}


int CALLBACK
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	// NOTE: Set Windows scheduler granularity to 1ms
	UINT DesiredSchedulerMS = 1;
	bool32 SleepIsGranular = timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR;

	Win32LoadXInput();

	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

	WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	// TODO: Query on Windows
#define MonitorRefreshHz 60
#define GameUpdateHz (MonitorRefreshHz / 2)
	real32 TargetSecondsPerFrame = 1.f / (real32)MonitorRefreshHz;

	if (RegisterClassA(&WindowClass))
	{
		HWND Window = CreateWindowExA(0, WindowClass.lpszClassName, "Handmade Hero",
									  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
									  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
									  CW_USEDEFAULT, 0, 0, Instance, 0);
		if (Window)
		{
			HDC DeviceContext = GetDC(Window);

			win32_sound_output SoundOutput = {};

			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.BytesPerSample = sizeof(int16) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
			// Get rid of latency sample count
			SoundOutput.LatencySampleCount = 3 * (SoundOutput.SamplesPerSecond / GameUpdateHz);
			SoundOutput.SafetyBytes = ((SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) /
									    GameUpdateHz) / 3;
			Win32InitDSound(Window, SoundOutput.SamplesPerSecond,
							SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GlobalRunning = true;

			int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
												   MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);


#if HANDMADE_INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
			LPVOID BaseAddress = 0;
#endif
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Gigabytes(1);
			GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
			GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
			GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

			uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
			GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, (size_t)TotalSize,
							 						   MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			GameMemory.TransientStorage = (uint8*)GameMemory.PermanentStorage +
										          GameMemory.PermanentStorageSize;


			if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{
				game_input Input[2] = {};
				game_input* NewInput = &Input[0];
				game_input* OldInput = &Input[1];

				LARGE_INTEGER FlipWallClock = Win32GetWallClock();
				LARGE_INTEGER LastCounter = Win32GetWallClock();
				uint64 LastCycleCount = __rdtsc();

				int DebugTimeMarkerIndex = 0;
				win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {0};

				DWORD AudioLatencyBytes = 0;
				real32 AudioLatencySeconds = 0;
				bool32 SoundIsValid = false;

				win32_game_code Game = Win32LoadGameCode();
				uint32 LoadCounter = 0;

				while (GlobalRunning)
				{
					if (LoadCounter++ > 120)
					{
						Win32UnloadGameCode(&Game);
						Game = Win32LoadGameCode();
						LoadCounter = 0;
					}

					// TODO: Zeroing macro
					game_controller_input* OldKeyboardController = GetController(OldInput, 0);
					game_controller_input* NewKeyboardController = GetController(NewInput, 0);
					*NewKeyboardController = {};
					NewKeyboardController ->IsConnected = true;
					for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
					{
						NewKeyboardController->Buttons[ButtonIndex].EndedDown =
							OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}

					Win32ProcessPendingMessages(NewKeyboardController);

					DWORD MaxControllerCount = XUSER_MAX_COUNT;
					if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
					{
						MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
					}

					// TODO: Should we poll this more frequently
					for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount;
						 ++ControllerIndex)
					{
						DWORD OurControllerIndex = ControllerIndex + 1;

						game_controller_input* OldController =
							GetController(OldInput, OurControllerIndex);
						game_controller_input* NewController =
							GetController(NewInput, OurControllerIndex);

						XINPUT_STATE ControllerState;
						if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
						{
							NewController->IsConnected = true;

							// NOTE: This controller is plugged in
							XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;


							// NOTE: Deadzone handling
							NewController->StickAverageX =  Win32ProcessXInputStickValue(
								Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
							NewController->StickAverageY = Win32ProcessXInputStickValue(
								Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

							if (NewController->StickAverageX != 0.f ||
								NewController->StickAverageY != 0.f )
							{
								NewController->IsAnalog = true;
							}

							if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
							{
								NewController->StickAverageY = 1.f;
								NewController->IsAnalog = false;
							}
							if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
							{
								NewController->StickAverageY = -1.f;
								NewController->IsAnalog = false;
							}
							if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
							{
								NewController->StickAverageX = -1.f;
								NewController->IsAnalog = false;
							}
							if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
							{
								NewController->StickAverageX = 1.f;
								NewController->IsAnalog = false;
							}

							real32 Threshold = 0.5f;
							Win32ProcessXInputDigitalButton(
								((NewController->StickAverageX < -Threshold) ? 1 : 0), 1,
								&OldController->MoveLeft, &NewController->MoveLeft);
							Win32ProcessXInputDigitalButton(
								((NewController->StickAverageX > Threshold) ? 1 : 0), 1,
								&OldController->MoveRight, &NewController->MoveRight);
							Win32ProcessXInputDigitalButton(
								((NewController->StickAverageY < -Threshold) ? 1 : 0), 1,
								&OldController->MoveUp, &NewController->MoveUp);
							Win32ProcessXInputDigitalButton(
								((NewController->StickAverageY > Threshold) ? 1 : 0), 1,
								&OldController->MoveDown, &NewController->MoveDown);

							Win32ProcessXInputDigitalButton(
								Pad->wButtons, XINPUT_GAMEPAD_A,
								&OldController->ActionDown, &NewController->ActionDown);
							Win32ProcessXInputDigitalButton(
								Pad->wButtons, XINPUT_GAMEPAD_B,
								&OldController->ActionRight, &NewController->ActionRight);
							Win32ProcessXInputDigitalButton(
								Pad->wButtons, XINPUT_GAMEPAD_X,
								&OldController->ActionLeft, &NewController->ActionLeft);
							Win32ProcessXInputDigitalButton(
								Pad->wButtons, XINPUT_GAMEPAD_Y,
								&OldController->ActionUp, &NewController->ActionUp);
							Win32ProcessXInputDigitalButton(
								Pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER,
								&OldController->LeftShoulder, &NewController->LeftShoulder);
							Win32ProcessXInputDigitalButton(
								Pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER,
								&OldController->RightShoulder, &NewController->RightShoulder);
							Win32ProcessXInputDigitalButton(
								Pad->wButtons, XINPUT_GAMEPAD_START,
								&OldController->Start, &NewController->Start);
							Win32ProcessXInputDigitalButton(
								Pad->wButtons, XINPUT_GAMEPAD_BACK,
								&OldController->Back, &NewController->Back);
						}
						else
						{
							// NOTE: The controller is not available
							NewController->IsConnected = false;
						}
					}

					if (!GlobalPause)
					{
						game_offscreen_buffer Buffer = {};
						Buffer.Memory = GlobalBackbuffer.Memory;
						Buffer.Width = GlobalBackbuffer.Width;
						Buffer.Height = GlobalBackbuffer.Height;
						Buffer.Pitch = GlobalBackbuffer.Pitch;
						Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);

						LARGE_INTEGER AudioWallClock = Win32GetWallClock();
						real32 FromBeginToAudioSeconds = 1000.f * Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

						DWORD PlayCursor;
						DWORD WriteCursor;
						if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
						{
							if (!SoundIsValid)
							{
								SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
								SoundIsValid = true;
							}

							DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
												SoundOutput.SecondaryBufferSize;

							DWORD ExpectedSoundBytesPerFrame =
								(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz;
							real32 SecondsLeftUntilFlip = TargetSecondsPerFrame - FromBeginToAudioSeconds;
							DWORD ExpectedBytesUntilFlip =
								(DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame);

							DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;

							DWORD SafeWriteCursor = WriteCursor;
							if (SafeWriteCursor < PlayCursor)
							{
								SafeWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							Assert(SafeWriteCursor >= PlayCursor);
							SafeWriteCursor += SoundOutput.SafetyBytes;

							bool32 AudioCardIsLowLatency = SafeWriteCursor < ExpectedFrameBoundaryByte;

							DWORD TargetCursor = 0;
							if (AudioCardIsLowLatency)
							{
								TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
							}
							else
							{
								TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame +
										   	   SoundOutput.SafetyBytes;
							}
							TargetCursor = TargetCursor % SoundOutput.SecondaryBufferSize;

							DWORD BytesToWrite = 0;
							if (ByteToLock > TargetCursor)
							{
								BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
								BytesToWrite += TargetCursor;
							}
							else
							{
								BytesToWrite = TargetCursor - ByteToLock;
							}

							game_sound_output_buffer SoundBuffer = {};
							SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
							SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
							SoundBuffer.Samples = Samples;
							Game.GetSoundSamples(&GameMemory, &SoundBuffer);

#if HANDMADE_INTERNAL
							win32_debug_time_marker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
							Marker->OutputPlayCursor = PlayCursor;
							Marker->OutputWriteCursor = WriteCursor;
							Marker->OutputLocation = ByteToLock;
							Marker->OutputByteCount = BytesToWrite;

							DWORD UnwrappedWriteCursor = WriteCursor;
							if (UnwrappedWriteCursor < PlayCursor)
							{
								UnwrappedWriteCursor = SoundOutput.SecondaryBufferSize;
							}
							AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
							AudioLatencySeconds =
								((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) /
								 (real32)SoundOutput.SamplesPerSecond;

                        	char TextBuffer[256];
                        	_snprintf_s(TextBuffer, sizeof(TextBuffer),
                        	            "BTL:%u TC:%u BTW:%u - PC:%u WC:%u DELTA:%u (%fs)\n",
                        	            ByteToLock, TargetCursor, BytesToWrite,
                        	            PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
                        	OutputDebugStringA(TextBuffer);
#endif
							Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
						}
						else
						{
							SoundIsValid = false;
						}

						LARGE_INTEGER WorkCounter = Win32GetWallClock();
						real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

						real32 SecondsElapsedForFrame = WorkSecondsElapsed;
						if (SecondsElapsedForFrame < TargetSecondsPerFrame)
						{
							if (SleepIsGranular)
							{
								DWORD SleepMs = (DWORD)(1000.f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
								if (SleepMs > 0)
									Sleep(SleepMs);
							}

							while (SecondsElapsedForFrame < TargetSecondsPerFrame)
							{

								SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter,
																				Win32GetWallClock());
							}
						}
						else
						{
							// TODO: Missed frame rate!
							// TODO: Logging
						}

						LARGE_INTEGER EndCounter = Win32GetWallClock();
						real64 MSPerFrame = 1000.f * Win32GetSecondsElapsed(LastCounter, EndCounter);
						LastCounter = EndCounter;

						win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
						Win32DebugSyncDisplay(&GlobalBackbuffer, ArrayCount(DebugTimeMarkers),
										  	DebugTimeMarkers, &SoundOutput,
										  	TargetSecondsPerFrame, DebugTimeMarkerIndex - 1);
#endif
						Win32DisplayBufferInWindow(DeviceContext, Dimension.Width,
											   	   Dimension.Height, &GlobalBackbuffer);

						FlipWallClock = Win32GetWallClock();

#if HANDMADE_INTERNAL
						// NOTE: Debug code
						{
							DWORD PlayCursor;
							DWORD WriteCursor;
							if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
							{
								Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
								win32_debug_time_marker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
								Marker->FlipPlayCursor = PlayCursor;
								Marker->FlipWriteCursor = WriteCursor;
							}
						}
#endif

						game_input* Temp = NewInput;
						NewInput = OldInput;
						OldInput = Temp;

						uint64 EndCycleCount = __rdtsc();
						uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
						LastCycleCount = EndCycleCount;

#if 1
						real64 FPS = 0.f; //(real64)GlobalPerfCountFrequency / (real64)CounterElapsed;
						real64 MCPF = (real64)CyclesElapsed / (1000.f * 1000.f);

						char FPSBuffer[256];
						_snprintf_s(FPSBuffer, sizeof(FPSBuffer),
						    	"%.01fms/f, %.01f/s, %.01fmc/f \n", MSPerFrame, FPS, MCPF);
						OutputDebugStringA(FPSBuffer);
#endif

#if HANDMADE_INTERNAL
						++DebugTimeMarkerIndex;
						if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers))
							DebugTimeMarkerIndex = 0;
#endif
					}
				}
			}
		}
		else
		{
			// TODO: Logging
		}
	}
	else
	{
		// TODO: Logging
	}

	return (0);
}
