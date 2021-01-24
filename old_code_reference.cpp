/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Ihor Szlachtycz $
   $Notice: (C) Copyright 2014 by Dream.Inc, Inc. All Rights Reserved. $
   ======================================================================== */

//
// =======================================================================================================================================
//

#if 0

inline void UiStatePushText(ui_state* UiState, f32 MaxCharHeight, v2 TopLeftTextPos, f32 MaxSentenceWidth, char* Text, v4 TintColor)
{
    // NOTE: https://github.com/justinmeiners/stb-truetype-example/blob/master/source/main.c
    // TODO: Add constraints to text and text properties like centered

    // NOTE: If we don't have a sentence width, don't apply it
    if (MaxSentenceWidth == 0)
    {
        MaxSentenceWidth = INT32_MAX;
    }

    ui_font* Font = &UiState->Font;
    f32 FrontTextZ = UiStateGetZ(UiState);
    f32 BackTextZ = UiStateGetZ(UiState);

    // NOTE: Calculate our text start pos on the line
    v2 StartPos = V2(TopLeftTextPos.x, TopLeftTextPos.y - MaxCharHeight*(Font->MaxAscent));

    // NOTE: We walk our glyphs using floats and round when we generate glyphs to render
    v2 CurrPos = StartPos;
    char PrevGlyph = 0;
    char CurrGlyph = 0;
    while (*Text)
    {
        CurrGlyph = *Text;

        ui_glyph* Glyph = Font->GlyphArray + CurrGlyph;
        f32 Kerning = 0.0f;
        f32 ScaledStepX = MaxCharHeight*Glyph->StepX;
        b32 StartNewLine = CurrGlyph == '\n';
        if (!StartNewLine)
        {
            // NOTE: Check if we have enough space on the current line to draw the next glyph
            if (PrevGlyph != 0)
            {
                Kerning = MaxCharHeight*Font->KernArray[CurrGlyph*255 + PrevGlyph];
            }
        
            StartNewLine = ((CurrPos.x + Kerning + ScaledStepX - StartPos.x) > MaxSentenceWidth);
        }
        
        if (StartNewLine)
        {
            CurrPos.y += UiFontGetLineAdvance(Font, MaxCharHeight);
            CurrPos.x = StartPos.x;
            PrevGlyph = 0;
            if (CurrGlyph == '\n')
            {
                Text += 1;
            }
            
            // NOTE: Don't start with spaces on a new line
            while (CurrGlyph == ' ')
            {
                Text += 1;
                CurrGlyph = *Text;
            }

            continue;
        }

        // NOTE: We have enough space for the glyph, apply kerning
        CurrPos.x += Kerning;
        
        if (CurrGlyph != ' ')
        {
            // NOTE: Glyph dim will be the aabb size, but we offset by align pos since some chars like 'p'
            // need to be descend a bit
            v2 CharDim = Glyph->Dim * MaxCharHeight;
            aabb2 GlyphBounds = AabbMinMax(V2(0, 0), CharDim);
            GlyphBounds = Translate(GlyphBounds, CurrPos + MaxCharHeight*Glyph->AlignPos);

            // NOTE: Store black version of glyph behind white version
            UiStatePushGlyph(UiState, CurrGlyph, FrontTextZ, GlyphBounds, TintColor);
            UiStatePushGlyph(UiState, CurrGlyph, FrontTextZ, GlyphBounds, V4(0, 0, 0, 1));
        }
        
        CurrPos.x += ScaledStepX;
        PrevGlyph = CurrGlyph;
        ++Text;
    }
}

//
// NOTE: Ui Render Functions
//

inline void UiSetTexturedRectClipRect(ui_state* UiState, aabb2i Bounds, b32 IsReset)
{
    ui_textured_rect_job* Job = &UiState->TexturedRectJob;
    u32 Index = Job->NumClipRects;

    // NOTE: Make sure we don't reset the same clip rect
    if (!IsReset && Index != 0)
    {
        Assert(!(Bounds.Min.x == Job->ClipRectArray[Index-1].Bounds.Min.x &&
                 Bounds.Min.y == Job->ClipRectArray[Index-1].Bounds.Min.y &&
                 Bounds.Max.x == Job->ClipRectArray[Index-1].Bounds.Max.x &&
                 Bounds.Max.y == Job->ClipRectArray[Index-1].Bounds.Max.y));
    }
    
    Job->ClipRectArray[Index].Bounds = Bounds;
    Job->ClipRectArray[Index].NumAffected = 0;
    Job->NumClipRects += 1;
}

inline void UiResetTexturedRectClipRect(ui_state* UiState)
{
    UiSetTexturedRectClipRect(UiState, AabbiMinMax(V2i(0, 0), V2i(UiState->RenderWidth, UiState->RenderHeight)), true);
}

inline void UiSetGlyphClipRect(ui_state* UiState, aabb2i Bounds, b32 IsReset = false)
{
    ui_glyph_job* Job = &UiState->GlyphJob;
    u32 Index = Job->NumClipRects;

    // NOTE: Make sure we don't reset the same clip rect
    if (!IsReset && Index != 0)
    {
        Assert(!(Bounds.Min.x == Job->ClipRectArray[Index-1].Bounds.Min.x &&
                 Bounds.Min.y == Job->ClipRectArray[Index-1].Bounds.Min.y &&
                 Bounds.Max.x == Job->ClipRectArray[Index-1].Bounds.Max.x &&
                 Bounds.Max.y == Job->ClipRectArray[Index-1].Bounds.Max.y));
    }
    
    Job->ClipRectArray[Index].Bounds = Bounds;
    Job->ClipRectArray[Index].NumAffected = 0;
    Job->NumClipRects += 1;
}

inline void UiResetGlyphClipRect(ui_state* UiState)
{
    UiSetGlyphClipRect(UiState, AabbiMinMax(V2i(0, 0), V2i(UiState->RenderWidth, UiState->RenderHeight)), true);
}

//
// NOTE: Ui Helper Functions
//

// NOTE: Use this so that perfectly placed UI can't have the mouse hit two elements at a time
inline b32 UiIntersect(aabb2 A, v2 B)
{
    return (A.Min.x <= B.x && A.Max.x > B.x &&
            A.Min.y <= B.y && A.Max.y > B.y);
}

inline b32 UiHandleOverlap(ui_state* UiState, aabb2 Bounds, u32 Flags)
{
    b32 Overlapped = false;
    
    // NOTE: Check if we hovered over the the element
    if (!(Flags & UiElementFlag_DontCheckHover))
    {
        input_pointer* MainPointer = &UiState->InputState->CurrInput.MainPointer;
        Overlapped = UiIntersect(Bounds, MainPointer->PixelPos);
        UiState->MouseTouchingUi = UiState->MouseTouchingUi || Overlapped;
    }

    return Overlapped;
}

