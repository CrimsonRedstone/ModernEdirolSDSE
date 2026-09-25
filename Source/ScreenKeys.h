#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// On-screen piano for trying a part. Off until the user opens it.
// Computer-key mapping (FL Studio style) lives in the editor.
class ScreenKeys : public juce::Component
{
public:
    std::function<void(int pitch, bool on)> onNote;

    ScreenKeys()
    {
        downBtn.setButtonText("-");
        upBtn.setButtonText("+");
        downBtn.onClick = [this]
        {
            setOctave(octave - 1);
            if (onOctave) onOctave(octave);
        };
        upBtn.onClick = [this]
        {
            setOctave(octave + 1);
            if (onOctave) onOctave(octave);
        };
        addAndMakeVisible(downBtn);
        addAndMakeVisible(upBtn);
    }

    std::function<void(int)> onOctave;

    void setOctave(int o)
    {
        octave = juce::jlimit(0, 8, o);
        repaint();
    }

    int getOctave() const { return octave; }

    void setLit(int pitch, bool on)
    {
        if (pitch < 0 || pitch > 127)
            return;
        if (lit[pitch] == on)
            return;
        lit[pitch] = on;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff1a1d27));
        g.fillRoundedRectangle(r, 8.0f);
        auto keys = r.reduced(8.0f, 6.0f);
        keys.removeFromLeft(92.0f);
        const float whiteW = keys.getWidth() / 14.0f;
        const int base = (octave + 1) * 12;
        for (int i = 0; i < 14; ++i)
        {
            const int pitch = base + whitePc[i];
            const bool on = pitch >= 0 && pitch < 128 && lit[pitch];
            g.setColour(on ? juce::Colour(0xffe8a317) : juce::Colour(0xffece8df));
            g.fillRoundedRectangle(keys.getX() + whiteW * (float) i + 1.0f, keys.getY(),
                                   whiteW - 2.0f, keys.getHeight(), 3.0f);
            if (i == 0 || i == 7)
            {
                g.setColour(juce::Colour(0xff1a1408));
                g.setFont(juce::FontOptions(11.0f));
                g.drawText("C" + juce::String(octave + (i == 7 ? 1 : 0)),
                           (int) (keys.getX() + whiteW * (float) i), (int) (keys.getBottom() - 16.0f),
                           (int) whiteW, 14, juce::Justification::centred);
            }
        }
        const float bh = keys.getHeight() * 0.58f;
        for (int i = 0; i < 14; ++i)
        {
            const int pc = whitePc[i];
            if ((pc % 12) == 4 || (pc % 12) == 11)
                continue;
            const int pitch = base + pc + 1;
            const bool on = pitch >= 0 && pitch < 128 && lit[pitch];
            const float bw = whiteW * 0.58f;
            const float x = keys.getX() + whiteW * ((float) i + 1.0f) - bw * 0.5f;
            g.setColour(on ? juce::Colour(0xffe8a317) : juce::Colour(0xff101218));
            g.fillRoundedRectangle(x, keys.getY(), bw, bh, 2.0f);
        }
        g.setColour(juce::Colour(0xff8b8f9c));
        g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        g.drawText("KEYS", 10, 8, 70, 16, juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("Z row plays", 8, 24, 78, 14, juce::Justification::centredLeft);
    }

    void resized() override
    {
        downBtn.setBounds(8, getHeight() - 28, 28, 22);
        upBtn.setBounds(40, getHeight() - 28, 28, 22);
    }

    void mouseDown(const juce::MouseEvent& e) override { pressAt(e.position); }
    void mouseDrag(const juce::MouseEvent& e) override { pressAt(e.position); }
    void mouseUp(const juce::MouseEvent&) override { releaseHeld(); }

private:
    static constexpr int whitePc[14] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23 };

    int pitchAt(juce::Point<float> p) const
    {
        auto keys = getLocalBounds().toFloat().reduced(8.0f, 6.0f);
        keys.removeFromLeft(92.0f);
        if (! keys.contains(p))
            return -1;
        const float whiteW = keys.getWidth() / 14.0f;
        const int base = (octave + 1) * 12;
        const float bh = keys.getHeight() * 0.58f;
        if (p.y < keys.getY() + bh)
        {
            for (int i = 0; i < 14; ++i)
            {
                const int pc = whitePc[i];
                if ((pc % 12) == 4 || (pc % 12) == 11)
                    continue;
                const float bw = whiteW * 0.58f;
                const float x = keys.getX() + whiteW * ((float) i + 1.0f) - bw * 0.5f;
                if (p.x >= x && p.x < x + bw)
                    return juce::jlimit(0, 127, base + pc + 1);
            }
        }
        const int wi = juce::jlimit(0, 13, (int) ((p.x - keys.getX()) / whiteW));
        return juce::jlimit(0, 127, base + whitePc[wi]);
    }

    void pressAt(juce::Point<float> p)
    {
        const int pitch = pitchAt(p);
        if (pitch == held)
            return;
        releaseHeld();
        if (pitch < 0)
            return;
        held = pitch;
        lit[pitch] = true;
        if (onNote) onNote(pitch, true);
        repaint();
    }

    void releaseHeld()
    {
        if (held < 0)
            return;
        const int n = held;
        held = -1;
        if (n >= 0 && n < 128)
            lit[n] = false;
        if (onNote) onNote(n, false);
        repaint();
    }

    int octave { 4 };
    int held { -1 };
    bool lit[128] {};
    juce::TextButton downBtn, upBtn;
};
