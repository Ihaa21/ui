/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Ihor Szlachtycz $
   $Notice: (C) Copyright 2014 by Dream.Inc, Inc. All Rights Reserved. $
   ======================================================================== */
        
        // NOTE: This is the API i want
#if 0
        UiSetLayer(2);
        UiDisplayU32("Gold", Gold, V2());
        UiDisplayU32("Lives", Lives, V2());

        if (UiTextureButton(Pos, Size, TextureId_SettingsIcon))
        {
            PlayState->UiPlayingState.SettingsOpen = true;
            ChangeLevelState(PlayState, LevelState_Paused);
        }

        UiBeginPanel();
        UiPanelImage(Size, TextureId_ToolTip);
        UiPanelNextRow();

        for ()
        {
            switch (Button)
            {
                        
            }
        }

        // NOTE: Draws our background?
        UiEndPanel();

        UiSetLayer(1);
        // NOTE: Draw fullscreen game over?
#endif

// TODO: We got the same code in editor
#define UI_FILE_LINE_ID() ((u64)(__FILE__) + (u64)(__LINE__))

//
// NOTE: Ui Render Functions
//

inline f32 UiGetZ(ui_state* UiState)
{
    f32 Result = UiState->CurrZ;
    UiState->CurrZ += UiState->StepZ;

    return Result;
}

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

inline void UiPushTexture(ui_state* UiState, aabb2 Bounds, asset_texture_id TextureId, v4 TintColor)
{
    ui_textured_rect_job* Job = &UiState->TexturedRectJob;

    // NOTE: Record total number of textured rects
    u32 SubIndex = Job->NumTexturedRects & (4 - 1);
    Job->NumTexturedRects += 1;

    block_list_block* Block = Job->Sentinel.Prev;

    // NOTE: Get the textured rect x4 that we will be using
    ui_textured_rect_x4* TexturedRect = 0;
    if (SubIndex == 0)
    {
        TexturedRect = BlockListAddEntryAligned(UiState->BlockArena, &Job->Sentinel, ui_textured_rect_x4, 16);
    }
    else
    {
        TexturedRect = (ui_textured_rect_x4*)Block->Data + Block->NumEntries - 1;
    }

    // NOTE: Count number of textured rects affected by clip rect
    Job->ClipRectArray[Job->NumClipRects - 1].NumAffected += 1;

    aabb2 NormalizedBounds = {};
    NormalizedBounds.Min = Bounds.Min / V2(UiState->RenderWidth, UiState->RenderHeight);
    NormalizedBounds.Max = Bounds.Max / V2(UiState->RenderWidth, UiState->RenderHeight);

    WriteScalar(&TexturedRect->Bounds, SubIndex, NormalizedBounds);
    WriteScalar(&TexturedRect->Z, SubIndex, UiGetZ(UiState));
    WriteScalar(&TexturedRect->TextureId, SubIndex, TextureId.All);
    WriteScalar(&TexturedRect->TintColor, SubIndex, TintColor);
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
    UiImage(UiState, Constraint, Flags, Bounds, AssetTextureId(Texture_White), Color);
}

inline void UiRectOutline(ui_state* UiState, aabb2 Bounds, v4 Color, f32 Thickness)
{
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
    
    return Result;
}

inline void UiScrollMenu(ui_state* UiState, ui_constraint Constraint, u32 FontId, aabb2 MenuBounds, f32 CharHeight, f32 TextPad,
                         f32 SliderWidth, u32 NumOptions, char** Options)
{
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
}

inline void UiDropDown(ui_state* UiState, ui_constraint Constraint, u32 FontId, aabb2 ButtonBounds, f32 TextPad,
                       f32 SliderWidth, u32 NumOptions, u32 MaxNumShownOptions, char** Options, u32* ChosenOption)
{
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
}

inline void UiVerticalSlider(ui_state* UiState, ui_constraint Constraint, aabb2 SliderBounds,
                             v2 KnobRadius, f32* PercentValue)
{
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
}

