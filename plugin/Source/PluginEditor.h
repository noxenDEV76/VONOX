#pragma once
#include "PluginProcessor.h"

// Look: galaxy theme. Black window with animated white stars; on top, the pre-rendered transparent artwork
// (plugin/assets/background.png: steel plates, chains, sockets). Everything that moves or shows a value is drawn live here.
namespace vfont {
juce::Font orbitron(float height);                       // numeric readouts, titles
juce::Font rajdhani(float height, bool semiBold = false); // labels, body text
}
namespace theme {
inline const juce::Colour blue(70, 150, 255), violet(170, 85, 255), magenta(255, 70, 190), cyan(90, 225, 255), lavender(214, 200, 255);
}

// Twinkling, slowly drifting white stars (some with a 4-point flare). Used for the window, the ADVANCED panel and the preset browser.
struct StarField {
    struct S { float x, y, vx, vy, size, ph, spd, a; int tint; };
    std::vector<S> s; float w = 1, h = 1; juce::Random rng;
    void init(int n, float W, float H, float drift) {
        w = W; h = H; s.clear();
        for (int i = 0; i < n; ++i) {
            const float big = rng.nextFloat() < 0.07f ? 1.f : 0.f;
            s.push_back({ rng.nextFloat() * W, rng.nextFloat() * H, (rng.nextFloat() - .5f) * drift * 0.6f, -(0.15f + rng.nextFloat() * 0.85f) * drift,
                          0.9f + rng.nextFloat() * 1.1f + big * 1.6f, rng.nextFloat() * 6.28f, 0.03f + rng.nextFloat() * 0.07f, 0.35f + rng.nextFloat() * 0.65f,
                          rng.nextFloat() < 0.22f ? 1 : (rng.nextFloat() < 0.12f ? 2 : 0) });
        }
    }
    void tick() {
        for (auto& q : s) {
            q.ph += q.spd; q.x += q.vx; q.y += q.vy;
            if (q.y < -4.f) { q.y = h + 4.f; q.x = rng.nextFloat() * w; }
            if (q.x < -4.f) q.x = w + 4.f; else if (q.x > w + 4.f) q.x = -4.f;
        }
    }
    void paint(juce::Graphics& g, float alpha = 1.f) const {
        for (const auto& q : s) {
            const float tw = 0.5f + 0.5f * std::sin(q.ph), a = alpha * q.a * (0.3f + 0.7f * tw);
            const juce::Colour c = q.tint == 1 ? juce::Colour(190, 200, 255) : (q.tint == 2 ? juce::Colour(255, 215, 245) : juce::Colours::white);
            if (q.size > 2.2f) {
                g.setColour(c.withAlpha(a * 0.5f)); const float L = q.size * 3.2f;
                g.drawLine(q.x - L, q.y, q.x + L, q.y, 0.8f); g.drawLine(q.x, q.y - L, q.x, q.y + L, 0.8f);
                g.setColour(c.withAlpha(a * 0.16f)); g.fillEllipse(q.x - q.size * 2.f, q.y - q.size * 2.f, q.size * 4.f, q.size * 4.f);
            }
            g.setColour(c.withAlpha(a)); g.fillEllipse(q.x - q.size * .5f, q.y - q.size * .5f, q.size, q.size);
        }
    }
};

struct VonoxKnob : public juce::Slider {
    VonoxKnob(const juce::Image& body, juce::Colour accent, int radius, const juce::String& unit, int decimals);
    void paint(juce::Graphics&) override;
    void valueChanged() override { pulse = 1.f; }
    void tick() {   // ~30 Hz: eases the hover halo, decays the "value changed" pulse
        const float target = (isMouseOverOrDragging() || isMouseButtonDown()) ? 1.f : 0.f;
        const float h0 = hover, p0 = pulse; hover += (target - hover) * 0.25f; pulse *= 0.86f; if (pulse < 0.01f) pulse = 0.f;
        if (std::abs(hover - h0) > 0.003f || std::abs(pulse - p0) > 0.003f) repaint();
    }
    static constexpr int kTopPad = 24;
    juce::Image body; juce::Colour accent; int r; juce::String unit; int dec; float hover = 0.f, pulse = 0.f;
};

