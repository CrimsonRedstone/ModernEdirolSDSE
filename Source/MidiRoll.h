#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>
#include "MidiPlayer.h"
#include "Skin.h"
#include "AeternaCompanion.h"

// Falling-notes piano-roll (Synthesia / Guitar Hero highway).
// Pitch on X, time coming toward a keyboard at the bottom.
// Channel colours stay on the bars - no gem shapes.
// Hitting a note blooms a Guitar Hero glow on the key.
class MidiRoll : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onPopOut, onFullScreen;
    std::function<void(int)> onAeternaMode;

    MidiRoll()
    {
        setOpaque(false);
        setWantsKeyboardFocus(true);
        popBtn.setButtonText("POP OUT");
        fullBtn.setButtonText("FULL");
        aetBtn.setButtonText("Aeterna On");
        popBtn.setConnectedEdges(juce::Button::ConnectedOnRight);
        fullBtn.setConnectedEdges(juce::Button::ConnectedOnLeft);
        popBtn.onClick = [this] { if (onPopOut) onPopOut(); };
        fullBtn.onClick = [this] { if (onFullScreen) onFullScreen(); };
        aetBtn.onClick = [this]
        {
            const int next = aeterna.getMode() == 0 ? 1 : 0;
            aeterna.setMode(next);
            refreshAeternaButton();
            if (onAeternaMode)
                onAeternaMode(next);
            repaint();
        };
        aetBtn.setTooltip("Aeterna on the piano roll. Click to turn her on or off.");
        addAndMakeVisible(popBtn);
        addAndMakeVisible(fullBtn);
        addAndMakeVisible(aetBtn);
        for (int i = 0; i < 128; ++i)
        {
            prevOn[i] = false;
            soundingNow[i] = false;
            soundColNow[i] = 0;
        }
    }

    ~MidiRoll() override { stopTimer(); }

    void attach(MidiPlayerEngine* e)
    {
        if (engine == e)
            return;
        engine = e;
        seenGen = -1;
        score.clear();
        channelMask = 0;
        scoreLength = 0;
        lastPlaying = false;
        flashes.clear();
        for (int i = 0; i < 128; ++i)
            prevOn[i] = false;
        if (engine != nullptr)
            engine->copyScore(seenGen, score, scoreLength, loNote, hiNote, channelMask);
        holdPicture = false;
        refitKeys();
        aeterna.noteScoreChanged();
    }

    void setPortLetter(char c)
    {
        const char letter = (c == 'B' || c == 'b') ? 'B' : 'A';
        if (portLetter == letter)
            return;
        portLetter = letter;
        repaint();
    }

    void setActive(bool v)
    {
        if (v == active)
            return;
        active = v;
        if (v)
            startTimerHz(60);
        else
            stopTimer();
        repaint();
    }

    void setPalette(const SkinPalette& p)
    {
        pal = &p;
        auto cSurf = juce::Colour(p.surface2);
        auto cText = juce::Colour(p.text);
        for (auto* b : { &popBtn, &fullBtn, &aetBtn })
        {
            b->setColour(juce::TextButton::buttonColourId, cSurf);
            b->setColour(juce::TextButton::textColourOffId, cText);
        }
        refreshAeternaButton();
        repaint();
    }

    void setPartNames(const juce::StringArray& names16) { partNames = names16; }

    void setSilencedMask(std::uint32_t mask16) { silenced = mask16; }

    void setChrome(bool popped, bool fullScreen)
    {
        popBtn.setButtonText(popped ? "DOCK" : "POP OUT");
        fullBtn.setButtonText(fullScreen ? "EXIT FULL" : "FULL");
    }

    void setAeternaMode(int mode)
    {
        aeterna.setMode(mode);
        refreshAeternaButton();
        repaint();
    }

    void setAeternaLinger(bool v) { aeterna.setLinger(v); }
    void setAeternaBpm(double bpm) { aeterna.setBpm(bpm); }
    void setAeternaDrums(std::uint32_t mask)
    {
        aeterna.setDrumMask(mask);
        if (mask == drumMask)
            return;
        drumMask = mask;
        fitValid = false;
        if (! score.empty())
            refitKeys();
    }

    // STOP (not pause) should show the start of the file.
    void snapDisplayToEngine() { snapDisplay = true; }

    int getAeternaMode() const { return aeterna.getMode(); }

    void setBanner(const juce::String& s)
    {
        if (banner == s)
            return;
        banner = s;
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(16, 12);
        auto cap = r.removeFromTop(18);
        fullBtn.setBounds(cap.removeFromRight(104).reduced(2, 0));
        popBtn.setBounds(cap.removeFromRight(92).reduced(2, 0));
        aetBtn.setBounds(cap.removeFromRight(118).reduced(2, 0));
    }

    void paint(juce::Graphics& g) override
    {
        const bool playingNow = engine != nullptr && engine->isLoaded() && engine->isPlaying();
        if (engine != nullptr && (playingNow || ! holdPicture))
        {
            const int genBefore = seenGen;
            engine->copyScore(seenGen, score, scoreLength, loNote, hiNote, channelMask);
            if (seenGen != genBefore)
                refitKeys();
        }

        auto cBg = juce::Colour(pal ? pal->bg : 0xff101218);
        auto cSurf = juce::Colour(pal ? pal->surface : 0xff1a1d27);
        auto cAcc = juce::Colour(pal ? pal->accent : 0xffe8a317);
        auto cAcc2 = juce::Colour(pal ? pal->accent2 : 0xff3dbaa0);
        auto cText = juce::Colour(pal ? pal->text : 0xffece8df);
        auto cMut = juce::Colour(pal ? pal->muted : 0xff8b8f9c);
        auto cBorder = juce::Colour(pal ? pal->border : 0xff323646);

        if (isOpaque())
            g.fillAll(cBg);

        auto r = getLocalBounds().toFloat().reduced(8.0f);
        g.setColour(cSurf);
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(cBorder);
        g.drawRoundedRectangle(r, 10.0f, 1.0f);

        const bool haveTape = engine != nullptr && engine->isLoaded();
        int lo = fitValid ? fitLo : (haveTape ? loNote : 36);
        int hi = fitValid ? fitHi : (haveTape ? hiNote : 84);
        lo = juce::jlimit(0, 127, lo);
        hi = juce::jlimit(lo, 127, hi);
        lo -= lo % 12;
        if ((hi % 12) != 11)
            hi += 11 - (hi % 12);
        hi = juce::jmin(127, hi);
        if (hi < lo)
            hi = juce::jmin(127, lo + 11);
        const int spanLo = lo;
        const int span = juce::jmax(12, hi - spanLo + 1);

        std::uint32_t visMask = 0;
        int visNotes = 0;
        if (haveTape)
        {
            for (const auto& n : score)
            {
                if (n.note < spanLo || n.note >= spanLo + span)
                    continue;
                if (n.channel >= 1 && n.channel <= 16)
                    visMask |= (1u << (n.channel - 1));
                ++visNotes;
            }
        }
        const std::uint32_t mask = haveTape ? visMask : 0;
        const int used = countBits(mask);
        const int legendH = (! haveTape) ? 22 : (used > 8 ? 38 : 22);

        auto inner = r.reduced(12.0f, 10.0f);
        auto caption = inner.removeFromTop(16.0f);
        auto legend = inner.removeFromBottom((float) legendH);
        inner.removeFromTop(4.0f);
        inner.removeFromBottom(4.0f);

        g.setColour(cMut);
        g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        juce::String cap;
        if (! haveTape)
            cap = "Load a .mid - notes fall toward the keyboard";
        else
        {
            cap = (portLetter == 'B' ? "PART B  |  " : "PART A  |  ");
            cap += juce::String(used) + " colours  |  "
                + juce::String(visNotes) + " notes  |  "
                + (lastPlaying ? "PLAYING" : (displayPos > 0.08 ? "PAUSED" : "CUED"));
        }
        if (banner.isNotEmpty() && haveTape)
            cap = banner + "  |  " + cap;
        g.drawText(cap, caption.withTrimmedRight(340.0f).toNearestInt(),
                   juce::Justification::centredLeft);

        auto well = inner;
        const auto wellFill = cBg.interpolatedWith(juce::Colours::black, 0.78f);
        g.setColour(wellFill);
        g.fillRoundedRectangle(well, 7.0f);
        g.setColour(cAcc.withAlpha(0.28f));
        g.drawRoundedRectangle(well, 7.0f, 1.0f);

        const float keyH = juce::jlimit(48.0f, 92.0f, well.getHeight() * 0.22f);
        auto keys = well.removeFromBottom(keyH);
        auto plot = well.reduced(6.0f, 4.0f);

        float keyX[128];
        float keyWf[128];
        for (int i = 0; i < 128; ++i)
        {
            keyX[i] = 0.0f;
            keyWf[i] = 0.0f;
        }
        int nWhite = 0;
        for (int n = spanLo; n < spanLo + span; ++n)
            if (! isBlackKey(n))
                ++nWhite;
        nWhite = juce::jmax(1, nWhite);
        const float whiteW = keys.getWidth() / (float) nWhite;
        float wx = keys.getX();
        for (int n = spanLo; n < spanLo + span; ++n)
        {
            if (isBlackKey(n))
                continue;
            keyX[n] = wx;
            keyWf[n] = whiteW;
            wx += whiteW;
        }
        for (int n = spanLo; n < spanLo + span; ++n)
        {
            if (! isBlackKey(n))
                continue;
            int prev = n - 1;
            while (prev >= spanLo && isBlackKey(prev))
                --prev;
            const float pw = (prev >= spanLo) ? keyWf[prev] : whiteW;
            const float px = (prev >= spanLo) ? keyX[prev] : keys.getX();
            const int pc = ((n % 12) + 12) % 12;
            float nudge = 0.0f;
            if (pc == 1 || pc == 6)
                nudge = -0.12f;
            else if (pc == 3 || pc == 10)
                nudge = 0.12f;
            const float bw = pw * 0.58f;
            const float boundary = px + pw;
            keyWf[n] = bw;
            keyX[n] = juce::jlimit(keys.getX(), keys.getRight() - bw,
                                   boundary - bw * 0.5f + nudge * pw);
        }

        const double length = juce::jmax(0.001, haveTape ? scoreLength : 1.0);
        const double pos = juce::jlimit(0.0, length, displayPos);
        const float lookAhead = juce::jlimit(3.2f, 6.5f, plot.getHeight() / 88.0f);
        const float zFar = lookAhead;
        const float hitY = plot.getBottom();
        const float farY = plot.getY();
        const float highwayH = juce::jmax(8.0f, hitY - farY);
        const float vanishX = plot.getCentreX();

        {
            juce::Graphics::ScopedSaveState clip(g);
            g.reduceClipRegion(plot.toNearestInt());

            // Fog toward the horizon.
            juce::ColourGradient fog(wellFill.withAlpha(0.0f), 0.0f, hitY,
                                     wellFill.brighter(0.08f).withAlpha(0.55f), 0.0f, farY, false);
            g.setGradientFill(fog);
            g.fillRect(plot);

            // Converging octave lanes.
            for (int n = spanLo; n <= spanLo + span; ++n)
            {
                if ((n % 12) != 0)
                    continue;
                const float xN = noteCenter(n, spanLo, span, keyX, keyWf, keys);
                juce::Path lane;
                const float xF = vanishX + (xN - vanishX) * 0.30f;
                lane.startNewSubPath(xN, hitY);
                lane.lineTo(xF, farY);
                g.setColour(cMut.withAlpha(0.10f));
                g.strokePath(lane, juce::PathStrokeType(1.0f));
            }

            // Time rings ride the playhead toward the keyboard (0.5 s apart).
            const float phase = (float) std::fmod(juce::jmax(0.0, displayPos), 0.5);
            for (int k = 0; k <= 16; ++k)
            {
                const float z = 0.5f * (float) k - phase;
                if (z < 0.02f || z > zFar)
                    continue;
                const float p = perspective(z, zFar);
                const float y = hitY - p * highwayH;
                const float inset = juce::jmap(p, 0.0f, 1.0f, 0.0f, plot.getWidth() * 0.34f);
                g.setColour(cMut.withAlpha(0.10f + 0.08f * (1.0f - p)));
                g.drawLine(plot.getX() + inset, y, plot.getRight() - inset, y, 1.0f);
            }

            if (haveTape)
            {
                struct Vis
                {
                    int idx;
                    float dFar;
                    bool on;
                };
                Vis visArr[320];
                int nVis = 0;
                for (int i = 0; i < (int) score.size(); ++i)
                {
                    const auto& hit = score[(size_t) i];
                    if ((silenced & (1u << (hit.channel - 1))) != 0)
                        continue;
                    if (hit.note < spanLo || hit.note >= spanLo + span)
                        continue;
                    const float z0 = (float) (hit.startSec - pos);
                    const float z1 = (float) (hit.endSec - pos);
                    if (z1 < -0.05f || z0 > zFar * 1.05f)
                        continue;
                    Vis item;
                    item.idx = i;
                    item.dFar = juce::jmax(z0, z1);
                    item.on = (pos >= hit.startSec && pos < hit.endSec);
                    if (nVis < 320)
                    {
                        visArr[nVis] = item;
                        ++nVis;
                        continue;
                    }
                    int far = 0;
                    for (int k = 1; k < nVis; ++k)
                        if (visArr[k].dFar > visArr[far].dFar)
                            far = k;
                    if (item.dFar < visArr[far].dFar)
                        visArr[far] = item;
                }
                // Far to near so nearer notes paint on top.
                for (int a = 0; a < nVis; ++a)
                {
                    int best = a;
                    for (int b = a + 1; b < nVis; ++b)
                        if (visArr[b].dFar > visArr[best].dFar)
                            best = b;
                    const Vis tmp = visArr[a];
                    visArr[a] = visArr[best];
                    visArr[best] = tmp;
                }

                for (int i = 0; i < nVis; ++i)
                {
                    if (! visArr[i].on)
                        drawFalling(g, score[(size_t) visArr[i].idx], pos, zFar, hitY, highwayH,
                                    vanishX, spanLo, span, keyX, keyWf, keys, false);
                }
                for (int i = 0; i < nVis; ++i)
                {
                    if (visArr[i].on)
                        drawFalling(g, score[(size_t) visArr[i].idx], pos, zFar, hitY, highwayH,
                                    vanishX, spanLo, span, keyX, keyWf, keys, true);
                }
            }
            else
            {
                g.setColour(cMut.withAlpha(0.7f));
                g.setFont(juce::FontOptions(15.0f));
                g.drawText("NO TAPE", plot.toNearestInt(), juce::Justification::centred);
            }

            // Hit line.
            g.setColour(cAcc.withAlpha(0.12f));
            g.fillRect(plot.getX(), hitY - 10.0f, plot.getWidth(), 10.0f);
            g.setColour(cAcc);
            g.fillRect(plot.getX(), hitY - 2.0f, plot.getWidth(), 3.0f);
            g.setColour(cAcc2.withAlpha(0.85f));
            g.fillRect(plot.getX(), hitY - 3.0f, plot.getWidth(), 1.5f);

            // Guitar Hero beams from sounding keys.
            if (haveTape)
            {
                for (int n = spanLo; n < spanLo + span; ++n)
                {
                    if (! soundingNow[n])
                        continue;
                    const float cx = noteCenter(n, spanLo, span, keyX, keyWf, keys);
                    const float w = juce::jmax(4.0f, keyWf[n] * 0.9f);
                    auto col = juce::Colour(soundColNow[n]);
                    juce::ColourGradient beam(col.withAlpha(0.55f), cx, hitY,
                                              col.withAlpha(0.0f), cx, hitY - highwayH * 0.55f, false);
                    g.setGradientFill(beam);
                    g.fillRect(cx - w * 0.5f, hitY - highwayH * 0.55f, w, highwayH * 0.55f);
                    g.setColour(col.withAlpha(0.22f));
                    g.fillEllipse(cx - w * 1.1f, hitY - 16.0f, w * 2.2f, 22.0f);
                }
            }

            // Hit flashes.
            const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
            for (int i = 0; i < (int) flashes.size(); ++i)
            {
                const double age = now - flashes[(size_t) i].born;
                if (age < 0.0 || age > 0.38)
                    continue;
                const float t = (float) (age / 0.38);
                const int n = flashes[(size_t) i].note;
                const float cx = noteCenter(n, spanLo, span, keyX, keyWf, keys);
                auto col = juce::Colour(flashes[(size_t) i].argb);
                const float rad = 10.0f + t * 34.0f;
                g.setColour(col.withAlpha((1.0f - t) * 0.45f));
                g.fillEllipse(cx - rad, hitY - rad * 0.55f, rad * 2.0f, rad * 1.1f);
                g.setColour(juce::Colours::white.withAlpha((1.0f - t) * 0.55f));
                g.fillEllipse(cx - 5.0f, hitY - 8.0f, 10.0f, 10.0f);
            }
        }

        if (aeterna.getMode() != 0)
        {
            aeterna.paint(g, pos, spanLo, span, keyX, keyWf, keys, zFar, hitY,
                          highwayH, vanishX, plot);
        }

        drawKeyboardH(g, keys, spanLo, span, keyX, keyWf, wellFill, cMut, cText);
        drawLegend(g, legend, mask, cMut, cText, haveTape);
    }

    void timerCallback() override
    {
        const bool loadedNow = engine != nullptr && engine->isLoaded();
        const bool playingNow = loadedNow && engine->isPlaying();
        if (snapDisplay)
            holdPicture = false;
        if (engine != nullptr && (playingNow || ! holdPicture))
        {
            const int genBefore = seenGen;
            engine->copyScore(seenGen, score, scoreLength, loNote, hiNote, channelMask);
            if (seenGen != genBefore)
                refitKeys();
        }
        if (! loadedNow)
        {
            fitValid = false;
            holdPicture = false;
        }

        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (engine != nullptr && engine->isLoaded())
        {
            const double eng = engine->getPosition();
            const bool playing = engine->isPlaying();
            const double len = juce::jmax(0.001, scoreLength);
            if (snapDisplay)
            {
                displayPos = juce::jlimit(0.0, len, eng);
                pauseHold = displayPos;
                snapDisplay = false;
                aeterna.noteSeekOrStop();
            }
            else if (playing)
            {
                if (! lastPlaying)
                {
                    // A pause that zeroed the engine clock must not blank the roll.
                    if (eng < 0.05 && pauseHold > 0.35)
                        displayPos = pauseHold;
                    else
                        displayPos = eng;
                }
                else if (eng + 0.4 < displayPos)
                {
                    displayPos = eng;
                    aeterna.noteSeekOrStop();
                }
                else
                {
                    displayPos += now - lastWallSec;
                    if (std::abs(displayPos - eng) > 0.18)
                        displayPos = eng;
                }
                displayPos = juce::jlimit(0.0, len, displayPos);
                pauseHold = displayPos;
            }
            else
            {
                // Hold the picture for the whole pause. Following a clock that
                // jumps to 0 hides every note until play.
                if (lastPlaying)
                    pauseHold = displayPos;
                const bool bogusZero = eng < 0.05 && pauseHold > 0.35;
                if (! bogusZero)
                    displayPos = juce::jlimit(0.0, len, eng);
                else
                    displayPos = pauseHold;
            }
            lastPlaying = playing;
            if (! playing)
                holdPicture = true;
            else
                holdPicture = false;
        }
        else
        {
            lastPlaying = false;
            displayPos = 0.0;
        }

        for (int i = 0; i < 128; ++i)
        {
            soundingNow[i] = false;
            soundColNow[i] = 0;
        }
        if (engine != nullptr && engine->isLoaded())
        {
            const double pos = displayPos;
            for (const auto& hit : score)
            {
                if (pos < hit.startSec || pos >= hit.endSec)
                    continue;
                if (hit.note < 0 || hit.note > 127)
                    continue;
                if ((silenced & (1u << (hit.channel - 1))) != 0)
                    continue;
                soundingNow[hit.note] = true;
                soundColNow[hit.note] = colourForChannel(hit.channel).getARGB();
                if (! prevOn[hit.note])
                {
                    HitFlash f;
                    f.note = hit.note;
                    f.argb = soundColNow[hit.note];
                    f.born = now;
                    flashes.push_back(f);
                }
            }
        }
        for (int i = 0; i < 128; ++i)
            prevOn[i] = soundingNow[i];

        int w = 0;
        for (int i = 0; i < (int) flashes.size(); ++i)
        {
            if (now - flashes[(size_t) i].born < 0.40)
            {
                if (w != i)
                    flashes[(size_t) w] = flashes[(size_t) i];
                ++w;
            }
        }
        flashes.resize((size_t) w);

        {
            const double dt = now - lastWallSec;
            const bool loaded = engine != nullptr && engine->isLoaded();
            const bool playing = loaded && engine->isPlaying();
            const bool jumped = loaded && playing && lastPlaying && displayPos + 0.05 < aetPrevPos;
            aeterna.tick(dt, displayPos, aetPrevPos, playing, loaded, jumped, score, silenced);
            aetPrevPos = displayPos;
        }

        lastWallSec = now;
        repaint();
    }

    static juce::Colour colourForChannel(int ch1to16)
    {
        static const juce::uint32 kCol[16] = {
            0xffff3b30, 0xff0a84ff, 0xff30d158, 0xffbf5af2,
            0xffff9f0a, 0xff64d2ff, 0xffffd60a, 0xffff375f,
            0xffac8e68, 0xff7dffb3, 0xff5e5ce6, 0xffff6482,
            0xff00c7be, 0xffd0ff00, 0xffda8fff, 0xff8e8e93
        };
        const int i = juce::jlimit(0, 15, ch1to16 - 1);
        return juce::Colour(kCol[i]);
    }

    bool noteIsDrum(const PlayerNote& n) const
    {
        if (n.channel < 1 || n.channel > 16)
            return false;
        if (n.channel == 10)
            return true;
        return (drumMask & (1u << (n.channel - 1))) != 0;
    }

    // Every pitched note, snapped out to whole octaves (C through B).
    // At least five octaves so a mid-range file still shows more than C3-C6.
    // A note outside the window widens it. Pause does not, while the notes still fit.
    void refitKeys()
    {
        int loN = 128;
        int hiN = -1;
        auto consider = [&](bool drumsToo)
        {
            loN = 128;
            hiN = -1;
            for (const auto& n : score)
            {
                if (n.note < 0 || n.note > 127)
                    continue;
                if (! drumsToo && noteIsDrum(n))
                    continue;
                loN = juce::jmin(loN, n.note);
                hiN = juce::jmax(hiN, n.note);
            }
        };
        consider(false);
        if (hiN < loN)
            consider(true);
        if (hiN < loN)
        {
            fitLo = 36;
            fitHi = 95;
            fitValid = true;
            return;
        }
        int lo = (loN / 12) * 12;
        int hi = (hiN / 12) * 12 + 11;
        if (hi < hiN)
            hi = juce::jmin(127, hiN);
        const int minSpan = 59;
        if (hi - lo < minSpan)
        {
            const int midOct = ((loN + hiN) / 2) / 12;
            lo = midOct * 12 - 24;
            if (lo < 0)
                lo = 0;
            lo = (lo / 12) * 12;
            hi = lo + minSpan;
            if (hi > 127)
            {
                hi = 127;
                lo = (juce::jmax(0, hi - minSpan) / 12) * 12;
            }
            if (lo > loN)
                lo = (loN / 12) * 12;
            if (hi < hiN)
                hi = juce::jmin(127, (hiN / 12) * 12 + 11);
        }
        hi = juce::jmin(127, hi);
        lo = juce::jlimit(0, hi, lo);
        if (fitValid && fitLo <= loN && fitHi >= hiN
            && std::abs(fitLo - lo) <= 12 && std::abs(fitHi - hi) <= 12)
            return;
        fitLo = lo;
        fitHi = hi;
        fitValid = true;
    }

