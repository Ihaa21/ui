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
                          vk_descriptor_manager* DescriptorManager, vk_pipeline_manager* PipelineManager,
                          vk_transfer_manager* TransferManager, VkFormat ColorFormat, VkImageLayout FinalLayout, ui_state* UiState)
{
    temp_mem TempMem = BeginTempMem(TempArena);
    
    // IMPORTANT: Its assumed this is happening during a transfer operation
    *UiState = {};
    u64 Size = MegaBytes(16);
    UiState->Device = Device;
    UiState->GpuLocalMemoryId = LocalMemType;
    UiState->GpuArena = VkLinearArenaCreate(Device, LocalMemType, Size);
    UiState->StepZ = 1.0f / 4096.0f;
    UiState->CurrZ = 0.0f;

    UiState->CpuBlockArena = PlatformBlockArenaCreate(MegaBytes(1), 32);
    
    // NOTE: Quad
    {
        // NOTE: Vertices
        {
            f32 Vertices[] = 
                {
                    -0.5, -0.5, 0, 0, 0,
                     0.5, -0.5, 0, 1, 0,
                     0.5,  0.5, 0, 1, 1,
                    -0.5,  0.5, 0, 0, 1,
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

    // NOTE: Color Target
    {        
        vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(TempArena);
        
        u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, ColorFormat, VK_ATTACHMENT_LOAD_OP_LOAD,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                FinalLayout);
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
        UiState->ClipRectArena = BlockArenaCreate(&UiState->CpuBlockArena);
        UiState->NumClipRects = 0;
        UiState->NumClipRectsPerBlock = u32(BlockArenaGetBlockSize(&UiState->ClipRectArena) / sizeof(ui_clip_rect));
    }
    
    // NOTE: Rect Data
    {
        UiState->RectArena = BlockArenaCreate(&UiState->CpuBlockArena);
        UiState->NumRects = 0;
        UiState->NumRectsPerBlock = u32(BlockArenaGetBlockSize(&UiState->RectArena) / sizeof(ui_render_rect));
        
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
        UiState->GlyphArena = BlockArenaCreate(&UiState->CpuBlockArena);
        UiState->NumGlyphs = 0;
        UiState->NumGlyphsPerBlock = u32(BlockArenaGetBlockSize(&UiState->GlyphArena) / sizeof(ui_render_glyph));
        
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

    EndTempMem(TempMem);
}

inline void UiStateBegin(ui_state* UiState, f32 FrameTime, u32 RenderWidth, u32 RenderHeight, ui_frame_input CurrRawInput)
{
    UiState->FrameTime = FrameTime;
    UiState->RenderWidth = RenderWidth;
    UiState->RenderHeight = RenderHeight;
    
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
            if (UiState->CurrInput.MouseDown && !(UiState->MouseFlags & UiMouseFlag_Pressed))
            {
                UiState->MouseFlags |= UiMouseFlag_Held;
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

inline void UiStateEnd(ui_state* UiState, vk_descriptor_manager* DescriptorManager)
{
    // NOTE: Handle interactions
    {
        ui_frame_input* CurrInput = &UiState->CurrInput;
        ui_frame_input* PrevInput = &UiState->PrevInput;
        ui_interaction* Hot = &UiState->Hot;
        ui_interaction* Selected = &UiState->Selected;
        switch (Hot->Type)
        {
            case UiElementType_CheckBox:
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
                // TODO: Handle ctrl all inputs to delete
                if (UiKeyDown(CurrInput, VK_LEFT))
                {
                    NumberBox->SelectedChar = Clamp(NumberBox->SelectedChar - 1, 0, NumberBox->TextLength);
                }
                if (UiKeyDown(CurrInput, VK_RIGHT))
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
                if (UiKeyDown(CurrInput, u8(VK_OEM_MINUS)) && NumberBox->SelectedChar == 0 && NumberBox->Text[0] != '-')
                {
                    UiTextBoxAddChar(NumberBox->Text, &NumberBox->TextLength, &NumberBox->SelectedChar, '-');
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
                    (UiState->Hot.Type == UiElementType_None && UiState->MouseFlags != 0) ||
                    UiKeyDown(CurrInput, VK_RETURN))
                {
                    ReadFloat(String(NumberBox->Text, NumberBox->TextLength), NumberBox->OutFloat);
                    *NumberBox->OutFloat = Max(NumberBox->MinValue, Min(NumberBox->MaxValue, *NumberBox->OutFloat));
                    UiState->Selected = {};
                }

                UiState->ProcessedInteraction = true;
            } break;
        }

        // NOTE: Clear hot interaction for next frame
        *Hot = {};
    }

    // NOTE: ReAllocate temp arena for per frame allocations
    {
        // TODO: Is there a cleaner way to do this so we only have to define things once??
        // NOTE: Calculate the size of our allocation
        u64 TotalMemoryRequired = 0;

        if (UiState->DepthImage.Image == VK_NULL_HANDLE)
        {
            VkImageDestroy(UiState->Device, UiState->DepthImage);
        }
        UiState->DepthImage.Image = VkImageHandleCreate(UiState->Device, UiState->RenderWidth, UiState->RenderHeight, VK_FORMAT_D32_SFLOAT,
                                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        VkMemoryRequirements DepthRequirements = VkImageGetMemoryRequirements(UiState->Device, UiState->DepthImage.Image);
        TotalMemoryRequired = VkIncrementPointer(TotalMemoryRequired, DepthRequirements);
        
        if (UiState->GpuRectBuffer == VK_NULL_HANDLE)
        {
            vkDestroyBuffer(UiState->Device, UiState->GpuRectBuffer, 0);
        }
        VkMemoryRequirements RectRequirements = {};
        if (UiState->NumRects > 0)
        {
            UiState->GpuRectBuffer = VkBufferHandleCreate(UiState->Device, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                          sizeof(ui_render_rect) * UiState->NumRects);
            RectRequirements = VkBufferGetMemoryRequirements(UiState->Device, UiState->GpuRectBuffer);
            TotalMemoryRequired = VkIncrementPointer(TotalMemoryRequired, RectRequirements);
        }
        
        if (UiState->GpuGlyphBuffer == VK_NULL_HANDLE)
        {
            vkDestroyBuffer(UiState->Device, UiState->GpuGlyphBuffer, 0);
        }
        VkMemoryRequirements GlyphRequirements = {};
        if (UiState->NumGlyphs > 0)
        {
            UiState->GpuGlyphBuffer = VkBufferHandleCreate(UiState->Device, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                           sizeof(ui_render_glyph_gpu) * UiState->NumGlyphs);
            GlyphRequirements = VkBufferGetMemoryRequirements(UiState->Device, UiState->GpuGlyphBuffer);
            TotalMemoryRequired = VkIncrementPointer(TotalMemoryRequired, GlyphRequirements);
        }
        
        if (UiState->GpuTempArena.Memory != VK_NULL_HANDLE)
        {
            VkLinearArenaDestroy(UiState->Device, &UiState->GpuTempArena);
        }

        // NOTE: Create allocation and bind all resources
        UiState->GpuTempArena = VkLinearArenaCreate(UiState->Device, UiState->GpuLocalMemoryId, TotalMemoryRequired);

        VkImageBindMemory(UiState->Device, &UiState->GpuTempArena, UiState->DepthImage.Image, DepthRequirements);
        UiState->DepthImage.View = VkImageViewCreate(UiState->Device, UiState->DepthImage.Image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT,
                                                     VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1);
        if (UiState->NumRects > 0)
        {
            VkBufferBindMemory(UiState->Device, &UiState->GpuTempArena, UiState->GpuRectBuffer, RectRequirements);
        }
        if (UiState->NumGlyphs > 0)
        {
            VkBufferBindMemory(UiState->Device, &UiState->GpuTempArena, UiState->GpuGlyphBuffer, GlyphRequirements);
        }
    }
    
    // NOTE: Upload rects to GPU
    if (UiState->NumRects > 0)
    {
        VkDescriptorBufferWrite(DescriptorManager, UiState->UiDescriptor, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, UiState->GpuRectBuffer);

        ui_render_rect* GpuData = VkTransferPushWriteArray(&RenderState->TransferManager, UiState->GpuRectBuffer, ui_render_rect, UiState->NumRects,
                                                           BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                           BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));

        v2 InvResolution = V2(1.0f) / V2(UiState->RenderWidth, UiState->RenderHeight);
        u32 GlobalRectId = 0;
        for (block* CurrBlock = UiState->RectArena.Next; CurrBlock; CurrBlock = CurrBlock->Next)
        {
            ui_render_rect* RectArray = BlockGetData(CurrBlock, ui_render_rect);
            u32 NumRectsInBlock = Min(UiState->NumRects - GlobalRectId, UiState->NumRectsPerBlock);
            
            for (uint BlockRectId = 0; BlockRectId < NumRectsInBlock; ++BlockRectId, ++GlobalRectId)
            {
                ui_render_rect* CurrRect = RectArray + BlockRectId;

                ui_render_rect* GpuRect = GpuData + GlobalRectId;
                // NOTE: Normalize the rect to be in 0-1 range, x points right, y points up
                GpuRect->Center = CurrRect->Center * InvResolution;
                GpuRect->Center.y = 1.0f - GpuRect->Center.y;
                GpuRect->Radius = 2.0f * CurrRect->Radius * InvResolution;

                GpuRect->Z = CurrRect->Z;
                GpuRect->Color = PreMulAlpha(CurrRect->Color);
            }
        }
    }

    // NOTE: Upload glyphs to GPU
    if (UiState->NumGlyphs > 0)
    {
        VkDescriptorBufferWrite(DescriptorManager, UiState->UiDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, UiState->GpuGlyphBuffer);
        
        ui_render_glyph_gpu* GpuData = VkTransferPushWriteArray(&RenderState->TransferManager, UiState->GpuGlyphBuffer, ui_render_glyph_gpu, UiState->NumGlyphs,
                                                                BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                                BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));

        // NOTE: Loop back to front so reverse order (we want to render back to front)
        v2 InvResolution = V2(1.0f) / V2(UiState->RenderWidth, UiState->RenderHeight);
        u32 GlobalGlyphId = 0;
        for (block* CurrBlock = UiState->GlyphArena.Next; CurrBlock; CurrBlock = CurrBlock->Next)
        {
            ui_render_glyph* GlyphArray = BlockGetData(CurrBlock, ui_render_glyph);
            u32 NumGlyphsInBlock = Min(UiState->NumGlyphs - GlobalGlyphId, UiState->NumGlyphsPerBlock);
            
            for (uint BlockGlyphId = 0; BlockGlyphId < NumGlyphsInBlock; ++BlockGlyphId, ++GlobalGlyphId)
            {
                ui_render_glyph* CurrGlyph = GlyphArray + BlockGlyphId;

                ui_render_glyph_gpu* GpuGlyph = GpuData + (UiState->NumGlyphs - GlobalGlyphId - 1);
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
    
    VkDescriptorManagerFlush(UiState->Device, DescriptorManager);
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
        u32 GlobalClipRectId = 0;
        for (block* CurrBlock = UiState->ClipRectArena.Next; CurrBlock; CurrBlock = CurrBlock->Next)
        {
            ui_clip_rect* ClipRectArray = BlockGetData(CurrBlock, ui_clip_rect);
            u32 NumClipRectsInBlock = Min(UiState->NumClipRects - GlobalClipRectId, UiState->NumClipRectsPerBlock);
            
            for (u32 BlockClipRectId = 0; BlockClipRectId < NumClipRectsInBlock; ++BlockClipRectId)
            {
                ui_clip_rect* ClipRect = ClipRectArray + BlockClipRectId;

                if (ClipRect->NumRectsAffected > 0)
                {
                    VkRect2D Scissor = {};
                    Scissor.extent.width = AabbGetDim(ClipRect->Bounds).x;
                    Scissor.extent.height = AabbGetDim(ClipRect->Bounds).y;
                    Scissor.offset.x = Clamp(ClipRect->Bounds.Min.x, 0, i32(UiState->RenderWidth));
                    Scissor.offset.y = Clamp(i32(UiState->RenderHeight - ClipRect->Bounds.Min.y - Scissor.extent.height), 0, i32(UiState->RenderHeight));
                    vkCmdSetScissor(Commands.Buffer, 0, 1, &Scissor);

                    vkCmdDrawIndexed(Commands.Buffer, 6, ClipRect->NumRectsAffected, 0, 0, CurrRectId);

                    CurrRectId += ClipRect->NumRectsAffected;
                }
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
        u32 GlobalClipRectId = 0;
        for (block* CurrBlock = UiState->ClipRectArena.Prev; CurrBlock; CurrBlock = CurrBlock->Prev)
        {
            ui_clip_rect* ClipRectArray = BlockGetData(CurrBlock, ui_clip_rect);
            u32 NumClipRectsInBlock = Min(UiState->NumClipRects - GlobalClipRectId, UiState->NumClipRectsPerBlock);
            
            for (i32 BlockClipRectId = i32(NumClipRectsInBlock) - 1; BlockClipRectId >= 0; --BlockClipRectId)
            {
                ui_clip_rect* ClipRect = ClipRectArray + BlockClipRectId;

                if (ClipRect->NumGlyphsAffected > 0)
                {
                    VkRect2D Scissor = {};
                    Scissor.extent.width = AabbGetDim(ClipRect->Bounds).x;
                    Scissor.extent.height = AabbGetDim(ClipRect->Bounds).y;
                    Scissor.offset.x = Clamp(ClipRect->Bounds.Min.x, 0, i32(UiState->RenderWidth));
                    Scissor.offset.y = Clamp(i32(UiState->RenderHeight - ClipRect->Bounds.Min.y - Scissor.extent.height), 0, i32(UiState->RenderHeight));
                    vkCmdSetScissor(Commands.Buffer, 0, 1, &Scissor);

                    vkCmdDrawIndexed(Commands.Buffer, 6, ClipRect->NumGlyphsAffected, 0, 0, CurrGlyphId);
                    CurrGlyphId += ClipRect->NumGlyphsAffected;
                }
            }
        }
    }

    vkCmdEndRenderPass(Commands.Buffer);
        
    // NOTE: Clear CPU memory
    ArenaClear(&UiState->CpuBlockArena);
    ArenaClear(&UiState->ClipRectArena);
    ArenaClear(&UiState->RectArena);
    ArenaClear(&UiState->GlyphArena);
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
    UiState->NumClipRects += 1;
    UiState->CurrClipRect = PushStruct(&UiState->ClipRectArena, ui_clip_rect); 
    *UiState->CurrClipRect = {};
    UiState->CurrClipRect->Bounds = Bounds;
}

inline void UiStateResetClipRect(ui_state* UiState)
{
    UiStateSetClipRect(UiState, AabbMinMax(V2i(0, 0), V2i(UiState->RenderWidth, UiState->RenderHeight)));
}

inline void UiStatePushRect(ui_state* UiState, aabb2 Bounds, v4 Color)
{
    UiState->NumRects += 1;
    ui_render_rect* CurrRect = PushStruct(&UiState->RectArena, ui_render_rect);
    *CurrRect = {};
    CurrRect->Center = AabbGetCenter(Bounds);
    CurrRect->Radius = AabbGetRadius(Bounds);
    CurrRect->Z = UiStateGetZ(UiState);
    CurrRect->Color = Color;

    Assert(UiState->CurrClipRect);
    UiState->CurrClipRect->NumRectsAffected += 1;
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
    UiState->NumGlyphs += 1;
    ui_render_glyph* CurrGlyph = PushStruct(&UiState->GlyphArena, ui_render_glyph);
    *CurrGlyph = {};
    CurrGlyph->Center = AabbGetCenter(Bounds);
    CurrGlyph->Radius = AabbGetRadius(Bounds);
    CurrGlyph->GlyphOffset = Glyph;
    CurrGlyph->Z = Z;
    CurrGlyph->Color = Color;

    Assert(UiState->CurrClipRect);
    UiState->CurrClipRect->NumGlyphsAffected += 1;
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

inline void UiCheckBox(ui_state* UiState, aabb2 Bounds, b32* Bool)
{
    Assert(Bounds.Min.x < Bounds.Max.x);
    Assert(Bounds.Min.y < Bounds.Max.y);
    
    ui_interaction_type Interaction = UiStateProcessElement(UiState, Bounds, UiElementHash(UiElementType_CheckBox, u32(uintptr_t(Bool))));
    v4 Color = V4(0.95f);
    switch(Interaction)
    {
        case UiInteractionType_Hover:
        {
            Color = V4(1.0f);
        } break;

        case UiInteractionType_Selected:
        {
            Color = V4(1.0f);
            Bounds = Enlarge(Bounds, V2(-1));
        } break;

        case UiInteractionType_Released:
        {
            *Bool = !*Bool;
        } break;
    }
    
    UiStatePushRectOutline(UiState, Bounds, Color, 2);
    if (*Bool)
    {
        UiStatePushRect(UiState, Enlarge(Bounds, V2(-4)), V4(0.2f, 1.0f, 0.2f, 1.0f));
    }
    UiStatePushRect(UiState, Bounds, V4(0, 0, 0, 1));
}

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
            TextColor.rgb = UI_HOVER_TEXT_COLOR.rgb;
        } break;

        case UiInteractionType_Selected:
        {
            TextColor.rgb = UI_SELECTED_TEXT_COLOR.rgb;
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

inline void UiNumberBox(ui_state* UiState, f32 MaxCharHeight, aabb2 Bounds, f32 MinValue, f32 MaxValue, f32* Value)
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
            
            if (!(RecenteredTextArea.Min.x <= SelectedGlyphBounds.Min.x && RecenteredTextArea.Max.x >= SelectedGlyphBounds.Max.x))
            {
                NewStartDrawChar = SelectedData->StartDrawChar + 1;
            }
        }

        DrawText = SelectedData->Text + NewStartDrawChar;
        SelectedData->StartDrawChar = NewStartDrawChar;

        // NOTE: Check if we have to add a interaction with this element
        if (Intersects && ((UiState->MouseFlags & UiMouseFlag_Pressed)) ||
            (UiInteractionsAreSame(Hash, UiState->PrevHot.Hash) && (UiState->MouseFlags & UiMouseFlag_Held)) ||
            (UiInteractionsAreSame(Hash, UiState->PrevHot.Hash) && Intersects && UiState->MouseFlags & UiMouseFlag_Released))
        {
            Interaction.FloatNumberBox = *SelectedData;
            UiStateAddInteraction(UiState, Interaction);
        }
    }
    else if (Intersects && ((UiState->MouseFlags & UiMouseFlag_Pressed)) ||
             (UiInteractionsAreSame(Hash, UiState->PrevHot.Hash) && (UiState->MouseFlags & UiMouseFlag_Held)) ||
             (UiInteractionsAreSame(Hash, UiState->PrevHot.Hash) && Intersects && UiState->MouseFlags & UiMouseFlag_Released))
    {        
        // TODO: Probably should be in our string lib
        DrawText = DrawTextSpace;
        snprintf(DrawText, sizeof(DrawText), "%f", *Value);
        Interaction.FloatNumberBox.TextLength = u32(strlen(DrawText));
        Interaction.FloatNumberBox.HasDecimal = true;
        Interaction.FloatNumberBox.MinValue = MinValue;
        Interaction.FloatNumberBox.MaxValue = MaxValue;
        Interaction.FloatNumberBox.OutFloat = Value;
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

    TextColor = Intersects ? UI_HOVER_TEXT_COLOR : TextColor;
    TextColor = Selected ? UI_SELECTED_TEXT_COLOR : TextColor;

    UiStatePushRectOutline(UiState, Bounds, V4(0.4f, 0.4f, 0.6f, 1.0f), 2);

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
    
    UiStatePushRect(UiState, Bounds, V4(0, 0, 0, 1));
    UiStateResetClipRect(UiState);
}

inline void UiNumberBox(ui_state* UiState, f32 MaxCharHeight, aabb2 Bounds, f32* Value)
{
    UiNumberBox(UiState, MaxCharHeight, Bounds, F32_MIN, F32_MAX, Value);
}