// Sprite button (artwork has an 8 px transparent margin around the button). No square hover: a rounded glow outline follows the button shape,
// state changes cross-fade, presses send a ripple, momentary buttons flash.
struct SpriteButton : public juce::Button {
    SpriteButton(const juce::String& n, const juce::Image& on, const juce::Image& off, juce::Colour accent = theme::blue)
        : juce::Button(n), imgOn(on), imgOff(off), glow(accent) {}
    void paintButton(juce::Graphics& g, bool, bool down) override {
        g.drawImageAt(imgOff, 0, 0);
        const float onA = juce::jlimit(0.f, 1.f, juce::jmax(onAmt, flash));
        if (onA > 0.01f) { g.setOpacity(onA); g.drawImageAt(imgOn, 0, 0); g.setOpacity(1.f); }
        const auto body = getLocalBounds().toFloat().reduced(8.f);
        if (hover > 0.01f) {
            g.setColour(glow.withAlpha(0.10f * hover)); g.fillRoundedRectangle(body, 9.f);
            g.setColour(glow.withAlpha(0.22f * hover)); g.drawRoundedRectangle(body.expanded(2.f), 11.f, 3.f);
            g.setColour(glow.brighter(0.4f).withAlpha(0.8f * hover)); g.drawRoundedRectangle(body.expanded(0.5f), 9.5f, 1.3f);
        }
        if (down) { g.setColour(juce::Colours::black.withAlpha(0.22f)); g.fillRoundedRectangle(body, 9.f); }
        if (ripple > 0.f && ripple < 1.f) { g.setColour(glow.brighter(0.5f).withAlpha((1.f - ripple) * 0.7f)); g.drawRoundedRectangle(body.expanded(ripple * 7.f), 9.f + ripple * 5.f, 2.f); }
    }
    void buttonStateChanged() override { if (isDown() && !wasDown) ripple = 0.001f; wasDown = isDown(); }
    void clicked() override { if (!getClickingTogglesState()) flash = 1.f; }
    void tick() {
        const float t = getToggleState() ? 1.f : 0.f, o0 = onAmt, h0 = hover, f0 = flash, r0 = ripple;
        onAmt += (t - onAmt) * 0.3f; hover += ((isMouseOver() ? 1.f : 0.f) - hover) * 0.3f;
        flash *= 0.84f; if (flash < 0.02f) flash = 0.f;
        if (ripple > 0.f) { ripple += 0.09f; if (ripple >= 1.f) ripple = 0.f; }
        if (std::abs(onAmt - o0) > 0.003f || std::abs(hover - h0) > 0.003f || flash != f0 || ripple != r0) repaint();
    }
    juce::Image imgOn, imgOff; juce::Colour glow; float onAmt = 0.f, hover = 0.f, flash = 0.f, ripple = 0.f; bool wasDown = false;
};

struct AutoMixButton : public juce::Button {
    AutoMixButton() : juce::Button("automix") {}
    void paintButton(juce::Graphics& g, bool, bool down) override {
        auto b = getLocalBounds().toFloat().reduced(2.f);
        if (busyFlag) { g.setColour(theme::cyan.withAlpha(0.10f + 0.10f * std::sin(juce::Time::getMillisecondCounter() * 0.012f))); g.fillRoundedRectangle(b, 9.f); }
        if (hover > 0.01f) { g.setColour(theme::violet.withAlpha(0.14f * hover)); g.fillRoundedRectangle(b, 9.f); g.setColour(theme::cyan.withAlpha(0.7f * hover)); g.drawRoundedRectangle(b, 9.f, 1.4f); }
        if (down) { g.setColour(juce::Colours::black.withAlpha(0.25f)); g.fillRoundedRectangle(b, 9.f); }
        if (ripple > 0.f && ripple < 1.f) { g.setColour(theme::cyan.withAlpha((1.f - ripple) * 0.7f)); g.drawRoundedRectangle(b.expanded(ripple * 8.f), 9.f + ripple * 6.f, 2.f); }
    }
    void buttonStateChanged() override { if (isDown() && !wasDown) ripple = 0.001f; wasDown = isDown(); }
    void tick() {
        const float h0 = hover, r0 = ripple; hover += ((isMouseOver() ? 1.f : 0.f) - hover) * 0.3f;
        if (ripple > 0.f) { ripple += 0.08f; if (ripple >= 1.f) ripple = 0.f; }
        if (std::abs(hover - h0) > 0.003f || ripple != r0 || busyFlag) repaint();
    }
    bool busyFlag = false; float hover = 0.f, ripple = 0.f; bool wasDown = false;
};

