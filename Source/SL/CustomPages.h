#pragma once

#include <fatar_sl_link/fatar_sl_link.h>

#include <algorithm>
#include <array>

#include "../Loeric/LoericController.h"
#include "../Loeric/LoericStore.h"
#include "../Soundfont/SoundfontAudioProcessor.h"

//==============================================================================
/** Single SL page that controls the LOERIC server.

    Layout (320x240):
      Header   LOERIC + server status (DISCONNECTED / IDLE / PLAYING). The whole UI accent colour follows the status.
      Knobs    Z1 Tempo, Z2 Transpose, Z3 Responsiveness, Z4 placeholder (/api/change).
      Content  The selected tab: Tune / Model / Config lists, or the Repeat counter.
      Footer   The 4 tabs (SELECT_ZOOM 1..4).

    Controls:
      Joystick up/down (or rotate)  scroll the list / change the repetitions
      Joystick press                 confirm the highlighted list item
      APPLY                          start the performance (toggles while playing, see handleApply)
      CANCEL                         stop the performance
      Encoder A                      Soundfont player volume (local, 0..1)   - popup like MasterPage
      Encoder B                      LOERIC volume (/api/volume, 0..1)       - popup like MasterPage

    The 4 tabs are offline settings: while playing they are locked (LEDs off, greyed out).
    Everything runs on the message thread (SL Link and LoericStore both deliver there). */
