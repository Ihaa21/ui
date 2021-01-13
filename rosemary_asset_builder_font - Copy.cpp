/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Ihor Szlachtycz $
   $Notice: (C) Copyright 2014 by Dream.Inc, Inc. All Rights Reserved. $
   ======================================================================== */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

struct file_glyph_render_info
{
    v2 MinUv;
    v2 MaxUv;
};

struct file_glyph
{
    v2 Dim;
    // TODO: What should we be calling this?
    v2 AlignPos;
    f32 StepX;
};

struct file_font
{
    union
    {
        u32 AssetTypeId;
        u32 FileId;
    };

    // NOTE: Where assuming that the glyphs are in one range
    char MinGlyph;
    char MaxGlyph;

    // NOTE: Vertical font data    
    f32 MaxAscent;
    f32 MaxDescent;
    f32 LineGap;

    // NOTE: Atlas data
    u32 AtlasWidth;
    u32 AtlasHeight;
    u32 AtlasSize;

    u64 KernArrayOffset;
    u64 GlyphArrayOffset;
    u64 GlyphRenderInfoArrayOffset;
    u64 AtlasOffset;
};

internal void LoadFont(u32 FontId, u64 Size, u8* Data, char MinGlyph, char MaxGlyph, file_font* Font)
{
    Font->AssetTypeId = FontId;
    
    // NOTE: Calculate required space in asset file
    // IMPORTANT: We are setting the resolution of each glyph here
    f32 CharHeight = 160.0f;
    
    // IMPORTANT: We are limiting/setting the resolution of our textures here
    Font->AtlasWidth = 1024;
    Font->AtlasHeight = 1024;
    Font->AtlasSize = Font->AtlasWidth*Font->AtlasHeight*sizeof(u8);

    u32 NumGlyphsToLoad = MaxGlyph - MinGlyph;
    
    stbtt_packedchar* StbCharData = (stbtt_packedchar*)malloc(sizeof(stbtt_packedchar)*NumGlyphsToLoad);

    // NOTE: Populate atlas data
    {
        u8* AtlasPixels = (u8*)StatePushData(Font->AtlasSize, &Font->AtlasOffset);
        
        stbtt_pack_context PackContext;
        i32 Return = 0;
        
        stbtt_PackSetOversampling(&PackContext, 2, 2);
        Return = stbtt_PackBegin(&PackContext, AtlasPixels, Font->AtlasWidth, Font->AtlasHeight, 0, 1, 0);
        Assert(Return == 1);

        Return = stbtt_PackFontRange(&PackContext, Data, 0, CharHeight, MinGlyph, NumGlyphsToLoad, StbCharData);
        Assert(Return == 1);
        
        stbtt_PackEnd(&PackContext);
    }

    // NOTE: Populate font meta data
    stbtt_fontinfo FontInfo;
    // NOTE: These values convert kerning and step from stbtt space to our 0->1 space
    f32 StbToUiWidth;
    f32 StbToUiHeight;
    f32 StbToPixelsHeight;
    f32 PixelsToUvWidth = 1.0f / (f32)Font->AtlasWidth;
    f32 PixelsToUvHeight = 1.0f / (f32)Font->AtlasHeight;
    {
        stbtt_InitFont(&FontInfo, Data, stbtt_GetFontOffsetForIndex(Data, 0));

        i32 MaxAscent, MaxDescent, LineGap;
        stbtt_GetFontVMetrics(&FontInfo, &MaxAscent, &MaxDescent, &LineGap);

        Assert(MaxAscent > 0);
        Assert(MaxDescent < 0);

        StbToUiHeight = 1.0f / (f32)(MaxAscent - MaxDescent);
        StbToUiWidth = StbToUiHeight;
        
        Font->MaxAscent = StbToUiHeight * (f32)MaxAscent;
        Font->MaxDescent = StbToUiHeight * (f32)MaxDescent;
        Font->LineGap = StbToUiHeight * (f32)LineGap;
        Font->MinGlyph = MinGlyph;
        Font->MaxGlyph = MaxGlyph;

        StbToPixelsHeight = stbtt_ScaleForPixelHeight(&FontInfo, CharHeight);
    }
    
    // NOTE: Populate kerning array
    {
        u32 KernArraySize = Square(NumGlyphsToLoad)*sizeof(f32);
        f32* KernData = (f32*)StatePushData(KernArraySize, &Font->KernArrayOffset);
        memset(KernData, KernArraySize, 0);
        
        for (char Glyph1 = MinGlyph; Glyph1 < MaxGlyph; ++Glyph1)
        {
            for (char Glyph2 = MinGlyph; Glyph2 < MaxGlyph; ++Glyph2)
            {            
                u32 ArrayIndex = (Glyph2 - MinGlyph)*NumGlyphsToLoad + (Glyph1 - MinGlyph);
                KernData[ArrayIndex] = StbToUiWidth*(f32)stbtt_GetCodepointKernAdvance(&FontInfo, Glyph1, Glyph2);

                Assert(Abs(KernData[ArrayIndex]) >= 0 && Abs(KernData[ArrayIndex]) < 1);
            }
        }
    }

    // NOTE: Populate glyph placement/render data
    {
        file_glyph* CurrGlyph = StatePushArray(file_glyph, NumGlyphsToLoad, &Font->GlyphArrayOffset);
        file_glyph_render_info* CurrRenderInfo = StatePushArray(file_glyph_render_info, NumGlyphsToLoad, &Font->GlyphRenderInfoArrayOffset);

        for (u32 GlyphId = 0; GlyphId < NumGlyphsToLoad; ++GlyphId, ++CurrGlyph, ++CurrRenderInfo)
        {
            stbtt_packedchar CharData = StbCharData[GlyphId];
            
            CurrRenderInfo->MinUv.x = CharData.x0 * PixelsToUvWidth;
            CurrRenderInfo->MinUv.y = CharData.y1 * PixelsToUvHeight;
            CurrRenderInfo->MaxUv.x = CharData.x1 * PixelsToUvWidth;
            CurrRenderInfo->MaxUv.y = CharData.y0 * PixelsToUvHeight;

            i32 StepX;
            stbtt_GetCodepointHMetrics(&FontInfo, GlyphId + Font->MinGlyph, &StepX, 0);
            CurrGlyph->StepX = StbToUiWidth*(f32)StepX;

            Assert(CurrGlyph->StepX > 0 && CurrGlyph->StepX < 1);

            int MinX, MinY, MaxX, MaxY;
            stbtt_GetCodepointBox(&FontInfo, GlyphId + Font->MinGlyph, &MinX, &MinY, &MaxX, &MaxY);

            // NOTE: Sometimes spaces have messed up dim y's
            if ((GlyphId + Font->MinGlyph) != ' ')
            {
                CurrGlyph->Dim = V2(StbToUiWidth, StbToUiHeight)*V2(MaxX - MinX, MaxY - MinY);
                CurrGlyph->AlignPos = V2(StbToUiWidth, StbToUiHeight)*V2(MinX, MinY);

                Assert(CurrGlyph->Dim.x + CurrGlyph->AlignPos.x <= 1.0f);
                Assert(CurrGlyph->Dim.y + CurrGlyph->AlignPos.y <= 1.0f);
            }
        }
    }

    free(StbCharData);
}

