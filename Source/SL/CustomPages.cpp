#include "CustomPages.h"

// ==========================================================================================
// LoericPage
// ==========================================================================================

LoericPage::LoericPage(juce::String pageId,
                       juce::String title,
                       juce::String shortname,
                       SLLinkManager &slLink,
                       juce::AudioProcessorValueTreeState &state,
                       LoericStore &storeToUse,
                       LoericController &controllerToUse,
                       SoundfontAudioProcessor &soundfontToUse)
    : UIPage(pageId, title, "", shortname, juce::Colours::lightcoral, {}, slLink, state),
      store(storeToUse),
      controller(controllerToUse),
      soundfont(soundfontToUse)
{
    backColour = juce::Colours::black;
    textColour = juce::Colours::white;

    //                         name       statusKey         min    max   step  value  decimals
    knobs[KnobTempo]          = {"Tempo",    "tempo",          40,    250,  1,    120,   0};
    knobs[KnobTranspose]      = {"Transpose", "transpose",     -24,   24,   1,    0,     0};
    knobs[KnobResponsiveness] = {"Respons.", "responsiveness", 0,     1,    0.01, 0.5,   2};
    knobs[KnobZ4]             = {"N/A",      "",               0,     1,    0.01, 0.5,   2};
    knobs[KnobZ4].enabled = juce::String(z4ChangePath).isNotEmpty();

    loericVolume  = {"LOERIC VOLUME",    "volume", 0, 1, 0.01, 1.0, 2};
    soundfontGain = {"SOUNDFONT VOLUME", "",       0, 1, 0.01, juce::jlimit(0.0, 1.0, (double)soundfont.getGain()), 2};

    store.addListener(this);

    // Catch up with whatever the store already knows.
    const auto &s = store.getState();
    if (s.tunesLoaded)       loericTunesChanged(s.tunes);
    if (s.instrumentsLoaded) loericInstrumentsChanged(s.instruments);
    if (s.statusReceived)    loericStatusChanged(s.status, s.status.diff({}));
}

LoericPage::~LoericPage()
{
    stopTimer(sendTimerId);
    stopTimer(popupTimerId);
    stopTimer(messageTimerId);
    store.removeListener(this);
}

//==============================================================================
// Status

LoericPage::ServerState LoericPage::serverState() const
{
    const auto &s = store.getState();

    if (!s.connected)
        return ServerState::Disconnected;

    return s.status.running ? ServerState::Playing : ServerState::Idle;
}

juce::Colour LoericPage::statusColour() const
{
    switch (serverState())
    {
    case ServerState::Playing: return colourPlaying;
    case ServerState::Idle:    return colourIdle;
    case ServerState::Disconnected: break;
    }
    return colourDisconnected;
}

juce::String LoericPage::statusText() const
{
    switch (serverState())
    {
    case ServerState::Playing:
    {
        const auto tune = tuneName(store.getState().status.currentTune);
        return tune.isEmpty() ? "PLAYING" : "PLAYING - " + tune;
    }
    case ServerState::Idle: return "IDLE";
    case ServerState::Disconnected: break;
    }
    return "DISCONNECTED";
}

//==============================================================================
// Page lifecycle

void LoericPage::activatePage()
{
    isActive = true;
    soundfontGain.value = juce::jlimit(0.0, 1.0, (double)soundfont.getGain());
    UIPage::activatePage(); // handlers + draw()
}

void LoericPage::deactivatePage()
{
    isActive = false;
    stopTimer(popupTimerId);
    popupEncoder = -1;

    slLinkManager.clearAllRGBLeds();
    slLinkManager.clearAllWhiteLeds();
    slLinkManager.clearDisplay(juce::Colours::black);

    UIPage::deactivatePage();
}

//==============================================================================
// Drawing