inline void UiFadeImage(ui_state* UiState, ui_constraint Constraint, aabb2 Bounds,
                        asset_texture_id TextureId, v4 TintColor = V4(1, 1, 1, 1))
{
    Assert(UiState->NumFadeImages < UiState->MaxNumFadeImages);
    ui_fade_image_saved_state* FadeImage = UiState->FadeImageSave + UiState->NumFadeImages++;

    FadeImage->Bounds = Bounds;
    FadeImage->Constraint = Constraint;
    FadeImage->TextureId = TextureId;
    FadeImage->TintColor = TintColor;
}

inline void UiFadeText(ui_state* UiState, ui_constraint Constraint, asset_font_id FontId, f32 CharHeight,
                       v2 TextPos, f32 SentenceWidth, char* Text, v4 TintColor = V4(1, 1, 1, 1))
{
    Assert(UiState->NumFadeText < UiState->MaxNumFadeText);
    ui_fade_text_saved_state* FadeText = UiState->FadeTextSave + UiState->NumFadeText++;
    
    FadeText->Text = Text;
    FadeText->Constraint = Constraint;
    FadeText->FontId = FontId;
    FadeText->CharHeight = CharHeight;
    FadeText->TextPos = TextPos;
    FadeText->SentenceWidth = SentenceWidth;
    FadeText->TintColor = TintColor;
}

//
// NOTE: Panel functions
//

// NOTE: Element creation code

#if 0
UiSetLayer(2);
UiDisplayU32("Gold", Gold, V2());
UiDisplayU32("Lives", Lives, V2());
UiTextureButton(Pos, Size, TextureId_SettingsIcon)
UiSlider();

UiBeginPanel();
UiPanelImage(Size, TextureId_ToolTip);
UiPanelDropDown("AssetId", AssetOptions, NumAssetOptions, &RenderId);
UiPanelF32("PosX", &Pos.x);
UiPanelU32("RoundId", &RoundId);
UiPanelNextRow();
UiEndPanel();
#endif

inline ui_panel UiBeginPanel(ui_state* UiState, v2 StartPos, f32 Size, f32 MaxWidth)
{
    Assert(MaxWidth >= Size);
    
    ui_panel Result = {};
#if 0
    Result.UiState = UiState;
    Result.MaxWidth = MaxWidth;
    Result.StartPos = StartPos;
    Result.MinPosY = 1.0f; // NOTE: Y decreases so we set to 1
    Result.Size = Size;
    Result.StepGap = 0.2f*V2(Size, Size);
    Result.RowStepY = Size;
    Result.CurrPos = StartPos - V2(0.0f, Result.StepGap.y);
#endif    
    return Result;
}

inline void UiPanelNextRow(ui_panel* Panel)
{
#if 0
    Panel->CurrPos.x = Panel->StartPos.x;
    Panel->CurrPos.y -= Panel->RowStepY + Panel->StepGap.y;

    // NOTE: Get the current maximum Y of panel
    Panel->MinPosY = Min(Panel->MinPosY, Panel->CurrPos.y);
#endif
}

// NOTE: We do this because macros are dumb in C++, https://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments
#define UI_PANEL_BUTTON_GET_MACRO(_1,_2,_3,_4,NAME,...) NAME
#define UiPanelButton(...) EXPAND(UI_PANEL_BUTTON_GET_MACRO(__VA_ARGS__, UiPanelButton4, UiPanelButton3, UiPanelButton2)(__VA_ARGS__))
#define UiPanelButton2(Panel, Name) UiPanelButton_(Panel, Name)
#define UiPanelButton3(Panel, Flags, TextureId) UiPanelButton_(Panel, Flags, TextureId, (u32)UI_FILE_LINE_ID())
#define UiPanelButton4(Panel, Flags, TextureId, RefId) UiPanelButton_(Panel, Flags, TextureId, (u32)UI_FILE_LINE_ID() + RefId)
inline ui_button_interaction UiPanelButton_(ui_panel* Panel, u32 Flags, u32 TextureId, u32 RefId)
{
#if 0
    Panel->CurrPos.x += Panel->StepGap.x;
    
    aabb2 ButtonBounds = AabbMinMax(V2(Panel->CurrPos.x, Panel->CurrPos.y - Panel->Size),
                                    V2(Panel->CurrPos.x + Panel->Size, Panel->CurrPos.y));

    // TODO: FIX
#if 0
    if (!(Flags & UiElementFlag_NotAspectCorrect))
    {
        // NOTE: We handle the aspect ratio here so remove the flag to not do it twice
        ButtonBounds = UiMakeAspectCorrect(Panel->UiState->AspectRatio, ButtonBounds);
        Flags |= UiElementFlag_NotAspectCorrect;
    }
#endif
    
    Panel->CurrPos.x += AabbGetDim(ButtonBounds).x;
#endif
    
    // TODO: FIX
    return {}; //UiButton(Panel->UiState, ButtonBounds, Flags, TextureId, RefId);
}