inline v2 UiApplyConstraint(ui_state* UiState, ui_constraint Constraint, v2 Pos)
{
    v2 Result = {};

    // TODO: Left edge and bottom edge do the same thing
    switch (Constraint)
    {
        case UiConstraint_None:
        {
            Result = Pos;
        } break;

        case UiConstraint_Center:
        {
            // NOTE: We treat every vector in aabb as having its origin at the center of the screen
            v2 ScreenCenter = V2(f32(UiState->RenderWidth) * 0.5f, f32(UiState->RenderHeight) * 0.5f);

            Result = Pos + ScreenCenter;
        } break;

        case UiConstraint_LeftEdge:
        {
            // NOTE: We treat every vector's x in aabb as having its origin the left edge
            v2 LeftEdge = V2(0, 0);

            Result = Pos + LeftEdge;
        } break;
        
        case UiConstraint_RightEdge:
        {
            // NOTE: We treat every vector's x in aabb as having its origin the right edge
            v2 RightEdge = V2(f32(UiState->RenderWidth), 0.0f);

            Result = Pos + RightEdge;
        } break;
        
        case UiConstraint_TopEdge:
        {
            // NOTE: We treat every vector's y in aabb as having its origin the top edge
            v2 TopEdge = V2(0.0f, f32(UiState->RenderHeight));

            Result = Pos + TopEdge;
        } break;
        
        case UiConstraint_BottomEdge:
        {
            // NOTE: We treat every vector's y in aabb as having its origin the bottom edge
            v2 BottomEdge = V2(0, 0);

            Result = Pos + BottomEdge;
        } break;
    }

    return Result;
}

inline aabb2 UiApplyConstraint(ui_state* UiState, ui_constraint Constraint, aabb2 Bounds)
{
    aabb2 Result = {};
    Result.Min = UiApplyConstraint(UiState, Constraint, Bounds.Min);
    Result.Max = UiApplyConstraint(UiState, Constraint, Bounds.Max);

    return Result;
}

inline u64 UiScrollNumVisible(f32 MenuY, f32 RowY, f32 T)
{
    u64 Result = CeilU64(MenuY / RowY);

    if (T != 0.0f)
    {
        Result += 1;
    }
    
    // TODO: Make this be actually accurate, not just overestimate
    // TODO: We re ordered some stuff so we should be okay if we use data and return from scroll result instead
    //if (UiStartY / 
    return Result;
}

inline ui_scroll_result UiGetScrollData(f32 MenuDim, f32 ElementDim, u64 NumElements, f32 T)
{
    ui_scroll_result Result = {};
    
    u64 NumVisibleElements = UiScrollNumVisible(MenuDim, ElementDim, T);
    f32 UiTotalDim = ElementDim*NumElements;
    f32 UiStartPos = T*(UiTotalDim - MenuDim);

    Result.StartElement = FloorI32(UiStartPos / ElementDim);
    Result.EndElement = Min(NumElements, Result.StartElement + NumVisibleElements);
    Result.Offset = UiStartPos - f32(Result.StartElement)*ElementDim;
    
    Assert(Result.StartElement < NumElements);
    Assert(Result.EndElement <= NumElements);

    return Result;
}

//
// NOTE: Ui Interaction Initializers
//

#define UiScrollX(InputState, UiBounds, RequiredDelta, ScrollSpeed, CurrScrollVel) UiScroll_(InputState, UiBounds, true, RequiredDelta, ScrollSpeed, CurrScrollVel, uptr(CurrScrollVel) | INTERACTION_FILE_LINE_ID())
#define UiScrollY(InputState, UiBounds, RequiredDelta, ScrollSpeed, CurrScrollVel) UiScroll_(InputState, UiBounds, false, RequiredDelta, ScrollSpeed, CurrScrollVel, uptr(CurrScrollVel) | INTERACTION_FILE_LINE_ID())
inline void UiScroll_(input_state* InputState, aabb2 UiBounds, b32 ScrollX, f32 RequiredDelta, f32 ScrollSpeed, f32* CurrScrollVel, u64 RefId)
{
#if 0
    interaction_ref UiScrollRef = {};
    UiScrollRef.Id = RefId;

    input_pointer* CurrPointer = &InputState->CurrInput.MainPointer;
    input_pointer* PrevPointer = &InputState->PrevInput.MainPointer;
    // NOTE: We only add this interaction if we have a single pointer on the screen
    if (!InputIsMainPointerOnly(&InputState->CurrInput))
    {
        return;
    }
    
    // NOTE: Check if we moved our delta enough
    b32 MovedDelta = false;
    if (ScrollX)
    {
        MovedDelta = Abs(CurrPointer->ScreenDelta.x) >= RequiredDelta;
    }
    else
    {
        MovedDelta = Abs(CurrPointer->ScreenDelta.y) >= RequiredDelta;
    }

    f32 PrevMouseAxis = PrevPointer->PixelPos.x;
    if (!ScrollX)
    {
        PrevMouseAxis = PrevPointer->PixelPos.y;
    }
    
    if (InputInteractionsAreSame(UiScrollRef, InputState->PrevHot.Ref))
    {
        interaction Interaction = InputState->PrevHot;
        Interaction.UiScroll.PrevMouseAxis = PrevMouseAxis;
        
        InputAddInteraction(InputState, Interaction);
    }
    else if (MovedDelta && UiIntersect(UiBounds, CurrPointer->PixelPos))
    {
        // TODO: Needs to be smoothed out, try on mobile and see how it feels
        // NOTE: Create the tower interaction
        interaction Interaction = {};
        Interaction.Type = Interaction_UiScroll;
        Interaction.Ref = UiScrollRef;
        Interaction.UiScroll.ScrollX = ScrollX;
        Interaction.UiScroll.ScrollSpeed = ScrollSpeed;
        Interaction.UiScroll.CurrScrollVel = CurrScrollVel;
        Interaction.UiScroll.PrevMouseAxis = PrevMouseAxis;

        InputAddInteraction(InputState, Interaction);
    }
    else
    {
        // NOTE: Only apply drag when we let go of the scroll and want velocity to continue
        *CurrScrollVel *= 0.85f;
    }
#endif
}

//
// NOTE: Ui Element Functions
//

inline void UiImage(ui_state* UiState, ui_constraint Constraint, u32 Flags, aabb2 Bounds,
                    asset_texture_id TextureId, v4 TintColor = V4(1, 1, 1, 1))
{
    Bounds = UiApplyConstraint(UiState, Constraint, Bounds);
    UiHandleOverlap(UiState, Bounds, Flags);
    UiPushTexture(UiState, Bounds, TextureId, TintColor);
}

inline void UiRect(ui_state* UiState, ui_constraint Constraint, u32 Flags, aabb2 Bounds, v4 Color)
{
#if 0
    UiImage(UiState, Constraint, Flags, Bounds, AssetTextureId(Texture_White), Color);
#endif
}