void LoericPage::draw()
{
    if (!isActive)
        return;

    pageColour = foreColour = statusColour();

    if (popupEncoder >= 0)
    {
        drawEncoderABPopup(true);
        return;
    }

    setRGBLeds();
    setWhiteLeds();

    drawHeader();
    drawContent();

    for (int k = 0; k < NumKnobs; ++k)
        drawKnob(k);

    drawFooter();
}

void LoericPage::drawHeader()
{
    const auto colour = statusColour();

    slLinkManager.drawRectangle(0, 0, slmk2::LCD_WIDTH, HEADER_HEIGHT, backColour);

    // Status square on the left
    slLinkManager.drawRectangle(5, 5, 28, 28, colour);

    if (!pageTitle.isEmpty())
        slLinkManager.drawText(pageTitle, 38, 3, 250, slmk2::TextAlignment::CENTER, slmk2::TextSize::LARGE,
                               textColour, backColour);

    slLinkManager.drawText(statusText().toUpperCase(), 38, 28, 250, slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL,
                           colour, backColour);

    // Separation line in the status colour
    slLinkManager.drawRectangle(0, HEADER_HEIGHT, slmk2::LCD_WIDTH, 2, colour);
}

void LoericPage::drawContent()
{
    slLinkManager.drawRectangle(0, CONTENT_Y, slmk2::LCD_WIDTH, CONTENT_HEIGHT, backColour);

    switch (currentTab)
    {
    case Tab::Tune:
    {
        const bool has = tuneBrowse >= 0 && tuneBrowse < (int)tunes.size();
        drawListContent(has ? tunes[(size_t)tuneBrowse].name : juce::String(), tuneBrowse, (int)tunes.size(),
                        has && tunes[(size_t)tuneBrowse].id == selectedTuneId,
                        store.getState().tunesLoaded ? "No tunes on the server" : "Loading tunes...");
        break;
    }
    case Tab::Model:
    {
        const bool has = modelBrowse >= 0 && modelBrowse < (int)models.size();
        drawListContent(has ? models[(size_t)modelBrowse].name : juce::String(), modelBrowse, (int)models.size(),
                        has && models[(size_t)modelBrowse].id == selectedModelId,
                        store.getState().instrumentsLoaded ? "No models on the server" : "Loading models...");
        break;
    }
    case Tab::Config:
    {
        const bool has = configBrowse >= 0 && configBrowse < (int)configs.size();
        juce::String text;

        if (has)
        {
            const auto &c = configs[(size_t)configBrowse];
            text = c.name.isNotEmpty() ? c.name : c.id;
            if (c.id == defaultConfigId)
                text << " (default)";
        }

        juce::String empty = selectedTuneId.isEmpty()                       ? "Select a tune first"
                             : store.getState().findConfigs(selectedTuneId) ? "No configs for this tune"
                                                                             : "Loading configs...";

        drawListContent(text, configBrowse, (int)configs.size(),
                        has && configs[(size_t)configBrowse].id == selectedConfigId, empty);
        break;
    }
    case Tab::Repeat:
        drawRepeatContent();
        break;
    }
}

void LoericPage::drawListContent(const juce::String &itemText, int itemIndex, int itemCount, bool isConfirmed,
                                 const juce::String &emptyText)
{
    const bool locked = isLocked();
    const auto accent = statusColour();

    if (itemCount == 0)
    {
        slLinkManager.drawText(emptyText, 10, CONTENT_Y + 12, 300, slmk2::TextAlignment::CENTER, slmk2::TextSize::MEDIUM,
                               greyText, backColour);
        return;
    }

    if (!locked)
        slLinkManager.drawBitmap(6, CONTENT_Y + 4, slmk2::BitmapGroup::BMP_GDX_NAVIGATION,
                                 static_cast<uint8_t>(slmk2::NAV_iidx::ROTATE), textColour, backColour);

    // Main line: the highlighted item. Confirmed item in the accent colour, a pending one in white.
    const auto itemColour = locked ? greyText : (isConfirmed ? accent : textColour);
    slLinkManager.drawText(itemText, 38, CONTENT_Y + 2, 244, slmk2::TextAlignment::CENTER, slmk2::TextSize::MEDIUM,
                           itemColour, backColour);

    // Second line: position + hint (or a transient message)
    juce::String hint;
    if (message.isNotEmpty())
        hint = message;
    else if (locked)
        hint = "Locked while playing";
    else
        hint = juce::String(itemIndex + 1) + " / " + juce::String(itemCount) + (isConfirmed ? "   SELECTED" : "   press to select");

    slLinkManager.drawText(hint, 10, CONTENT_Y + 28, 300, slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL,
                           message.isNotEmpty() ? textColour : greyText, backColour);
}