inline ui_button_interaction UiPanelButton_(ui_panel* Panel, char* Name)
{
    ui_button_interaction Result = {};
    
#if 0
    // TODO: Customize??
    u32 FontId = Font_General;
    
    Panel->CurrPos.x += Panel->StepGap.x;

    // NOTE: Calculate text and button bounds
    aabb2 ButtonBounds = {};
    aabb2 TextBounds = {};
    {
        f32 TextInStep = Panel->Size * 0.1f;
        f32 TextMinY = Panel->CurrPos.y - Panel->Size + TextInStep;
        f32 TextMaxY = Panel->CurrPos.y - TextInStep;
        v2 TextStartPos = V2(Panel->CurrPos.x + TextInStep, UiTextGetStartY(Panel->UiState, FontId, TextMinY, TextMaxY));

        // TODO: FIX
        TextBounds = {}; //UiSizeText(Panel->UiState, Font_General, TextStartPos, TextMaxY - TextMinY, Name);
        ButtonBounds = AabbMinMax(V2(TextBounds.Min.x - TextInStep, Panel->CurrPos.y - Panel->Size),
                                  V2(TextBounds.Max.x + TextInStep, Panel->CurrPos.y));
    }

    // NOTE: Make sure our bounds are correct
    f32 Epsilon = 0.001f;
    Assert((ButtonBounds.Max.y - ButtonBounds.Min.y) - Epsilon <= Panel->Size);
    Assert((ButtonBounds.Max.y - ButtonBounds.Min.y) + Epsilon >= Panel->Size);

    ui_button_interaction Result = UiProcessButton(Panel->UiState, ButtonBounds, UiElementRef(UiType_Button, (u32)Name));

    // NOTE: Animate button
    v4 Color = V4(1, 1, 1, 1);
    if (Result == UiButtonInteraction_Hover)
    {
        Color = V4(0, 1, 1, 1);
    }
    else if (Result == UiButtonInteraction_Selected)
    {
        Color = V4(0.4f, 0.6f, 0.6f, 1.0f);
    }

    // TODO: Can this be a text button?
    // TODO: FIX
    //UiRenderText(Panel->UiState, FontId, V2(TextBounds.Min.x, TextBounds.Max.y), Name, Color);
    //UiRect(Panel->UiState, ButtonBounds, 0, V4(0.5f, 0.5f, 1.0f, 1.0f));

    Panel->CurrPos.x += AabbGetDim(ButtonBounds).x;
#endif
    
    return Result;
}

inline void UiPanelDropDown(ui_panel* Panel, char** Options, u32 NumOptions, u32* OptionResult)
{
#if 0
    // TODO: Customize??
    u32 NumShownOptions = 4;
    
    Panel->CurrPos.x += Panel->StepGap.x;

    f32 DropDownWidth = 4*Panel->Size;
    aabb2 DropDownBounds = AabbMinMax(V2(Panel->CurrPos.x, Panel->CurrPos.y - Panel->Size),
                                      V2(Panel->CurrPos.x + DropDownWidth, Panel->CurrPos.y));

    Panel->CurrPos.x += DropDownWidth;

    // TODO: FIX
    //UiDropDown(Panel->UiState, DropDownBounds, NumOptions, NumShownOptions, Options, OptionResult);
#endif
}