class LoericPage : public UIPage,
                   private LoericStore::Listener,
                   private juce::MultiTimer
{
public:
    LoericPage(juce::String pageId,
               juce::String pageTitle,
               juce::String pageShortname,
               SLLinkManager &slLinkManager,
               juce::AudioProcessorValueTreeState &apvts,
               LoericStore &store,
               LoericController &controller,
               SoundfontAudioProcessor &soundfont);

    ~LoericPage() override;

    void draw() override;
    void drawHeader() override;
    void drawFooter() override;

    void setRGBLeds() override;
    void setWhiteLeds() override;

    void activatePage() override;
    void deactivatePage() override;

private:
    //==========================================================================
    enum class Tab { Tune = 0, Model, Config, Repeat };
    enum class ServerState { Disconnected, Idle, Playing };

    /** A value shown on a knob (or on the A/B popup). Kept locally and synced from /api/status. */
    struct LiveValue
    {
        juce::String name;      // label on the display
        juce::String statusKey; // key in status.parameters ("" = not reported by the server)
        double min = 0, max = 1, step = 0.01, value = 0;
        int decimals = 2;
        bool enabled = true;          // false = placeholder, drawn as N/A
        bool dirty = false;           // changed locally, not sent yet
        double lastLocalEditMs = 0;   // ignore server echoes for a while after a local edit

        double normalised() const { return max > min ? (value - min) / (max - min) : 0.0; }
        juce::String text() const { return decimals == 0 ? juce::String(juce::roundToInt(value)) : juce::String(value, decimals); }
    };

    enum Knob { KnobTempo = 0, KnobTranspose, KnobResponsiveness, KnobZ4, NumKnobs };

    //==========================================================================
    // Configuration
    /** /api/change path used for live transpose. (Taken from the demo/mock server: verify on the real one.) */
    static constexpr const char *transposeChangePath = "groover/modules/transpose/steps";
    /** Z4 placeholder: set a path (and name/range in the constructor) to enable it. */
    static constexpr const char *z4ChangePath = "";

    static constexpr int minRepetitions = 1, maxRepetitions = 99;

    static constexpr int sendTimerId = 1, popupTimerId = 2, messageTimerId = 3;
    static constexpr int sendIntervalMs = 80;     // coalesce encoder ticks into one HTTP request
    static constexpr int popupTimeoutMs = 1000;   // same as MasterPage::ENCODER_AB_TIMEOUT_MS
    static constexpr int messageTimeoutMs = 2000;
    static constexpr double localEditHoldMs = 1500;

    //==========================================================================
    // Layout
    static constexpr uint16_t HEADER_HEIGHT = 48;
    // Header 0..50 | Knobs 54..129 | Tab content 148..204 | Tabs 200..235
    static constexpr uint16_t KNOB_Y = 60, KNOB_NAME_Y = 111, KNOB_BMP_WIDTH = 61;
    static constexpr uint16_t CONTENT_Y = 148, CONTENT_HEIGHT = 50;
    static constexpr uint16_t KNOB_HOR_MARGIN = 12, KNOB_HOR_OFFSET = 61 + 17;
    static constexpr uint16_t KNOBTEXT_X_OFFSET = 14, KNOBTEXT_Y_OFFSET = 23;
    static constexpr uint16_t FOOTER_Y = 200, FOOTER_BUTTON_HEIGHT = 35;

    //==========================================================================
    // Drawing
    void drawContent();
    void drawListContent(const juce::String &itemText, int itemIndex, int itemCount, bool isConfirmed,
                         const juce::String &emptyText);
    void drawRepeatContent();
    void drawKnob(int knob);
    void drawFooterButton(int idx);
    void drawEncoderABPopup(bool drawBackground);
    void showMessage(const juce::String &text);

    juce::Colour statusColour() const;
    juce::String statusText() const;

    // Input
    void pageHandleEncoderItem(const slmk2::SLEncoder encoderIndex, const uint8_t tickValue) override;
    void pageHandleButtonItem(const slmk2::SLButton buttonIndex, const uint8_t state) override;
    void moveSelection(int delta);
    void confirmSelection();
    void handleApply();
    void handleCancel();
    void selectTab(Tab newTab);

    // Server
    void sendDirtyValues();
    void loericConnectionChanged(bool connected, const juce::String &error) override;
    void loericStatusChanged(const LoericState::Status &status, const LoericState::Status::Change &change) override;
    void loericTunesChanged(const std::vector<LoericState::Tune> &tunes) override;
    void loericInstrumentsChanged(const std::vector<LoericState::Instrument> &instruments) override;
    void loericConfigsChanged(const juce::String &tuneId, const LoericState::Configs &configs) override;

    void timerCallback(int timerId) override;

    // Selection helpers
    void selectTune(const juce::String &tuneId, bool fromServer);
    void rebuildConfigList();
    juce::String tuneName(const juce::String &tuneId) const;

    ServerState serverState() const;
    bool isLocked() const { return serverState() == ServerState::Playing; }

    //==========================================================================
    LoericStore &store;
    LoericController &controller;
    SoundfontAudioProcessor &soundfont;

    bool isActive = false;
    Tab currentTab = Tab::Tune;

    // Lists (copies of the store, filtered) and selection
    std::vector<LoericState::Tune> tunes;
    std::vector<LoericState::Instrument> models;
    std::vector<LoericState::Config> configs;  // configs of the selected tune
    juce::String defaultConfigId;              // of the selected tune

    int tuneBrowse = 0, modelBrowse = 0, configBrowse = 0;   // highlighted items
    juce::String selectedTuneId, selectedModelId, selectedConfigId; // confirmed items
    int repetitions = 1;

    std::array<LiveValue, NumKnobs> knobs;
    LiveValue loericVolume;   // encoder B
    LiveValue soundfontGain;  // encoder A (local, never sent)

    int popupEncoder = -1;    // -1 = no popup, 0 = A, 1 = B
    juce::String message;     // transient message in the content area

    // Colours
    const juce::Colour colourDisconnected = juce::Colours::lightcoral;
    const juce::Colour colourIdle = juce::Colours::orange;
    const juce::Colour colourPlaying = juce::Colours::limegreen;
    const juce::Colour greyText = juce::Colour::fromString("#ff8a8a8a");
    const juce::Colour lockedColour = juce::Colour::fromString("#ff505050");

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoericPage)
};
