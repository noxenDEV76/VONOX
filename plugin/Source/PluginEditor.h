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

class VonoxEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit VonoxEditor(VonoxProcessor&);
    ~VonoxEditor() override;
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void showPresetMenu();
    void cycleStyle(int dir);
    static juce::Image res(const char* name);

    VonoxProcessor& proc;
    juce::Image background, fireTitle, fireAuto;
    std::vector<std::unique_ptr<VonoxKnob>> knobs;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> knobAtt;
    VonoxKnob* mixKnob = nullptr;
    SpriteButton bypassBtn, aBtn, bBtn, levelBtn; AutoMixButton autoBtn;
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