inline void UiRectOutline(ui_state* UiState, aabb2 Bounds, v4 Color, f32 Thickness)
{
#if 0
    v2 Center = AabbGetCenter(Bounds);
    v2 Radius = AabbGetRadius(Bounds);
    f32 HalfThickness = Thickness/2;
    
    aabb2 TopBox = AabbCenterRadius(Center + V2(0.0f, Radius.y - HalfThickness), V2(Radius.x, HalfThickness));
    aabb2 LeftBox = AabbCenterRadius(Center + V2(-Radius.x + HalfThickness, 0.0f), V2(HalfThickness, Radius.y));
    aabb2 RightBox = AabbCenterRadius(Center + V2(Radius.x - HalfThickness, 0.0f), V2(HalfThickness, Radius.y));
    aabb2 BottomBox = AabbCenterRadius(Center + V2(0.0f, -Radius.y + HalfThickness), V2(Radius.x, HalfThickness));

    UiRect(UiState, UiConstraint_None, 0, TopBox, Color);
    UiRect(UiState, UiConstraint_None, 0, LeftBox, Color);
    UiRect(UiState, UiConstraint_None, 0, RightBox, Color);
    UiRect(UiState, UiConstraint_None, 0, BottomBox, Color);
#endif
}

inline f32 UiTextGetLineAdvance(ui_state* UiState, ui_font* Font, f32 CharHeight)
{
    f32 Result = -CharHeight*(Font->MaxAscent - Font->MaxDescent + Font->LineGap);
    return Result;
}

inline f32 UiTextGetStartY(ui_state* UiState, u32 FontId, f32 MinY, f32 MaxY)
{
    ui_font* Font = UiState->FontArray + FontId;
    f32 CharHeight = MaxY - MinY;
    f32 StartY = MaxY - CharHeight*(Font->MaxAscent);

    return StartY;
}

inline aabb2 UiGetTextBounds(ui_state* UiState, ui_constraint Constraint, u32 FontId, f32 CharHeight,
                             v2 TopLeftTextPos, f32 SentenceWidth, char* Text)
{
    // TODO: Floats seem to fail for me at higher resolutions and so we get text that displays slightly different at larger resolutions.
    // It might not be a issue but we can fix this by doing all the math in fixed point, or normalizing to working closer to 0, and then
    // offset only for displaying.
    
    // NOTE: https://github.com/justinmeiners/stb-truetype-example/blob/master/source/main.c
    aabb2 Result = {};

    // NOTE: If we don't have a sentence width, don't apply it
    if (SentenceWidth == 0)
    {
        SentenceWidth = INT32_MAX;
    }

    ui_font* Font = UiState->FontArray + FontId;

    // NOTE: We map TopLeftPos to the pos of where the line will be
    v2 StartPos = UiApplyConstraint(UiState, Constraint, TopLeftTextPos);
    StartPos = V2((f32)StartPos.x, (f32)StartPos.y - CharHeight*(Font->MaxAscent));

    // NOTE: This is figuring out our max x and max y (the y starts and finishes at the line)
    v2 CurrPos = StartPos;
    f32 MaxX = 0.0f;
    char PrevGlyph = 0;
    char CurrGlyph = 0;
    while (*Text)
    {
        CurrGlyph = *Text;

        file_glyph* Glyph = Font->GlyphArray + CurrGlyph - Font->MinGlyph;
        f32 Kerning = 0.0f;
        f32 ScaledStepX = CharHeight*Glyph->StepX;
        b32 StartNewLine = CurrGlyph == '\n';
        if (!StartNewLine)
        {
            // NOTE: Check if we have enough space on the current line to draw the next glyph
            Assert(CurrGlyph >= (char)Font->MinGlyph && CurrGlyph < (char)Font->MaxGlyph);
            if (PrevGlyph != 0)
            {
                u32 NumGlyphs = Font->MaxGlyph - Font->MinGlyph;
                Kerning = CharHeight*Font->KernArray[(CurrGlyph - Font->MinGlyph)*NumGlyphs + (PrevGlyph - Font->MinGlyph)];
            }
        
            StartNewLine = ((CurrPos.x + Kerning + ScaledStepX - StartPos.x) > SentenceWidth);
        }
        
        if (StartNewLine)
        {
            MaxX = Max(MaxX, CurrPos.x);
            CurrPos.y += UiTextGetLineAdvance(UiState, Font, CharHeight);
            CurrPos.x = StartPos.x;
            PrevGlyph = 0;
            if (CurrGlyph == '\n')
            {
                Text += 1;
            }

            // NOTE: Don't start with spaces on a new line
            while (CurrGlyph == ' ')
            {
                Text += 1;
                CurrGlyph = *Text;
            }
            
            continue;
        }

        // NOTE: We have enough space for the glyph, apply kerning
        CurrPos.x += Kerning;
                
        CurrPos.x += ScaledStepX;
        PrevGlyph = CurrGlyph;
        ++Text;
    }

    MaxX = Max(MaxX, CurrPos.x);
    Result = AabbMinMax(V2(StartPos.x, CurrPos.y), V2(MaxX, StartPos.y));

    // NOTE: Add the max size of a glyph to the edges of our box
    Result.Min.y += Font->MaxDescent*CharHeight;
    Result.Max.y += Font->MaxAscent*CharHeight;
    
    return Result;
}

inline void UiText(ui_state* UiState, ui_constraint Constraint, u32 FontId, f32 CharHeight,
                   v2 TopLeftTextPos, f32 SentenceWidth, char* Text, v4 TintColor)
{
#if 0
    // TODO: Floats seem to fail for me at higher resolutions and so we get text that displays slightly different at larger resolutions.
    // It might not be a issue but we can fix this by doing all the math in fixed point, or normalizing to working closer to 0, and then
    // offset only for displaying.

    // NOTE: https://github.com/justinmeiners/stb-truetype-example/blob/master/source/main.c
    // TODO: Add constraints to text and text properties like centered

    // NOTE: If we don't have a sentence width, don't apply it
    if (SentenceWidth == 0)
    {
        SentenceWidth = INT32_MAX;
    }

    ui_font* Font = UiState->FontArray + FontId;

    // NOTE: Calculate our text start pos on the line
    v2 StartPos = UiApplyConstraint(UiState, Constraint, TopLeftTextPos);
    StartPos = V2(StartPos.x, StartPos.y - CharHeight*(Font->MaxAscent));

    // NOTE: We walk our glyphs using floats and round when we generate glyphs to render
    v2 CurrPos = StartPos;
    char PrevGlyph = 0;
    char CurrGlyph = 0;
    while (*Text)
    {
        CurrGlyph = *Text;

        file_glyph* Glyph = Font->GlyphArray + CurrGlyph - Font->MinGlyph;
        f32 Kerning = 0.0f;
        f32 ScaledStepX = CharHeight*Glyph->StepX;
        b32 StartNewLine = CurrGlyph == '\n';
        if (!StartNewLine)
        {
            // NOTE: Check if we have enough space on the current line to draw the next glyph
            Assert(CurrGlyph >= (char)Font->MinGlyph && CurrGlyph < (char)Font->MaxGlyph);
            if (PrevGlyph != 0)
            {
                u32 NumGlyphs = Font->MaxGlyph - Font->MinGlyph;
                Kerning = CharHeight*Font->KernArray[(CurrGlyph - Font->MinGlyph)*NumGlyphs + (PrevGlyph - Font->MinGlyph)];
            }
        
            StartNewLine = ((CurrPos.x + Kerning + ScaledStepX - StartPos.x) > SentenceWidth);
        }
        
        if (StartNewLine)
        {
            CurrPos.y += UiTextGetLineAdvance(UiState, Font, CharHeight);
            CurrPos.x = StartPos.x;
            PrevGlyph = 0;
            if (CurrGlyph == '\n')
            {
                Text += 1;
            }
            
            // NOTE: Don't start with spaces on a new line
            while (CurrGlyph == ' ')
            {
                Text += 1;
                CurrGlyph = *Text;
            }

            continue;
        }

        // NOTE: We have enough space for the glyph, apply kerning
        CurrPos.x += Kerning;
        
        if (CurrGlyph != ' ')
        {
            char GlyphOffset = CurrGlyph - Font->MinGlyph;

            // NOTE: Glyph dim will be the aabb size, but we offset by align pos since some chars like 'p'
            // need to be descend a bit
            v2 CharDim = Glyph->Dim * CharHeight;
            aabb2 GlyphBounds = AabbMinMax(V2(0, 0), CharDim);
            GlyphBounds = Translate(GlyphBounds, CurrPos + CharHeight*Glyph->AlignPos);

            ui_glyph_job* Job = &UiState->GlyphJob;

            // NOTE: Store black version of glyph
            Assert(Job->NumGlyphs < UiState->MaxNumGlyphs);
            Job->Z[Job->NumGlyphs] = UiGetZ(UiState);
            Job->GlyphOffset[Job->NumGlyphs] = GlyphOffset;
            StoreAabb2(Job->Bounds, Job->NumGlyphs, GlyphBounds);
            StoreV4(Job->Color, Job->NumGlyphs, V4(0, 0, 0, 0));
            Job->ClipRectArray[Job->NumClipRects - 1].NumAffected += 1;
            Job->NumGlyphs += 1;

            // NOTE: Store colored version of glyph
            Assert(Job->NumGlyphs < UiState->MaxNumGlyphs);
            Job->Z[Job->NumGlyphs] = UiGetZ(UiState);
            Job->GlyphOffset[Job->NumGlyphs] = GlyphOffset;
            StoreAabb2(Job->Bounds, Job->NumGlyphs, GlyphBounds);
            StoreV4(Job->Color, Job->NumGlyphs, TintColor);
            Job->ClipRectArray[Job->NumClipRects - 1].NumAffected += 1;
            Job->NumGlyphs += 1;
        }
        
        CurrPos.x += ScaledStepX;
        PrevGlyph = CurrGlyph;
        ++Text;
    }
#endif
}