inline void UiPanelF32(ui_panel* Panel, char* Name, f32* Val)
{
#if 0
    play_state* InputState = Panel->UiState->InputState;
    // TODO: Customize?
    u32 FontId = Font_General;
    input_mouse* Input = Panel->UiState->Input;
    
    Panel->CurrPos.x += Panel->StepGap.x;

    char NewText[512];
    Snprintf(NewText, sizeof(NewText), "%s: %f", Name, *Val);

    aabb2 TextBounds = {};
    {
        f32 TextMinY = Panel->CurrPos.y - Panel->Size;
        f32 TextMaxY = Panel->CurrPos.y;
        v2 TextStartPos = V2(Panel->CurrPos.x, UiTextGetStartY(Panel->UiState, FontId, TextMinY, TextMaxY));
        // TODO: FIX
        TextBounds = {}; //UiSizeText(Panel->UiState, FontId, TextStartPos, TextMaxY - TextMinY, NewText);
    }

    interaction_ref F32Ref = UiElementRef(UiType_DragF32, (u32)Val);

    b32 IntersectText = UiIntersect(TextBounds, Panel->UiState->Input->ScreenPos);
    b32 MouseDownOrReleased = ((Input->ButtonFlags & MouseButtonFlag_PressedOrHeld) ||
                               (Input->ButtonFlags & MouseButtonFlag_Released));
    Panel->UiState->MouseTouchingUi = Panel->UiState->MouseTouchingUi || IntersectText;
    if (IntersectText || (MouseDownOrReleased && InteractionsAreSame(InputState->PrevHot.Ref, F32Ref)))
    {
        interaction DragF32Interaction = {};
        DragF32Interaction.Type = Interaction_DragF32;
        DragF32Interaction.Ref = F32Ref;
        DragF32Interaction.DragF32.Value = Val;
        
        PlayerAddInteraction(InputState, DragF32Interaction);
    }

    // NOTE: Check ui drag interaction from last frame
    ui_button_interaction Interaction = UiButtonInteraction_None;
    if (InteractionsAreSame(F32Ref, InputState->PrevHot.Ref))
    {
        Interaction = InputState->PrevHot.DragF32.Interaction;
    }

    v4 Color = V4(1, 1, 1, 1);
    if (Interaction == UiButtonInteraction_Hover)
    {
        Color = V4(0, 1, 1, 1);
    }
    else if (Interaction == UiButtonInteraction_Selected)
    {
        Color = V4(0.4f, 0.6f, 0.6f, 1.0f);
    }

    // TODO: FIX
    //UiRenderText(Panel->UiState, FontId, V2(TextBounds.Min.x, TextBounds.Max.y), NewText, Color);

    // NOTE: Update panel
    Panel->CurrPos.x += AabbGetDim(TextBounds).x;
    UiPanelNextRow(Panel);
#endif
}

inline void UiEndPanel(ui_panel* Panel)
{
#if 0
    // NOTE: Get the maximum bounds of panel
    Panel->MinPosY = Min(Panel->MinPosY, Panel->CurrPos.y);

    // NOTE: Add border to the panel
    Panel->MinPosY -= Panel->RowStepY + Panel->StepGap.y;
    
    // NOTE: Create background of the panel
    v2 Min = V2(Panel->StartPos.x, Panel->MinPosY);
    v2 Max = V2(Panel->StartPos.x + Panel->MaxWidth, Panel->StartPos.y);
    // TODO: FIX
    //UiRect(Panel->UiState, AabbMinMax(Min, Max), 0, V4(0, 0, 0, 1));
#endif
}

//
// NOTE: UiState Functions
//