void LoericPage::drawRepeatContent()
{
    const bool locked = isLocked();

    if (!locked)
        slLinkManager.drawBitmap(6, CONTENT_Y + 4, slmk2::BitmapGroup::BMP_GDX_NAVIGATION,
                                 static_cast<uint8_t>(slmk2::NAV_iidx::ROTATE), textColour, backColour);

    const juce::String text = "Repetitions: " + juce::String(repetitions);
    slLinkManager.drawText(text, 38, CONTENT_Y + 2, 244, slmk2::TextAlignment::CENTER, slmk2::TextSize::MEDIUM,
                           locked ? greyText : statusColour(), backColour);

    const juce::String hint = message.isNotEmpty() ? message
                              : locked             ? juce::String("Locked while playing")
                                                   : juce::String("Rotate joystick to change");

    slLinkManager.drawText(hint, 10, CONTENT_Y + 28, 300, slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL,
                           message.isNotEmpty() ? textColour : greyText, backColour);
}

void LoericPage::drawKnob(int k)
{
    if (!isActive || popupEncoder >= 0)
        return;

    const auto &v = knobs[(size_t)k];
    const uint16_t knobX = KNOB_HOR_MARGIN + (uint16_t)k * KNOB_HOR_OFFSET;
    const uint16_t textWiderThanKnobBy = 10;
    const uint16_t nameX = knobX - textWiderThanKnobBy / 2;
    const uint16_t nameW = KNOB_BMP_WIDTH + textWiderThanKnobBy;

    // A knob the server can't receive (disconnected) or a placeholder is drawn greyed out.
    const bool usable = v.enabled && store.getState().connected;
    const auto knobColour = usable ? statusColour() : juce::Colours::grey;
    const auto valueColour = usable ? textColour : juce::Colours::grey;

    const auto bmpIdx = v.enabled ? static_cast<uint8_t>(v.normalised() * static_cast<double>(slmk2::KNOB_iidx::MAX))
                                  : static_cast<uint8_t>(slmk2::KNOB_iidx::OFF);

    slLinkManager.drawBitmap(knobX, KNOB_Y, slmk2::BitmapGroup::BMP_GDX_KNOB, bmpIdx, knobColour, backColour);

    slLinkManager.drawText(v.enabled ? v.text() : juce::String("--"), knobX + KNOBTEXT_X_OFFSET, KNOB_Y + KNOBTEXT_Y_OFFSET, 35,
                           slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL, valueColour, backColour);

    slLinkManager.drawText(v.name, nameX, KNOB_NAME_Y, nameW,
                           slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL, valueColour, backColour);
}

void LoericPage::drawFooter()
{
    slLinkManager.drawRectangle(0, FOOTER_Y - 2, slmk2::LCD_WIDTH, slmk2::LCD_HEIGHT - FOOTER_Y + 2, backColour);

    for (int idx = 0; idx < 4; ++idx)
        drawFooterButton(idx);
}