inline void UiText(ui_state* UiState, ui_constraint Constraint, u32 FontId, aabb2 Bounds, f32 CharHeight, char* Text, v4 TintColor)
{
    v2 BoundsDim = AabbGetDim(Bounds);
    f32 SentenceWidth = BoundsDim.x;
    v2 TopLeftTextPos = V2(Bounds.Min.x, Bounds.Max.y);
    
    UiText(UiState, Constraint, FontId, CharHeight, TopLeftTextPos, SentenceWidth, Text, TintColor);
}

inline void UiText(ui_state* UiState, ui_constraint Constraint, u32 FontId, aabb2 Bounds, char* Text, v4 TintColor)
{
    v2 BoundsDim = AabbGetDim(Bounds);
    f32 SentenceWidth = BoundsDim.x;
    f32 CharHeight = BoundsDim.y;
    v2 TopLeftTextPos = V2(Bounds.Min.x, Bounds.Max.y);
    
    UiText(UiState, Constraint, FontId, CharHeight, TopLeftTextPos, SentenceWidth, Text, TintColor);
}

inline interaction_ref UiElementRef(u32 Type, u32 Id)
{
    interaction_ref Result = {};
    Result.Id = ((u64)Type << 32) | (u64)Id;
    
    return Result;
}

inline ui_button_interaction UiProcessElement(ui_state* UiState, u32 Flags, aabb2 Bounds,
                                              interaction_ref Ref)
{
    // IMPORTANT: We assume pixel space coords, no constraints are handled here
    input_state* InputState = UiState->InputState;

    // NOTE: Check if we add a element interaction
    Assert(!(Flags & UiElementFlag_DontCheckHover));
    b32 Intersects = UiHandleOverlap(UiState, Bounds, Flags);
    if (Intersects)
    {
        interaction Interaction = {};
        Interaction.Type = Interaction_Button;
        Interaction.Ref = Ref;        
        InputAddInteraction(InputState, Interaction);
    }

    // NOTE: Check ui button interaction from last frame
    ui_button_interaction Result = UiButtonInteraction_None;
    if (InputInteractionsAreSame(Ref, InputState->PrevHot.Ref))
    {
        Result = InputState->PrevHot.Button;
    }

    return Result;
}

// NOTE: We do this because macros are dumb in C++, https://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments
#define UI_BUTTON_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,NAME,...) NAME
#define UiButton(...) EXPAND(UI_BUTTON_GET_MACRO(__VA_ARGS__, UiButton10, UiButton9)(__VA_ARGS__))
#define UiButton9(UiState, Constraint, Bounds, Flags, TextureId, FontId, FontSize, TextOffset, Text) UiButton_(UiState, Constraint, Bounds, Flags, TextureId, FontId, FontSize, TextOffset, Text, (u32)UI_FILE_LINE_ID())
#define UiButton10(UiState, Constraint, Bounds, Flags, TextureId, FontId, FontSize, TextOffset, Text, RefId) UiButton_(UiState, Constraint, Bounds, Flags, TextureId, FontId, FontSize, TextOffset, Text, (u32)UI_FILE_LINE_ID() + RefId)
inline ui_button_interaction UiButton_(ui_state* UiState, ui_constraint Constraint, aabb2 Bounds,
                                       u32 Flags, asset_texture_id TextureId, asset_font_id FontId,
                                       f32 FontSize, f32 TextOffset, char* Text, u32 RefId)
{
    Assert(Bounds.Min.x < Bounds.Max.x);
    Assert(Bounds.Min.y < Bounds.Max.y);
    
    Bounds = UiApplyConstraint(UiState, Constraint, Bounds);
    ui_button_interaction Result = UiProcessElement(UiState, Flags, Bounds, UiElementRef(UiType_Button, TextureId.All * RefId));

#if 0
    // NOTE: Animate button
    b32 RenderHighlights = false;
    aabb2 HighlightBounds = {};
    v4 HighlightColor = {};
    
    aabb2 RenderBounds = Bounds;
    if (Result == UiButtonInteraction_Hover)
    {
        RenderHighlights = true;
        HighlightBounds = RenderBounds;
        HighlightColor = V4(1, 1, 1, 1);
    }
    else if (Result == UiButtonInteraction_Selected)
    {
        v2 BoundsDim = AabbGetDim(RenderBounds);
        RenderBounds = Enlarge(RenderBounds, -0.03f*BoundsDim);

        RenderHighlights = true;
        HighlightBounds = RenderBounds;
        HighlightColor = V4(1, 0, 0, 1);
    }
    else if (Result == UiButtonInteraction_Released)
    {
        v2 BoundsDim = AabbGetDim(RenderBounds);
        RenderBounds = Enlarge(RenderBounds, -0.03f*BoundsDim);

        RenderHighlights = true;
        HighlightBounds = RenderBounds;
        HighlightColor = V4(1, 1, 0, 1);
    }

    UiPushTexture(UiState, RenderBounds, TextureId, V4(1, 1, 1, 1));

    if (Text)
    {
        // NOTE: Render Text Bounds represents the area we can render text in
        aabb2 RenderTextBounds = Enlarge(RenderBounds, -V2(TextOffset, TextOffset));
        v2 RenderDim = AabbGetDim(RenderTextBounds);
        
        aabb2 TextBounds = UiGetTextBounds(UiState, UiConstraint_None, FontId, FontSize, V2(0, 0), RenderDim.x, Text);
        v2 TextBoundsDim = AabbGetDim(TextBounds);

        v2 HalfOffset = {};
        
        // NOTE: Center it in x
        Assert(TextBoundsDim.x < RenderDim.x);
        HalfOffset.x = (RenderDim.x - TextBoundsDim.x) * 0.5f;

        // NOTE: Try to center in y
        if (RenderDim.y > TextBoundsDim.y)
        {
            HalfOffset.y = -(RenderDim.y - TextBoundsDim.y) * 0.5f;
        }

        v2 TextStartPos = V2(RenderTextBounds.Min.x, RenderTextBounds.Max.y) + HalfOffset;
        UiText(UiState, UiConstraint_None, FontId, FontSize, TextStartPos, TextBoundsDim.x, Text);
    }
    
    if (RenderHighlights)
    {
        UiRectOutline(UiState, HighlightBounds, HighlightColor, 2);
    }
#endif
    
    return Result;
}

