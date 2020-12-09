
//
// NOTE: Panel functions
//

// NOTE: Element creation code

#if 0
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
#endif