private:
    struct HitFlash
    {
        int note;
        juce::uint32 argb;
        double born;
    };

    static int countBits(std::uint32_t m)
    {
        int n = 0;
        while (m) { n += (int) (m & 1u); m >>= 1; }
        return n;
    }

    static bool isBlackKey(int note)
    {
        const int pc = ((note % 12) + 12) % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    static float perspective(float z, float zFar)
    {
        if (zFar <= 0.001f)
            return 0.0f;
        const float u = juce::jmax(0.0f, z / zFar);
        const float k = 2.6f;
        const float p = 1.0f - 1.0f / (1.0f + k * u);
        const float p1 = 1.0f - 1.0f / (1.0f + k);
        return p / p1;
    }

    static float noteCenter(int note, int spanLo, int span, const float keyX[128],
                            const float keyWf[128], juce::Rectangle<float> keys)
    {
        if (note >= 0 && note < 128 && keyWf[note] > 0.5f)
            return keyX[note] + keyWf[note] * 0.5f;
        const float t = (float) (note - spanLo) / (float) juce::jmax(1, span);
        return keys.getX() + t * keys.getWidth();
    }

    static void drawFalling(juce::Graphics& g, const PlayerNote& hit, double pos, float zFar,
                            float hitY, float highwayH, float vanishX, int spanLo, int span,
                            const float keyX[128], const float keyWf[128],
                            juce::Rectangle<float> keys, bool on)
    {
        float z0 = (float) (hit.startSec - pos);
        float z1 = (float) (hit.endSec - pos);
        if (z1 < 0.0f)
            return;
        if (z0 < 0.0f)
            z0 = 0.0f;
        if (z0 > zFar * 1.08f)
            return;
        if (z1 > zFar * 1.15f)
            z1 = zFar * 1.15f;

        const float p0 = juce::jlimit(0.0f, 1.2f, perspective(z0, zFar));
        const float p1 = juce::jlimit(0.0f, 1.2f, perspective(z1, zFar));
        const float y0 = hitY - p0 * highwayH;
        const float y1 = hitY - p1 * highwayH;
        if (std::abs(y1 - y0) < 1.2f && ! on)
            return;

        const float cxN = noteCenter(hit.note, spanLo, span, keyX, keyWf, keys);
        const float baseW = juce::jmax(5.0f, (hit.note < 128 ? keyWf[hit.note] : 10.0f) * 0.86f);
        const float x0 = vanishX + (cxN - vanishX) * (1.0f - p0 * 0.70f);
        const float x1 = vanishX + (cxN - vanishX) * (1.0f - p1 * 0.70f);
        const float w0 = baseW * (1.0f - p0 * 0.72f);
        const float w1 = baseW * (1.0f - p1 * 0.72f);

        juce::Path body;
        body.startNewSubPath(x0 - w0 * 0.5f, y0);
        body.lineTo(x0 + w0 * 0.5f, y0);
        body.lineTo(x1 + w1 * 0.5f, y1);
        body.lineTo(x1 - w1 * 0.5f, y1);
        body.closeSubPath();

        auto col = colourForChannel(hit.channel);
        if (on)
        {
            g.setColour(col.withAlpha(0.22f));
            juce::Path bloom;
            bloom.startNewSubPath(x0 - w0 * 0.95f, y0);
            bloom.lineTo(x0 + w0 * 0.95f, y0);
            bloom.lineTo(x1 + w1 * 0.95f, y1);
            bloom.lineTo(x1 - w1 * 0.95f, y1);
            bloom.closeSubPath();
            g.fillPath(bloom);
            g.setColour(col.brighter(0.28f));
            g.fillPath(body);
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.fillRect(x0 - w0 * 0.5f, y0 - 2.0f, w0, 3.0f);
        }
        else
        {
            const float fade = juce::jlimit(0.35f, 0.88f, 0.88f - p0 * 0.35f);
            g.setColour(col.withAlpha(fade));
            g.fillPath(body);
            g.setColour(col.brighter(0.15f).withAlpha(fade * 0.7f));
            g.strokePath(body, juce::PathStrokeType(1.0f));
        }
    }

    void drawKeyboardH(juce::Graphics& g, juce::Rectangle<float> keys, int spanLo, int span,
                       const float keyX[128], const float keyWf[128],
                       juce::Colour well, juce::Colour mut, juce::Colour text)
    {
        g.setColour(well.darker(0.15f));
        g.fillRect(keys);
        // White keys first.
        for (int n = spanLo; n < spanLo + span; ++n)
        {
            if (isBlackKey(n) || keyWf[n] < 0.5f)
                continue;
            const bool on = soundingNow[n];
            auto fill = on ? juce::Colour(soundColNow[n]) : well.brighter(0.22f);
            g.setColour(fill);
            g.fillRect(keyX[n] + 0.6f, keys.getY() + 2.0f, keyWf[n] - 1.2f, keys.getHeight() - 4.0f);
            if (on)
            {
                g.setColour(juce::Colours::white.withAlpha(0.55f));
                g.fillRect(keyX[n] + 0.6f, keys.getY() + 2.0f, keyWf[n] - 1.2f, 5.0f);
            }
            g.setColour(mut.withAlpha(0.35f));
            g.drawVerticalLine((int) (keyX[n] + keyWf[n]), keys.getY() + 2.0f, keys.getBottom() - 2.0f);
            if ((n % 12) == 0)
            {
                g.setColour(on ? juce::Colours::white : text.withAlpha(0.75f));
                g.setFont(juce::FontOptions(10.0f));
                g.drawText("C" + juce::String(n / 12 - 1),
                           (int) keyX[n], (int) (keys.getBottom() - 16.0f),
                           (int) keyWf[n], 14, juce::Justification::centred);
            }
        }
        // Black keys on top.
        const float bh = keys.getHeight() * 0.58f;
        for (int n = spanLo; n < spanLo + span; ++n)
        {
            if (! isBlackKey(n) || keyWf[n] < 0.5f)
                continue;
            const bool on = soundingNow[n];
            auto fill = on ? juce::Colour(soundColNow[n]) : well.darker(0.45f);
            g.setColour(fill);
            g.fillRoundedRectangle(keyX[n], keys.getY() + 2.0f, keyWf[n], bh, 2.0f);
            if (on)
            {
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.fillRect(keyX[n] + 1.0f, keys.getY() + 2.0f, keyWf[n] - 2.0f, 4.0f);
            }
        }
        g.setColour(mut.withAlpha(0.5f));
        g.drawHorizontalLine((int) keys.getY(), keys.getX(), keys.getRight());
    }

    void drawLegend(juce::Graphics& g, juce::Rectangle<float> legend,
                    std::uint32_t mask, juce::Colour mut, juce::Colour text, bool haveTape)
    {
        int used = 0;
        int channels[16];
        for (int ch = 1; ch <= 16; ++ch)
            if ((mask & (1u << (ch - 1))) != 0)
                channels[used++] = ch;
        if (used == 0)
        {
            g.setColour(mut);
            g.setFont(juce::FontOptions(12.0f));
            g.drawText("Nothing on the tape yet", legend.toNearestInt(), juce::Justification::centred);
            return;
        }
        const int cols = juce::jmin(8, used);
        const int rows = (used + cols - 1) / cols;
        const float rowH = legend.getHeight() / (float) rows;
        const float slotW = legend.getWidth() / (float) cols;
        for (int i = 0; i < used; ++i)
        {
            const int ch = channels[i];
            const int row = i / cols;
            const int col = i % cols;
            auto slotR = juce::Rectangle<float>(legend.getX() + slotW * (float) col,
                                                legend.getY() + rowH * (float) row,
                                                slotW, rowH);
            auto chip = slotR.removeFromLeft(10.0f).withSizeKeepingCentre(8.0f, 8.0f);
            auto colr = colourForChannel(ch);
            if (haveTape && (silenced & (1u << (ch - 1))) != 0)
                colr = colr.withMultipliedAlpha(0.3f);
            g.setColour(colr);
            g.fillEllipse(chip);
            g.setColour(text.withAlpha(0.9f));
            g.setFont(juce::FontOptions(11.0f));
            juce::String label;
            label += portLetter;
            label += juce::String(ch);
            if (haveTape && ch - 1 < partNames.size())
            {
                const auto n = partNames[ch - 1];
                if (n.isNotEmpty())
                    label += " " + n;
            }
            g.drawText(label, slotR.reduced(4.0f, 0).toNearestInt(), juce::Justification::centredLeft);
        }
    }

    MidiPlayerEngine* engine { nullptr };
    char portLetter { 'A' };
    juce::String banner;
    const SkinPalette* pal { &kSkins[0] };
    juce::StringArray partNames;
    std::vector<PlayerNote> score;
    std::vector<HitFlash> flashes;
    double scoreLength { 0.0 };
    double displayPos { 0.0 };
    double lastWallSec { 0.0 };
    int loNote { 48 }, hiNote { 72 };
    int fitLo { 48 }, fitHi { 95 };
    bool fitValid { false };
    bool holdPicture { false };
    std::uint32_t drumMask { 0 };
    int seenGen { -1 };
    std::uint32_t channelMask { 0 };
    std::uint32_t silenced { 0 };
    bool active { false };
    bool lastPlaying { false };
    bool snapDisplay { false };
    double pauseHold { 0.0 };
    double aetPrevPos { 0.0 };
    bool prevOn[128];
    bool soundingNow[128];
    juce::uint32 soundColNow[128];
    juce::TextButton popBtn, fullBtn, aetBtn;
    AeternaCompanion aeterna;

    void refreshAeternaButton()
    {
        const int m = aeterna.getMode();
        if (m <= 0)
        {
            aetBtn.setButtonText("Aeterna Off");
            aetBtn.setTooltip("Aeterna is off. Click to put her on the notes.");
        }
        else
        {
            aetBtn.setButtonText("Aeterna On");
            aetBtn.setTooltip("Aeterna follows the notes. She hops, dances, and rides the long ones. Click to turn her off.");
        }
        auto cAcc = juce::Colour(pal ? pal->accent : 0xffe8a317);
        auto cSurf = juce::Colour(pal ? pal->surface2 : 0xff242833);
        aetBtn.setColour(juce::TextButton::buttonColourId, m == 0 ? cSurf : cAcc.withAlpha(0.85f));
        aetBtn.setColour(juce::TextButton::textColourOffId,
                         m == 0 ? juce::Colour(pal ? pal->text : 0xffece8df)
                                : juce::Colour(0xff1a1408));
    }
};
