#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// One part's scale/octave tune. 64 is 0 cents. Each step is one cent.
class ScaleTune : public juce::Component
{
public:
    std::function<void(int semitone, int value)> onChange;
    std::function<void()> onReset;

    ScaleTune()
    {
        reset.setButtonText("RESET");
        reset.onClick = [this] { if (onReset) onReset(); };
        addAndMakeVisible(reset);
        for (int i = 0; i < 12; ++i)
            value[i] = 64;
    }

    void setValues(const int* twelve)
    {
        for (int i = 0; i < 12; ++i)
            value[i] = juce::jlimit(0, 127, twelve[i]);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff101218));
        g.fillRoundedRectangle(r, 6.0f);
        auto row = r.reduced(6.0f, 4.0f);
        row.removeFromRight(64.0f);
        g.setColour(juce::Colour(0xff8b8f9c));
        g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        g.drawText("SCALE TUNE  (cents, 0 is even)", row.removeFromTop(14.0f).toNearestInt(),
                   juce::Justification::centredLeft);
        const float colW = row.getWidth() / 12.0f;
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i)
        {
            auto col = juce::Rectangle<float>(row.getX() + colW * (float) i, row.getY(), colW, row.getHeight());
            auto name = col.removeFromBottom(14.0f);
            auto num = col.removeFromBottom(14.0f);
            auto track = col.reduced(colW * 0.28f, 2.0f);
            g.setColour(juce::Colour(0xff323646));
            g.fillRoundedRectangle(track, 2.0f);
            const float u = (float) value[i] / 127.0f;
            const float y = track.getBottom() - u * track.getHeight();
            g.setColour(juce::Colour(0xffe8a317));
            g.fillRect(track.getX(), y, track.getWidth(), track.getBottom() - y);
            g.setColour(juce::Colour(0xffece8df));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(juce::String(value[i] - 64), num.toNearestInt(), juce::Justification::centred);
            g.setColour(juce::Colour(0xff8b8f9c));
            g.drawText(names[i], name.toNearestInt(), juce::Justification::centred);
        }
    }

    void resized() override
    {
        reset.setBounds(getWidth() - 62, 8, 54, 22);
    }

    void mouseDown(const juce::MouseEvent& e) override { dragAt(e.position); }
    void mouseDrag(const juce::MouseEvent& e) override { dragAt(e.position); }

private:
    void dragAt(juce::Point<float> p)
    {
        auto row = getLocalBounds().toFloat().reduced(6.0f, 4.0f);
        row.removeFromRight(64.0f);
        row.removeFromTop(14.0f);
        auto trackZone = row;
        trackZone.removeFromBottom(28.0f);
        if (! trackZone.contains(p) && (p.y < trackZone.getY() || p.y > trackZone.getBottom()))
            return;
        const float colW = row.getWidth() / 12.0f;
        const int i = juce::jlimit(0, 11, (int) ((p.x - row.getX()) / colW));
        const float u = juce::jlimit(0.0f, 1.0f, (trackZone.getBottom() - p.y) / trackZone.getHeight());
        const int v = juce::jlimit(0, 127, (int) std::lround(u * 127.0f));
        if (v == value[i])
            return;
        value[i] = v;
        if (onChange)
            onChange(i, v);
        repaint();
    }

    int value[12];
    juce::TextButton reset;
};
