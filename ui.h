#pragma once

/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Ihor Szlachtycz $
   $Notice: (C) Copyright 2014 by Dream.Inc, Inc. All Rights Reserved. $
   ======================================================================== */

#include "math\math.h"
#include "memory\memory.h"

#define UI_HOVER_TEXT_COLOR V4(0.7f, 0.7f, 0.3f, 1.0f)
#define UI_SELECTED_TEXT_COLOR V4(0.9f, 0.9f, 0.1f, 1.0f)

//
// NOTE: Render Data
//

struct ui_clip_rect
{
    aabb2i Bounds;
    u32 NumRectsAffected;
    u32 NumGlyphsAffected;
};

struct ui_render_rect
{
    v2 Center;
    v2 Radius;
    f32 Z;
    u32 Pad[3];
    v4 Color;
};

struct ui_render_glyph
{
    char GlyphOffset;
    f32 Z;
    v2 Center;
    v2 Radius;
    v4 Color;
};

struct ui_render_glyph_gpu
{
    v2 Center;
    v2 Radius;
    v2 MinUv;
    v2 MaxUv;
    v4 Color;
    f32 Z;
    u32 Pad[3];
};

//
// NOTE: Font
//

struct ui_glyph
{
    v2 Dim;
    // NOTE: How we align the glyph along the line of text
    v2 AlignPos;
    f32 StepX;
    v2 MinUv;
    v2 MaxUv;
};

struct ui_font
{
    // TODO: Merge into ui_state?
    // NOTE: Vertical font data    
    f32 MaxAscent;
    f32 MaxDescent;
    f32 LineGap;

    f32* KernArray;
    ui_glyph* GlyphArray;
    // TODO: Add mips
    vk_image Atlas;
    VkSampler AtlasSampler;
};

//
// NOTE: Input
//

#define UI_INPUT_KEY_WAIT_TIME 2.0f
#define UI_MIN_KEY_SPAM_TIME 0.2f

enum ui_button_flags
{
    UiKeyFlag_Down = 1 << 0,
    UiKeyFlag_OutputInput = 1 << 1,
};

struct ui_input_key
{
    u32 Flags;
    f32 TimeTillNextInput; // NOTE: When we notify the UI that the key is down
};

enum ui_mouse_flags
{
    UiMouseFlag_Pressed = 1 << 0,
    UiMouseFlag_Held = 1 << 1,
    UiMouseFlag_Released = 1 << 2,
    UiMouseFlag_PressedOrHeld = 1 << 3,
    UiMouseFlag_HeldOrReleased = 1 << 4,
};

struct ui_frame_input
{
    b32 MouseDown;
    v2 MousePixelPos;
    f32 MouseScroll;
    
    // NOTE: Keyboard input
    union
    {
        ui_input_key Keys[256];
        b32 KeysDown[256];
    };
};

//
// NOTE: Interactions
//

enum ui_interaction_type
{
    UiInteractionType_None,

    UiInteractionType_Hover,
    UiInteractionType_Selected,
    UiInteractionType_Released,
};

struct ui_slider_interaction
{
    aabb2 Bounds;
    f32 MinValue;
    f32 MaxValue;
    f32 MouseRelativeX;
    f32* SliderValue;
};

struct ui_drag_box_interaction
{
    v2* TopLeftPos;
    v2 MouseRelativePos;
};

struct ui_float_number_box_interaction
{
    // TODO: Set below to 16bit, we can't rn cuz it causes math lib stuff to get type collisions...
    i32 SelectedChar;
    i32 StartDrawChar;
    b32 HasDecimal;
    i32 TextLength;
    
    char Text[64];

    f32 MinValue;
    f32 MaxValue;
    f32* OutFloat;
};

enum ui_element_type
{
    UiElementType_None,
    
    UiElementType_Button,
    UiElementType_HorizontalSlider,
    UiElementType_DraggableBox,
    UiElementType_Image,
    UiElementType_FloatNumberBox,
    UiElementType_CheckBox,
};

struct ui_interaction
{
    ui_element_type Type;
    u64 Hash;

    union
    {
        ui_interaction_type Button;
        ui_slider_interaction Slider;
        ui_drag_box_interaction DragBox;
        ui_float_number_box_interaction FloatNumberBox;
    };
};

struct ui_state
{
    VkDevice Device;
    u32 GpuLocalMemoryId;
    vk_linear_arena GpuArena;
    vk_linear_arena GpuTempArena; // NOTE: For single frame data
    
    platform_block_arena CpuBlockArena;
    
    // NOTE: Font data
    ui_font Font;
    
    // NOTE: For making sure ui gets rendered in submission order
    f32 CurrZ;
    f32 StepZ;
    
    u32 RenderWidth;
    u32 RenderHeight;
    vk_image DepthImage;
    VkFramebuffer FrameBuffer;
    VkRenderPass RenderPass;
    
    VkDescriptorPool DescriptorPool;
    VkDescriptorSetLayout UiDescLayout;
    VkDescriptorSet UiDescriptor;
    
    // NOTE: Input data
    f32 FrameTime;
    b32 MouseTouchingUi;
    b32 ProcessedInteraction;
    u32 MouseFlags;
    ui_frame_input CurrInput;
    ui_frame_input PrevInput;
    ui_interaction Hot;
    ui_interaction PrevHot;
    ui_interaction Selected;

    // NOTE: General Render Data
    VkBuffer QuadVertices;
    VkBuffer QuadIndices;

    u32 NumClipRects;
    u32 NumClipRectsPerBlock;
    block_arena ClipRectArena;
    ui_clip_rect* CurrClipRect;
    
    // NOTE: Render Rect Data
    u32 NumRects;
    u32 NumRectsPerBlock;
    block_arena RectArena;
    VkBuffer GpuRectBuffer;
    vk_pipeline* RectPipeline;

    // NOTE: Render Glyph Data
    u32 NumGlyphs;
    u32 NumGlyphsPerBlock;
    block_arena GlyphArena;
    VkBuffer GpuGlyphBuffer;
    vk_pipeline* GlyphPipeline;
};

inline void UiStateResetClipRect(ui_state* UiState);
inline void UiTextBoxAddChar_(char* Ptr, u32 MaxSize, u32* CurrSize, char NewValue);

#include "ui.cpp"
#include "ui_panel.h"
#include "ui_panel.cpp"