void LoericPage::drawFooterButton(int idx)
{
    static const juce::StringArray tabNames = {"Tune", "Model", "Config", "Repeat"};

    const uint16_t buttonX = KNOB_HOR_MARGIN + (uint16_t)idx * KNOB_HOR_OFFSET - 5;
    const uint16_t BUTTON_WIDTH = KNOB_BMP_WIDTH + 10;
    const uint16_t STROKE = 1;

    const bool isSelected = idx == static_cast<int>(currentTab);
    const bool locked = isLocked();

    juce::Colour frame, text, fill = backColour;

    if (locked)
    {
        frame = lockedColour;
        text = lockedColour;
    }
    else if (isSelected)
    {
        frame = statusColour();
        fill = statusColour();
        text = juce::Colours::black;
    }
    else
    {
        frame = juce::Colours::lightgrey;
        text = juce::Colours::lightgrey;
    }

    slLinkManager.drawRectangle(buttonX, FOOTER_Y, BUTTON_WIDTH, FOOTER_BUTTON_HEIGHT, frame);
    slLinkManager.drawRectangle(buttonX + STROKE, FOOTER_Y + STROKE, BUTTON_WIDTH - 2 * STROKE,
                                FOOTER_BUTTON_HEIGHT - 2 * STROKE, fill);

    const uint16_t textY = FOOTER_Y + (FOOTER_BUTTON_HEIGHT - slmk2::TEXT_HEIGHT_MEDIUM) / 2;
    slLinkManager.drawText(tabNames[idx], buttonX + 1, textY, BUTTON_WIDTH - 2,
                           slmk2::TextAlignment::CENTER, slmk2::TextSize::MEDIUM, text, fill);
}

// Same look and behaviour as MasterPage::drawEncoderABbackground / drawEncoderAB.
void LoericPage::drawEncoderABPopup(bool drawBackground)
{
    if (!isActive || popupEncoder < 0)
        return;

    const auto &v = (popupEncoder == 0) ? soundfontGain : loericVolume;
    const auto colour = statusColour();

    if (drawBackground)
    {
        const uint16_t margin = 10, stroke = 4;
        slLinkManager.drawRectangle(margin, margin, slmk2::LCD_WIDTH - margin * 2, slmk2::LCD_HEIGHT - margin * 2, colour);
        slLinkManager.drawRectangle(margin + stroke, margin + stroke,
                                    slmk2::LCD_WIDTH - (margin + stroke) * 2, slmk2::LCD_HEIGHT - (margin + stroke) * 2, backColour);

        const juce::String title = (popupEncoder == 0) ? "A - " + v.name : "B - " + v.name;
        slLinkManager.drawText(title, 40, 25, 240, slmk2::TextAlignment::CENTER, slmk2::TextSize::MEDIUM, textColour, backColour);
    }

    const bool usable = (popupEncoder == 0) || store.getState().connected;
    const uint16_t x = 130, y = 103;

    slLinkManager.drawBitmap(x, y, slmk2::BitmapGroup::BMP_GDX_KNOB,
                             static_cast<uint8_t>(v.normalised() * static_cast<double>(slmk2::KNOB_iidx::MAX)),
                             usable ? colour : juce::Colours::grey, backColour);

    slLinkManager.drawText(v.text(), x + KNOBTEXT_X_OFFSET, y + KNOBTEXT_Y_OFFSET, 35,
                           slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL, textColour, backColour);

    slLinkManager.drawText(usable ? juce::String() : juce::String("Server disconnected"), 40, 175, 240,
                           slmk2::TextAlignment::CENTER, slmk2::TextSize::SMALL, greyText, backColour);
}

void LoericPage::showMessage(const juce::String &text)
{
    message = text;
    startTimer(messageTimerId, messageTimeoutMs);

    if (isActive && popupEncoder < 0)
        drawContent();
}

//==============================================================================
// LEDs

void LoericPage::setRGBLeds()
{
    const bool connected = store.getState().connected;

    for (int k = 0; k < NumKnobs; ++k)
    {
        const bool on = connected && knobs[(size_t)k].enabled;
        slLinkManager.setRGBLed(static_cast<slmk2::SLLedRgb>(k), on ? statusColour() : juce::Colours::transparentBlack);
    }
}