// ADVANCED panel knob: brushed-metal body with a turning knurled edge (no outline circle).
struct MiniKnob : public juce::Slider {
    MiniKnob(const juce::String& n, const juce::String& u, int d, juce::Colour c) : label(n), unit(u), dec(d), accent(c) {
        setSliderStyle(juce::Slider::RotaryVerticalDrag); setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        setMouseDragSensitivity(160); setScrollWheelEnabled(true); setPopupMenuEnabled(false); }
    void paint(juce::Graphics& g) override;
    juce::String label, unit; int dec; juce::Colour accent;
};

class AdvancedPanel : public juce::Component {
public:
    AdvancedPanel(VonoxProcessor&, const juce::Image& bg);
    void paint(juce::Graphics&) override;
    void resized() override { vp.setBounds(getLocalBounds().reduced(10, 34)); }
    void tick() { stars.tick(); repaint(); }
private:
    VonoxProcessor& proc; juce::Image bg; juce::Viewport vp; StarField stars;
    struct Content : public juce::Component { void paint(juce::Graphics&) override; std::vector<std::pair<juce::String, juce::Rectangle<int>>> heads, labels; } content;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles; std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> tAtt;
    std::vector<std::unique_ptr<juce::TextButton>> buttons; std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::unique_ptr<MiniKnob>> knobs; std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> kAtt; std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> cAtt;
};

// Full-window preset browser: black panel, white names, tiny white particles drifting in the background.
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
    std::vector<Row> rows; StarField stars;
};

class VonoxEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit VonoxEditor(VonoxProcessor&);
    ~VonoxEditor() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void showBrowser(bool on) { if (on) browser->open(); else browser->close(); }
    void showAdvanced(bool on) { adv->setVisible(on); advBtn.setToggleState(on, juce::dontSendNotification); }
private:
    void timerCallback() override;
    void showPresetMenu() { browser->open(); }
    void cycleStyle(int dir);
    void burst(float x, float y);
    static juce::Image res(const char* name);

    VonoxProcessor& proc;
    juce::Image background, fireTitle, fireAuto;
    std::vector<std::unique_ptr<VonoxKnob>> knobs;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> knobAtt;
    VonoxKnob* mixKnob = nullptr;
    SpriteButton bypassBtn, aBtn, bBtn, resetBtn, presetsBtn, advBtn;
    std::unique_ptr<AdvancedPanel> adv; std::unique_ptr<PresetBrowser> browser; AutoMixButton autoBtn;
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAtt;

    float frameF = 0.f; int frame = 0, lastAutoCount = 0; float mIn = 0, mOut = 0, level = 0, bypassPulse = 0;
    juce::String statusLine;
    StarField stars;
    struct VP { float x, y, vx, vy, size, ph, a; };
    std::vector<VP> voiceStars; juce::Path glassPath;
    struct B { float x, y, vx, vy, life, size; };
    std::vector<B> bursts;
    juce::Random rng;
    juce::Rectangle<int> styleBox{ 466, 198, 168, 28 }, leftArrow{ 466, 198, 30, 28 }, rightArrow{ 604, 198, 30, 28 };
    juce::SharedResourcePointer<juce::TooltipWindow> tooltip;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VonoxEditor)
};
