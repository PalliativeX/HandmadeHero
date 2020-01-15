#include "handmade.h"
#include "handmade_tile.cpp"
#include "handmade_random.h"

internal void
GameOutputSound(game_state* GameState, game_sound_output_buffer* SoundBuffer, int ToneHz)
{
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

    int16* SampleOut = SoundBuffer->Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; SampleIndex++)
    {
#if 0
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
 #else
        int16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

#if 0
        GameState->tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
        if (GameState->tSine > 2.f * Pi32)
        {
            GameState->tSine -= 2.f * Pi32;
        }
#endif
    }
}


internal void
DrawRectangle(game_offscreen_buffer* Buffer, v2 vMin, v2 vMax, real32 R, real32 G, real32 B)
{
    int32 MinX = RoundReal32ToInt32(vMin.X);
    int32 MinY = RoundReal32ToInt32(vMin.Y);
    int32 MaxX = RoundReal32ToInt32(vMax.X);
    int32 MaxY = RoundReal32ToInt32(vMax.Y);

    if(MinX < 0)
    {
        MinX = 0;
    }
    if(MinY < 0)
    {
        MinY = 0;
    }
    if(MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }
    if(MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }

    uint32 Color = (RoundReal32ToUInt32(R * 255.f) << 16) |
                   (RoundReal32ToUInt32(G * 255.f) << 8) |
                   (RoundReal32ToUInt32(B * 255.f) << 0);

    uint8* Row = (uint8*)Buffer->Memory + MinX*Buffer->BytesPerPixel + MinY*Buffer->Pitch;
    for(int Y = MinY; Y < MaxY; ++Y)
    {
        uint32* Pixel = (uint32*)Row;
        for(int X = MinX; X < MaxX; ++X)
        {
            *Pixel++ = Color;
        }

        Row += Buffer->Pitch;
    }
}


internal void
DrawBitmap(game_offscreen_buffer* Buffer, loaded_bitmap* Bitmap,
           real32 RealX, real32 RealY,
           int32 AlignX = 0, int32 AlignY = 0)
{
    RealX -= (real32)AlignX;
    RealY -= (real32)AlignY;

    int32 MinX = RoundReal32ToInt32(RealX);
    int32 MinY = RoundReal32ToInt32(RealY);
    int32 MaxX = RoundReal32ToInt32(RealX + (real32)Bitmap->Width);
    int32 MaxY = RoundReal32ToInt32(RealY + (real32)Bitmap->Height);

    int32 SourceOffsetX = 0;
    if(MinX < 0)
    {
        SourceOffsetX = -MinX;
        MinX = 0;
    }

    int32 SourceOffsetY = 0;
    if(MinY < 0)
    {
        SourceOffsetY = -MinY;
        MinY = 0;
    }

    if(MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }

    if(MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }

    uint32* SourceRow = Bitmap->Pixels + Bitmap->Width*(Bitmap->Height - 1);
    SourceRow += -SourceOffsetY*Bitmap->Width + SourceOffsetX;
    uint8* DestRow = (uint8*)Buffer->Memory +
                      MinX*Buffer->BytesPerPixel +
                      MinY*Buffer->Pitch;
    for (int Y = MinY; Y < MaxY; Y++)
    {
        uint32* Dest = (uint32*)DestRow;
        uint32* Source = SourceRow;
        for (int X = MinX; X < MaxX; X++)
        {
            real32 A = (real32)((*Source >> 24) & 0xFF) / 255.0f;
            real32 SR = (real32)((*Source >> 16) & 0xFF);
            real32 SG = (real32)((*Source >> 8) & 0xFF);
            real32 SB = (real32)((*Source >> 0) & 0xFF);

            real32 DR = (real32)((*Dest >> 16) & 0xFF);
            real32 DG = (real32)((*Dest >> 8) & 0xFF);
            real32 DB = (real32)((*Dest >> 0) & 0xFF);

            real32 R = (1.0f-A)*DR + A*SR;
            real32 G = (1.0f-A)*DG + A*SG;
            real32 B = (1.0f-A)*DB + A*SB;

            *Dest = (((uint32)(R + 0.5f) << 16) |
                     ((uint32)(G + 0.5f) << 8) |
                     ((uint32)(B + 0.5f) << 0));

            ++Dest;
            ++Source;
        }

        DestRow += Buffer->Pitch;
        SourceRow -= Bitmap->Width;
    }
}