inline void UiScrollMenu(ui_state* UiState, ui_constraint Constraint, u32 FontId, aabb2 MenuBounds, f32 CharHeight, f32 TextPad,
                         f32 SliderWidth, u32 NumOptions, char** Options)
{
#if 0
    // TODO: Convert all UI to use float positions that represent pixels (or fixed point). We want sub pixel precision and it gets weird
    // with some text we are rendering
    Assert(UiState->NumScrollMenus < UiState->MaxNumScrollMenus);

    u32 ScrollId = UiState->NumScrollMenus++;

    MenuBounds = UiApplyConstraint(UiState, Constraint, MenuBounds);

    // NOTE: Draw menu
    UiRect(UiState, UiConstraint_None, 0, MenuBounds, V4(0.0f, 0.0f, 0.0f, 0.7f));
    UiRectOutline(UiState, MenuBounds, V4(0.0f, 0.0f, 0.0f, 1.0f), 10);
    MenuBounds = Enlarge(MenuBounds, V2(-12, -12));

    if (NumOptions == 0)
    {
        return;
    }
    
    v2 MenuDim = AabbGetDim(MenuBounds);
    f32 OptionDimY = CharHeight + 2*TextPad;
    f32* SliderY = UiState->ScrollMenuSave.ScrollPos + ScrollId;
    ui_scroll_result ScrollResult = UiGetScrollData(MenuDim.y, OptionDimY, NumOptions, *SliderY);
        
    aabb2 OptionBounds = AabbMinMax(V2(0.0f, -OptionDimY), V2(MenuDim.x - SliderWidth, 0.0f));
    OptionBounds = Translate(OptionBounds, V2(MenuBounds.Min.x, MenuBounds.Max.y));
    OptionBounds = Translate(OptionBounds, V2(0.0f, ScrollResult.Offset));

    aabb2i ClipBounds = AabbiMinMax(V2i(MenuBounds.Min), V2i(MenuBounds.Max));
    UiSetGlyphClipRect(UiState, ClipBounds);
    
    for (u64 OptionId = ScrollResult.StartElement; OptionId < ScrollResult.EndElement; ++OptionId)
    {
        // NOTE: Map option bounds to render space
        aabb2 RenderBounds = OptionBounds;
        UiText(UiState, UiConstraint_None, FontId, Enlarge(RenderBounds, V2(-TextPad, -TextPad)), Options[OptionId]);

        OptionBounds = Translate(OptionBounds, V2(0.0f, -OptionDimY));
    }

    UiResetGlyphClipRect(UiState);
    
    // NOTE: Create vertical scroll
    // TODO: For something more robust, we want to use a hashing scheme to be able to access our data
    // if the ScrollId changes
    u64 NumShownOptions = ScrollResult.EndElement - ScrollResult.StartElement;
    if (NumShownOptions != i32(NumOptions))
    {
        aabb2 ScrollBounds = AabbMinMax(V2(MenuBounds.Max.x - SliderWidth, MenuBounds.Min.y), MenuBounds.Max);
        UiVerticalSlider(UiState, UiConstraint_None, ScrollBounds, V2(SliderWidth/2, SliderWidth/2), SliderY);
    }
#endif
}