void LoericPage::setWhiteLeds()
{
    const auto state = serverState();
    const bool locked = state == ServerState::Playing;

    // Tabs: only the selected one is lit; all off while playing (not available).
    for (int i = 0; i < 4; ++i)
        slLinkManager.setWhiteLed(static_cast<slmk2::SLLed>(i), !locked && i == static_cast<int>(currentTab));

    const bool canStart = state == ServerState::Idle && selectedTuneId.isNotEmpty() && selectedModelId.isNotEmpty();
    slLinkManager.setWhiteLed(slmk2::SLLed::APPLY, canStart || locked);
    slLinkManager.setWhiteLed(slmk2::SLLed::CANCEL, locked);
}

//==============================================================================
// Input

void LoericPage::pageHandleEncoderItem(const slmk2::SLEncoder encoderIndex, const uint8_t tickValue)
{
    const int ticks = static_cast<int>(tickValue) - 64;
    if (ticks == 0)
        return;

    if (encoderIndex == slmk2::SLEncoder::JOYSTICK)
    {
        // Clockwise = down the list, but = more repetitions on the Repeat tab.
        const int direction = ticks > 0 ? 1 : -1;
        moveSelection(currentTab == Tab::Repeat ? -direction : direction);
        return;
    }

    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (encoderIndex == slmk2::SLEncoder::A || encoderIndex == slmk2::SLEncoder::B)
    {
        const int which = (encoderIndex == slmk2::SLEncoder::A) ? 0 : 1;
        auto &v = (which == 0) ? soundfontGain : loericVolume;

        v.value = juce::jlimit(v.min, v.max, v.value + ticks * v.step);
        v.lastLocalEditMs = now;

        if (which == 0)
            soundfont.setGain(static_cast<float>(v.value));
        else
        {
            v.dirty = true;
            startTimer(sendTimerId, sendIntervalMs);
        }

        const bool newPopup = popupEncoder != which;
        popupEncoder = which;
        drawEncoderABPopup(newPopup);
        startTimer(popupTimerId, popupTimeoutMs); // restart on every tick
        return;
    }

    if (encoderIndex >= slmk2::SLEncoder::Z1 && encoderIndex <= slmk2::SLEncoder::Z4)
    {
        auto &v = knobs[static_cast<size_t>(encoderIndex)];

        if (!v.enabled)
            return;

        const auto previous = v.value;
        v.value = juce::jlimit(v.min, v.max, v.value + ticks * v.step);

        if (juce::exactlyEqual(v.value, previous))
            return;

        v.lastLocalEditMs = now;
        v.dirty = true;
        startTimer(sendTimerId, sendIntervalMs);

        // Close the A/B popup if a zone encoder is used
        if (popupEncoder >= 0)
        {
            stopTimer(popupTimerId);
            popupEncoder = -1;
            slLinkManager.clearDisplay(backColour);
            draw();
        }
        else
            drawKnob(static_cast<int>(encoderIndex));
    }
}

void LoericPage::pageHandleButtonItem(const slmk2::SLButton buttonIndex, const uint8_t state)
{
    if (state != 1) // short press only
        return;

    switch (buttonIndex)
    {
    case slmk2::SLButton::SELECT_ZOOM_1:
    case slmk2::SLButton::SELECT_ZOOM_2:
    case slmk2::SLButton::SELECT_ZOOM_3:
    case slmk2::SLButton::SELECT_ZOOM_4:
        selectTab(static_cast<Tab>(static_cast<int>(buttonIndex) - static_cast<int>(slmk2::SLButton::SELECT_ZOOM_1)));
        break;

    case slmk2::SLButton::JOYSTICK_UP:   moveSelection(-1); break;
    case slmk2::SLButton::JOYSTICK_DOWN: moveSelection(+1); break;
    case slmk2::SLButton::JOYSTICK_MAIN: confirmSelection(); break;
    case slmk2::SLButton::APPLY:         handleApply(); break;
    case slmk2::SLButton::CANCEL:        handleCancel(); break;

    default:
        // Single-page app: page navigation buttons are ignored on purpose
        // (UIPage::manageNavigation needs a "currentPage" parameter that this app doesn't have).
        break;
    }
}

