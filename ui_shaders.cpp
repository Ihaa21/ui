#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

struct ui_rect
{
    // NOTE: These values are predivided by resolution
    vec2 Center;
    vec2 Radius;
    
    float Z;
    vec4 Color;
};

struct ui_glyph
{
    // NOTE: These values are predivided by resolution
    vec2 Center;
    vec2 Radius;

    vec2 MinUv;
    vec2 MaxUv;
    vec4 Color;
    float Z;
};

layout(set = 0, binding = 0) buffer ui_rect_array
{
    ui_rect UiRectArray[];
};

layout(set = 0, binding = 1) buffer ui_glyph_array
{
    ui_glyph UiGlyphArray[];
};

layout(set = 0, binding = 2) uniform sampler2D UiGlyphAtlas;

#if RECT_VERTEX

layout(location = 0) in vec3 InPos;

layout(location = 0) out flat uint OutInstanceId;

void main()
{
    ui_rect Rect = UiRectArray[gl_InstanceIndex];

    vec2 UiSpacePos = InPos.xy * Rect.Radius + Rect.Center;
    vec2 NdcPos = 2.0f * UiSpacePos - 1.0f; 
    
    gl_Position = vec4(NdcPos, Rect.Z, 1);
    OutInstanceId = gl_InstanceIndex;
}

#endif

#if RECT_FRAGMENT

layout(location = 0) in flat uint InInstanceId;

layout(location = 0) out vec4 OutColor;

void main()
{
    ui_rect Rect = UiRectArray[InInstanceId];
    OutColor = Rect.Color;
}

#endif

#if GLYPH_VERTEX

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec2 InUv;

layout(location = 0) out flat uint OutInstanceId;
layout(location = 1) out vec2 OutUv;

void main()
{
    ui_glyph Glyph = UiGlyphArray[gl_InstanceIndex];

    vec2 UiSpacePos = InPos.xy * Glyph.Radius + Glyph.Center;
    vec2 NdcPos = 2.0f * UiSpacePos - 1.0f; 
    gl_Position = vec4(NdcPos, Glyph.Z, 1);
    
    OutInstanceId = gl_InstanceIndex;

    // NOTE: Small hack to make 0 -> min uv, 1 -> max uv without branches
    vec2 Uv = InUv;
    Uv.x = (Uv.x == 0) ? Glyph.MinUv.x : Glyph.MaxUv.x;
    // NOTE: Y is reversed due to vulkan coordinates being mapped like this above
    Uv.y = (Uv.y == 0) ? Glyph.MaxUv.y : Glyph.MinUv.y;
    OutUv = Uv;
}

#endif

#if GLYPH_FRAGMENT

layout(location = 0) in flat uint InInstanceId;
layout(location = 1) in vec2 InUv;

layout(location = 0) out vec4 OutColor;

void main()
{
    ui_glyph Glyph = UiGlyphArray[InInstanceId];
    OutColor = texture(UiGlyphAtlas, InUv).xxxx * Glyph.Color;
}

#endif
