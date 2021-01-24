
//
// NOTE: Ui Panel
//

struct ui_panel
{
    ui_state* UiState;
    v2* TopLeftPos;
    char* Name;

    // NOTE: Building variables
    v2 CurrPos;
    f32 PanelMaxX;
    f32 CurrRowMaxY;

    // NOTE: Standard sized values
    aabb2 SliderBounds;
    v2 KnobRadius;

    f32 MaxCharHeight;

    aabb2 NumberBoxBounds;
    
    f32 TitleHeight;
    f32 BorderGap;
    f32 ElementGap; // NOTE Gap in X between elements
    f32 LineGap; // NOTE: Gap in Y between rows
    f32 IndentAmount;
};