void LoericPage::selectTab(Tab newTab)
{
    if (isLocked() || newTab == currentTab)
        return;

    currentTab = newTab;
    message.clear();

    setWhiteLeds();
    drawContent();
    drawFooter();
}

void LoericPage::moveSelection(int delta)
{
    if (isLocked() || popupEncoder >= 0)
        return;

    auto move = [delta](int &index, size_t size)
    {
        if (size == 0)
            return false;
        const int newIndex = juce::jlimit(0, (int)size - 1, index + delta);
        const bool moved = newIndex != index;
        index = newIndex;
        return moved;
    };

    bool changed = false;

    switch (currentTab)
    {
    case Tab::Tune:   changed = move(tuneBrowse, tunes.size()); break;
    case Tab::Model:  changed = move(modelBrowse, models.size()); break;
    case Tab::Config: changed = move(configBrowse, configs.size()); break;
    case Tab::Repeat:
    {
        // Up = more repetitions
        const int newValue = juce::jlimit(minRepetitions, maxRepetitions, repetitions - delta);
        changed = newValue != repetitions;
        repetitions = newValue;
        break;
    }
    }

    if (changed)
    {
        message.clear();
        drawContent();
    }
}

void LoericPage::confirmSelection()
{
    if (isLocked() || popupEncoder >= 0)
        return;

    switch (currentTab)
    {
    case Tab::Tune:
        if (tuneBrowse < (int)tunes.size())
            selectTune(tunes[(size_t)tuneBrowse].id, false);
        break;

    case Tab::Model:
        if (modelBrowse < (int)models.size())
            selectedModelId = models[(size_t)modelBrowse].id;
        break;

    case Tab::Config:
        if (configBrowse < (int)configs.size())
            selectedConfigId = configs[(size_t)configBrowse].id;
        break;

    case Tab::Repeat:
        return; // value is applied directly
    }

    message.clear();
    setWhiteLeds();
    drawContent();
}

void LoericPage::handleApply()
{
    switch (serverState())
    {
    case ServerState::Disconnected:
        showMessage("Server disconnected");
        return;

    case ServerState::Playing:
        // TODO: pause. The LOERIC client has no pause endpoint yet, so APPLY stops while playing.
        controller.stopPerformance();
        return;

    case ServerState::Idle:
        break;
    }

    if (selectedTuneId.isEmpty())   { showMessage("Select a tune first");  return; }
    if (selectedModelId.isEmpty())  { showMessage("Select a model first"); return; }

    // Send pending knob values first so the start request and the live values agree.
    sendDirtyValues();

    auto *o = new juce::DynamicObject();
    o->setProperty("tune", selectedTuneId);
    if (selectedConfigId.isNotEmpty())
        o->setProperty("config", selectedConfigId);
    o->setProperty("instrument_model", selectedModelId);
    o->setProperty("repetitions", repetitions);
    o->setProperty("transpose", juce::roundToInt(knobs[KnobTranspose].value));
    o->setProperty("responsiveness", knobs[KnobResponsiveness].value);
    o->setProperty("volume", loericVolume.value);
    o->setProperty("tempo", juce::roundToInt(knobs[KnobTempo].value));

    controller.startPerformance(juce::var(o));
    showMessage("Starting...");
}

void LoericPage::handleCancel()
{
    if (serverState() == ServerState::Playing)
        controller.stopPerformance();
}

//==============================================================================
// Server -> UI