#if 0
internal load_request LoadFont(u64 Size, u8* Data, u32 MinGlyph, u32 MaxGlyph, file_font* Font)
{
    load_request Result = {};

    stbtt_fontinfo FontInfo;
    stbtt_InitFont(&FontInfo, Data, stbtt_GetFontOffsetForIndex(Data, 0));
    f32 ScaleFactor = stbtt_ScaleForPixelHeight(&FontInfo, 128.0f);

    stbtt_GetFontVMetrics(&FontInfo, &Font->MaxAscent, &Font->MaxDescent, &Font->LineGap);
    Font->MaxAscent = (i32)((f32)Font->MaxAscent * ScaleFactor);
    Font->MaxDescent = (i32)((f32)Font->MaxDescent * ScaleFactor);
    Font->LineGap = (i32)((f32)Font->LineGap * ScaleFactor);
    Font->MinGlyph = MinGlyph;
    Font->MaxGlyph = MaxGlyph;
    
    // NOTE: Figure out how much memory we are going to need
    u32 NumGlyphsToLoad = Font->MaxGlyph - Font->MinGlyph;
    u32 StepArraySize = NumGlyphsToLoad*sizeof(f32);
    u32 KernArraySize = Square(NumGlyphsToLoad)*sizeof(f32);

    // IMPORTANT: We are limiting/setting the resolution of our textures here
    u32 AtlasWidth = 1024;
    u32 AtlasHeight = 1024;
    u32 AtlasSize = AtlasWidth*AtlasHeight*sizeof(u32);
    
    Result.Size = NumGlyphsToLoad*sizeof(file_glyph) + KernArraySize + StepArraySize + AtlasSize;

#if 0
    for (u32 BitmapIndex = 0, CodePoint = Font->MinGlyph;
         CodePoint < Font->MaxGlyph; ++BitmapIndex, ++CodePoint)
    {
        i32 x0, x1, y0, y1;
        stbtt_GetCodepointBitmapBox(&FontInfo, CodePoint, ScaleFactor, ScaleFactor, &x0, &y0,
                                    &x1, &y1);
        
        Result.Size += (x1-x0)*(y1-y0)*sizeof(u32);
    }
#endif
    
    Font->StepArrayOffset = State.CurrentDataOffset;
    Font->KernArrayOffset = State.CurrentDataOffset + StepArraySize;
    Font->GlyphArrayOffset = State.CurrentDataOffset + KernArraySize + StepArraySize;
    Result.Data = malloc(Result.Size);
    
    // NOTE: Load step array
    f32* StepData = (f32*)Result.Data;
    for (u32 Glyph = MinGlyph; Glyph < MaxGlyph; ++Glyph)
    {
        i32 Step;
        stbtt_GetCodepointHMetrics(&FontInfo, Glyph, &Step, 0);
        StepData[Glyph - MinGlyph] = ScaleFactor*(f32)Step;
    }
    
    // NOTE: Load kerning array
    u32 KernDim = MaxGlyph - MinGlyph;
    f32* KernData = ShiftPtrByBytes(Result.Data, StepArraySize, f32);
    for (u32 Glyph1 = MinGlyph; Glyph1 < MaxGlyph; ++Glyph1)
    {
        for (u32 Glyph2 = MinGlyph; Glyph2 < MaxGlyph; ++Glyph2)
        {            
            u32 ArrayIndex = (Glyph2 - MinGlyph)*KernDim + (Glyph1 - MinGlyph);
            i32 t = stbtt_GetCodepointKernAdvance(&FontInfo, Glyph1, Glyph2);
            KernData[ArrayIndex] =  (ScaleFactor*
                                     (f32)stbtt_GetCodepointKernAdvance(&FontInfo, Glyph1, Glyph2));
        }
    }

#if 0
    file_glyph* CurrentGlyph = ShiftPtrByBytes(Result.Data, StepArraySize + KernArraySize, file_glyph);
    u8* PixelMem = ShiftPtrByBytes(Result.Data, KernArraySize + sizeof(file_glyph)*NumGlyphsToLoad +
                                   StepArraySize, u8);
    mm CurrentByte = (State.CurrentDataOffset + StepArraySize + KernArraySize +
                      sizeof(file_glyph)*NumGlyphsToLoad);
    for (u32 CodePoint = Font->MinGlyph; CodePoint < Font->MaxGlyph;
         ++CodePoint, ++CurrentGlyph)
    {
        i32 Width, Height, XOffset, YOffset;
        u8 *MonoBitmap = stbtt_GetCodepointBitmap(&FontInfo, 0, ScaleFactor, CodePoint, &Width, &Height,
                                                  &XOffset, &YOffset);
        
        // TODO: Normalize all our coordinates?
        i32 x0, x1, y0, y1;
        stbtt_GetCodepointBitmapBox(&FontInfo, CodePoint, ScaleFactor, ScaleFactor, &x0, &y0, &x1, &y1);
        *CurrentGlyph = {};
        CurrentGlyph->Width = Width;
        CurrentGlyph->Height = Height;
        CurrentGlyph->AlignPos = V2(x0, y0) + 0.5f*V2(x1 - x0, y1 - y0);
        CurrentGlyph->PixelOffset = CurrentByte;

        Assert(x1 - x0 == Width);
        Assert(y1 - y0 == Height);
        
        u8* SourcePixels = MonoBitmap;
        u8* DestRow = PixelMem + sizeof(u32)*Width*(Height - 1);
        for (i32 Y = 0; Y < Height; ++Y)
        {
            u32* DestPixel = (u32*)DestRow;
        
            for (i32 X = 0; X < Width; ++X)
            {
                u8 Gray = *SourcePixels++;
                                
                v4 Texel = SRGBToLinear(V4(Gray, Gray, Gray, Gray));
                Texel.rgb *= Texel.a;
                Texel = LinearToSRGB(Texel);
                
                *DestPixel++ = (((u32)(Texel.a + 0.5f) << 24) |
                                ((u32)(Texel.r + 0.5f) << 16) |
                                ((u32)(Texel.g + 0.5f) << 8) |
                                ((u32)(Texel.b + 0.5f)));
            }

            DestRow -= Width*sizeof(u32);
        }
        
        stbtt_FreeBitmap(MonoBitmap, 0);
        PixelMem += sizeof(u32)*CurrentGlyph->Width*CurrentGlyph->Height;
        CurrentByte += sizeof(u32)*CurrentGlyph->Width*CurrentGlyph->Height;
    }
#endif
    
    return Result;
}
#endif