inline void UiStateCreate(ui_state* UiState, linear_arena* Arena, block_arena* BlockArena, input_state* InputState, assets* Assets,
                          render_state* RenderState)
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

    // NOTE: Process fade images
    {
        u32 CurrFadeImage = 0;
        while (CurrFadeImage < UiState->NumFadeImages)
        {
            ui_fade_image_saved_state* FadeImage = UiState->FadeImageSave + CurrFadeImage;

            if (FadeImage->TintColor.a > 0.95)
            {
                FadeImage->TintColor.a -= (0.05f*2.0f*FrameTime);
            }
            else
            {
                FadeImage->TintColor.a -= (0.95f*3.0f*FrameTime);
            }

            if (FadeImage->TintColor.a <= 0.0f)
            {
                // NOTE: Remove the fade image by swapping
                if (UiState->NumFadeImages == 1)
                {
                    UiState->NumFadeImages = 0;
                    continue;
                }
                else
                {
                    *FadeImage = UiState->FadeImageSave[--UiState->NumFadeImages];
                    continue;
                }
            }

            UiImage(UiState, FadeImage->Constraint, 0, FadeImage->Bounds, FadeImage->TextureId, FadeImage->TintColor);

            CurrFadeImage += 1;
        }
    }

    // NOTE: Process fade text
    {
        u32 CurrFadeText = 0;
        while (CurrFadeText < UiState->NumFadeText)
        {
            ui_fade_text_saved_state* FadeText = UiState->FadeTextSave + CurrFadeText;

            if (FadeText->TintColor.a > 0.95)
            {
                FadeText->TintColor.a -= 0.25f*FrameTime;
            }
            else
            {
                FadeText->TintColor.a -= 1.0f*FrameTime;
            }

            if (FadeText->TintColor.a <= 0.0f)
            {
                // NOTE: Remove the fade image by swapping
                if (UiState->NumFadeText == 1)
                {
                    UiState->NumFadeText = 0;
                    continue;
                }
                else
                {
                    *FadeText = UiState->FadeTextSave[--UiState->NumFadeText];
                    continue;
                }
            }
            
            UiText(UiState, FadeText->Constraint, FadeText->FontId, FadeText->CharHeight,
                   FadeText->TextPos, FadeText->SentenceWidth, FadeText->Text, FadeText->TintColor);

            CurrFadeText += 1;
        }
    }
    
    // NOTE: Setup render data
    // TODO: We hard set the font here right now, support multiple fonts
    UiState->GlyphJob.FontId = Font_General;
    RenderAddGlyphJobs(RenderState, 1, &UiState->GlyphJob);
    RenderAddTexturedRectJob(RenderState, &UiState->TexturedRectJob);
    
    // NOTE: Reset ui data
    UiState->NumScrollMenus = 0;
    UiState->NumDropDowns = 0;
    UiState->CurrZ = UiState->StepZ;
    
#if 0
    case ButtonType_Settings:
    {
        settings_data* Data = (settings_data*)CurrElement->Button.Data;
        if (Data->IsInputState)
        {
            Data->InputState->SettingsOpen = true;
            ChangeLevelState(Data->InputState, LevelState_Paused);
        }
        else
        {
            Data->MenuState->SettingsOpen = true;
            // TODO: Fix this
            //GameState->StopTimers = true;
        }
    } break;
                        
    case ButtonType_ChooseLevel:
    {
        u32* MenuState = (u32*)CurrElement->Button.Data;
        // TODO: We want a fade in
        *MenuState = MenuState_LevelSelect;
    } break;

    case ButtonType_LevelSelector:
    {
        level_selector* Selector = (level_selector*)CurrElement->Button.Data;
        if (Selector->VelX == 0.0f)
        {
            *Selector->LevelToPlay = Selector->LevelId;
        }
    } break;
#endif

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

//
// NOTE: Layout functions
//

inline v2 GetCenteredTextPos(aabb2 TextBounds)
{
    // IMPORTANT: Its assumed we are centering around (0, 0)
    v2 Radius = AabbGetRadius(TextBounds);
    v2 Offset = TextBounds.Max - Radius;
    
    aabb2 CenteredBounds = Translate(TextBounds, -Offset);
    
    v2 TopLeftPos = V2(CenteredBounds.Min.x, CenteredBounds.Max.y);
    return TopLeftPos;
}