void LoericPage::sendDirtyValues()
{
    stopTimer(sendTimerId);

    if (!store.getState().connected)
    {
        // Keep the local values; they'll go out with the next start request.
        for (auto &k : knobs) k.dirty = false;
        loericVolume.dirty = false;
        return;
    }

    auto &tempo = knobs[KnobTempo];
    if (tempo.dirty) { tempo.dirty = false; controller.setTempo(tempo.value); }

    auto &transpose = knobs[KnobTranspose];
    if (transpose.dirty) { transpose.dirty = false; controller.change(transposeChangePath, juce::roundToInt(transpose.value)); }

    auto &resp = knobs[KnobResponsiveness];
    if (resp.dirty) { resp.dirty = false; controller.setResponsiveness(resp.value); }

    auto &z4 = knobs[KnobZ4];
    if (z4.dirty) { z4.dirty = false; controller.change(z4ChangePath, z4.value); }

    if (loericVolume.dirty) { loericVolume.dirty = false; controller.setVolume(loericVolume.value); }
}

void LoericPage::loericConnectionChanged(bool, const juce::String &)
{
    if (isActive && popupEncoder < 0)
        draw();
    else if (isActive)
        drawEncoderABPopup(true);
}

void LoericPage::loericStatusChanged(const LoericState::Status &status, const LoericState::Status::Change &change)
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    // Live values follow the server unless the user just moved them.
    auto sync = [&](LiveValue &v) -> bool
    {
        if (v.statusKey.isEmpty() || !change.parameters.contains(v.statusKey))
            return false;
        if (v.dirty || now - v.lastLocalEditMs < localEditHoldMs)
            return false;

        const auto value = status.getParameter(v.statusKey);
        if (value.isVoid() || !(value.isDouble() || value.isInt() || value.isInt64()))
            return false;

        v.value = juce::jlimit(v.min, v.max, (double)value);
        return true;
    };

    std::array<bool, NumKnobs> knobChanged{};
    for (int k = 0; k < NumKnobs; ++k)
        knobChanged[(size_t)k] = sync(knobs[(size_t)k]);

    const bool volumeChanged = sync(loericVolume);

    // Offline settings follow the server while idle (e.g. a selection made in the web UI).
    if (!status.running && change.parameters.contains("repetitions"))
    {
        const auto r = status.getParameter("repetitions");
        if (r.isInt() || r.isInt64() || r.isDouble())
            repetitions = juce::jlimit(minRepetitions, maxRepetitions, (int)r);
    }

    if (change.tune && status.currentTune.isNotEmpty())
        selectTune(status.currentTune, true);

    if (change.instrument && status.currentInstrument.isNotEmpty())
    {
        for (size_t i = 0; i < models.size(); ++i)
            if (models[i].id == status.currentInstrument || models[i].name == status.currentInstrument)
            {
                selectedModelId = models[i].id;
                modelBrowse = (int)i;
            }
    }

    if (change.config && status.currentConfig.isNotEmpty())
    {
        selectedConfigId = status.currentConfig;
        rebuildConfigList();
    }

    if (!isActive)
        return;

    if (change.running || change.tune || change.config || change.instrument)
    {
        if (change.running)
            message.clear();

        if (popupEncoder < 0)
            draw(); // colours, locks and LEDs depend on the running state
        return;
    }

    if (popupEncoder >= 0)
    {
        if (volumeChanged && popupEncoder == 1)
            drawEncoderABPopup(false);
        return;
    }

    for (int k = 0; k < NumKnobs; ++k)
        if (knobChanged[(size_t)k])
            drawKnob(k);

    if (currentTab == Tab::Repeat && change.parameters.contains("repetitions"))
        drawContent();
}

void LoericPage::loericTunesChanged(const std::vector<LoericState::Tune> &allTunes)
{
    tunes.clear();

    // The server also lists dot-files as tunes: hide them.
    for (const auto &t : allTunes)
        if (!t.id.startsWithChar('.') && !t.name.startsWithChar('.'))
            tunes.push_back(t);

    std::sort(tunes.begin(), tunes.end(), [](const auto &a, const auto &b)
              { return a.name.compareNatural(b.name) < 0; });

    tuneBrowse = 0;
    for (size_t i = 0; i < tunes.size(); ++i)
        if (tunes[i].id == selectedTuneId)
            tuneBrowse = (int)i;

    if (isActive && popupEncoder < 0 && currentTab == Tab::Tune)
        drawContent();
}