inline void UiDropDown(ui_state* UiState, ui_constraint Constraint, u32 FontId, aabb2 ButtonBounds, f32 TextPad,
                       f32 SliderWidth, u32 NumOptions, u32 MaxNumShownOptions, char** Options, u32* ChosenOption)
{
#if 0
    // TODO: Because we have Z, when we hold the scroll bar and move to the button and let go, we let
    // go of the scrolling bar and click the button. We want the scroll bar to take priority but Z
    // prevents that. I think we should stick to layers, and have scroll bars take priority in a layer
    // when possible. This won't be perfect still, but somehow we need to enforce it. Maybe we add this
    // as a special case to add interaction, where it takes the scroll interaction over other stuff if
    // it was in prev hot
    Assert(UiState->NumDropDowns < UiState->MaxNumDropDowns);
    
    // NOTE: Handle constraints
    ButtonBounds = UiApplyConstraint(UiState, Constraint, ButtonBounds);
    
    u32 DropDownId = UiState->NumDropDowns++;
    interaction_ref DropDownRef = UiElementRef(UiType_DropDown, DropDownId);

    b32 IsOpen = UiState->DropDownSave.IsOpen[DropDownId];
    if (UiProcessElement(UiState, 0, ButtonBounds, DropDownRef) == UiButtonInteraction_Released)
    {
        UiState->DropDownSave.IsOpen[DropDownId] = !UiState->DropDownSave.IsOpen[DropDownId];
        IsOpen = UiState->DropDownSave.IsOpen[DropDownId];
    }

    // NOTE: Draw main drop down button
    UiRect(UiState, UiConstraint_None, 0, ButtonBounds, V4(0.1f, 0.2f, 0.1f, 0.7f));
    // NOTE: This text doesn't have the slider to the right of it so its larger
    aabb2 MainTextBounds = Enlarge(ButtonBounds, -V2(TextPad, TextPad));
    UiText(UiState, UiConstraint_None, FontId, MainTextBounds, Options[*ChosenOption]);
    
    // NOTE: Returns the clicked on option
    if (IsOpen)
    {
        // NOTE: We are working in bottom left pos now
        v2 ButtonPos = ButtonBounds.Min;
        v2 ButtonDim = AabbGetDim(ButtonBounds);
        i32 NumShownOptions = Min(NumOptions, MaxNumShownOptions);
        v2 DropMenuDim = V2(ButtonDim.x, ButtonDim.y*NumShownOptions);

        // NOTE: Shift drop menu to be as low as its dim in y says it should be
        v2 DropMenuPos = ButtonPos - V2(0.0f, DropMenuDim.y);
        aabb2 DropMenuBounds = AabbMinMax(DropMenuPos, DropMenuPos + DropMenuDim);
        UiRect(UiState, UiConstraint_None, 0, DropMenuBounds, V4(0.0f, 0.0f, 0.5f, 1.0f));

        f32* SliderY = UiState->DropDownSave.ScrollPos + DropDownId;
        ui_scroll_result ScrollResult = UiGetScrollData(DropMenuDim.y, ButtonDim.y, NumOptions, *SliderY);

        // NOTE: Draw menu
        {
            aabb2 MenuBounds = AabbMinMax(DropMenuBounds.Min, V2(DropMenuBounds.Max.x - SliderWidth, DropMenuBounds.Max.y));
            v2 MenuDim = AabbGetDim(MenuBounds);
            UiRect(UiState, UiConstraint_None, 0, MenuBounds, V4(0.0f, 0.0f, 0.5f, 1.0f));

            aabb2i ClipBounds = AabbiMinMax(V2i(MenuBounds.Min), V2i(MenuBounds.Max));
            UiSetGlyphClipRect(UiState, ClipBounds);
        
            aabb2 OptionBounds = AabbMinMax(V2(0.0f, -ButtonDim.y), V2(MenuDim.x, 0.0f));
            OptionBounds = Translate(OptionBounds, V2(MenuBounds.Min.x, MenuBounds.Max.y));
            OptionBounds = Translate(OptionBounds, V2(0.0f, ScrollResult.Offset));

            b32 CloseDropDown = false;
            for (u64 OptionId = ScrollResult.StartElement; OptionId < ScrollResult.EndElement; ++OptionId)
            {
                aabb2 RenderBounds = OptionBounds;
                
                v4 TextColor = V4(1, 1, 1, 1);
                ui_button_interaction OptionInteraction = UiProcessElement(UiState, UiConstraint_None, RenderBounds, UiElementRef(UiType_Button, (u32)Options[OptionId]));
                if (OptionInteraction != UiButtonInteraction_None)
                {
                    // TODO: Make color part of settings
                    TextColor = V4(1, 1, 0, 1);
                }
                if (OptionInteraction == UiButtonInteraction_Released)
                {
                    CloseDropDown = true;
                    *ChosenOption = u32(OptionId);
                }

                UiText(UiState, UiConstraint_None, FontId, Enlarge(RenderBounds, -V2(TextPad, TextPad)), Options[OptionId], TextColor);
                OptionBounds = Translate(OptionBounds, V2(0.0f, -ButtonDim.y));
            }

            UiResetGlyphClipRect(UiState);

            // NOTE: We selected a drop down item so close it
            if (CloseDropDown)
            {
                UiState->DropDownSave.IsOpen[DropDownId] = false;
            }
        }

        // NOTE: Create vertical scroll
        // TODO: For something more robust, we want to use a hashing scheme to be able to access our data
        // if the ScrollId changes
        if (NumShownOptions < i32(NumOptions))
        {
            aabb2 SliderBounds = AabbMinMax(V2(DropMenuBounds.Max.x - SliderWidth, DropMenuBounds.Min.y), DropMenuBounds.Max);
            UiVerticalSlider(UiState, UiConstraint_None, SliderBounds, V2(SliderWidth/2, SliderWidth/2), SliderY);
        }
    }
#endif
}

inline void UiDropDown(ui_state* UiState, ui_constraint Constraint, u32 FontId, v2 TopLeftPos, f32 TextPad, f32 TextWidth, f32 CharHeight,
                       f32 SliderWidth, u32 NumOptions, u32 MaxNumShownOptions, char** Options, u32* ChosenOption)
{
    aabb2 ButtonBounds = AabbMinMax(TopLeftPos - V2(0.0f, CharHeight + 2*TextPad), TopLeftPos + V2(TextWidth + 2*TextPad + SliderWidth, 0.0f));
    UiDropDown(UiState, Constraint, FontId, ButtonBounds, TextPad, SliderWidth, NumOptions, MaxNumShownOptions, Options, ChosenOption);
}

// TODO: Concat with vertical slider, the code is almost identical
inline void UiHorizontalSlider(ui_state* UiState, ui_constraint Constraint, aabb2 SliderBounds,
                               v2 KnobRadius, f32* PercentValue)
{
#if 0
    input_state* InputState = UiState->InputState;
    interaction_ref SliderRef = UiElementRef(UiType_HorizontalSlider, u32(uintptr_t(PercentValue)));

    // NOTE: Generate all our collision bounds
    SliderBounds = UiApplyConstraint(UiState, Constraint, SliderBounds);
    aabb2 CollisionBounds = Enlarge(SliderBounds, V2(-KnobRadius.x, 0.0f));
    v2 KnobCenter = V2(Lerp(CollisionBounds.Min.x, CollisionBounds.Max.x, *PercentValue),
                         Lerp(CollisionBounds.Min.y, CollisionBounds.Max.y, 0.5f));
    aabb2 SliderKnob = AabbCenterRadius(KnobCenter, KnobRadius);

    // NOTE: Check for slider interaction (we keep it press and hold and move the mouse off)
    if (InputIsMainPointerOnly(&InputState->CurrInput))
    {
        input_pointer* CurrPointer = &InputState->CurrInput.MainPointer;
        
        b32 IntersectsSlider = UiHandleOverlap(UiState, SliderBounds, 0);
        b32 MouseDownOrReleased = ((CurrPointer->ButtonFlags & MouseButtonFlag_PressedOrHeld) ||
                                   (CurrPointer->ButtonFlags & MouseButtonFlag_Released));
        UiState->MouseTouchingUi = UiState->MouseTouchingUi || IntersectsSlider;    
        if (MouseDownOrReleased && (IntersectsSlider || InputInteractionsAreSame(SliderRef, InputState->PrevHot.Ref)))
        {
            interaction SliderInteraction = {};
            SliderInteraction.Type = Interaction_HorizontalSlider;
            SliderInteraction.Ref = SliderRef;
            SliderInteraction.Slider.Bounds = CollisionBounds;
            SliderInteraction.Slider.Percent = PercentValue;

            InputAddInteraction(InputState, SliderInteraction);
        }
    }
    
    UiRect(UiState, UiConstraint_None, 0, SliderBounds, V4(0.0f, 0.0f, 0.0f, 0.7f));
    UiRect(UiState, UiConstraint_None, 0, SliderKnob, V4(0, 0, 0, 1));
    UiRectOutline(UiState, SliderKnob, V4(0.2f, 0.2f, 0.2f, 1.0f), 2);
#endif
}

