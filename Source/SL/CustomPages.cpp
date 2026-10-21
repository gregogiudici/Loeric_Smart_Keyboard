#include "CustomPages.h"

// ==========================================================================================
// SoundFontPage
// ==========================================================================================

void LoericPage::draw()
{
    // First set the RGB LEDs for the knobs on this page
    setRGBLeds();
    setWhiteLeds();

    drawHeader();

    for (size_t i = 0; i < 4; ++i)
    {
        drawItem(i + (subIndex * 4));
    }

    int index = 1;
    juce::String text = embeddedSounds[index];
    text = juce::String("--- ") + text + juce::String(" ---");
    slLinkManager.drawText(text, 38, 65, 280, slmk2::TextAlignment::CENTER, slmk2::TextSize::MEDIUM,
                           juce::Colours::white, backColour);

    drawFooter();
}

void LoericPage::drawItem(uint16_t itemIndex)
{
    // Draw only if the parameter ID is valid
    if (itemIndex >= pageParamIDs.size())
    {
        return;
    }

    if (auto *param = apvts.getParameter(pageParamIDs[itemIndex]))
    {
        // Determine if this index is a knob (0–3) or a button (4–7)
        const int idx = itemIndex % 4;

        // Compute horizontal position based on idx value
        const uint16_t hor = KNOB_HOR_MARGIN + idx * KNOB_HOR_OFFSET;

        {
            // -- Draw knob ----------------------------------------------------------------------------------------- <
            const uint16_t knobX = hor;
            const uint16_t knobY = 105; // 65;
            size_t knobIdx = static_cast<size_t>(param->getValue() * (static_cast<float>(slmk2::KNOB_iidx::MAX)));
            slLinkManager.drawBitmap(knobX, knobY, slmk2::BitmapGroup::BMP_GDX_KNOB,
                                     static_cast<uint8_t>(knobIdx), pageColour, backColour);

            // -- Draw text value ----------------------------------------------------------------------------------- <

            slLinkManager.drawText(param->getCurrentValueAsText(), knobX + KNOBTEXT_X_OFFSET, knobY + KNOBTEXT_Y_OFFSET, 35,
                                   slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL, textColour, backColour);

            // -- Draw knob name ------------------------------------------------------------------------------------ <

            uint16_t textWiderThanKnobBy = 10;
            uint16_t tnameX = knobX - textWiderThanKnobBy / 2;
            uint16_t tnameY = 162;                      // 122;
            uint16_t tnameW = 61 + textWiderThanKnobBy; // should be the knob bmp width

            slLinkManager.drawText(param->getName(10),
                                   tnameX, tnameY, tnameW,
                                   slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL, textColour, backColour);
        }
    }
    else
    {
        const int idx = itemIndex % 4;

        // Compute horizontal position based on idx value
        const uint16_t hor = KNOB_HOR_MARGIN + idx * KNOB_HOR_OFFSET;
        {
            // -- Draw knob ----------------------------------------------------------------------------------------- <
            const uint16_t knobX = hor;
            const uint16_t knobY = 105; // 65;
            juce::Colour grayColour = juce::Colours::grey;
            slLinkManager.drawBitmap(knobX, knobY, slmk2::BitmapGroup::BMP_GDX_KNOB, slmk2::KNOB_iidx::OFF, grayColour, backColour);
            slLinkManager.drawText("--", knobX + KNOBTEXT_X_OFFSET, knobY + KNOBTEXT_Y_OFFSET, 35,
                                   slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL, grayColour, backColour);
            uint16_t textWiderThanKnobBy = 10;
            uint16_t tnameX = knobX - textWiderThanKnobBy / 2;
            uint16_t tnameY = 122;
            uint16_t tnameW = 61 + textWiderThanKnobBy; // should be the knob bmp width

            slLinkManager.drawText("N/A",
                                   tnameX, tnameY, tnameW,
                                   slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL, grayColour, backColour);
        }
    }
}

void LoericPage::drawHeader()
{
    slLinkManager.drawRectangle(0, 0, slmk2::LCD_WIDTH, static_cast<uint16_t>(HEADER_HEIGHT), backColour);

    slLinkManager.drawRectangle(5, 5, 28, 28, pageColour);

    slLinkManager.drawBitmap(16, 65, slmk2::BitmapGroup::BMP_GDX_NAVIGATION, static_cast<uint8_t>(slmk2::NAV_iidx::UP_DOWN), textColour, backColour);

    if (!pageTitle.isEmpty())
        slLinkManager.drawText(pageTitle, 38, 3, 250, slmk2::TextAlignment::CENTER, slmk2::TextSize::LARGE,
                               textColour, backColour);

    if (!pageSubtitle.isEmpty())
        slLinkManager.drawText(pageSubtitle, 40, 28, 250, slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL,
                               textColour, backColour);

    // draw separation line
    slLinkManager.drawRectangle(0, HEADER_HEIGHT, slmk2::LCD_WIDTH, 2, juce::Colours::white);
}

void LoericPage::drawFooter()
{
    slLinkManager.drawRectangle(0, slmk2::LCD_HEIGHT - FOOTER_HEIGHT - 3, slmk2::LCD_WIDTH, FOOTER_HEIGHT, backColour);

    for (int idx = 0; idx < 4; ++idx)
    {
        drawFooterButtons(idx, (idx == subIndex));
    }
}

