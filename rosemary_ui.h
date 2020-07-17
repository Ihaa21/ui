#pragma once

/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Ihor Szlachtycz $
   $Notice: (C) Copyright 2014 by Dream.Inc, Inc. All Rights Reserved. $
   ======================================================================== */

//
// NOTE: Ui Helper Structs
//

struct ui_scroll_result
{
    u64 StartElement;
    u64 EndElement;
    f32 Offset;
};

//
// NOTE: Ui Render data
//

enum ui_text_flag
{
    UiTextFlag_Center = 1 << 0,
};

enum ui_constraint
{
    UiConstraint_None,

    UiConstraint_Center,
    UiConstraint_LeftEdge,
    UiConstraint_RightEdge,
    UiConstraint_TopEdge,
    UiConstraint_BottomEdge,
};

enum ui_element_flags
{
    UiElementFlag_DontCheckHover = 1 << 0,
};

//
// NOTE: Ui Interaction data
//

// TODO: We only need these cuz we don't hash well
enum ui_type
{
    UiType_None,
    
    UiType_Button,
    UiType_DropDown,
    UiType_HorizontalSlider,
    UiType_VerticalSlider,
    UiType_DragF32,
};

enum ui_button_interaction
{
    UiButtonInteraction_None,

    UiButtonInteraction_Hover,
    UiButtonInteraction_Selected,
    UiButtonInteraction_Released,
};

struct ui_slider_interaction
{
    aabb2 Bounds;
    f32* Percent;
};

struct ui_drag_f32_interaction
{
    v2 Start;
    f32 Original;
    f32* Value;
    ui_button_interaction Interaction;
};

struct ui_scroll_interaction
{
    b32 ScrollX;
    f32 PrevMouseAxis;
    f32 ScrollSpeed;
    f32* CurrScrollVel;
};

//
// NOTE: Ui retained data
//

struct ui_dropdown_saved_state
{
    b32* IsOpen;
    f32* ScrollPos;
};

struct ui_scroll_menu_saved_state
{
    f32* ScrollPos;
};

struct ui_fade_image_saved_state
{
    ui_constraint Constraint;
    aabb2 Bounds;
    asset_texture_id TextureId;
    v4 TintColor;
};

struct ui_fade_text_saved_state
{
    ui_constraint Constraint;
    char* Text;
    asset_font_id FontId;
    f32 CharHeight;
    v2 TextPos;
    f32 SentenceWidth;
    v4 TintColor;
};

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

//
// NOTE: Glyph rendering data
//

struct ui_font
{
    // NOTE: Where assuming that the glyphs are in one range
    char MinGlyph;
    char MaxGlyph;

    // NOTE: Vertical font data    
    f32 MaxAscent;
    f32 MaxDescent;
    f32 LineGap;

    f32* KernArray;
    file_glyph* GlyphArray;
};

//
// NOTE: Render jobs
//

// TODO: Make this affect both glyphs and textured rects?
struct ui_clip_rect
{
    aabb2i Bounds;
    u32 NumAffected;
};

struct ui_textured_rect_x4
{
    aabb2_x4 Bounds;
    v1_x4 Z;
    v1u_x4 TextureId;
    v4_x4 TintColor;
};

struct ui_textured_rect_job
{
    u32 NumTexturedRects;

    u32 NumClipRects;
    ui_clip_rect* ClipRectArray;

    block_list_block Sentinel;
};

struct ui_glyph_job
{
    u32 NumClipRects;
    ui_clip_rect* ClipRectArray;

    u32 FontId;
    u32 NumGlyphs;
    char* GlyphOffset;
    f32* Z;
    aabb2_soa Bounds;
    v4_soa Color;
};

// TODO: Store linked list of arrays for each ui element type and keep it in soa format?
// Then you can allocate 4kb arrays of every type and remove some of the switch statements
struct input_state;
struct ui_state
{
    block_arena* BlockArena;
    input_state* InputState;
    render_state* RenderState;

    i32 RenderWidth;
    i32 RenderHeight;
    
    // NOTE: Input data
    b32 MouseTouchingUi;
    b32 ProcessedInteraction;
    
    // NOTE: Font data
    ui_font* FontArray;
    
    // NOTE: For making sure ui gets rendered in submission order
    f32 CurrZ;
    f32 StepZ;
    
    // NOTE: Ui element retained info
    u32 MaxNumDropDowns;
    u32 NumDropDowns;
    ui_dropdown_saved_state DropDownSave;

    u32 MaxNumScrollMenus;
    u32 NumScrollMenus;
    ui_scroll_menu_saved_state ScrollMenuSave;

    u32 MaxNumFadeImages;
    u32 NumFadeImages;
    ui_fade_image_saved_state* FadeImageSave;

    u32 MaxNumFadeText;
    u32 NumFadeText;
    ui_fade_text_saved_state* FadeTextSave;
    
    // NOTE: Render data
    u32 MaxNumClipRects;
    ui_textured_rect_job TexturedRectJob;

    // TODO: Add support for multiple fonts
    u32 MaxNumGlyphs;
    ui_glyph_job GlyphJob;
};

inline void UiSetTexturedRectClipRect(ui_state* UiState, aabb2i Bounds, b32 IsReset = false);
inline void UiResetTexturedRectClipRect(ui_state* UiState);
inline void UiResetGlyphClipRect(ui_state* UiState);

inline void UiVerticalSlider(ui_state* UiState, ui_constraint Constraint, aabb2 SliderBounds,
                             v2 KnobRadius, f32* PercentValue);

inline void UiRect(ui_state* UiState, ui_constraint Constraint, u32 Flags, aabb2 Bounds, v4 Color);
inline void UiRectOutline(ui_state* UiState, aabb2 Bounds, v4 Color, f32 Thickness);
inline aabb2 UiGetTextBounds(ui_state* UiState, ui_constraint Constraint, u32 FontId, f32 CharHeight,
                             v2 TextPos, f32 SentenceWidth, char* Text);
inline void UiText(ui_state* UiState, ui_constraint Constraint, u32 FontId, f32 CharHeight,
                   v2 TextPos, f32 SentenceWidth, char* Text, v4 TintColor = V4(1, 1, 1, 1));
inline void UiText(ui_state* UiState, ui_constraint Constraint, u32 FontId, aabb2 Bounds, char* Text, v4 TintColor = V4(1, 1, 1, 1));
inline void UiText(ui_state* UiState, ui_constraint Constraint, u32 FontId, aabb2 Bounds, f32 CharHeight, char* Text, v4 TintColor = V4(1, 1, 1, 1));

inline void UiScrollMenu(ui_state* UiState, ui_constraint Constraint, u32 FontId, aabb2 MenuBounds, f32 CharHeight, f32 TextPad,
                         f32 SliderWidth, u32 NumOptions, char** Options);
inline void UiDropDown(ui_state* UiState, ui_constraint Constraint, u32 FontId, aabb2 ButtonBounds, f32 TextPad,
                       f32 SliderWidth, u32 NumOptions, u32 MaxNumShownOptions, char** Options, u32* ChosenOption);
