#pragma once

//
// NOTE: Ui Panel
//

struct ui_state;
struct ui_panel
{
    ui_state* UiState;

    f32 MaxWidth;
    v2 StartPos;
    f32 MinPosY;
    f32 RowStepY;

    v2 StepGap;
    f32 Size;
    
    v2 CurrPos;
};
