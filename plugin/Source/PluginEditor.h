#pragma once
#include "PluginProcessor.h"

// Look: the artwork (steel plates, chains, V lettering, knob sockets) is a pre-rendered bitmap
// (plugin/assets/background.png). Everything that moves or shows a value is drawn live here.
struct VonoxKnob : public juce::Slider {
    VonoxKnob(const juce::Image& body, juce::Colour accent, int radius, const juce::String& unit, int decimals);
    void paint(juce::Graphics&) override;
    juce::Point<float> centre() const { return { getWidth() * 0.5f, float(kTopPad) + getHeight() * 0.0f + (getHeight() - kTopPad) * 0.5f }; }
    static constexpr int kTopPad = 24;
    juce::Image body; juce::Colour accent; int r; juce::String unit; int dec;
};

struct SpriteButton : public juce::Button {
    SpriteButton(const juce::String& n, const juce::Image& on, const juce::Image& off) : juce::Button(n), imgOn(on), imgOff(off) {}
    void paintButton(juce::Graphics& g, bool over, bool down) override {
        g.drawImageAt(getToggleState() ? imgOn : imgOff, 0, 0);
        if (down) g.fillAll(juce::Colours::black.withAlpha(0.25f)); else if (over) g.fillAll(juce::Colours::white.withAlpha(0.07f));
    }
    juce::Image imgOn, imgOff;
};

struct AutoMixButton : public juce::Button {
    AutoMixButton() : juce::Button("automix") {}
    void paintButton(juce::Graphics& g, bool over, bool down) override {
        auto b = getLocalBounds().toFloat();
        if (busyFlag) g.setColour(juce::Colours::white.withAlpha(0.10f + 0.10f * std::sin(juce::Time::getMillisecondCounter() * 0.012f)));
        else if (down) g.setColour(juce::Colours::black.withAlpha(0.25f)); else if (over) g.setColour(juce::Colours::white.withAlpha(0.08f)); else return;
        g.fillRoundedRectangle(b, 9.f);
    }
    bool busyFlag = false;
};

struct MiniKnob : public juce::Slider {
    MiniKnob(const juce::String& n, const juce::String& u, int d, juce::Colour c) : label(n), unit(u), dec(d), accent(c) {
        setSliderStyle(juce::Slider::RotaryVerticalDrag); setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        setMouseDragSensitivity(160); setScrollWheelEnabled(true); setPopupMenuEnabled(false); }
    void paint(juce::Graphics& g) override {
        const float cx = getWidth() * 0.5f, cy = 20.f, r = 14.f;
        const float norm = float(valueToProportionOfLength(getValue())), a0 = 225.f * 0.0174533f, a1 = a0 + 270.f * 0.0174533f * norm;
        juce::Path tr; tr.addCentredArc(cx, cy, r + 3, r + 3, 0.f, a0, a0 + 270.f * 0.0174533f, true);
        g.setColour(juce::Colours::black.withAlpha(0.7f)); g.strokePath(tr, juce::PathStrokeType(3.f));
        if (norm > 0.001f) { juce::Path v; v.addCentredArc(cx, cy, r + 3, r + 3, 0.f, a0, a1, true); g.setColour(accent); g.strokePath(v, juce::PathStrokeType(3.f)); }
        g.setColour(juce::Colour(205, 205, 215)); g.fillEllipse(cx - r, cy - r, 2 * r, 2 * r);
        g.setColour(juce::Colour(40, 40, 50)); g.drawEllipse(cx - r, cy - r, 2 * r, 2 * r, 1.5f);
        g.setColour(accent); g.drawLine(cx + std::sin(a1) * 4.f, cy - std::cos(a1) * 4.f, cx + std::sin(a1) * (r - 2), cy - std::cos(a1) * (r - 2), 2.5f);
        g.setColour(juce::Colour(255, 205, 225)); g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText(juce::String(getValue(), dec) + (unit.isEmpty() ? "" : " " + unit), 0, 38, getWidth(), 13, juce::Justification::centred);
        g.setColour(juce::Colours::white.withAlpha(0.85f)); g.setFont(juce::FontOptions(10.f));
        g.drawFittedText(label, 2, 51, getWidth() - 4, 24, juce::Justification::centredTop, 2);
    }
    juce::String label, unit; int dec; juce::Colour accent;
};

