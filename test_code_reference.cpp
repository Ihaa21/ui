
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
