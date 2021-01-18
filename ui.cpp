/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Ihor Szlachtycz $
   $Notice: (C) Copyright 2014 by Dream.Inc, Inc. All Rights Reserved. $
   ======================================================================== */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define UI_FILE_LINE_ID() ((u64)(__FILE__) + (u64)(__LINE__))

//
// NOTE: Ui General Helpers
//

inline b32 UiIntersect(aabb2 A, v2 B)
{
    // NOTE: Use this so that perfectly placed UI can't have the mouse hit two elements at a time
    b32 Result =  (A.Min.x <= B.x && A.Max.x > B.x &&
                   A.Min.y <= B.y && A.Max.y > B.y);
    return Result;
}

inline b32 UiContained(aabb2 Parent, aabb2 Child)
{
    b32 Result =  (Parent.Min.x <= Child.Min.x && Parent.Max.x >= Child.Max.x &&
                   Parent.Min.y <= Child.Min.y && Parent.Max.y >= Child.Max.y);
    return Result;
}

inline u64 UiElementHash(u32 Type, u32 Id)
{
    u64 Result = 0;
    Result = ((u64)Type << 32) | (u64)Id;
    
    return Result;
}

inline b32 UiInteractionsAreSame(u64 Hash1, u64 Hash2)
{
    b32 Result = Hash1 == Hash2;
    return Result;
}

inline b32 UiInteractionsAreSame(ui_interaction Interaction1, ui_interaction Interaction2)
{
    b32 Result = UiInteractionsAreSame(Interaction1.Hash, Interaction2.Hash);
    return Result;
}

inline b32 UiKeyDown(ui_frame_input* Input, u8 Key)
{
    b32 Result = Input->Keys[Key].Flags & UiKeyFlag_OutputInput;
    return Result;
}

#define UiTextBoxAddChar(Array, CurrSize, SelectedChar, NewValue) UiTextBoxAddChar_((Array), sizeof(Array) / sizeof(char), CurrSize, SelectedChar, NewValue)
inline void UiTextBoxAddChar_(char* Ptr, i32 MaxSize, i32* CurrSize, i32* SelectedChar, char NewValue)
{
    if (*CurrSize < MaxSize)
    {
        // NOTE: Shift all the characters up by 1
        for (i32 ShiftCharId = *CurrSize; ShiftCharId >= *SelectedChar; --ShiftCharId)
        {
            Ptr[ShiftCharId + 1] = Ptr[ShiftCharId];
        }

        Ptr[*SelectedChar] = NewValue;
        *SelectedChar += 1;
        *CurrSize += 1;
    }
}

inline void UiTextBoxRemoveChar(char* Ptr, i32* CurrSize, i32 DeleteCharId)
{
    if (DeleteCharId >= 0 && DeleteCharId < *CurrSize)
    {
        // NOTE: Shift all the characters down by 1
        for (i32 ShiftCharId = DeleteCharId; ShiftCharId < *CurrSize; ++ShiftCharId)
        {
            Ptr[ShiftCharId] = Ptr[ShiftCharId + 1];
        }
        *CurrSize -= 1;
    }
}

//
// NOTE: Ui State Functions
//