class AdvancedPanel : public juce::Component {
public:
    AdvancedPanel(VonoxProcessor&, const juce::Image& bg);
    void paint(juce::Graphics&) override;
    void resized() override { vp.setBounds(getLocalBounds().reduced(10, 34)); }
private:
    VonoxProcessor& proc; juce::Image bg; juce::Viewport vp;
    struct Content : public juce::Component { void paint(juce::Graphics&) override; std::vector<std::pair<juce::String, juce::Rectangle<int>>> heads, labels; } content;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles; std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> tAtt;
    std::vector<std::unique_ptr<juce::TextButton>> buttons; std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::unique_ptr<MiniKnob>> knobs; std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> kAtt; std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> cAtt;
};


// Full-window preset browser: black panel, white artist names grouped in categories, tiny white particles drifting in the background.
class PresetBrowser : public juce::Component, private juce::Timer {
public:
    explicit PresetBrowser(VonoxProcessor&);
    ~PresetBrowser() override { stopTimer(); }
    void open() { refresh(); setVisible(true); toFront(true); startTimerHz(30); }
    void close() { setVisible(false); stopTimer(); }
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override { hoverRow = hoverCat = -1; repaint(); }
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    std::function<void()> onPicked;
private:
    struct Row { juce::String title, sub; int program = -1; juce::String user; bool isSave = false; };
    void timerCallback() override;
    void refresh();
    void rebuildRows();
    juce::Rectangle<int> panel() const { return getLocalBounds().withSizeKeepingCentre(700, 520); }
    juce::Rectangle<int> catArea() const { return panel().withTrimmedTop(70).withWidth(190).withTrimmedBottom(14); }
    juce::Rectangle<int> listArea() const { return panel().withTrimmedTop(70).withTrimmedLeft(200).withTrimmedBottom(14).withTrimmedRight(10); }
    int rowH = 46;
    VonoxProcessor& proc;
    juce::StringArray cats; int selCat = 0, hoverCat = -1, hoverRow = -1; float scroll = 0.f;
    std::vector<Row> rows;
    struct P { float x, y, vy, vx, a, size, ph; };
    std::vector<P> parts; juce::Random rng;
};

class VonoxEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit VonoxEditor(VonoxProcessor&);
    ~VonoxEditor() override;
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void showBrowser(bool on) { if (on) browser->open(); else browser->close(); }
    void showAdvanced(bool on) { adv->setVisible(on); advBtn.setToggleState(on, juce::dontSendNotification); }
private:
    void timerCallback() override;
    void showPresetMenu() { browser->open(); }
    void cycleStyle(int dir);
    static juce::Image res(const char* name);

    VonoxProcessor& proc;
    juce::Image background, fireTitle, fireAuto;
    std::vector<std::unique_ptr<VonoxKnob>> knobs;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> knobAtt;
    VonoxKnob* mixKnob = nullptr;
    SpriteButton bypassBtn, aBtn, bBtn, levelBtn, advBtn; std::unique_ptr<AdvancedPanel> adv; std::unique_ptr<PresetBrowser> browser; AutoMixButton autoBtn;
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAtt, levelAtt;
    juce::ParameterAttachment* dummy = nullptr;

    int frame = 0, lastAutoCount = 0; float mIn = 0, mOut = 0, mGr = 0;
    juce::String statusLine;
    struct P { float x, y, vy, vx, life, size; };
    std::vector<P> parts; juce::Random rng;
    juce::Rectangle<int> styleBox{ 458, 198, 184, 30 }, leftArrow{ 458, 198, 34, 30 }, rightArrow{ 608, 198, 34, 30 };
    juce::Rectangle<int> mixZone{ 480, 290, 140, 56 };
    juce::SharedResourcePointer<juce::TooltipWindow> tooltip;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VonoxEditor)
};
