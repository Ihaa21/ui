
//
// NOTE: Ui Panel
//

inline ui_panel UiPanelBegin(ui_state* UiState, v2* TopLeftPos, char* Name)
{
    ui_panel Result = {};
    Result.UiState = UiState;
    Result.TopLeftPos = TopLeftPos; // NOTE: Passed into input to be dragged around
    Result.Name = Name;
    
    // NOTE: Set standard sized values
    {
        // NOTE: All bounds have the min at the top left corner
        
        Result.KnobRadius = V2(20);
        Result.SliderBounds = AabbCenterRadius(V2(100.0f, -Result.KnobRadius.y), V2(100, 5));

        Result.MaxCharHeight = 40.0f;

        v2 NumberBoxRadius = V2(55, 20);
        Result.NumberBoxBounds = AabbCenterRadius(V2(1, -1) * NumberBoxRadius, NumberBoxRadius);

        Result.CheckBoxBounds = AabbCenterRadius(V2(Result.MaxCharHeight/2, -Result.MaxCharHeight/2), V2(Result.MaxCharHeight/2));

        // IMPORTANT: All bounds should be created such that top left is 0,0
        Result.BorderGap = 15.0f;
        Result.ElementGap = 10.0f;
        Result.LineGap = 6.0f;
        Result.TitleHeight = Result.MaxCharHeight + 4.0f;
        Result.IndentAmount = 50.0f;
    }

    // NOTE: Set building variables
    {
        Result.CurrPos = *TopLeftPos + V2(Result.BorderGap, -Result.BorderGap - Result.TitleHeight);
    }

    return Result;
}

inline void UiPanelNextRow(ui_panel* Panel)
{
    Panel->PanelMaxX = Max(Panel->PanelMaxX, Panel->CurrPos.x);
    Panel->CurrPos.x = Panel->TopLeftPos->x + Panel->BorderGap;
    Panel->CurrPos.y -= Panel->CurrRowMaxY + Panel->LineGap;
    Panel->CurrRowMaxY = 0.0f;
}

inline void UiPanelIndent(ui_panel* Panel)
{
    Panel->CurrPos.x += Panel->IndentAmount;
}

inline void UiPanelNextRowIndent(ui_panel* Panel)
{
    UiPanelNextRow(Panel);
    UiPanelIndent(Panel);
}

inline void UiPanelEnd(ui_panel* Panel)
{
    // NOTE: Finis the row we are currently on
    UiPanelNextRow(Panel);
    // NOTE: Undo line gap since we don't have a new line beneath us
    Panel->CurrPos.y += Panel->LineGap;
    
    aabb2 PanelBounds = {};
    PanelBounds.Min.x = Panel->TopLeftPos->x;
    PanelBounds.Min.y = Panel->CurrPos.y - Panel->BorderGap;
    PanelBounds.Max.x = Panel->PanelMaxX + Panel->BorderGap;
    PanelBounds.Max.y = Panel->TopLeftPos->y;

    // NOTE: Title
    {
        aabb2 TitleBounds = {};
        TitleBounds.Min.x = PanelBounds.Min.x;
        TitleBounds.Min.y = PanelBounds.Max.y - Panel->TitleHeight;
        TitleBounds.Max.x = PanelBounds.Max.x;
        TitleBounds.Max.y = PanelBounds.Max.y;

        // NOTE: Calculate interaction with the panel for dragging
        UiDragabbleBoxNoRender(Panel->UiState, TitleBounds, Panel->TopLeftPos);

        aabb2 TextBounds = UiStateGetTextSizeCentered(Panel->UiState, Panel->MaxCharHeight, Panel->Name);
        TextBounds = Translate(TextBounds, AabbGetCenter(TitleBounds));

        UiStatePushTextNoFormat(Panel->UiState, TextBounds, Panel->MaxCharHeight, Panel->Name, V4(1));
        UiRect(Panel->UiState, TitleBounds, V4(0.05f, 0.05f, 0.05f, 1.0f));
    }

    // NOTE: Background
    UiStatePushRectOutline(Panel->UiState, PanelBounds, V4(0.0f, 0.0f, 0.0f, 1.0f), RoundToF32(Panel->BorderGap / 2.0f));
    UiRect(Panel->UiState, PanelBounds, V4(0.1f, 0.4f, 0.7f, 1.0f));
}

inline void UiPanelCheckBox(ui_panel* Panel, b32* Bool)
{
    aabb2 Bounds = Translate(Panel->CheckBoxBounds, Panel->CurrPos);
    UiCheckBox(Panel->UiState, Bounds, Bool);
    
    // NOTE: Increment current position in panel
    Panel->CurrPos.x += AabbGetDim(Bounds).x + Panel->ElementGap;
    Panel->CurrRowMaxY = Max(Panel->CurrRowMaxY, AabbGetDim(Bounds).y);
}

inline void UiPanelHorizontalSlider(ui_panel* Panel, f32 MinValue, f32 MaxValue, f32* PercentValue)
{
    aabb2 SliderBounds = Translate(Panel->SliderBounds, Panel->CurrPos);
    UiHorizontalSlider(Panel->UiState, SliderBounds, Panel->KnobRadius, MinValue, MaxValue, PercentValue);

    // NOTE: Increment current position in panel
    f32 MaxDimY = Max(AabbGetDim(SliderBounds).y, 2*Panel->KnobRadius.y);
    Panel->CurrPos.x += AabbGetDim(SliderBounds).x + Panel->ElementGap;
    Panel->CurrRowMaxY = Max(Panel->CurrRowMaxY, MaxDimY);
}

inline void UiPanelText(ui_panel* Panel, char* Text)
{
    aabb2 TextBounds = UiStateGetTextSizeCentered(Panel->UiState, Panel->MaxCharHeight, Text);
    TextBounds = Translate(TextBounds, Panel->CurrPos + V2(1, -1) * AabbGetRadius(TextBounds));
    UiStatePushTextNoFormat(Panel->UiState, TextBounds, Panel->MaxCharHeight, Text, V4(1, 1, 1, 1));

    // NOTE: Increment current position in panel
    Panel->CurrPos.x += AabbGetDim(TextBounds).x + Panel->ElementGap;
    Panel->CurrRowMaxY = Max(Panel->CurrRowMaxY, AabbGetDim(TextBounds).y);
}

inline void UiPanelNumberBox(ui_panel* Panel, f32 MinValue, f32 MaxValue, f32* FloatValue)
{
    aabb2 Bounds = Translate(Panel->NumberBoxBounds, Panel->CurrPos);
    UiNumberBox(Panel->UiState, Panel->MaxCharHeight, Bounds, MinValue, MaxValue, FloatValue);
    
    // NOTE: Increment current position in panel
    Panel->CurrPos.x += AabbGetDim(Bounds).x + Panel->ElementGap;
    Panel->CurrRowMaxY = Max(Panel->CurrRowMaxY, AabbGetDim(Bounds).y);
}

inline void UiPanelNumberBox(ui_panel* Panel, f32* FloatValue)
{
    UiPanelNumberBox(Panel, F32_MIN, F32_MAX, FloatValue);
}

/*
   TODO: Implement later
inline void UiPanelButton(ui_panel* Panel)
{
}

inline void UiPanelCheckMark(ui_panel* Panel)
{
}
*/