inline void UiVerticalSlider(ui_state* UiState, ui_constraint Constraint, aabb2 SliderBounds,
                             v2 KnobRadius, f32* PercentValue)
{
#if 0
    input_state* InputState = UiState->InputState;
    interaction_ref SliderRef = UiElementRef(UiType_HorizontalSlider, u32(uintptr_t(PercentValue)));
    
    // NOTE: Shrink our bounds to fit the knob
    SliderBounds = UiApplyConstraint(UiState, Constraint, SliderBounds);
    aabb2 CollisionBounds = Enlarge(SliderBounds, V2(0.0f, -KnobRadius.y));
    v2 KnobCenter = V2(Lerp(CollisionBounds.Min.x, CollisionBounds.Max.x, 0.5f),
                         Lerp(CollisionBounds.Max.y, CollisionBounds.Min.y, *PercentValue));
    aabb2 SliderKnob = AabbCenterRadius(KnobCenter, KnobRadius);

    // NOTE: Check for slider interaction (we keep it press and hold and move the mouse off)
    if (InputIsMainPointerOnly(&InputState->CurrInput))
    {
        input_pointer* CurrPointer = &InputState->CurrInput.MainPointer;
        
        b32 IntersectsSlider = UiHandleOverlap(UiState, SliderBounds, 0);
        b32 MouseDownOrReleased = ((CurrPointer->ButtonFlags & MouseButtonFlag_PressedOrHeld) ||
                                   (CurrPointer->ButtonFlags & MouseButtonFlag_Released));
        UiState->MouseTouchingUi = UiState->MouseTouchingUi || IntersectsSlider;
        if (MouseDownOrReleased && (IntersectsSlider || InputInteractionsAreSame(SliderRef, InputState->PrevHot.Ref)))
        {
            interaction SliderInteraction = {};
            SliderInteraction.Type = Interaction_VerticalSlider;
            SliderInteraction.Ref = SliderRef;
            SliderInteraction.Slider.Bounds = CollisionBounds;
            SliderInteraction.Slider.Percent = PercentValue;

            InputAddInteraction(InputState, SliderInteraction);
        }
    }
    
    UiRect(UiState, UiConstraint_None, 0, SliderBounds, V4(0.0f, 0.0f, 0.0f, 0.7f));
    UiRect(UiState, UiConstraint_None, 0, SliderKnob, V4(0, 0, 0, 1));
    UiRectOutline(UiState, SliderKnob, V4(0.2f, 0.2f, 0.2f, 1.0f), 2);
#endif
}

//
// NOTE: UiState Functions
//

inline ui_state UiStateCreate()
{
    *UiState = {};
    UiState->BlockArena = BlockArena;
    UiState->InputState = InputState;
    
    UiState->RenderState = RenderState;
    UiState->RenderWidth = RenderState->Settings.Width;
    UiState->RenderHeight = RenderState->Settings.Height;
    UiState->StepZ = 1.0f / 4096.0f;
    UiState->CurrZ = UiState->StepZ;
        
    // NOTE: Allocate font data
    {
        UiState->FontArray = PushArray(Arena, ui_font, Font_Num);

        ui_font* CurrFont = UiState->FontArray;
        for (u32 FontId = 0; FontId < Font_Num; ++FontId, ++CurrFont)
        {
            file_font FileFont = Assets->Fonts[FontId];

            CurrFont->MinGlyph = FileFont.MinGlyph;
            CurrFont->MaxGlyph = FileFont.MaxGlyph;
            CurrFont->MaxAscent = FileFont.MaxAscent;
            CurrFont->MaxDescent = FileFont.MaxDescent;
            CurrFont->LineGap = FileFont.LineGap;

            u32 NumGlyphs = FileFont.MaxGlyph - FileFont.MinGlyph;

            u32 KernArraySize = (u32)(FileFont.GlyphArrayOffset - FileFont.KernArrayOffset);
            CurrFont->KernArray = (f32*)PushSize(Arena, KernArraySize);
            PlatformApi.ReadAssetFile(FileFont.FileId, FileFont.KernArrayOffset, KernArraySize, CurrFont->KernArray);

            u32 GlyphArraySize = sizeof(file_glyph)*NumGlyphs;
            CurrFont->GlyphArray = PushArray(Arena, file_glyph, NumGlyphs);
            PlatformApi.ReadAssetFile(FileFont.FileId, FileFont.GlyphArrayOffset, GlyphArraySize, CurrFont->GlyphArray);
        }
    }

    // NOTE: Allocate ui render data
    {
        UiState->MaxNumClipRects = 50;
        
        UiState->TexturedRectJob = {};
        UiState->TexturedRectJob.ClipRectArray = PushArray(Arena, ui_clip_rect, UiState->MaxNumClipRects);
        BlockListInitSentinel(&UiState->TexturedRectJob.Sentinel);
        
        UiState->MaxNumGlyphs = 50000;
        UiState->GlyphJob = {};
        UiState->GlyphJob.ClipRectArray = PushArray(Arena, ui_clip_rect, UiState->MaxNumClipRects);
        UiState->GlyphJob.GlyphOffset = PushArray(Arena, char, UiState->MaxNumGlyphs);
        UiState->GlyphJob.Z = PushArray(Arena, f32, UiState->MaxNumGlyphs);
        UiState->GlyphJob.Bounds.Min.x = PushArray(Arena, f32, UiState->MaxNumGlyphs);
        UiState->GlyphJob.Bounds.Min.y = PushArray(Arena, f32, UiState->MaxNumGlyphs);
        UiState->GlyphJob.Bounds.Max.x = PushArray(Arena, f32, UiState->MaxNumGlyphs);
        UiState->GlyphJob.Bounds.Max.y = PushArray(Arena, f32, UiState->MaxNumGlyphs);
        UiState->GlyphJob.Color.x = PushArray(Arena, f32, UiState->MaxNumGlyphs);
        UiState->GlyphJob.Color.y = PushArray(Arena, f32, UiState->MaxNumGlyphs);
        UiState->GlyphJob.Color.z = PushArray(Arena, f32, UiState->MaxNumGlyphs);
        UiState->GlyphJob.Color.w = PushArray(Arena, f32, UiState->MaxNumGlyphs);
    }
    
    // NOTE: Allocate ui saved state data
    {
        UiState->MaxNumDropDowns = 1000;
        UiState->NumDropDowns = 0;
        UiState->DropDownSave.IsOpen = PushArray(Arena, b32, UiState->MaxNumDropDowns);
        UiState->DropDownSave.ScrollPos = PushArray(Arena, f32, UiState->MaxNumDropDowns);

        UiState->MaxNumScrollMenus = 1000;
        UiState->NumScrollMenus = 0;
        UiState->ScrollMenuSave.ScrollPos = PushArray(Arena, f32, UiState->MaxNumScrollMenus);

        UiState->MaxNumFadeImages = 100;
        UiState->FadeImageSave = PushArray(Arena, ui_fade_image_saved_state, UiState->MaxNumFadeImages);

        UiState->MaxNumFadeText = 100;
        UiState->FadeTextSave = PushArray(Arena, ui_fade_text_saved_state, UiState->MaxNumFadeText);
    }
}

inline void UiStateBeginFrame(ui_state* UiState)
{
    UiState->MouseTouchingUi = false;
    UiState->ProcessedInteraction = false;

    // NOTE: Set the clip rects
    UiResetGlyphClipRect(UiState);
    UiResetTexturedRectClipRect(UiState);
}