#pragma pack(push, 1)
struct bitmap_header
{
    uint16 FileType;
    uint32 FileSize;
    uint16 Reserved1;
    uint16 Reserved2;
    uint32 BitmapOffset;
    uint32 Size;
    int32 Width;
    int32 Height;
    uint16 Planes;
    uint16 BitsPerPixel;
    uint32 Compression;
    uint32 SizeOfBitmap;
    int32 HorizResolution;
    int32 VertResolution;
    uint32 ColorsUsed;
    uint32 ColorsImportant;

    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;
};
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP(thread_context* Thread, debug_platform_read_entire_file* ReadEntireFile, char* FileName)
{
    loaded_bitmap Result = {};

    // NOTE: Byte order in memory is AA BB GG RR, bottom up
    // In little endian -> 0xRRGGBBAA

    debug_read_file_result ReadResult = ReadEntireFile(Thread, FileName);
    if (ReadResult.ContentsSize != 0)
    {
        bitmap_header* Header = (bitmap_header*)ReadResult.Contents;
        uint32* Pixels = (uint32*)((uint8*)ReadResult.Contents + Header->BitmapOffset);
        Result.Pixels = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;

        Assert(Header->Compression == 3);

        // NOTE: Byte order in memory is determined by the Header itself,
        // so we have to read out the masks and convert the pixels ourselves
        uint32 RedMask = Header->RedMask;
        uint32 GreenMask = Header->GreenMask;
        uint32 BlueMask = Header->BlueMask;
        uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

        bit_scan_result RedShift = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenShift = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueShift = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaShift = FindLeastSignificantSetBit(AlphaMask);

        Assert(RedShift.Found);
        Assert(GreenShift.Found);
        Assert(BlueShift.Found);
        Assert(AlphaShift.Found);

        uint32* SourceDest = Pixels;
        for (int32 Y = 0; Y < Header->Height; Y++)
        {
            for (int32 X = 0; X < Header->Width; X++)
            {
                uint32 C = *SourceDest;
                *SourceDest++ = ((((C >> AlphaShift.Index) & 0xFF) << 24) |
                                 (((C >> RedShift.Index)   & 0xFF) << 16) |
                                 (((C >> GreenShift.Index) & 0xFF) << 8)  |
                                 (((C >> BlueShift.Index)  & 0xFF) << 0));
            }
        }
    }

    return (Result);
}


extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           (ArrayCount(Input->Controllers[0].Buttons)));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    real32 PlayerHeight = 1.4f;
    real32 PlayerWidth = 0.75f * PlayerHeight;

    game_state* GameState = (game_state*)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        GameState->Backdrop =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_background.bmp");

        hero_bitmaps *Bitmap;

        Bitmap = GameState->HeroBitmaps;
        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_right_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_right_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_right_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;
        ++Bitmap;

        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_back_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_back_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_back_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;
        ++Bitmap;

        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_left_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_left_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_left_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;
        ++Bitmap;

        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_front_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_front_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "C:/dev/handmade/data/test/test_hero_front_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;
        ++Bitmap;

        GameState->CameraP.AbsTileX = 17/2;
        GameState->CameraP.AbsTileY = 9/2;

        GameState->PlayerP.AbsTileX = 1;
        GameState->PlayerP.AbsTileY = 3;
        GameState->PlayerP.Offset.X = 5.0f;
        GameState->PlayerP.Offset.Y = 5.0f;

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8*)Memory->PermanentStorage + sizeof(game_state));

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world* World = GameState->World;
        World->TileMap = PushStruct(&GameState->WorldArena, tile_map);

        tile_map* TileMap = World->TileMap;

        TileMap->ChunkShift = 4;
        TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
        TileMap->ChunkDim = (1 << TileMap->ChunkShift);

        TileMap->TileChunkCountX = 128;
        TileMap->TileChunkCountY = 128;
        TileMap->TileChunkCountZ = 2;
        TileMap->TileChunks = PushArray(&GameState->WorldArena,
                                        TileMap->TileChunkCountX*
                                        TileMap->TileChunkCountY*
                                        TileMap->TileChunkCountZ,
                                        tile_chunk);

        TileMap->TileSideInMeters = 1.4f;

        uint32 RandomNumberIndex = 0;
        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;
        uint32 ScreenX = 0;
        uint32 ScreenY = 0;
        uint32 AbsTileZ = 0;

        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;

        for (uint32 ScreenIndex = 0; ScreenIndex < 100; ++ScreenIndex)
        {
            // TODO: Make a random number generator!
            Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));

            uint32 RandomChoice;
            if (DoorUp || DoorDown)
            {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
            }
            else
            {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
            }

            bool32 CreatedZDoor = false;
            if (RandomChoice == 2)
            {
                CreatedZDoor = true;
                if (AbsTileZ == 0)
                {
                    DoorUp = true;
                }
                else
                {
                    DoorDown = true;
                }
            }
            else if (RandomChoice == 1)
            {
                DoorRight = true;
            }
            else
            {
                DoorTop = true;
            }

            for (uint32 TileY = 0; TileY < TilesPerHeight; ++TileY)
            {
                for (uint32 TileX = 0; TileX < TilesPerWidth; ++TileX)
                {
                    uint32 AbsTileX = ScreenX*TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY*TilesPerHeight + TileY;

                    uint32 TileValue = 1;
                    if ((TileX == 0) && (!DoorLeft || (TileY != (TilesPerHeight/2))))
                    {
                        TileValue = 2;
                    }

                    if ((TileX == (TilesPerWidth - 1)) && (!DoorRight || (TileY != (TilesPerHeight/2))))
                    {
                        TileValue = 2;
                    }

                    if ((TileY == 0) && (!DoorBottom || (TileX != (TilesPerWidth/2))))
                    {
                        TileValue = 2;
                    }

                    if ((TileY == (TilesPerHeight - 1)) && (!DoorTop || (TileX != (TilesPerWidth/2))))
                    {
                        TileValue = 2;
                    }

                    if ((TileX == 10) && (TileY == 6))
                    {
                        if (DoorUp)
                        {
                            TileValue = 3;
                        }

                        if (DoorDown)
                        {
                            TileValue = 4;
                        }
                    }

                    SetTileValue(&GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY, AbsTileZ,
                                 TileValue);
                }
            }

            DoorLeft = DoorRight;
            DoorBottom = DoorTop;

            if (CreatedZDoor)
            {
                DoorDown = !DoorDown;
                DoorUp = !DoorUp;
            }
            else
            {
                DoorUp = false;
                DoorDown = false;
            }

            DoorRight = false;
            DoorTop = false;

            if (RandomChoice == 2)
            {
                if (AbsTileZ == 0)
                {
                    AbsTileZ = 1;
                }
                else
                {
                    AbsTileZ = 0;
                }
            }
            else if (RandomChoice == 1)
            {
                ScreenX += 1;
            }
            else
            {
                ScreenY += 1;
            }
        }

        Memory->IsInitialized = true;
    }

    world* World = GameState->World;
    tile_map* TileMap = World->TileMap;

    int32 TileSideInPixels = 60;
    real32 MetersToPixels = (real32)TileSideInPixels / (real32)TileMap->TileSideInMeters;

    real32 LowerLeftX = -(real32)TileSideInPixels / 2;
    real32 LowerLeftY = (real32)Buffer->Height;

    for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ControllerIndex++)
    {
        game_controller_input* Controller = GetController(Input, ControllerIndex);
        if (Controller->IsAnalog)
        {
            // NOTE: Use analog movement tuning
        }
        else
        {
            // NOTE: Use digital movement tuning
            v2 ddPlayer = {};

            // TODO: Measure timing
            if (Controller->MoveUp.EndedDown)
            {
                GameState->HeroFacingDirection = 1;
                ddPlayer.Y = 1.f;
            }
            if (Controller->MoveDown.EndedDown)
            {
                GameState->HeroFacingDirection = 3;
                ddPlayer.Y = -1.f;
            }
            if (Controller->MoveLeft.EndedDown)
            {
                GameState->HeroFacingDirection = 2;
                ddPlayer.X = -1.f;
            }
            if (Controller->MoveRight.EndedDown)
            {
                GameState->HeroFacingDirection = 0;
                ddPlayer.X = 1.f;
            }
            real32 PlayerSpeed = 10.f;
            if (Controller->ActionUp.EndedDown)
            {
                PlayerSpeed = 30.f;
            }
            ddPlayer *= PlayerSpeed; // NOTE: Both in meters

            if ((ddPlayer.X != 0.f) && (ddPlayer.Y != 0.f))
            {
                ddPlayer *= 0.707106781187f;
            }

            // TODO: ODE
            ddPlayer += 1.5f * -GameState->dPlayerP;

            tile_map_position NewPlayerP = GameState->PlayerP;
            NewPlayerP.Offset = 0.5f*ddPlayer*Square(Input->dtForFrame) +
                                GameState->dPlayerP*Input->dtForFrame +
                                NewPlayerP.Offset;
            GameState->dPlayerP = ddPlayer*Input->dtForFrame + GameState->dPlayerP;
            NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);

            tile_map_position PlayerLeft = NewPlayerP;
            PlayerLeft.Offset.X -= 0.5f*PlayerWidth;
            PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

            tile_map_position PlayerRight = NewPlayerP;
            PlayerRight.Offset.X += 0.5f*PlayerWidth;
            PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);

            bool32 Collided = false;
            tile_map_position ColP = {};
            if (!IsTileMapPointEmpty(TileMap, NewPlayerP))
            {
                ColP = NewPlayerP;
                Collided = true;
            }
            if (!IsTileMapPointEmpty(TileMap, PlayerLeft))
            {
                ColP = PlayerLeft;
                Collided = true;
            }
            if (!IsTileMapPointEmpty(TileMap, PlayerRight))
            {
                ColP = PlayerRight;
                Collided = true;
            }

            if (Collided)
            {
                v2 r = {0, 0};
                if (ColP.AbsTileX < GameState->PlayerP.AbsTileX)
                {
                    r = v2{1, 0};
                };
                if (ColP.AbsTileX > GameState->PlayerP.AbsTileX)
                {
                    r = v2{-1, 0};
                };
                if (ColP.AbsTileY < GameState->PlayerP.AbsTileY)
                {
                    r = v2{0, 1};
                };
                if (ColP.AbsTileY > GameState->PlayerP.AbsTileY)
                {
                    r = v2{0, -1};
                }
                GameState->dPlayerP = GameState->dPlayerP - Inner(GameState->dPlayerP, r) * r;
            }
            else
            {
                if (!AreOnSameTile(&GameState->PlayerP, &NewPlayerP))
                {
                    uint32 NewTileValue = GetTileValue(TileMap, NewPlayerP);

                    if (NewTileValue == 3)
                    {
                        ++NewPlayerP.AbsTileZ;
                    }
                    else if (NewTileValue == 4)
                    {
                        --NewPlayerP.AbsTileZ;
                    }
                }
                GameState->PlayerP = NewPlayerP;
            }

            GameState->CameraP.AbsTileZ = GameState->PlayerP.AbsTileZ;

            tile_map_difference Diff = Subtract(TileMap, &GameState->PlayerP, &GameState->CameraP);
            if (Diff.dXY.X > (9.f * TileMap->TileSideInMeters))
            {
                GameState->CameraP.AbsTileX += 17;
            }
            if(Diff.dXY.X < -(9.0f*TileMap->TileSideInMeters))
            {
                GameState->CameraP.AbsTileX -= 17;
            }
            if(Diff.dXY.Y > (5.0f*TileMap->TileSideInMeters))
            {
                GameState->CameraP.AbsTileY += 9;
            }
            if(Diff.dXY.Y < -(5.0f*TileMap->TileSideInMeters))
            {
                GameState->CameraP.AbsTileY -= 9;
            }
        }
    }

    DrawBitmap(Buffer, &GameState->Backdrop, 0, 0);

    // NOTE: Center of the screen
    real32 ScreenCenterX = 0.5f*(real32)Buffer->Width;
    real32 ScreenCenterY = 0.5f*(real32)Buffer->Height;

    for (int32 RelRow = -10; RelRow < 10; RelRow++)
    {
        for (int32 RelColumn = -20; RelColumn < 20; RelColumn++)
        {
            uint32 Column = GameState->CameraP.AbsTileX + RelColumn;
            uint32 Row = GameState->CameraP.AbsTileY + RelRow;
            uint32 TileID = GetTileValue(TileMap, Column, Row, GameState->CameraP.AbsTileZ);

            if (TileID > 1)
            {
                real32 Gray = 0.5f;
                if (TileID == 2)
                {
                    Gray = 1.0f;
                }

                if (TileID > 2)
                {
                    Gray = 0.25f;
                }

                if ((Column == GameState->CameraP.AbsTileX) &&
                   (Row == GameState->CameraP.AbsTileY))
                {
                    Gray = 0.0f;
                }

                v2 TileSide = { 0.5f*TileSideInPixels, 0.5f*TileSideInPixels };
                v2 Cen = { ScreenCenterX - MetersToPixels*GameState->CameraP.Offset.X + ((real32)RelColumn)*TileSideInPixels,
                           ScreenCenterY + MetersToPixels*GameState->CameraP.Offset.Y - ((real32)RelRow)*TileSideInPixels };
                v2 Min = Cen - TileSide;
                v2 Max = Cen + TileSide;
                DrawRectangle(Buffer, Min, Max, Gray, Gray, Gray);
            }
        }
    }

    tile_map_difference Diff = Subtract(TileMap, &GameState->PlayerP, &GameState->CameraP);

    real32 PlayerR = 1.f;
    real32 PlayerG = 1.f;
    real32 PlayerB = 0.f;

    real32 PlayerGroundPointX = ScreenCenterX + MetersToPixels*Diff.dXY.X;
    real32 PlayerGroundPointY = ScreenCenterY - MetersToPixels*Diff.dXY.Y;
    v2 PlayerLeftTop = { PlayerGroundPointX - 0.5f*MetersToPixels*PlayerWidth,
                         PlayerGroundPointY - MetersToPixels*PlayerHeight };
    v2 PlayerWidthHeight = { PlayerWidth, PlayerHeight };
    DrawRectangle(Buffer,
                  PlayerLeftTop, PlayerLeftTop + MetersToPixels*PlayerWidthHeight,
                  PlayerR, PlayerG, PlayerB);


    hero_bitmaps *HeroBitmaps = &GameState->HeroBitmaps[GameState->HeroFacingDirection];
    DrawBitmap(Buffer, &HeroBitmaps->Torso, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
    DrawBitmap(Buffer, &HeroBitmaps->Cape, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
    DrawBitmap(Buffer, &HeroBitmaps->Head, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
}


extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, 400);
}


/*
internal void
RenderWeirdGradient(game_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset)
{
    uint8* Row = (uint8 *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32* Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer->Width; ++X)
        {
            // B G R x
            uint8 Blue = (uint8)(X + BlueOffset);
            uint8 Green = (uint8)(Y + GreenOffset);

            *Pixel++ = ((Green << 0) | Blue);
        }

        Row += Buffer->Pitch;
    }
}
*/