void LoericPage::drawFooterButtons(int idx, bool isSelected)
{
    // draw empty button
    const uint16_t buttonX = KNOB_HOR_MARGIN + idx * KNOB_HOR_OFFSET - 5;
    const uint16_t buttonY = 200; // 150;
    const uint16_t BUTTON_HEIGHT = 35, BUTTON_WIDTH = KNOB_BMP_WIDTH + 10;
    const uint16_t BUTTON_STROKE = 1;
    juce::Colour colour = isSelected ? juce::Colours::white : juce::Colours::lightgrey;
    slLinkManager.drawRectangle(buttonX, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT, colour);
    // Then draw inner rectangle with backColour fill
    slLinkManager.drawRectangle(buttonX + BUTTON_STROKE, buttonY + BUTTON_STROKE,
                                BUTTON_WIDTH - 2.0f * BUTTON_STROKE, BUTTON_HEIGHT - 2.0f * BUTTON_STROKE, backColour);

    const uint16_t textYmargin = (BUTTON_HEIGHT - slmk2::TEXT_HEIGHT_SMALL) / 2.0f;
    const uint16_t textXmargin = 1;
    // Then draw button text
    slLinkManager.drawText(subSections[idx], buttonX + textXmargin, buttonY + textYmargin, BUTTON_WIDTH - 2.0f * textXmargin,
                           slmk2::TextAlignment::CENTER, slmk2::TextSize::MEDIUM, colour, juce::Colours::transparentBlack);
}

void LoericPage::parameterChanged(const juce::String &parameterID, float newValue)
{
    // Find the index of the parameterID
    auto it = std::find(pageParamIDs.begin(), pageParamIDs.end(), parameterID);
    if (it != pageParamIDs.end())
    {
        size_t paramIndex = std::distance(pageParamIDs.begin(), it);
        if (paramIndex >= subIndex * 4 && paramIndex < (subIndex + 1) * 4)
        {
            drawItem(paramIndex);
        }
    }
}

void LoericPage::pageHandleEncoderItem(const slmk2::SLEncoder encoderIndex, const uint8_t tickValue)
{
    // Handle encoder changes for the knobs on this page
    if (encoderIndex >= slmk2::SLEncoder::Z1 && encoderIndex <= slmk2::SLEncoder::Z4)
    {
        size_t paramIdx = static_cast<size_t>(encoderIndex);
        if (paramIdx < pageParamIDs.size())
        {
            if (auto *param = apvts.getParameter(pageParamIDs[paramIdx + subIndex * 4]))
            {
                int relativeTick = tickValue - 64;
                float vv = param->getValue() * 127.f;
                vv += float(relativeTick);
                param->setValueNotifyingHost(juce::jlimit(0.f, 127.f, vv) / 127.f);
            }
        }
    }
}

void LoericPage::pageHandleButtonItem(const slmk2::SLButton buttonIndex, const uint8_t tickValue)
{
    if (buttonIndex >= slmk2::SLButton::SELECT_ZOOM_1 && buttonIndex <= slmk2::SLButton::SELECT_ZOOM_4)
    {
        int newIndex = static_cast<int>(buttonIndex) % 4;
        if (newIndex != subIndex)
        {
            slLinkManager.setWhiteLed(static_cast<slmk2::SLLed>(subIndex), false);
            drawFooterButtons(subIndex, false);
            subIndex = newIndex;
            slLinkManager.setWhiteLed(static_cast<slmk2::SLLed>(subIndex), true);
            drawFooterButtons(subIndex, true);
            for (int i = 0; i < 4; ++i)
            {
                drawItem(i + subIndex * 4);
            }
        }
    }
    else if (buttonIndex >= slmk2::SLButton::Z1 && buttonIndex <= slmk2::SLButton::Z4)
    {
        auto index = static_cast<size_t>(buttonIndex) + (subIndex * 4);
        if (auto param = apvts.getParameter(pageParamIDs[index]))
        {
            param->setValueNotifyingHost(param->getDefaultValue());
        }
    }
    else if (buttonIndex == slmk2::SLButton::JOYSTICK_DOWN)
    {
        if (embeddedSoundIndex < embeddedSounds.size() - 1)
        {
            embeddedSoundIndex += 1;
            DBG("Embedded Sound Index: " << embeddedSoundIndex << " loaded: " << embeddedSounds[embeddedSoundIndex]);
        }
    }
    else if (buttonIndex == slmk2::SLButton::JOYSTICK_UP)
    {
        if (embeddedSoundIndex > 1)
        {
            embeddedSoundIndex -= 1;
            DBG("Embedded Sound Index: " << embeddedSoundIndex << " loaded: " << embeddedSounds[embeddedSoundIndex]);
        }
    }
    else
    {
        manageNavigation(buttonIndex, tickValue);
    }
}

void LoericPage::valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged,
                                             const juce::Identifier &property)
{
    if (treeWhosePropertyHasChanged != apvts.state)
        return;

    if (property == juce::Identifier(connectionStatusProperty))
    {
        const auto status = apvts.state.getProperty(connectionStatusProperty, 0);
        int statusInt = static_cast<int>(status);
        pageColour = juce::Colours::lightcoral;

        switch (statusInt)
        {
        case 0:
            pageSubtitle = "Disconnected";
            pageColour = juce::Colours::lightcoral;
            break;
        case 1:
            pageSubtitle = "Close Emergency";
            pageColour = juce::Colours::lightpink;
            break;
        case 2:
            pageSubtitle = "Inviting...";
            pageColour = juce::Colours::lightblue;
            break;
        case 4:
            pageSubtitle = "Listening...";
            pageColour = juce::Colours::lightyellow;
            break;
        case 8:
            pageSubtitle = "Connected";
            pageColour = juce::Colours::limegreen;
            break;
        }

        slLinkManager.drawRectangle(5, 5, 30, 30, pageColour);
        setRGBLeds();
        for (int i = 0; i < 4; ++i)
        {
            drawItem(i);
        }

        if (!pageSubtitle.isEmpty())
            slLinkManager.drawText(pageSubtitle, 40, 28, 250, slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL,
                                   textColour, backColour);
    }
}