inline void UiStateGenerateRenderJobs(ui_state* UiState)
{
    TIMED_FUNC();

    f32 FrameTime = UiState->InputState->CurrInput.FrameTime;
    render_state* RenderState = UiState->RenderState;
    
    // NOTE: Setup render data
    // TODO: We hard set the font here right now, support multiple fonts
    UiState->GlyphJob.FontId = Font_General;
    RenderAddGlyphJobs(RenderState, 1, &UiState->GlyphJob);
    RenderAddTexturedRectJob(RenderState, &UiState->TexturedRectJob);
    
    // NOTE: Reset ui data
    UiState->NumScrollMenus = 0;
    UiState->NumDropDowns = 0;
    UiState->CurrZ = UiState->StepZ;

    if (InputIsMainPointerOnly(&UiState->InputState->CurrInput))
    {
        input_pointer* CurrPointer = &UiState->InputState->CurrInput.MainPointer;
        // TODO: Should this only be released?
        UiState->ProcessedInteraction = (UiState->MouseTouchingUi &&
                                         (CurrPointer->ButtonFlags & MouseButtonFlag_Released));
    }
}

// TODO: We only need this cuz we are sharing data between ui and rendering and it cannot be invalidated
inline void UiStateEndFrame(ui_state* UiState)
{
    // NOTE: Reset render data
    UiState->GlyphJob.NumGlyphs = 0;
    UiState->GlyphJob.NumClipRects = 0;
    
    UiState->TexturedRectJob.NumTexturedRects = 0;
    UiState->TexturedRectJob.NumClipRects = 0;
    BlockListClear(UiState->BlockArena, &UiState->TexturedRectJob.Sentinel);
}

//
// NOTE: Ui Interaction Handling
//

#if 0
INPUT_INTERACTION_HANDLER(UiInteractionHandler)
{
    b32 Result = false;

    if (!(InputState->Hot.Type == Interaction_Button ||
          InputState->Hot.Type == Interaction_HorizontalSlider ||
          InputState->Hot.Type == Interaction_VerticalSlider ||
          InputState->Hot.Type == Interaction_UiScroll ||
          InputState->Hot.Type == Interaction_DragF32))
    {
        return Result;
    }
    
    input_pointer* MainPointer = &InputState->CurrInput.MainPointer;
    switch (InputState->Hot.Type)
    {
        case Interaction_Button:
        {
            ui_button_interaction* HotButtonInteraction = &InputState->Hot.Button;
            
            if (MainPointer->ButtonFlags & MouseButtonFlag_PressedOrHeld)
            {
                *HotButtonInteraction = UiButtonInteraction_Selected;
                InputState->PrevHot = InputState->Hot;
            }
            else if (MainPointer->ButtonFlags & MouseButtonFlag_Released)
            {
                if (InputInteractionsAreSame(PrevHot.Ref, InputState->Hot.Ref))
                {
                    *HotButtonInteraction = UiButtonInteraction_Released;
                    InputState->PrevHot = InputState->Hot;
                }
            }
            else
            {
                *HotButtonInteraction = UiButtonInteraction_Hover;
                InputState->PrevHot = InputState->Hot;
            }

            Result = true;
        } break;

        case Interaction_HorizontalSlider:
        {
            ui_slider_interaction* HotSliderInteraction = &InputState->Hot.Slider;
                    
            if ((MainPointer->ButtonFlags & MouseButtonFlag_PressedOrHeld) ||
                (MainPointer->ButtonFlags & MouseButtonFlag_Released))
            {
                aabb2 Bounds = HotSliderInteraction->Bounds;
                
                f32 NewPercent = (f32)(MainPointer->PixelPos.x - Bounds.Min.x) / (f32)(Bounds.Max.x - Bounds.Min.x);
                *HotSliderInteraction->Percent = Clamp(NewPercent, 0.0f, 1.0f);
                
                InputState->PrevHot = InputState->Hot;
            }

            Result = true;
        } break;

        case Interaction_VerticalSlider:
        {
            ui_slider_interaction* HotSliderInteraction = &InputState->Hot.Slider;
                    
            if ((MainPointer->ButtonFlags & MouseButtonFlag_PressedOrHeld) ||
                (MainPointer->ButtonFlags & MouseButtonFlag_Released))
            {
                aabb2 Bounds = HotSliderInteraction->Bounds;
                
                f32 NewPercent = (f32)(MainPointer->PixelPos.y - Bounds.Min.y) / (f32)(Bounds.Max.y - Bounds.Min.y);
                *HotSliderInteraction->Percent = Clamp(1.0f - NewPercent, 0.0f, 1.0f);
                
                InputState->PrevHot = InputState->Hot;
            }

            Result = true;
        } break;

        case Interaction_UiScroll:
        {
            ui_scroll_interaction* UiScroll = &InputState->Hot.UiScroll;

            if (MainPointer->ButtonFlags & MouseButtonFlag_PressedOrHeld)
            {
                f32 Dir = 0.0f;
                if (UiScroll->ScrollX)
                {
                    Dir = UiScroll->PrevMouseAxis - MainPointer->PixelPos.x;
                }
                else
                {
                    Dir = MainPointer->PixelPos.y - UiScroll->PrevMouseAxis;
                }
                *UiScroll->CurrScrollVel = UiScroll->ScrollSpeed*Dir;

                DebugPrintLog("%f\n", *UiScroll->CurrScrollVel);
                
                InputState->PrevHot = InputState->Hot;
            }

            Result = true;
        } break;

        case Interaction_DragF32:
        {
            ui_drag_f32_interaction PrevDragF32Interaction = PrevHot.DragF32;
            ui_drag_f32_interaction* HotDragF32Interaction = &InputState->Hot.DragF32;
                    
            if (MainPointer->ButtonFlags & MouseButtonFlag_Pressed)
            {
                HotDragF32Interaction->Start = MainPointer->ScreenPos;
                HotDragF32Interaction->Original = *HotDragF32Interaction->Value;
                HotDragF32Interaction->Interaction = UiButtonInteraction_Selected;

                InputState->PrevHot = InputState->Hot;
            }
            else if ((MainPointer->ButtonFlags & MouseButtonFlag_PressedOrHeld) ||
                     (MainPointer->ButtonFlags & MouseButtonFlag_Released))
            {
                if (InputInteractionsAreSame(PrevHot.Ref, InputState->Hot.Ref))
                {
                    // NOTE: Copy state from prev
                    HotDragF32Interaction->Start = PrevDragF32Interaction.Start;
                    HotDragF32Interaction->Original = PrevDragF32Interaction.Original;

                    // TODO: Make this entirely based on x val since it causes jumps otherwise
                    // NOTE: Update dragged value
                    f32 ScaleSign = Sign(MainPointer->ScreenPos.x - HotDragF32Interaction->Start.x);
                    f32 Dist = ScaleSign*Length(MainPointer->ScreenPos - HotDragF32Interaction->Start);
                    *HotDragF32Interaction->Value = HotDragF32Interaction->Original + Dist;

                    HotDragF32Interaction->Interaction = UiButtonInteraction_Selected;

                    InputState->PrevHot = InputState->Hot;
                }
            }
            else
            {
                HotDragF32Interaction->Interaction = UiButtonInteraction_Hover;

                InputState->PrevHot = InputState->Hot;
            }

            Result = true;
        } break;
    }

    return Result;
}
#endif

#endif


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
