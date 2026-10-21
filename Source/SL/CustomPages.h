#include <fatar_sl_link/fatar_sl_link.h>

class LoericPage : public UIPage,
                      private juce::ValueTree::Listener
{
public:
    LoericPage(
        juce::String pageId,
        juce::String pageTitle,
        juce::String pageSubtitle,
        juce::String pageShortname,
        juce::Colour pageColour,
        const std::vector<juce::String> &parameterIDs,
        SLLinkManager &slLinkManager,
        juce::AudioProcessorValueTreeState &apvts)
        : UIPage(pageId, pageTitle, pageSubtitle, pageShortname, pageColour, parameterIDs, slLinkManager, apvts)
    {
        this->foreColour = pageColour;
        this->backColour = juce::Colours::black;
        this->textColour = juce::Colours::white;
    }

    void draw() override;
    void drawHeader() override;
    void drawFooter() override;
    void drawFooterButtons(int idx, bool isSelected);

    void drawItem(uint16_t index);

    void setRGBLeds() override
    {
        slLinkManager.setRGBLed(slmk2::SLLedRgb::Z1, pageColour);
        slLinkManager.setRGBLed(slmk2::SLLedRgb::Z2, pageColour);
        slLinkManager.setRGBLed(slmk2::SLLedRgb::Z3, pageColour);
        slLinkManager.setRGBLed(slmk2::SLLedRgb::Z4, pageColour);
    }

    void setWhiteLeds() override
    {
        for (int i = 0; i < 4; ++i)
        {
            auto isActive = (i == subIndex);
            slLinkManager.setWhiteLed(static_cast<slmk2::SLLed>(i), isActive);
        }
    }

    void activatePage() override
    {
        apvts.state.addListener(this);
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
        UIPage::activatePage();
    }

    void deactivatePage() override
    {
        slLinkManager.setRGBLed(slmk2::SLLedRgb::Z1, juce::Colours::transparentBlack);
        slLinkManager.setRGBLed(slmk2::SLLedRgb::Z2, juce::Colours::transparentBlack);
        slLinkManager.setRGBLed(slmk2::SLLedRgb::Z3, juce::Colours::transparentBlack);
        slLinkManager.setRGBLed(slmk2::SLLedRgb::Z4, juce::Colours::transparentBlack);

        slLinkManager.setWhiteLed(static_cast<slmk2::SLLed>(subIndex), false);

        apvts.state.removeListener(this);
        UIPage::deactivatePage();
    }

    void valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged,
                                  const juce::Identifier &property) override;

    void valueTreeChildAdded(juce::ValueTree &, juce::ValueTree &) override {}
    void valueTreeChildRemoved(juce::ValueTree &, juce::ValueTree &, int) override {}
    void valueTreeChildOrderChanged(juce::ValueTree &, int, int) override {}
    void valueTreeParentChanged(juce::ValueTree &) override {}

private:
    static constexpr const char *connectionStatusProperty = "connectionStatus";

    // Subindex for the currently selected subsection (Tune, Model, Config, Repeat)
    juce::StringArray subSections = {"Tune", "Model", "Config", "Repeat"};
    int subIndex = 0;

    juce::StringArray embeddedSounds = {
        "",
        "Barbro Pers Polska",
        "Crehan's Fiddle",
        "Cronin's",
        "Dan Dowd's",
        "Gerry Holland's",
        "Handsome Young Maidens",
        "Hardiman's Jig",
        "Johnstone's Fancy",
        "Miss Galvin's",
        "John Doherty's",
    };
    int embeddedSoundIndex = 0;

    // Colours
    juce::Colour onButtonColour, offButtonColour;
    juce::Colour grayTextColour = juce::Colour::fromString("#bababa");
    const uint16_t HEADER_HEIGHT = 48, FOOTER_HEIGHT = 35, KNOB_HOR_MARGIN = 12, KNOB_HOR_OFFSET = 61 + 17; // 61 is the knob width, 17 is the margin between knobs

    const uint16_t KNOBTEXT_X_OFFSET = 14, KNOBTEXT_Y_OFFSET = 23; // Offset of text inside knob area, with respect to knob top-left corner
    const uint16_t KNOB_BMP_WIDTH = 61;
    const uint16_t INFO_TEXT_Y = 181;

    void parameterChanged(const juce::String &parameterID, float newValue) override;
    void pageHandleEncoderItem(const slmk2::SLEncoder encoderIndex, const uint8_t tickValue) override;
    void pageHandleButtonItem(const slmk2::SLButton buttonIndex, const uint8_t tickValue) override;
};