void LoericPage::loericInstrumentsChanged(const std::vector<LoericState::Instrument> &instruments)
{
    models = instruments;

    modelBrowse = 0;
    bool found = false;
    for (size_t i = 0; i < models.size(); ++i)
        if (models[i].id == selectedModelId)
        {
            modelBrowse = (int)i;
            found = true;
        }

    if (!found)
        selectedModelId.clear();

    // With a single model there is nothing to choose.
    if (selectedModelId.isEmpty() && models.size() == 1)
        selectedModelId = models.front().id;

    if (isActive && popupEncoder < 0)
    {
        setWhiteLeds();
        if (currentTab == Tab::Model)
            drawContent();
    }
}

void LoericPage::loericConfigsChanged(const juce::String &tuneId, const LoericState::Configs &)
{
    if (tuneId != selectedTuneId)
        return;

    rebuildConfigList();

    if (isActive && popupEncoder < 0 && currentTab == Tab::Config)
        drawContent();
}

void LoericPage::timerCallback(int timerId)
{
    switch (timerId)
    {
    case sendTimerId:
        sendDirtyValues();
        break;

    case popupTimerId:
        stopTimer(popupTimerId);
        popupEncoder = -1;
        if (isActive)
        {
            slLinkManager.clearDisplay(backColour);
            draw();
        }
        break;

    case messageTimerId:
        stopTimer(messageTimerId);
        message.clear();
        if (isActive && popupEncoder < 0)
            drawContent();
        break;

    default:
        break;
    }
}

//==============================================================================
// Selection helpers

void LoericPage::selectTune(const juce::String &tuneId, bool fromServer)
{
    const bool tuneChanged = tuneId != selectedTuneId;
    selectedTuneId = tuneId;

    for (size_t i = 0; i < tunes.size(); ++i)
        if (tunes[i].id == tuneId)
            tuneBrowse = (int)i;

    if (!tuneChanged)
        return;

    // A config belongs to a tune: a new tune starts from its default config
    // (unless the server already told us which config it uses).
    if (!fromServer || store.getState().status.currentConfig.isEmpty())
        selectedConfigId.clear();
    else
        selectedConfigId = store.getState().status.currentConfig;

    configs.clear();
    defaultConfigId.clear();
    configBrowse = 0;

    if (store.getState().findConfigs(tuneId) != nullptr)
        rebuildConfigList();
    else
        controller.requestConfigs(tuneId);
}

void LoericPage::rebuildConfigList()
{
    configs.clear();
    defaultConfigId.clear();
    configBrowse = 0;

    const auto *c = store.getState().findConfigs(selectedTuneId);
    if (c == nullptr)
        return;

    configs = c->list;

    if (c->defaultConfig)
    {
        defaultConfigId = c->defaultConfig->id;

        const bool listed = std::any_of(configs.begin(), configs.end(),
                                        [this](const auto &x) { return x.id == defaultConfigId; });
        if (!listed)
            configs.insert(configs.begin(), *c->defaultConfig);
    }

    const bool selectionValid = std::any_of(configs.begin(), configs.end(),
                                            [this](const auto &x) { return x.id == selectedConfigId; });
    if (!selectionValid)
        selectedConfigId = defaultConfigId; // empty = let the server use its default

    for (size_t i = 0; i < configs.size(); ++i)
        if (configs[i].id == selectedConfigId)
            configBrowse = (int)i;
}

juce::String LoericPage::tuneName(const juce::String &tuneId) const
{
    if (const auto *t = store.getState().findTune(tuneId))
        return t->name.isNotEmpty() ? t->name : t->id;
    return tuneId;
}