inline void UiStateCreate(VkDevice Device, linear_arena* CpuArena, linear_arena* TempArena, u32 LocalMemType,
                          vk_descriptor_manager* DescriptorManager,vk_pipeline_manager* PipelineManager,
                          vk_transfer_manager* TransferManager, VkFormat ColorFormat, ui_state* UiState)
{
    temp_mem TempMem = BeginTempMem(TempArena);
    
    // IMPORTANT: Its assumed this is happening during a transfer operation
    *UiState = {};
    u64 Size = MegaBytes(16);
    UiState->GpuArena = VkLinearArenaCreate(VkMemoryAllocate(Device, LocalMemType, Size), Size);
    UiState->StepZ = 1.0f / 4096.0f;
    UiState->CurrZ = 0.0f;
    
    // NOTE: Quad
    {
        // NOTE: Vertices
        {
            f32 Vertices[] = 
                {
                    -0.5, -0.5, 0,   0, 0,
                    0.5, -0.5, 0,   1, 0,
                    0.5,  0.5, 0,   1, 1,
                    -0.5,  0.5, 0,   0, 1,
                };

            UiState->QuadVertices = VkBufferCreate(Device, &UiState->GpuArena, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                 sizeof(Vertices));
            void* GpuMemory = VkTransferPushWrite(TransferManager, UiState->QuadVertices, sizeof(Vertices),
                                                  BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                  BarrierMask(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT));
            Copy(Vertices, GpuMemory, sizeof(Vertices));
        }

        // NOTE: Indices
        {
            u32 Indices[] =
                {
                    0, 1, 2,
                    2, 3, 0,
                };

            UiState->QuadIndices = VkBufferCreate(Device, &UiState->GpuArena, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                sizeof(Indices));
            void* GpuMemory = VkTransferPushWrite(TransferManager, UiState->QuadIndices, sizeof(Indices),
                                                  BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                  BarrierMask(VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT));
            Copy(Indices, GpuMemory, sizeof(Indices));
        }
    }
    
    // NOTE: Ui Descriptor Set
    {
        VkDescriptorPoolSize Pools[2] = {};
        Pools[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        Pools[0].descriptorCount = 2;
        Pools[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        Pools[1].descriptorCount = 1;
            
        VkDescriptorPoolCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        CreateInfo.maxSets = 1;
        CreateInfo.poolSizeCount = ArrayCount(Pools);
        CreateInfo.pPoolSizes = Pools;
        VkCheckResult(vkCreateDescriptorPool(Device, &CreateInfo, 0, &UiState->DescriptorPool));

        vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&UiState->UiDescLayout);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        VkDescriptorLayoutEnd(Device, &Builder);

        UiState->UiDescriptor = VkDescriptorSetAllocate(Device, UiState->DescriptorPool, UiState->UiDescLayout);
    }

    // NOTE: Load our font
    {
        ui_font* Font = &UiState->Font;
        
        // IMPORTANT: We are setting the resolution of each glyph here
        f32 CharHeight = 160.0f;
        u32 NumGlyphsToLoad = 255;

        // NOTE: Load font file
        u8* TtfData = 0;
        {
            FILE* TtfFile = fopen("C:\\Windows\\Fonts\\Arial.ttf", "rb");

            fseek(TtfFile, 0, SEEK_END);
            uint TtfFileSize = ftell(TtfFile);
            fseek(TtfFile, 0, SEEK_SET);
            TtfData = (u8*)PushSize(TempArena, TtfFileSize);
            
            fread(TtfData, TtfFileSize, 1, TtfFile);

            fclose(TtfFile);
        }
        
        // NOTE: Populate atlas data
        u32 AtlasWidth = 2048;
        u32 AtlasHeight = 2048;
        stbtt_packedchar* StbCharData = PushArray(TempArena, stbtt_packedchar, NumGlyphsToLoad);
        {
            u32 AtlasSize = AtlasWidth*AtlasHeight*sizeof(u8);

            Font->Atlas = VkImageCreate(Device, &UiState->GpuArena, AtlasWidth, AtlasHeight, VK_FORMAT_R8_UNORM,
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
            u8* AtlasPixels = VkTransferPushWriteImage(TransferManager, Font->Atlas.Image, AtlasWidth, AtlasHeight, sizeof(u8),
                                                       VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                       BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                       BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));

            Font->AtlasSampler = VkSamplerCreate(Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f);
            
            VkDescriptorImageWrite(DescriptorManager, UiState->UiDescriptor, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Font->Atlas.View, Font->AtlasSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            stbtt_pack_context PackContext;
            i32 Return = 0;
        
            stbtt_PackSetOversampling(&PackContext, 2, 2);
            Return = stbtt_PackBegin(&PackContext, AtlasPixels, AtlasWidth, AtlasHeight, 0, 1, 0);
            Assert(Return == 1);

            Return = stbtt_PackFontRange(&PackContext, TtfData, 0, CharHeight, 0, NumGlyphsToLoad, StbCharData);
            Assert(Return == 1);
        
            stbtt_PackEnd(&PackContext);
        }

        // NOTE: Populate font meta data
        stbtt_fontinfo FontInfo;
        // NOTE: These values convert kerning and step from stbtt space to our 0->1 space
        f32 StbToUiWidth;
        f32 StbToUiHeight;
        f32 StbToPixelsHeight;
        f32 PixelsToUvWidth = 1.0f / (f32)AtlasWidth;
        f32 PixelsToUvHeight = 1.0f / (f32)AtlasHeight;
        {
            stbtt_InitFont(&FontInfo, TtfData, stbtt_GetFontOffsetForIndex(TtfData, 0));

            i32 MaxAscent, MaxDescent, LineGap;
            stbtt_GetFontVMetrics(&FontInfo, &MaxAscent, &MaxDescent, &LineGap);

            Assert(MaxAscent > 0);
            Assert(MaxDescent < 0);

            StbToUiHeight = 1.0f / (f32)(MaxAscent - MaxDescent);
            StbToUiWidth = StbToUiHeight;
        
            Font->MaxAscent = StbToUiHeight * (f32)MaxAscent;
            Font->MaxDescent = StbToUiHeight * (f32)MaxDescent;
            Font->LineGap = StbToUiHeight * (f32)LineGap;

            StbToPixelsHeight = stbtt_ScaleForPixelHeight(&FontInfo, CharHeight);
        }

        // NOTE: Populate kerning array
        {
            Font->KernArray = PushArray(CpuArena, f32, Square(NumGlyphsToLoad));
            ZeroMem(Font->KernArray, sizeof(f32)*Square(NumGlyphsToLoad));
        
            for (char Glyph1 = 0; Glyph1 < char(NumGlyphsToLoad); ++Glyph1)
            {
                for (char Glyph2 = 0; Glyph2 < char(NumGlyphsToLoad); ++Glyph2)
                {            
                    u32 ArrayIndex = Glyph2*NumGlyphsToLoad + Glyph1;
                    f32* CurrKern = Font->KernArray + Glyph2*NumGlyphsToLoad + Glyph1;
                    *CurrKern = StbToUiWidth*(f32)stbtt_GetCodepointKernAdvance(&FontInfo, Glyph1, Glyph2);

                    Assert(Abs(*CurrKern) >= 0 && Abs(*CurrKern) < 1);
                }
            }
        }

        // NOTE: Populate glyph placement/render data
        {
            Font->GlyphArray = PushArray(CpuArena, ui_glyph, NumGlyphsToLoad);

            ui_glyph* CurrGlyph = Font->GlyphArray;
            for (u32 GlyphId = 0; GlyphId < NumGlyphsToLoad; ++GlyphId, ++CurrGlyph)
            {
                stbtt_packedchar CharData = StbCharData[GlyphId];
            
                CurrGlyph->MinUv.x = CharData.x0 * PixelsToUvWidth;
                CurrGlyph->MinUv.y = CharData.y1 * PixelsToUvHeight;
                CurrGlyph->MaxUv.x = CharData.x1 * PixelsToUvWidth;
                CurrGlyph->MaxUv.y = CharData.y0 * PixelsToUvHeight;

                i32 StepX;
                stbtt_GetCodepointHMetrics(&FontInfo, GlyphId, &StepX, 0);
                CurrGlyph->StepX = StbToUiWidth*(f32)StepX;

                Assert(CurrGlyph->StepX > 0 && CurrGlyph->StepX < 1);

                int MinX, MinY, MaxX, MaxY;
                stbtt_GetCodepointBox(&FontInfo, GlyphId, &MinX, &MinY, &MaxX, &MaxY);

                // NOTE: Sometimes spaces have messed up dim y's
                if (GlyphId != ' ')
                {
                    CurrGlyph->Dim = V2(StbToUiWidth, StbToUiHeight)*V2(MaxX - MinX, MaxY - MinY);
                    CurrGlyph->AlignPos = V2(StbToUiWidth, StbToUiHeight)*V2(MinX, MinY);

                    Assert(CurrGlyph->Dim.x + CurrGlyph->AlignPos.x <= 1.0f);
                    Assert(CurrGlyph->Dim.y + CurrGlyph->AlignPos.y <= 1.0f);
                }
            }
        }
    }

    // NOTE: Rect Color Target
    {        
        vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(TempArena);
        u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, ColorFormat, VK_ATTACHMENT_LOAD_OP_LOAD,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        u32 DepthId = VkRenderPassAttachmentAdd(&RpBuilder, VK_FORMAT_D32_SFLOAT, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        VkRenderPassSubPassBegin(&RpBuilder, VK_PIPELINE_BIND_POINT_GRAPHICS);
        VkRenderPassColorRefAdd(&RpBuilder, ColorId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderPassDepthRefAdd(&RpBuilder, DepthId, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        VkRenderPassSubPassEnd(&RpBuilder);
        UiState->RenderPass = VkRenderPassBuilderEnd(&RpBuilder, Device);
    }

    // NOTE: Clip Rect Data
    {
        UiState->MaxNumClipRects = 100;
        UiState->ClipRectArray = PushArray(CpuArena, ui_clip_rect, UiState->MaxNumClipRects);
    }
    
    // NOTE: Rect Data
    {
        UiState->MaxNumRects = 1000;
        UiState->RectArray = PushArray(CpuArena, ui_render_rect, UiState->MaxNumRects);
        UiState->GpuRectBuffer = VkBufferCreate(Device, &UiState->GpuArena, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                              sizeof(ui_render_rect) * UiState->MaxNumRects);
        VkDescriptorBufferWrite(DescriptorManager, UiState->UiDescriptor, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, UiState->GpuRectBuffer);

        vk_pipeline_builder Builder = VkPipelineBuilderBegin(TempArena);

        // NOTE: Shaders
        VkPipelineShaderAdd(&Builder, "..\\libs\\ui\\ui_rect_vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
        VkPipelineShaderAdd(&Builder, "..\\libs\\ui\\ui_rect_frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
                
        // NOTE: Specify input vertex data format
        VkPipelineVertexBindingBegin(&Builder);
        VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32B32_SFLOAT, sizeof(v3));
        VkPipelineVertexAttributeAddOffset(&Builder, sizeof(v2));
        VkPipelineVertexBindingEnd(&Builder);

        VkPipelineInputAssemblyAdd(&Builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
        VkPipelineDepthStateAdd(&Builder, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);

        VkPipelineColorAttachmentAdd(&Builder, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                     VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);

        VkDescriptorSetLayout DescriptorLayouts[] =
            {
                UiState->UiDescLayout,
            };
            
        UiState->RectPipeline = VkPipelineBuilderEnd(&Builder, Device, PipelineManager, UiState->RenderPass, 0, DescriptorLayouts,
                                                   ArrayCount(DescriptorLayouts));
    }

    // NOTE: Glyph Data
    {
        UiState->MaxNumGlyphs = 10000;
        UiState->GlyphArray = PushArray(CpuArena, ui_render_glyph, UiState->MaxNumGlyphs);
        UiState->GpuGlyphBuffer = VkBufferCreate(Device, &UiState->GpuArena, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                 sizeof(ui_render_glyph) * UiState->MaxNumGlyphs);
        VkDescriptorBufferWrite(DescriptorManager, UiState->UiDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, UiState->GpuGlyphBuffer);

        vk_pipeline_builder Builder = VkPipelineBuilderBegin(TempArena);

        // NOTE: Shaders
        VkPipelineShaderAdd(&Builder, "..\\libs\\ui\\ui_glyph_vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
        VkPipelineShaderAdd(&Builder, "..\\libs\\ui\\ui_glyph_frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
                
        // NOTE: Specify input vertex data format
        VkPipelineVertexBindingBegin(&Builder);
        VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32B32_SFLOAT, sizeof(v3));
        VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32_SFLOAT, sizeof(v2));
        VkPipelineVertexBindingEnd(&Builder);

        VkPipelineInputAssemblyAdd(&Builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
        VkPipelineDepthStateAdd(&Builder, VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS);

        VkPipelineColorAttachmentAdd(&Builder, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                     VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);

        VkDescriptorSetLayout DescriptorLayouts[] =
            {
                UiState->UiDescLayout,
            };
            
        UiState->GlyphPipeline = VkPipelineBuilderEnd(&Builder, Device, PipelineManager, UiState->RenderPass, 0, DescriptorLayouts,
                                                   ArrayCount(DescriptorLayouts));
    }

    VkDescriptorManagerFlush(Device, DescriptorManager);
    UiState->GpuTempMem = VkBeginTempMem(&UiState->GpuArena);

    EndTempMem(TempMem);
}

inline void UiStateBegin(ui_state* UiState, VkDevice Device, f32 FrameTime, u32 RenderWidth, u32 RenderHeight, ui_frame_input CurrRawInput)
{
    UiState->FrameTime = FrameTime;
    
    if (UiState->RenderWidth != RenderWidth || UiState->RenderWidth != RenderWidth)
    {
        VkEndTempMem(UiState->GpuTempMem);
        UiState->RenderWidth = RenderWidth;
        UiState->RenderHeight = RenderHeight;
        UiState->DepthImage = VkImageCreate(Device, &UiState->GpuArena, UiState->RenderWidth, UiState->RenderHeight, VK_FORMAT_D32_SFLOAT,
                                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    // NOTE: Ui Input
    {
        UiState->PrevInput = UiState->CurrInput;
        UiState->CurrInput = CurrRawInput;

        ui_frame_input* CurrInput = &UiState->CurrInput;
        ui_frame_input* PrevInput = &UiState->PrevInput;
        
        // NOTE: Handle Mouse input
        {
            UiState->MouseFlags = 0;
            if (UiState->CurrInput.MouseDown && !UiState->PrevInput.MouseDown)
            {
                UiState->MouseFlags |= UiMouseFlag_Pressed;
            }
            if (!UiState->CurrInput.MouseDown && UiState->PrevInput.MouseDown)
            {
                UiState->MouseFlags |= UiMouseFlag_Released;
            }
            if (UiState->CurrInput.MouseDown)
            {
                UiState->MouseFlags |= UiMouseFlag_PressedOrHeld;
            }
            if (UiState->CurrInput.MouseDown || (UiState->MouseFlags & UiMouseFlag_Released))
            {
                UiState->MouseFlags |= UiMouseFlag_HeldOrReleased;
            }
        }
        
        // NOTE: Handle keyboard inputs (we control for how many times the key is reported when held / time since next input upon press)
        {
            u32 HandledInputs = 0;
            for (u32 KeyId = 0; KeyId < ArrayCount(CurrRawInput.KeysDown); ++KeyId)
            {
                ui_input_key* PrevKey = PrevInput->Keys + KeyId;
                ui_input_key* CurrKey = CurrInput->Keys + KeyId;

                *CurrKey = {};
                CurrKey->TimeTillNextInput = Max(0.0f, PrevKey->TimeTillNextInput - FrameTime);

                // NOTE: Stagger when the UI sees input logic
                if (CurrRawInput.KeysDown[KeyId])
                {
                    CurrKey->Flags |= UiKeyFlag_Down;
                    
                    if ((PrevKey->Flags & UiKeyFlag_Down) == 0)
                    {
                        // NOTE: Key was just pressed
                        CurrKey->Flags |= UiKeyFlag_OutputInput;
                        CurrKey->TimeTillNextInput = UI_INPUT_KEY_WAIT_TIME;
                    }
                    else if (CurrKey->TimeTillNextInput == 0.0f)
                    {
                        // NOTE: Key is held
                        CurrKey->Flags |= UiKeyFlag_OutputInput;
                        CurrKey->TimeTillNextInput = UI_MIN_KEY_SPAM_TIME;
                    }
                }
            }
        }
    }
    
    // NOTE: Reset our values since we are immediate mode
    {
        UiState->CurrZ = 0.0f;
        UiState->NumRects = 0;
        UiState->NumGlyphs = 0;
        UiState->MouseTouchingUi = false;
        UiState->ProcessedInteraction = false;
        UiState->NumClipRects = 0;
        UiStateResetClipRect(UiState);
    }
}

inline void UiStateEnd(ui_state* UiState)
{
    // NOTE: Handle interactions
    {
        ui_frame_input* CurrInput = &UiState->CurrInput;
        ui_frame_input* PrevInput = &UiState->PrevInput;
        ui_interaction* Hot = &UiState->Hot;
        ui_interaction* Selected = &UiState->Selected;
        switch (Hot->Type)
        {
            case UiElementType_Button:
            {
                if (CurrInput->MouseDown)
                {
                    Hot->Button = UiInteractionType_Selected;
                    UiState->PrevHot = UiState->Hot;
                }
                else if (UiState->MouseFlags & UiMouseFlag_Released)
                {
                    if (UiInteractionsAreSame(UiState->PrevHot, *Hot))
                    {
                        Hot->Button = UiInteractionType_Released;
                        UiState->PrevHot = UiState->Hot;
                    }
                }
                else
                {
                    Hot->Button = UiInteractionType_Hover;
                    UiState->PrevHot = UiState->Hot;
                }
            } break;

            case UiElementType_HorizontalSlider:
            {
                ui_slider_interaction Slider = Hot->Slider;
                    
                if (UiState->MouseFlags & UiMouseFlag_HeldOrReleased)
                {
                    aabb2 Bounds = Slider.Bounds;
                    f32 MouseAdjustedX = CurrInput->MousePixelPos.x + Slider.MouseRelativeX;
                    f32 NewPercent = (f32)(MouseAdjustedX - Bounds.Min.x) / (f32)(Bounds.Max.x - Bounds.Min.x);
                    NewPercent = Clamp(NewPercent, 0.0f, 1.0f);
                    *Slider.SliderValue = Lerp(Slider.MinValue, Slider.MaxValue, NewPercent);
                    
                    UiState->PrevHot = UiState->Hot;
                }
            } break;

            case UiElementType_DraggableBox:
            {
                ui_drag_box_interaction DragBox = Hot->DragBox;

                if (CurrInput->MouseDown)
                {
                    *DragBox.TopLeftPos = CurrInput->MousePixelPos + DragBox.MouseRelativePos;
                    UiState->PrevHot = UiState->Hot;
                }
            } break;

            case UiElementType_Image:
            {
                if (CurrInput->MouseDown)
                {
                    UiState->PrevHot = UiState->Hot;
                }
            } break;

            case UiElementType_FloatNumberBox:
            {
                ui_float_number_box_interaction* NumberBox = &Hot->FloatNumberBox;

                if (UiState->MouseFlags & UiMouseFlag_Released)
                {
                    UiState->Selected = UiState->Hot;
                }
                else if (UiState->MouseFlags & UiMouseFlag_PressedOrHeld)
                {
                    UiState->PrevHot = UiState->Hot;
                }
            } break;
            
            default:
            {
                // NOTE: We don't interact with anything so clear our prev hot
                UiState->PrevHot = {};
            } break;
        }

        // NOTE: Handle selected interactions
        switch (Selected->Type)
        {
            case UiElementType_FloatNumberBox:
            {
                ui_float_number_box_interaction* NumberBox = &Selected->FloatNumberBox;

                // TODO: Handle the windows cmd keys in a cleaner way? The '.' ascii doesn't match the VKCOde
                // TODO: Why are these capital?? Windows seems to return this
                // TODO: Handle ctrl all inputs to delete
                if (UiKeyDown(CurrInput, 'A'))
                {
                    NumberBox->SelectedChar = Clamp(NumberBox->SelectedChar - 1, 0, NumberBox->TextLength);
                }
                if (UiKeyDown(CurrInput, 'D'))
                {
                    NumberBox->SelectedChar = Clamp(NumberBox->SelectedChar + 1, 0, NumberBox->TextLength);
                }
                if (UiKeyDown(CurrInput, VK_BACK))
                {
                    NumberBox->HasDecimal = NumberBox->HasDecimal && NumberBox->Text[NumberBox->SelectedChar - 1] != '.';
                    UiTextBoxRemoveChar(NumberBox->Text, &NumberBox->TextLength, NumberBox->SelectedChar - 1);
                    NumberBox->SelectedChar = Max(0, NumberBox->SelectedChar - 1);
                }
                if (UiKeyDown(CurrInput, VK_DELETE))
                {
                    NumberBox->HasDecimal = NumberBox->HasDecimal && NumberBox->Text[NumberBox->SelectedChar] != '.';
                    UiTextBoxRemoveChar(NumberBox->Text, &NumberBox->TextLength, NumberBox->SelectedChar);
                }
                if (UiKeyDown(CurrInput, u8(VK_OEM_PERIOD)) && !NumberBox->HasDecimal)
                {
                    UiTextBoxAddChar(NumberBox->Text, &NumberBox->TextLength, &NumberBox->SelectedChar, '.');
                    NumberBox->HasDecimal = true;
                }

                // NOTE: Handle digit key inputs
                for (char DigitKey = '0'; DigitKey <= '9'; ++DigitKey)
                {
                    if (UiKeyDown(CurrInput, DigitKey))
                    {
                        UiTextBoxAddChar(NumberBox->Text, &NumberBox->TextLength, &NumberBox->SelectedChar, DigitKey);
                    }
                }

                // NOTE: Handles unselecting when we interact with something else or click away from the element
                if ((UiState->Hot.Type != UiElementType_None && !UiInteractionsAreSame(*Selected, *Hot)) ||
                    UiState->Hot.Type == UiElementType_None && UiState->MouseFlags != 0)
                {
                    // TODO: Write out the text to float and save it
                    UiState->Selected = {};
                }

                UiState->ProcessedInteraction = true;
            } break;
        }

        // NOTE: Clear hot interaction for next frame
        *Hot = {};
    }
    
    // NOTE: Upload rects to GPU
    if (UiState->NumRects > 0)
    {
        ui_render_rect* GpuData = VkTransferPushWriteArray(&RenderState->TransferManager, UiState->GpuRectBuffer, ui_render_rect, UiState->NumRects,
                                                           BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                           BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));

        v2 InvResolution = V2(1.0f) / V2(UiState->RenderWidth, UiState->RenderHeight);
        for (uint RectId = 0; RectId < UiState->NumRects; ++RectId)
        {
            ui_render_rect* CurrRect = UiState->RectArray + RectId;

            ui_render_rect* GpuRect = GpuData + RectId;
            // NOTE: Normalize the rect to be in 0-1 range, x points right, y points up
            GpuRect->Center = CurrRect->Center * InvResolution;
            GpuRect->Center.y = 1.0f - GpuRect->Center.y;
            GpuRect->Radius = 2.0f * CurrRect->Radius * InvResolution;

            GpuRect->Z = CurrRect->Z;
            GpuRect->Color = PreMulAlpha(CurrRect->Color);
        }
    }

    // NOTE: Upload glyphs to GPU
    if (UiState->NumGlyphs > 0)
    {
        ui_render_glyph_gpu* GpuData = VkTransferPushWriteArray(&RenderState->TransferManager, UiState->GpuGlyphBuffer, ui_render_glyph_gpu, UiState->NumGlyphs,
                                                                BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                                BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));

        // NOTE: Loop back to front so reverse order (we want to render back to front)
        v2 InvResolution = V2(1.0f) / V2(UiState->RenderWidth, UiState->RenderHeight);
        for (uint GlyphId = 0; GlyphId < UiState->NumGlyphs; ++GlyphId)
        {
            ui_render_glyph* CurrGlyph = UiState->GlyphArray + GlyphId;

            ui_render_glyph_gpu* GpuGlyph = GpuData + (UiState->NumGlyphs - GlyphId - 1);
            // NOTE: Normalize the glyph to be in 0-1 range, x points right, y points up
            GpuGlyph->Center = CurrGlyph->Center * InvResolution;
            GpuGlyph->Center.y = 1.0f - GpuGlyph->Center.y;
            GpuGlyph->Radius = 2.0f * CurrGlyph->Radius * InvResolution;

            ui_glyph* UiGlyph = UiState->Font.GlyphArray + CurrGlyph->GlyphOffset;
            GpuGlyph->MinUv = UiGlyph->MinUv;
            GpuGlyph->MaxUv = UiGlyph->MaxUv;
            
            GpuGlyph->Z = CurrGlyph->Z;
            GpuGlyph->Color = PreMulAlpha(CurrGlyph->Color);
        }
    }
}

inline void UiStateRender(ui_state* UiState, VkDevice Device, vk_commands Commands, VkImageView ColorView)
{
    VkImageView Views[] =
    {
        ColorView,
        UiState->DepthImage.View,
    };
    VkFboReCreate(Device, UiState->RenderPass, Views, ArrayCount(Views), &UiState->FrameBuffer, UiState->RenderWidth,
                  UiState->RenderHeight);

    VkClearValue ClearValues[] =
    {
        VkClearColorCreate(0, 0, 0, 0),
        VkClearDepthStencilCreate(1, 0),
    };
    
    VkRenderPassBeginInfo BeginInfo = {};
    BeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    BeginInfo.renderPass = UiState->RenderPass;
    BeginInfo.framebuffer = UiState->FrameBuffer;
    BeginInfo.renderArea.offset = { 0, 0 };
    BeginInfo.renderArea.extent = { UiState->RenderWidth, UiState->RenderHeight };
    BeginInfo.clearValueCount = ArrayCount(ClearValues);
    BeginInfo.pClearValues = ClearValues;
    vkCmdBeginRenderPass(Commands.Buffer, &BeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport ViewPort = {};
    ViewPort.x = 0;
    ViewPort.y = 0;
    ViewPort.width = f32(UiState->RenderWidth);
    ViewPort.height = f32(UiState->RenderHeight);
    ViewPort.minDepth = 0.0f;
    ViewPort.maxDepth = 1.0f;
    vkCmdSetViewport(Commands.Buffer, 0, 1, &ViewPort);

    // NOTE: Render Rects
    {
        vkCmdBindPipeline(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UiState->RectPipeline->Handle);
        VkDescriptorSet DescriptorSets[] =
            {
                UiState->UiDescriptor,
            };
        vkCmdBindDescriptorSets(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UiState->RectPipeline->Layout, 0,
                                ArrayCount(DescriptorSets), DescriptorSets, 0, 0);
        
        VkDeviceSize Offset = 0;
        vkCmdBindVertexBuffers(Commands.Buffer, 0, 1, &UiState->QuadVertices, &Offset);
        vkCmdBindIndexBuffer(Commands.Buffer, UiState->QuadIndices, 0, VK_INDEX_TYPE_UINT32);

        u32 CurrRectId = 0;
        for (u32 ClipRectId = 0; ClipRectId < UiState->NumClipRects; ++ClipRectId)
        {
            ui_clip_rect* ClipRect = UiState->ClipRectArray + ClipRectId;

            if (ClipRect->NumRectsAffected > 0)
            {
                VkRect2D Scissor = {};
                Scissor.extent.width = AabbGetDim(ClipRect->Bounds).x;
                Scissor.extent.height = AabbGetDim(ClipRect->Bounds).y;
                Scissor.offset.x = ClipRect->Bounds.Min.x;
                Scissor.offset.y = UiState->RenderHeight - ClipRect->Bounds.Min.y - Scissor.extent.height;
                vkCmdSetScissor(Commands.Buffer, 0, 1, &Scissor);

                vkCmdDrawIndexed(Commands.Buffer, 6, ClipRect->NumRectsAffected, 0, 0, CurrRectId);

                CurrRectId += ClipRect->NumRectsAffected;
            }
        }
    }

    // TODO: Make below a separate sub pass due to transparency? 
    
    // NOTE: Render Glyphs
    {
        vkCmdBindPipeline(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UiState->GlyphPipeline->Handle);
        VkDescriptorSet DescriptorSets[] =
            {
                UiState->UiDescriptor,
            };
        vkCmdBindDescriptorSets(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UiState->GlyphPipeline->Layout, 0,
                                ArrayCount(DescriptorSets), DescriptorSets, 0, 0);
        
        VkDeviceSize Offset = 0;
        vkCmdBindVertexBuffers(Commands.Buffer, 0, 1, &UiState->QuadVertices, &Offset);
        vkCmdBindIndexBuffer(Commands.Buffer, UiState->QuadIndices, 0, VK_INDEX_TYPE_UINT32);

        // NOTE: We go in reverse for clip rects since we render back to front
        u32 CurrGlyphId = 0;
        for (i32 ClipRectId = UiState->NumClipRects - 1; ClipRectId >= 0; --ClipRectId)
        {
            ui_clip_rect* ClipRect = UiState->ClipRectArray + ClipRectId;

            if (ClipRect->NumGlyphsAffected > 0)
            {
                VkRect2D Scissor = {};
                Scissor.extent.width = AabbGetDim(ClipRect->Bounds).x;
                Scissor.extent.height = AabbGetDim(ClipRect->Bounds).y;
                Scissor.offset.x = ClipRect->Bounds.Min.x;
                Scissor.offset.y = UiState->RenderHeight - ClipRect->Bounds.Min.y - Scissor.extent.height;
                vkCmdSetScissor(Commands.Buffer, 0, 1, &Scissor);

                vkCmdDrawIndexed(Commands.Buffer, 6, ClipRect->NumGlyphsAffected, 0, 0, CurrGlyphId);
                CurrGlyphId += ClipRect->NumGlyphsAffected;
            }
        }
    }
    
    vkCmdEndRenderPass(Commands.Buffer);
}

//
// NOTE: UiState Input Helpers
//

inline void UiStateAddInteraction(ui_state* UiState, ui_interaction Interaction)
{
    // NOTE: Z value decreases so we start with front most stuff first, so we don't have to check Z here
    if (UiState->Hot.Type == UiElementType_None)
    {
        UiState->Hot = Interaction;
        UiState->ProcessedInteraction = true;
    }
    else
    {
        // NOTE: Below interactions keep interacting so they get priority
        if (UiInteractionsAreSame(Interaction, UiState->PrevHot) &&
            (Interaction.Type == UiElementType_Button ||
             Interaction.Type == UiElementType_HorizontalSlider || 
             Interaction.Type == UiElementType_DraggableBox ||
             Interaction.Type == UiElementType_Image ||
             Interaction.Type == UiElementType_FloatNumberBox))
        {
            UiState->Hot = Interaction;
            UiState->ProcessedInteraction = true;
        }
    }
}

inline ui_interaction_type UiStateProcessElement(ui_state* UiState, aabb2 Bounds, u64 Hash)
{
    // IMPORTANT: We assume pixel space coords
    // NOTE: Check if we add a element interaction
    b32 IsPrevHot = UiInteractionsAreSame(Hash, UiState->PrevHot.Hash);
    b32 Intersects = UiIntersect(Bounds, UiState->CurrInput.MousePixelPos);
    UiState->MouseTouchingUi = UiState->MouseTouchingUi || Intersects;
    if ((Intersects && UiState->MouseFlags & UiMouseFlag_Pressed) ||
        (IsPrevHot && (UiState->MouseFlags & UiMouseFlag_HeldOrReleased)))
    {
        ui_interaction Interaction = {};
        Interaction.Type = UiElementType_Button;
        Interaction.Hash = Hash;        
        UiStateAddInteraction(UiState, Interaction);
    }

    // NOTE: Check ui interaction from last frame
    ui_interaction_type Result = UiInteractionType_None;
    if (IsPrevHot)
    {
        Result = UiState->PrevHot.Button;
    }
    else if (Intersects)
    {
        Result = UiInteractionType_Hover;
    }

    return Result;
}

//
// NOTE: UiState Render Helpers
//

inline f32 UiStateGetZ(ui_state* UiState)
{
    f32 Result = UiState->CurrZ;
    UiState->CurrZ += UiState->StepZ;

    return Result;
}

inline void UiStateSetClipRect(ui_state* UiState, aabb2i Bounds)
{
    Assert(UiState->NumClipRects < UiState->MaxNumClipRects);

    ui_clip_rect* ClipRect = UiState->ClipRectArray + UiState->NumClipRects++;
    *ClipRect = {};
    ClipRect->Bounds = Bounds;
}

inline void UiStateResetClipRect(ui_state* UiState)
{
    UiStateSetClipRect(UiState, AabbMinMax(V2i(0, 0), V2i(UiState->RenderWidth, UiState->RenderHeight)));
}

inline void UiStatePushRect(ui_state* UiState, aabb2 Bounds, v4 Color)
{
    Assert(UiState->NumRects < UiState->MaxNumRects);
    ui_render_rect* CurrRect = UiState->RectArray + UiState->NumRects++;
    CurrRect->Center = AabbGetCenter(Bounds);
    CurrRect->Radius = AabbGetRadius(Bounds);
    CurrRect->Z = UiStateGetZ(UiState);
    CurrRect->Color = Color;

    Assert(UiState->NumClipRects >= 1);
    ui_clip_rect* ClipRect = UiState->ClipRectArray + UiState->NumClipRects - 1;
    ClipRect->NumRectsAffected += 1;
}

inline void UiStatePushRectOutline(ui_state* UiState, aabb2 Bounds, v4 Color, f32 Thickness)
{
    v2 TopLeft = V2(Bounds.Min.x, Bounds.Max.y);
    v2 TopRight = V2(Bounds.Max.x, Bounds.Max.y);
    v2 BotLeft = V2(Bounds.Min.x, Bounds.Min.y);
    v2 BotRight = V2(Bounds.Max.x, Bounds.Min.y);
    
    aabb2 TopBox = AabbMinMax(TopLeft - V2(0.0f, Thickness), TopRight);
    aabb2 BotBox = AabbMinMax(BotLeft, BotRight + V2(0.0f, Thickness));
    aabb2 LeftBox = AabbMinMax(BotLeft, TopLeft + V2(Thickness, 0.0f));
    aabb2 RightBox = AabbMinMax(BotRight - V2(Thickness, 0.0f), TopRight);

    UiStatePushRect(UiState, TopBox, Color);
    UiStatePushRect(UiState, LeftBox, Color);
    UiStatePushRect(UiState, RightBox, Color);
    UiStatePushRect(UiState, BotBox, Color);
}

inline void UiStatePushGlyph(ui_state* UiState, char Glyph, f32 Z, aabb2 Bounds, v4 Color)
{
    Assert(UiState->NumGlyphs < UiState->MaxNumGlyphs);
    ui_render_glyph* CurrGlyph = UiState->GlyphArray + UiState->NumGlyphs++;
    CurrGlyph->Center = AabbGetCenter(Bounds);
    CurrGlyph->Radius = AabbGetRadius(Bounds);
    CurrGlyph->GlyphOffset = Glyph;
    CurrGlyph->Z = Z;
    CurrGlyph->Color = Color;

    Assert(UiState->NumClipRects >= 1);
    ui_clip_rect* ClipRect = UiState->ClipRectArray + UiState->NumClipRects - 1;
    ClipRect->NumGlyphsAffected += 1;
}

inline f32 UiFontGetLineAdvance(ui_font* Font, f32 CharHeight)
{
    f32 Result = -CharHeight*(Font->MaxAscent - Font->MaxDescent + Font->LineGap);
    return Result;
}

inline f32 UiFontGetStartY(ui_font* Font, f32 MinY, f32 MaxY)
{
    f32 CharHeight = MaxY - MinY;
    f32 StartY = MaxY - CharHeight*(Font->MaxAscent);

    return StartY;
}

inline aabb2 UiStateGetTextSize(ui_state* UiState, f32 MaxCharHeight, char* Text)
{
    // IMPORTANT: This outputs the bounds centered around (0, 0) for easier translation
    aabb2 Result = {};
    
    // NOTE: https://github.com/justinmeiners/stb-truetype-example/blob/master/source/main.c
    ui_font* Font = &UiState->Font;

    // NOTE: We start at (0, 0) and record left to right, and downwards for our text bounds
    v2 StartPos = V2(0.0f);
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
        if (!StartNewLine && PrevGlyph != 0)
        {
            Kerning = MaxCharHeight*Font->KernArray[CurrGlyph*255 + PrevGlyph];
        }
        
        if (StartNewLine)
        {
            // NOTE: Remember the width for this line when calculating our bounds
            Result.Max.x = Max(Result.Max.x, CurrPos.x);
            
            CurrPos.y -= MaxCharHeight * (1.0f + Font->LineGap);
            CurrPos.x = StartPos.x;
            PrevGlyph = 0;
            Text += 1;

            continue;
        }

        // NOTE: Apply kerning and step
        CurrPos.x += Kerning + ScaledStepX;
        PrevGlyph = CurrGlyph;
        ++Text;
    }

    // NOTE: Remember the width of the last line when calculating bounds
    Result.Max.x = Max(Result.Max.x, CurrPos.x);
    // NOTE: Make bot left 0, 0
    Result.Max.y = -CurrPos.y + MaxCharHeight; 

    return Result;
}

inline aabb2 UiStateGetTextSizeCentered(ui_state* UiState, f32 MaxCharHeight, char* Text)
{
    aabb2 Result = UiStateGetTextSize(UiState, MaxCharHeight, Text);
    Result = Translate(Result, -AabbGetRadius(Result));

    return Result;
}

inline aabb2 UiStateGetGlyphBounds(ui_state* UiState, f32 MaxCharHeight, char* Text, u32 GlyphId)
{
    aabb2 Result = {};

    // NOTE: https://github.com/justinmeiners/stb-truetype-example/blob/master/source/main.c
    ui_font* Font = &UiState->Font;

    // NOTE: We start at (0, 0) and record left to right, and downwards for our text bounds
    v2 StartPos = V2(0.0f);
    v2 CurrPos = StartPos;
    char* StartText = Text;
    char PrevGlyph = 0;
    char CurrGlyph = 0;
    while (true)
    {
        CurrGlyph = *Text;

        ui_glyph* Glyph = Font->GlyphArray + CurrGlyph;
        f32 Kerning = 0.0f;
        f32 ScaledStepX = MaxCharHeight*Glyph->StepX;
        b32 StartNewLine = CurrGlyph == '\n';
        if (!StartNewLine && PrevGlyph != 0)
        {
            Kerning = MaxCharHeight*Font->KernArray[CurrGlyph*255 + PrevGlyph];
        }
        
        if (StartNewLine)
        {
            CurrPos.y -= MaxCharHeight * (1.0f + Font->LineGap);
            CurrPos.x = StartPos.x;
            PrevGlyph = 0;
            Text += 1;

            continue;
        }

        // NOTE: Apply kerning and step
        CurrPos.x += Kerning;
        if (u64(Text - StartText) == GlyphId)
        {
            if (CurrGlyph != ' ' || CurrGlyph == 0)
            {
                // NOTE: Glyph dim will be the aabb size, but we offset by align pos since some chars like 'p'
                // need to be descend a bit
                v2 CharDim = Glyph->Dim * MaxCharHeight;
                Result = AabbMinMax(V2(0, 0), CharDim);
                Result = Translate(Result, CurrPos + MaxCharHeight*(Glyph->AlignPos + V2(0.0f, -Font->MaxDescent)));
            }
            else
            {
                Result = AabbMinMax(V2(0, 0), V2(ScaledStepX, MaxCharHeight));
            }
            
            break;
        }
        CurrPos.x += ScaledStepX;
        PrevGlyph = CurrGlyph;
        ++Text;
    }
    
    return Result;
}

inline void UiStatePushTextNoFormat(ui_state* UiState, v2 TopLeftTextPos, f32 MaxCharHeight, char* Text, v4 TintColor)
{
    // NOTE: https://github.com/justinmeiners/stb-truetype-example/blob/master/source/main.c
    ui_font* Font = &UiState->Font;
    f32 FrontTextZ = UiStateGetZ(UiState);
    f32 BackTextZ = UiStateGetZ(UiState);

    // NOTE: Calculate our text start pos on the line
    v2 StartPos = V2(TopLeftTextPos.x, TopLeftTextPos.y - MaxCharHeight*(Font->MaxAscent));

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
        if (!StartNewLine && PrevGlyph != 0)
        {
            Kerning = MaxCharHeight*Font->KernArray[CurrGlyph*255 + PrevGlyph];
        }
        
        if (StartNewLine)
        {
            CurrPos.y += UiFontGetLineAdvance(Font, MaxCharHeight);
            CurrPos.x = StartPos.x;
            PrevGlyph = 0;
            Text += 1;

            continue;
        }

        // NOTE: Apply kerning
        CurrPos.x += Kerning;
        if (CurrGlyph != ' ')
        {
            // NOTE: Glyph dim will be the aabb size, but we offset by align pos since some chars like 'p'
            // need to be descend a bit
            v2 CharDim = Glyph->Dim * MaxCharHeight;
            aabb2 GlyphBounds = AabbMinMax(V2(0, 0), CharDim);
            GlyphBounds = Translate(GlyphBounds, CurrPos + MaxCharHeight*Glyph->AlignPos);

            UiStatePushGlyph(UiState, CurrGlyph, FrontTextZ, GlyphBounds, TintColor);
            // NOTE: Store black version of glyph behind white version
            //UiStatePushGlyph(UiState, CurrGlyph, FrontTextZ, Translate(Enlarge(GlyphBounds, V2(2)), V2(2, -1)), V4(0, 0, 0, 1));
        }
        
        CurrPos.x += ScaledStepX;
        PrevGlyph = CurrGlyph;
        ++Text;
    }
}

inline void UiStatePushTextNoFormat(ui_state* UiState, aabb2 Bounds, f32 MaxCharHeight, char* Text, v4 TintColor)
{
    v2 BoundsDim = AabbGetDim(Bounds);
    v2 TopLeftTextPos = V2(Bounds.Min.x, Bounds.Max.y);
    UiStatePushTextNoFormat(UiState, TopLeftTextPos, MaxCharHeight, Text, TintColor);
}

//
// NOTE: Ui Elements
//

#define UiButton(UiState, Bounds, Color) UiButton_(UiState, Bounds, Color, (u32)UI_FILE_LINE_ID())
inline ui_interaction_type UiButton_(ui_state* UiState, aabb2 Bounds, v4 Color, u32 Hash)
{
    // IMPORTANT: Bounds are in pixel coord space
    Assert(Bounds.Min.x < Bounds.Max.x);
    Assert(Bounds.Min.y < Bounds.Max.y);
    
    ui_interaction_type Result = UiStateProcessElement(UiState, Bounds, UiElementHash(UiElementType_Button, Hash));
    UiStatePushRect(UiState, Bounds, Color);
    
    return Result;
}

#define UiButtonAnimated(UiState, Bounds, Color) UiButtonAnimated_(UiState, Bounds, Color, (u32)UI_FILE_LINE_ID())
inline b32 UiButtonAnimated_(ui_state* UiState, aabb2 Bounds, v4 Color, u32 Hash)
{
    // IMPORTANT: Bounds are in pixel coord space
    Assert(Bounds.Min.x < Bounds.Max.x);
    Assert(Bounds.Min.y < Bounds.Max.y);
    
    ui_interaction_type Interaction = UiStateProcessElement(UiState, Bounds, UiElementHash(UiElementType_Button, Hash));

    aabb2 DrawBounds = Bounds;
    switch(Interaction)
    {
        case UiInteractionType_Hover:
        {
            UiStatePushRectOutline(UiState, Bounds, V4(0.7f, 0.7f, 0.7f, 1.0f), 3.0f);
        } break;

        case UiInteractionType_Selected:
        {
            DrawBounds = Enlarge(DrawBounds, V2(-3));
        } break;
    }

    UiStatePushRect(UiState, DrawBounds, Color);

    b32 Result = Interaction == UiInteractionType_Released;
    return Result;
}

#define UiButtonText(UiState, Bounds, MaxCharHeight, Text, Color) UiButtonText_(UiState, Bounds, MaxCharHeight, Text, Color, (u32)(UI_FILE_LINE_ID() + uintptr_t(Text)))
inline b32 UiButtonText_(ui_state* UiState, aabb2 Bounds, f32 MaxCharHeight, char* Text, v4 Color, u32 Hash)
{
    // IMPORTANT: Bounds are in pixel coord space
    Assert(Bounds.Min.x < Bounds.Max.x);
    Assert(Bounds.Min.y < Bounds.Max.y);
    
    ui_interaction_type Interaction = UiStateProcessElement(UiState, Bounds, UiElementHash(UiElementType_Button, Hash));
    
    v4 TextColor = V4(1);
    aabb2 DrawBounds = Bounds;
    switch(Interaction)
    {
        case UiInteractionType_Hover:
        {
            TextColor.rgb = V3(0.7f, 0.7f, 0.3f);
        } break;

        case UiInteractionType_Selected:
        {
            TextColor.rgb = V3(0.9f, 0.9f, 0.1f);
            DrawBounds = Enlarge(DrawBounds, V2(-3));
            MaxCharHeight -= 1.0f;
        } break;
    }

    // NOTE: Center the text to the button
    aabb2 TextBounds = UiStateGetTextSizeCentered(UiState, MaxCharHeight, Text);
    TextBounds = Translate(TextBounds, AabbGetCenter(DrawBounds));

    UiStatePushTextNoFormat(UiState, TextBounds, MaxCharHeight, Text, TextColor);
    UiStatePushRect(UiState, TextBounds, V4(1, 0, 0, 1));
    UiStatePushRect(UiState, DrawBounds, Color);

    b32 Result = Interaction == UiInteractionType_Released;
    return Result;
}

inline void UiHorizontalSlider(ui_state* UiState, aabb2 SliderBounds, v2 KnobRadius, f32 MinValue, f32 MaxValue, f32* SliderValue)
{
    Assert(MaxValue >= MinValue);
    Assert(*SliderValue >= MinValue && *SliderValue <= MaxValue);
    
    u64 Hash = UiElementHash(UiElementType_HorizontalSlider, u32(uintptr_t(SliderValue)));
    
    // NOTE: Generate all our collision bounds
    f32 PercentValue = (*SliderValue - MinValue) / (MaxValue - MinValue);
    aabb2 CollisionBounds = Enlarge(SliderBounds, V2(-KnobRadius.x, 0.0f));
    v2 KnobCenter = V2(Lerp(CollisionBounds.Min.x, CollisionBounds.Max.x, PercentValue),
                       Lerp(CollisionBounds.Min.y, CollisionBounds.Max.y, 0.5f));
    aabb2 SliderKnob = AabbCenterRadius(KnobCenter, KnobRadius);

    ui_interaction Interaction = {};
    Interaction.Type = UiElementType_HorizontalSlider;
    Interaction.Hash = Hash;
    Interaction.Slider.Bounds = CollisionBounds;
    Interaction.Slider.SliderValue = SliderValue;
    Interaction.Slider.MinValue = MinValue;
    Interaction.Slider.MaxValue = MaxValue;
    
    // NOTE: Check for slider interaction
    b32 IntersectsKnob = UiIntersect(SliderKnob, UiState->CurrInput.MousePixelPos);
    b32 IntersectsElement = (UiIntersect(SliderBounds, UiState->CurrInput.MousePixelPos) || IntersectsKnob);
    UiState->MouseTouchingUi = UiState->MouseTouchingUi || IntersectsElement;
    if ((UiState->MouseFlags & UiMouseFlag_Pressed) && IntersectsElement)
    {
        if (IntersectsKnob)
        {
            // NOTE: We calculate a mouse relative pos here
            Interaction.Slider.MouseRelativeX = AabbGetCenter(SliderKnob).x - UiState->CurrInput.MousePixelPos.x;
        }
        else
        {
            // NOTE: We recenter the knob to be at the center of our
            Interaction.Slider.MouseRelativeX = 0.0f;
        }
        UiStateAddInteraction(UiState, Interaction);
    }
    else if ((UiState->MouseFlags & UiMouseFlag_HeldOrReleased) &&
             UiInteractionsAreSame(Hash, UiState->PrevHot.Hash))
    {
        // NOTE: We keep interacting with the slider if we interacted last frame
        Interaction.Slider.MouseRelativeX = UiState->PrevHot.Slider.MouseRelativeX;
        UiStateAddInteraction(UiState, Interaction);
    }

    UiStatePushRectOutline(UiState, SliderKnob, V4(0.2f, 0.2f, 0.2f, 1.0f), 2);
    UiStatePushRect(UiState, SliderKnob, V4(0, 0, 0, 1));
    UiStatePushRect(UiState, SliderBounds, V4(0.0f, 0.0f, 0.0f, 1.0f));
}

inline void UiDragabbleBoxNoRender(ui_state* UiState, aabb2 Bounds, v2* TopLeftPos)
{
    u64 Hash = UiElementHash(UiElementType_DraggableBox, u32(uintptr_t(TopLeftPos)));
    
    ui_interaction Interaction = {};
    Interaction.Type = UiElementType_DraggableBox;
    Interaction.Hash = Hash;
    Interaction.DragBox.TopLeftPos = TopLeftPos;
    
    if (UiIntersect(Bounds, UiState->CurrInput.MousePixelPos) && (UiState->MouseFlags & UiMouseFlag_Pressed))
    {
        Interaction.DragBox.MouseRelativePos = *TopLeftPos - UiState->CurrInput.MousePixelPos;
        UiStateAddInteraction(UiState, Interaction);
    }
    else if (UiInteractionsAreSame(Hash, UiState->PrevHot.Hash) && (UiState->MouseFlags & UiMouseFlag_PressedOrHeld))
    {
        // NOTE: Sometimes mouse moves to fast but we know we are interacting with it so we only check for past interaction
        Interaction.DragBox.MouseRelativePos = UiState->PrevHot.DragBox.MouseRelativePos;
        UiStateAddInteraction(UiState, Interaction);
    }
}

#define UiRect(UiState, Bounds, Color) UiRect_(UiState, Bounds, Color, (u32)(UI_FILE_LINE_ID()))
inline void UiRect_(ui_state* UiState, aabb2 Bounds, v4 Color, u32 InputHash)
{
    u64 Hash = UiElementHash(UiElementType_Image, InputHash);
    
    if ((UiIntersect(Bounds, UiState->CurrInput.MousePixelPos) || UiInteractionsAreSame(Hash, UiState->PrevHot.Hash)) &&
        (UiState->MouseFlags & UiMouseFlag_PressedOrHeld))
    {
        ui_interaction Interaction = {};
        Interaction.Type = UiElementType_Image;
        Interaction.Hash = Hash;
        UiStateAddInteraction(UiState, Interaction);
    }

    UiStatePushRect(UiState, Bounds, Color);
}

inline void UiNumberBox(ui_state* UiState, f32 MaxCharHeight, aabb2 Bounds, f32* Value)
{
    u64 Hash = UiElementHash(UiElementType_DraggableBox, u32(uintptr_t(Value)));

    // NOTE: Text that gets drawn, we control which part of the text is visible by incrementing/decrementing where our draw text starts
    // in our entire number string
    char* DrawText = 0;
    char DrawTextSpace[64];

    ui_interaction Interaction = {};
    Interaction.Type = UiElementType_FloatNumberBox;
    Interaction.Hash = Hash;

    // NOTE: Area where text can be placed
    aabb2 TextAreaBounds = Enlarge(Bounds, V2(-5));

    v4 TextColor = V4(1);
    aabb2 SelectedGlyphBounds = {};
    b32 Selected = UiInteractionsAreSame(Hash, UiState->Selected.Hash);
    b32 Intersects = UiIntersect(Bounds, UiState->CurrInput.MousePixelPos);
    if (Selected)
    {
        // NOTE: We have this element selected, the inputs are now keyboard inputs
        ui_float_number_box_interaction* SelectedData = &UiState->Selected.FloatNumberBox;
            
        // NOTE: Move the text we are drawing so that the selected char is visible
        u32 NewStartDrawChar = SelectedData->StartDrawChar;
        if (SelectedData->StartDrawChar > SelectedData->SelectedChar)
        {
            NewStartDrawChar = SelectedData->SelectedChar;
        }
        else
        {
            // IMPORTANT: We assume we cannot have more than one key of the same type pressed in a frame
            SelectedGlyphBounds = UiStateGetGlyphBounds(UiState, MaxCharHeight, SelectedData->Text + NewStartDrawChar, SelectedData->SelectedChar - NewStartDrawChar);
            // NOTE: Move text area so that bottom corner is 0, 0
            aabb2 RecenteredTextArea = {};
            RecenteredTextArea.Max = AabbGetDim(TextAreaBounds);
            
            if (!UiContained(RecenteredTextArea, SelectedGlyphBounds))
            {
                NewStartDrawChar = SelectedData->StartDrawChar + 1;
            }
        }

        DrawText = SelectedData->Text + NewStartDrawChar;
        SelectedData->StartDrawChar = NewStartDrawChar;
    }
    else if (Intersects && ((UiState->MouseFlags & UiMouseFlag_Pressed)) ||
             UiInteractionsAreSame(Hash, UiState->PrevHot.Hash) && (UiState->MouseFlags & UiMouseFlag_HeldOrReleased))
    {
        // TODO: Probably should be in our string lib
        DrawText = DrawTextSpace;
        snprintf(DrawText, sizeof(DrawText), "%f", *Value);
        Interaction.FloatNumberBox.TextLength = u32(strlen(DrawText));
        Interaction.FloatNumberBox.HasDecimal = true;
        Copy(DrawText, Interaction.FloatNumberBox.Text, sizeof(DrawText));

        UiStateAddInteraction(UiState, Interaction);
    }
    else
    {
        DrawText = DrawTextSpace;
        snprintf(DrawText, sizeof(DrawText), "%f", *Value);
    }
    
    // NOTE: Calculate bounds and draw
    aabb2 TextBounds = UiStateGetTextSize(UiState, MaxCharHeight, DrawText);
    v2 TextOffset = AabbGetCenter(Bounds) - AabbGetRadius(TextBounds);
    TextBounds = Translate(TextBounds, TextOffset);
    TextOffset.x += TextAreaBounds.Min.x - TextBounds.Min.x;
    TextBounds.Min.x = TextAreaBounds.Min.x;
    
    if (Intersects || Selected)
    {
        TextColor = V4(0.2f, 0.9f, 0.9f, 1.0f);
    }

    UiStateSetClipRect(UiState, Aabb2i(Bounds));
    UiStatePushTextNoFormat(UiState, TextBounds, MaxCharHeight, DrawText, TextColor);

    if (Selected)
    {
        // NOTE: Highlight selected glyph
        SelectedGlyphBounds = Translate(SelectedGlyphBounds, TextOffset);
        SelectedGlyphBounds.Min.y = TextBounds.Min.y;
        SelectedGlyphBounds.Max.y = TextBounds.Max.y;
        UiStatePushRect(UiState, SelectedGlyphBounds, V4(1, 0, 0, 1));
    }
    
    UiStatePushRectOutline(UiState, Bounds, V4(1, 1, 1, 1), 2);
    UiStatePushRect(UiState, Bounds, V4(0, 0, 1, 1));
    UiStateResetClipRect(UiState);
}

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
        Result.KnobRadius = V2(20);
        Result.SliderBounds = AabbCenterRadius(V2(100.0f, -Result.KnobRadius.y), V2(100, 5));

        Result.MaxCharHeight = 40.0f;

        // IMPORTANT: All bounds should be created such that top left is 0,0
        Result.BorderGap = 15.0f;
        Result.ElementGap = 10.0f;
        Result.LineGap = 6.0f;
        Result.TitleHeight = Result.MaxCharHeight + 4.0f;
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

    Panel->CurrPos.x += AabbGetDim(TextBounds).x + Panel->ElementGap;
    Panel->CurrRowMaxY = Max(Panel->CurrRowMaxY, AabbGetDim(TextBounds).y);
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

//
// =======================================================================================================================================
//

// NOTE: UI Testing for each element if needed

#if 0
    {
        ui_state* UiState = &DemoState->UiState;

        ui_frame_input UiCurrInput = {};
        UiCurrInput.MouseDown = CurrInput->MouseDown;
        UiCurrInput.MousePixelPos = V2(CurrInput->MousePixelPos);
        UiCurrInput.MouseScroll = CurrInput->MouseScroll;
        Copy(CurrInput->KeysDown, UiCurrInput.KeysDown, sizeof(UiCurrInput.KeysDown));
        UiStateBegin(UiState, RenderState->Device, RenderState->WindowWidth, RenderState->WindowHeight, UiCurrInput);
        
#if 0
        local_global v4 ButtonColor = V4(1, 1, 1, 1);
        switch (UiButton(UiState, AabbCenterRadius(V2(100), V2(50)), ButtonColor))
        {
            case UiInteractionType_Hover:
            {
                ButtonColor = V4(1, 0, 0, 1);
            } break;

            case UiInteractionType_Selected:
            {
                ButtonColor = V4(0, 1, 0, 1);
            } break;

            case UiInteractionType_Released:
            {
                ButtonColor = V4(1, 0, 1, 1);
            } break;

            default:
            {
                ButtonColor = V4(1, 1, 1, 1);
            } break;
        }
#endif
        
        if (UiButtonAnimated(UiState, AabbCenterRadius(V2(100), V2(50)), V4(0.1f, 0.4f, 0.9f, 1.0f)))
        {
        }

        local_global f32 Test1 = 2.0f;
        local_global f32 Test2 = 5.0f;
        local_global f32 Test3 = 6.0f;
        UiHorizontalSlider(UiState, AabbCenterRadius(V2(200), V2(100, 5)), V2(20), 2, 3, &Test1);

        UiStatePushGlyph(UiState, 'A', 0.5, AabbCenterRadius(V2(300), V2(50)), V4(1));
        UiStatePushTextNoFormat(UiState, V2(40, 500), 20.0f, "Testing", V4(1));

        UiButtonText(UiState, AabbCenterRadius(V2(400, 100), V2(50)), 40.0f, "Test", V4(0.1f, 0.4f, 0.9f, 1.0f));

        local_global v2 PanelPos = V2(600, 300);
        ui_panel Panel = UiPanelBegin(UiState, &PanelPos, "Panel");
        UiPanelText(&Panel, "Text:");
        UiPanelHorizontalSlider(&Panel, 4, 10, &Test2);
        UiPanelNextRow(&Panel);
        UiPanelText(&Panel, "Header:");
        UiPanelNextRow(&Panel);
        UiPanelText(&Panel, "Text:");
        UiPanelHorizontalSlider(&Panel, 4, 10, &Test3);
        UiPanelEnd(&Panel);
        
        UiStateEnd(UiState);
    }

    // NOTE: Testing if the text boudns and glyph bounds all are calculated correctly
    {
        char* TestText = "100.0000";
        aabb2 SelectedGlyphBounds = UiStateGetGlyphBounds(UiState, MaxCharHeight, TestText, 3);
        aabb2 OldTextBounds = UiStateGetTextSize(UiState, MaxCharHeight, TestText);

        SelectedGlyphBounds = Translate(SelectedGlyphBounds, V2(20, 20));
        OldTextBounds = Translate(OldTextBounds, V2(20, 20));
        
        UiStatePushTextNoFormat(UiState, OldTextBounds, MaxCharHeight, TestText, V4(1));
        UiStatePushRect(UiState, SelectedGlyphBounds, V4(0, 0, 1, 1));
        UiStatePushRect(UiState, OldTextBounds, V4(1, 0, 0, 1));
    }


#endif

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
