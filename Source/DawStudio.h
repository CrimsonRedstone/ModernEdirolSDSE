#pragma once

// STUDIO tab: Ableton-style 32-lane arrange (1:1 with mixer / SD-80 parts)
// plus an FL-style piano roll + velocity graph. Separate from PLAYER / PLAYLIST.

#include <cmath>
#include <functional>
#include <juce_gui_extra/juce_gui_extra.h>
#include "DawSong.h"
#include "PluginProcessor.h"
#include "Skin.h"

class DawStudio : public juce::Component,
                  private juce::Timer,
                  private juce::Button::Listener,
                  private juce::ComboBox::Listener,
                  private juce::Slider::Listener
{
public:
    explicit DawStudio(ModernEdirolSd80Processor& p) : proc(p)
    {
        setOpaque(false);

        playBtn.setButtonText("PLAY");
        stopBtn.setButtonText("STOP");
        recBtn.setButtonText("REC");
        songBtn.setButtonText("SONG");
        patBtn.setButtonText("PAT");
        newBtn.setButtonText("NEW");
        saveBtn.setButtonText("SAVE");
        loadBtn.setButtonText("LOAD");
        importBtn.setButtonText("IMPORT");
        exportBtn.setButtonText("EXPORT");
        plusPatBtn.setButtonText("+ PAT");
        playBtn.addListener(this);
        stopBtn.addListener(this);
        recBtn.addListener(this);
        songBtn.addListener(this);
        patBtn.addListener(this);
        newBtn.addListener(this);
        saveBtn.addListener(this);
        loadBtn.addListener(this);
        importBtn.addListener(this);
        exportBtn.addListener(this);
        plusPatBtn.addListener(this);
        songBtn.setClickingTogglesState(true);
        patBtn.setClickingTogglesState(true);
        songBtn.setRadioGroupId(71);
        patBtn.setRadioGroupId(71);
        songBtn.setToggleState(true, juce::dontSendNotification);

        bpmSlider.setSliderStyle(juce::Slider::IncDecButtons);
        bpmSlider.setRange(20.0, 300.0, 0.1);
        bpmSlider.setValue(120.0, juce::dontSendNotification);
        bpmSlider.setTextValueSuffix(" BPM");
        bpmSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 72, 18);
        bpmSlider.addListener(this);

        snapBox.addItem("1/32", 1);
        snapBox.addItem("1/16", 2);
        snapBox.addItem("1/8", 3);
        snapBox.addItem("1/4", 4);
        snapBox.addItem("Bar", 5);
        snapBox.setSelectedId(2, juce::dontSendNotification);
        snapBox.addListener(this);

        patBox.addListener(this);
        rebuildPatBox();

        targetL.setJustificationType(juce::Justification::centredLeft);
        hintL.setJustificationType(juce::Justification::centredLeft);
        hintL.setText("R arms a lane. REC records live play onto armed lanes only. Double-click a clip to edit. Drag edges to resize.",
                      juce::dontSendNotification);

        addAndMakeVisible(playBtn);
        addAndMakeVisible(stopBtn);
        addAndMakeVisible(recBtn);
        addAndMakeVisible(songBtn);
        addAndMakeVisible(patBtn);
        addAndMakeVisible(newBtn);
        addAndMakeVisible(saveBtn);
        addAndMakeVisible(loadBtn);
        addAndMakeVisible(importBtn);
        addAndMakeVisible(exportBtn);
        addAndMakeVisible(plusPatBtn);
        addAndMakeVisible(bpmSlider);
        addAndMakeVisible(snapBox);
        addAndMakeVisible(patBox);
        addAndMakeVisible(targetL);
        addAndMakeVisible(hintL);

        arrange.setStudio(this);
        roll.setStudio(this);
        arrangeView.setViewedComponent(&arrange, false);
        arrangeView.setScrollBarsShown(true, true);
        addAndMakeVisible(arrangeView);
        addAndMakeVisible(splitBar);
        addAndMakeVisible(roll);
        splitBar.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        splitBar.setInterceptsMouseClicks(false, false);

        applyPalette(kSkins[0]);
        refreshChrome();
    }

    ~DawStudio() override
    {
        stopTimer();
        arrangeView.setViewedComponent(nullptr, false);
    }

    void setActive(bool v)
    {
        active = v;
        if (v)
            startTimerHz(30);
        else
            stopTimer();
        refreshChrome();
        repaint();
    }

    void applyPalette(const SkinPalette& p)
    {
        pal = p;
        auto cAcc = juce::Colour(p.accent);
        auto cAcc2 = juce::Colour(p.accent2);
        auto cMute = juce::Colour(p.mute);
        auto cText = juce::Colour(p.text);
        auto cBg = juce::Colour(p.bg);
        auto cSurf2 = juce::Colour(p.surface2);
        playBtn.setColour(juce::TextButton::buttonColourId, cAcc2);
        playBtn.setColour(juce::TextButton::textColourOffId, cBg);
        playBtn.setColour(juce::TextButton::buttonOnColourId, cAcc);
        stopBtn.setColour(juce::TextButton::buttonColourId, cMute);
        stopBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        recBtn.setColour(juce::TextButton::buttonColourId, cSurf2);
        recBtn.setColour(juce::TextButton::textColourOffId, cText);
        songBtn.setColour(juce::TextButton::buttonOnColourId, cAcc);
        songBtn.setColour(juce::TextButton::textColourOnId, cBg);
        patBtn.setColour(juce::TextButton::buttonOnColourId, cAcc);
        patBtn.setColour(juce::TextButton::textColourOnId, cBg);
        targetL.setColour(juce::Label::textColourId, cAcc);
        hintL.setColour(juce::Label::textColourId, juce::Colour(p.muted));
        bpmSlider.setColour(juce::Slider::textBoxTextColourId, cText);
        bpmSlider.setColour(juce::Slider::textBoxBackgroundColourId, cSurf2);
        arrange.repaint();
        roll.applyPalette(p);
        roll.repaint();
        repaint();
    }

    void refreshChrome()
    {
        auto& e = proc.daw;
        bpmSlider.setValue(e.getBpm(), juce::dontSendNotification);
        songBtn.setToggleState(e.isSongMode(), juce::dontSendNotification);
        patBtn.setToggleState(! e.isSongMode(), juce::dontSendNotification);
        playBtn.setToggleState(e.isPlaying(), juce::dontSendNotification);
        playBtn.setButtonText(e.isPlaying() ? "PLAYING" : "PLAY");
        const bool recOn = e.isRecording();
        recBtn.setButtonText(recOn ? "REC ON" : "REC");
        recBtn.setColour(juce::TextButton::buttonColourId,
                         recOn ? juce::Colour(0xffe23b3b) : juce::Colour(pal.surface2));
        recBtn.setColour(juce::TextButton::textColourOffId,
                         recOn ? juce::Colours::white : juce::Colour(pal.text));
        recBtn.setTooltip("Record live playing onto lanes marked R");
        rebuildPatBox();
        updateTarget();
        arrange.syncSize();
        arrange.repaint();
        roll.repaint();
    }

    void loadDropped(const juce::File& f)
    {
        if (f.hasFileExtension(".mesd80song"))
        {
            proc.dawLoadSong(f);
            refreshChrome();
            return;
        }
        if (f.hasFileExtension(".mid") || f.hasFileExtension(".midi"))
        {
            proc.dawImportMidi(f, proc.daw.getEditTrack());
            refreshChrome();
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(10, 8);
        auto bar = r.removeFromTop(32);
        playBtn.setBounds(bar.removeFromLeft(72).reduced(2));
        stopBtn.setBounds(bar.removeFromLeft(60).reduced(2));
        recBtn.setBounds(bar.removeFromLeft(68).reduced(2));
        bar.removeFromLeft(6);
        bpmSlider.setBounds(bar.removeFromLeft(150).reduced(2, 4));
        bar.removeFromLeft(6);
        songBtn.setBounds(bar.removeFromLeft(64).reduced(2));
        patBtn.setBounds(bar.removeFromLeft(56).reduced(2));
        bar.removeFromLeft(6);
        snapBox.setBounds(bar.removeFromLeft(78).reduced(2, 4));
        patBox.setBounds(bar.removeFromLeft(90).reduced(2, 4));
        plusPatBtn.setBounds(bar.removeFromLeft(64).reduced(2));
        bar.removeFromLeft(8);
        newBtn.setBounds(bar.removeFromLeft(56).reduced(2));
        saveBtn.setBounds(bar.removeFromLeft(60).reduced(2));
        loadBtn.setBounds(bar.removeFromLeft(60).reduced(2));
        importBtn.setBounds(bar.removeFromLeft(72).reduced(2));
        exportBtn.setBounds(bar.removeFromLeft(76).reduced(2));
        auto trow = r.removeFromTop(22);
        targetL.setBounds(trow.removeFromLeft(juce::jmax(280, trow.getWidth() / 2)));
        hintL.setBounds(trow);
        r.removeFromTop(6);
        const int rollH = juce::jlimit(180, r.getHeight() - 80,
                                       r.getHeight() * rollFrac / 100);
        auto rollR = r.removeFromBottom(rollH);
        splitBar.setBounds(r.removeFromBottom(6));
        arrangeView.setBounds(r);
        arrange.syncSize();
        roll.setBounds(rollR);
    }

    void paint(juce::Graphics& g) override
    {
        auto cSurf = juce::Colour(pal.surface);
        auto cBord = juce::Colour(pal.border);
        auto r = getLocalBounds().toFloat().reduced(4.0f);
        g.setColour(cSurf);
        g.fillRoundedRectangle(r, 8.0f);
        g.setColour(cBord);
        g.drawRoundedRectangle(r, 8.0f, 1.0f);
        g.setColour(juce::Colour(pal.muted));
        g.fillRect(splitBar.getBounds());
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (splitBar.getBounds().contains(e.getPosition()))
            splitDrag = true;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! splitDrag)
            return;
        const int h = juce::jmax(1, getHeight());
        rollFrac = juce::jlimit(28, 70, 100 - (e.y * 100 / h));
        resized();
    }

    void mouseUp(const juce::MouseEvent&) override { splitDrag = false; }

    int snapTicks() const
    {
        switch (snapBox.getSelectedId())
        {
            case 1: return 60;
            case 2: return 120;
            case 3: return 240;
            case 4: return 480;
            case 5: return kDawBar;
            default: return 120;
        }
    }

    static int snapTo(int tick, int grid)
    {
        grid = juce::jmax(1, grid);
        if (tick < 0) tick = 0;
        return ((tick + grid / 2) / grid) * grid;
    }

    static juce::Colour partColour(int part)
    {
        static const juce::uint32 cols[16] = {
            0xffff3b30, 0xff0a84ff, 0xff30d158, 0xffbf5af2,
            0xffff9f0a, 0xff64d2ff, 0xffffd60a, 0xffff375f,
            0xffac8e68, 0xff7dffb3, 0xff5e5ce6, 0xffff6482,
            0xff00c7be, 0xffd0ff00, 0xffda8fff, 0xff8e8e93
        };
        return juce::Colour(cols[part & 15]);
    }

    static juce::Colour patternColour(int pi)
    {
        static const juce::uint32 cols[12] = {
            0xff3b82f6, 0xff22c55e, 0xffeab308, 0xffef4444,
            0xffa855f7, 0xff06b6d4, 0xfff97316, 0xffec4899,
            0xff84cc16, 0xff6366f1, 0xff14b8a6, 0xfff43f5e
        };
        return juce::Colour(cols[((pi % 12) + 12) % 12]);
    }

    static juce::String partTag(int part)
    {
        const char bank = part < 16 ? 'A' : 'B';
        const int n = (part % 16) + 1;
        return juce::String::charToString(bank) + juce::String(n).paddedLeft('0', 2);
    }

    void selectTrack(int t)
    {
        proc.daw.setEditTrack(t);
        proc.setVisibleGroup(t >= 16 ? 1 : 0);
        proc.setSelectedPart(t);
        updateTarget();
        arrange.repaint();
        roll.repaint();
    }

    int focusClip { -1 };
    int focusTrack { -1 };

private:
    void timerCallback() override
    {
        const int g = proc.daw.getGen();
        if (g != seenGen)
        {
            seenGen = g;
            refreshChrome();
        }
        else if (proc.daw.isPlaying())
        {
            arrange.repaint();
            roll.repaint();
        }
        playBtn.setButtonText(proc.daw.isPlaying() ? "PLAYING" : "PLAY");
    }

    void updateTarget()
    {
        const int t = proc.daw.getEditTrack();
        const int pi = proc.daw.getPatternIndex();
        const auto name = proc.getPartPatchName(t);
        targetL.setText("EDITING  " + proc.daw.patternName(pi)
                            + "   on  " + partTag(t) + "  " + name,
                        juce::dontSendNotification);
    }

    void rebuildPatBox()
    {
        const int cur = proc.daw.getPatternIndex();
        patBox.clear(juce::dontSendNotification);
        const int n = proc.daw.numPatterns();
        for (int i = 0; i < n; ++i)
            patBox.addItem(proc.daw.patternName(i), i + 1);
        if (n > 0)
            patBox.setSelectedId(cur + 1, juce::dontSendNotification);
    }

    bool paramOn(int part, const char* key) const
    {
        if (auto* v = proc.apvts.getRawParameterValue(ModernEdirolSd80Processor::pid(part, key)))
            return v->load() > 0.5f;
        return false;
    }

    void toggleMix(int part, const char* key)
    {
        const bool on = paramOn(part, key);
        proc.setParamInt(ModernEdirolSd80Processor::pid(part, key), on ? 0 : 1);
        arrange.repaint();
    }

    void buttonClicked(juce::Button* b) override
    {
        if (b == &playBtn)
        {
            if (proc.daw.isPlaying())
                proc.dawPause();
            else
                proc.dawPlay();
            refreshChrome();
            return;
        }
        if (b == &stopBtn)
        {
            proc.dawStop();
            refreshChrome();
            return;
        }
        if (b == &recBtn)
        {
            proc.daw.setRecording(! proc.daw.isRecording());
            refreshChrome();
            return;
        }
        if (b == &songBtn)
        {
            proc.daw.setSongMode(true);
            refreshChrome();
            return;
        }
        if (b == &patBtn)
        {
            proc.daw.setSongMode(false);
            refreshChrome();
            return;
        }
        if (b == &plusPatBtn)
        {
            proc.daw.addPattern();
            refreshChrome();
            return;
        }
        if (b == &newBtn)
        {
            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withTitle("New song")
                    .withMessage("Clear the STUDIO song? Unsaved clips are lost.")
                    .withButton("Clear")
                    .withButton("Cancel")
                    .withAssociatedComponent(this),
                [this](int result)
                {
                    if (result == 1)
                    {
                        proc.daw.resetSong();
                        focusClip = -1;
                        refreshChrome();
                    }
                });
            return;
        }
        if (b == &saveBtn)
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Save STUDIO song",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                "*.mesd80song");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this, chooser](const juce::FileChooser& fc)
                                 {
                                     auto f = fc.getResult();
                                     if (f == juce::File()) return;
                                     if (! f.hasFileExtension(".mesd80song"))
                                         f = f.withFileExtension(".mesd80song");
                                     proc.dawSaveSong(f);
                                     refreshChrome();
                                 });
            return;
        }
        if (b == &loadBtn)
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Load STUDIO song",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                "*.mesd80song");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this, chooser](const juce::FileChooser& fc)
                                 {
                                     auto f = fc.getResult();
                                     if (f != juce::File())
                                     {
                                         proc.dawLoadSong(f);
                                         refreshChrome();
                                     }
                                 });
            return;
        }
        if (b == &importBtn)
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Import MIDI (one pattern per channel, mixer patches + CCs)",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                "*.mid;*.midi");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this, chooser](const juce::FileChooser& fc)
                                 {
                                     auto f = fc.getResult();
                                     if (f != juce::File())
                                     {
                                         proc.dawImportMidi(f, proc.daw.getEditTrack());
                                         refreshChrome();
                                     }
                                 });
            return;
        }
        if (b == &exportBtn)
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Export Part A MIDI (Part B writes -B.mid if used)",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                "*.mid");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this, chooser](const juce::FileChooser& fc)
                                 {
                                     auto f = fc.getResult();
                                     if (f == juce::File()) return;
                                     if (! f.hasFileExtension(".mid"))
                                         f = f.withFileExtension(".mid");
                                     proc.dawExportMidi(f);
                                 });
            return;
        }
    }

    void comboBoxChanged(juce::ComboBox* box) override
    {
        if (box == &patBox)
        {
            const int id = patBox.getSelectedId();
            if (id > 0)
                proc.daw.setPatternIndex(id - 1);
            updateTarget();
            roll.repaint();
            arrange.repaint();
        }
    }

    void sliderValueChanged(juce::Slider* s) override
    {
        if (s == &bpmSlider)
            proc.daw.setBpm(bpmSlider.getValue());
    }

    static void drawAutoCurve(juce::Graphics& g, const DawAutoLane& lane,
                              int headerW, int y, int subH, int width, double pxTick,
                              juce::Colour col)
    {
        if (lane.pts.empty())
            return;
        auto tickX = [headerW, pxTick](int tick) -> float
        {
            return (float) headerW + (float) ((double) tick * pxTick);
        };
        auto valY = [y, subH](float v) -> float
        {
            return (float) y + 4.0f + (1.0f - v) * (float) (subH - 8);
        };
        juce::Path path;
        const int t0 = lane.pts.front().tick;
        const int t1 = lane.pts.back().tick;
        const int span = juce::jmax(1, t1 - t0);
        const int steps = juce::jlimit(8, 400, span / 8);
        bool first = true;
        for (int s = 0; s <= steps; ++s)
        {
            const int tick = t0 + s * span / steps;
            const float v = DawEngine::laneAt(lane, tick);
            const float px = tickX(tick);
            const float py = valY(v);
            if (first) { path.startNewSubPath(px, py); first = false; }
            else path.lineTo(px, py);
        }
        juce::Path fill = path;
        fill.lineTo(tickX(t1), (float) (y + subH - 2));
        fill.lineTo(tickX(t0), (float) (y + subH - 2));
        fill.closeSubPath();
        g.setColour(col.withAlpha(0.18f));
        g.fillPath(fill);
        g.setColour(col);
        g.strokePath(path, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        for (const auto& pt : lane.pts)
        {
            const float px = tickX(pt.tick);
            const float py = valY(pt.value);
            g.setColour(col);
            g.fillEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f);
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.drawEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f, 1.0f);
        }
    }

    // ---- arrange ----
    class Arrange : public juce::Component
    {
    public:
        void setStudio(DawStudio* s) { st = s; }

        void syncSize()
        {
            if (st == nullptr) return;
            auto song = st->proc.daw.copySong();
            int h = rulerH + masterH;
            if (song.globalOpen)
                h += kDawGlobals * subH;
            for (int t = 0; t < kDawTracks; ++t)
            {
                h += rowH;
                for (int L = 0; L < kDawLanes; ++L)
                    if (song.tracks[t].lanes[L].open)
                        h += subH;
            }
            const int w = juce::jmax(st->getWidth(), headerW + 40
                + (int) std::llround(st->proc.daw.songLength() * pxTick) + 200);
            setSize(w, juce::jmax(h + 8, 200));
        }

        void paint(juce::Graphics& g) override
        {
            if (st == nullptr) return;
            auto& pal = st->pal;
            auto cBg = juce::Colour(pal.bg);
            auto cSurf = juce::Colour(pal.surface);
            auto cSurf2 = juce::Colour(pal.surface2);
            auto cBord = juce::Colour(pal.border);
            auto cText = juce::Colour(pal.text);
            auto cMut = juce::Colour(pal.muted);
            auto cAcc = juce::Colour(pal.accent);
            auto cAcc2 = juce::Colour(pal.accent2);
            auto cMute = juce::Colour(pal.mute);
            auto cSolo = juce::Colour(pal.solo);
            g.fillAll(cBg);

            const auto song = st->proc.daw.copySong();
            const int edit = st->proc.daw.getEditTrack();
            const int pi = st->proc.daw.getPatternIndex();
            const int len = st->proc.daw.songLength();
            const double pos = st->proc.daw.getPosTick();
            const int grid = st->snapTicks();

            g.setColour(cSurf);
            g.fillRect(0, 0, getWidth(), rulerH);
            g.setColour(cBg.darker(0.08f));
            g.fillRect(0, rulerH, getWidth(), masterH);
            g.setColour(cSurf2);
            g.fillRect(0, rulerH, headerW, masterH);
            g.setColour(cMut);
            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.drawText("BARS", 8, 0, headerW - 16, rulerH, juce::Justification::centredLeft);
            g.setColour(cAcc2);
            drawArrow(g, 10.0f, (float) rulerH + 6.0f, 10.0f, song.globalOpen);
            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.drawText("GLOBAL", 26, rulerH, 72, masterH, juce::Justification::centredLeft);
            g.setColour(cMut);
            g.setFont(juce::FontOptions(11.0f));
            g.drawText("Tempo / Vol / FX", 100, rulerH, headerW - 108, masterH,
                       juce::Justification::centredLeft);
            int lastNumX = -100;
            for (int t = 0; t <= len + kDawBar; t += kDawBar)
            {
                const int x = tickX(t);
                g.setColour(cBord);
                g.drawVerticalLine(x, 0.0f, (float) getHeight());
                if (x >= headerW && x - lastNumX >= 22)
                {
                    g.setColour(cMut);
                    g.setFont(juce::FontOptions(11.0f));
                    g.drawText(juce::String(t / kDawBar + 1), x + 4, 0, 32, rulerH,
                               juce::Justification::centredLeft);
                    lastNumX = x;
                }
            }
            for (int t = 0; t <= len; t += grid)
            {
                const int x = tickX(t);
                g.setColour(cBord.withAlpha(0.35f));
                g.drawVerticalLine(x, (float) (rulerH + masterH), (float) getHeight());
            }

            int y = rulerH + masterH;
            if (song.globalOpen)
            {
                for (int L = 0; L < kDawGlobals; ++L)
                {
                    g.setColour(cBg.darker(0.2f));
                    g.fillRect(0, y, getWidth(), subH);
                    g.setColour(cAcc);
                    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
                    g.drawText(juce::String(kDawGlobalName[L]), 8, y, headerW - 12, subH,
                               juce::Justification::centredLeft);
                    drawAutoCurve(g, song.globals[L], headerW, y, subH, getWidth(), pxTick, cAcc);
                    g.setColour(cBord.withAlpha(0.5f));
                    g.drawHorizontalLine(y + subH - 1, 0.0f, (float) getWidth());
                    y += subH;
                }
            }

            for (int part = 0; part < kDawTracks; ++part)
            {
                const bool sel = part == edit;
                auto row = juce::Rectangle<int>(0, y, getWidth(), rowH);
                g.setColour(sel ? cSurf2.brighter(0.08f) : ((part % 2) ? cSurf : cBg));
                g.fillRect(row);
                auto head = juce::Rectangle<int>(0, y, headerW, rowH);
                g.setColour(sel ? DawStudio::partColour(part).withAlpha(0.28f) : cSurf2);
                g.fillRect(head);
                g.setColour(cBord);
                g.drawRect(head);

                auto fold = juce::Rectangle<int>(2, y + 4, 18, rowH - 8);
                bool anyLane = false;
                for (int L = 0; L < kDawLanes; ++L)
                    if (song.tracks[part].lanes[L].open)
                        anyLane = true;
                g.setColour(cAcc2);
                drawArrow(g, (float) fold.getX() + 4.0f, (float) fold.getY() + 3.0f, 10.0f, anyLane);

                const bool muted = st->paramOn(part, "mute");
                const bool soloed = st->paramOn(part, "solo");
                const bool armed = st->proc.daw.isRecArmed(part);
                auto mR = juce::Rectangle<int>(22, y + 3, 18, rowH - 6);
                auto sR = juce::Rectangle<int>(42, y + 3, 18, rowH - 6);
                auto rR = juce::Rectangle<int>(62, y + 3, 18, rowH - 6);
                g.setColour(muted ? cMute : cSurf);
                g.fillRoundedRectangle(mR.toFloat(), 3.0f);
                g.setColour(muted ? juce::Colours::white : cMut);
                g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
                g.drawText("M", mR, juce::Justification::centred, false);
                g.setColour(soloed ? cSolo : cSurf);
                g.fillRoundedRectangle(sR.toFloat(), 3.0f);
                g.setColour(soloed ? juce::Colour(pal.bg) : cMut);
                g.drawText("S", sR, juce::Justification::centred, false);
                g.setColour(armed ? juce::Colour(0xffe23b3b) : cSurf);
                g.fillRoundedRectangle(rR.toFloat(), 3.0f);
                g.setColour(armed ? juce::Colours::white : cMut);
                g.drawText("R", rR, juce::Justification::centred, false);

                g.setColour(DawStudio::partColour(part));
                g.fillRect(84, y + 4, 4, rowH - 8);
                g.setColour(cText);
                g.setFont(juce::FontOptions(12.0f));
                const auto patch = st->proc.getPartPatchName(part);
                g.drawText(juce::String(part + 1).paddedLeft('0', 2) + "  "
                               + DawStudio::partTag(part) + "  " + patch,
                           juce::Rectangle<int>(92, y, headerW - 96, rowH),
                           juce::Justification::centredLeft, true);

                const auto& tr = song.tracks[part];
                for (int ci = 0; ci < (int) tr.clips.size(); ++ci)
                {
                    const auto& c = tr.clips[(size_t) ci];
                    int plen = kDawBar;
                    juce::String pname = "P";
                    if (c.pattern >= 0 && c.pattern < (int) song.patterns.size())
                    {
                        plen = song.patterns[(size_t) c.pattern].length;
                        pname = song.patterns[(size_t) c.pattern].name;
                    }
                    const int span = DawEngine::clipSpan(c, song.patterns[
                        (size_t) juce::jlimit(0, (int) song.patterns.size() - 1, c.pattern)]);
                    auto clipR = juce::Rectangle<int>(tickX(c.start), y + 3,
                                                      juce::jmax(12, (int) std::llround(span * pxTick)),
                                                      rowH - 6);
                    auto col = DawStudio::patternColour(c.pattern);
                    juce::ColourGradient grad(col.brighter(0.15f), (float) clipR.getX(), (float) clipR.getY(),
                                              col.darker(0.25f), (float) clipR.getRight(), (float) clipR.getY(), false);
                    g.setGradientFill(grad);
                    g.fillRoundedRectangle(clipR.toFloat(), 4.0f);
                    const bool editing = (part == st->focusTrack && ci == st->focusClip)
                                         || (part == edit && c.pattern == pi);
                    g.setColour(editing ? juce::Colours::white : col.brighter(0.35f));
                    g.drawRoundedRectangle(clipR.toFloat(), 4.0f, editing ? 2.0f : 1.0f);
                    g.setColour(juce::Colours::black.withAlpha(0.8f));
                    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
                    g.drawText(pname, clipR.reduced(6, 0), juce::Justification::centredLeft, true);
                    if (editing)
                    {
                        g.setColour(juce::Colours::white);
                        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
                        g.drawText("EDIT", clipR.reduced(6, 0), juce::Justification::centredRight, false);
                    }
                    // loop stripes if the clip is longer than the pattern
                    if (span > plen && plen > 0)
                    {
                        g.setColour(juce::Colours::white.withAlpha(0.18f));
                        for (int loop = plen; loop < span; loop += plen)
                            g.drawVerticalLine(tickX(c.start + loop), (float) clipR.getY(),
                                               (float) clipR.getBottom());
                    }
                }

                y += rowH;
                for (int L = 0; L < kDawLanes; ++L)
                {
                    if (! tr.lanes[L].open)
                        continue;
                    g.setColour(cBg.darker(0.15f));
                    g.fillRect(0, y, getWidth(), subH);
                    g.setColour(cAcc);
                    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
                    g.drawText(juce::String(kDawLaneName[L]) + "  CC" + juce::String(tr.lanes[L].cc),
                               6, y, headerW - 10, subH, juce::Justification::centredLeft);
                    drawAutoCurve(g, tr.lanes[L], headerW, y, subH, getWidth(), pxTick, cAcc);
                    g.setColour(cBord.withAlpha(0.5f));
                    g.drawHorizontalLine(y + subH - 1, 0.0f, (float) getWidth());
                    y += subH;
                }
                g.setColour(cBord.withAlpha(0.4f));
                g.drawHorizontalLine(y - 1, 0.0f, (float) getWidth());
            }

            const int px = tickX((int) pos);
            g.setColour(cAcc);
            g.drawVerticalLine(px, 0.0f, (float) getHeight());
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (st == nullptr) return;
            dragKind = 0;
            dragTrack = -1;
            dragIndex = -1;
            dragLane = -1;
            if (e.mods.isMiddleButtonDown())
            {
                dragKind = 5;
                panMouse = e.getScreenPosition();
                if (auto* vp = findParentComponentOfClass<juce::Viewport>())
                    panView = vp->getViewPosition();
                return;
            }
            Hit h = hitTest(e.getPosition());
            if (h.ruler)
            {
                if (e.x >= headerW)
                {
                    dragKind = 6;
                    st->proc.daw.seekTick((double) xToTick(e.x));
                    repaint();
                }
                return;
            }
            if (h.globalHead)
            {
                st->proc.daw.setGlobalOpen(! st->proc.daw.isGlobalOpen());
                syncSize();
                if (st) st->resized();
                return;
            }
            if (h.part < 0 && h.globalLane < 0)
                return;
            if (h.mute)
            {
                st->toggleMix(h.part, "mute");
                return;
            }
            if (h.solo)
            {
                st->toggleMix(h.part, "solo");
                return;
            }
            if (h.rec)
            {
                st->proc.daw.toggleRecArm(h.part);
                repaint();
                return;
            }
            if (h.part >= 0)
                st->selectTrack(h.part);
            if (h.fold)
            {
                if (e.mods.isPopupMenu())
                    showLaneMenu(h.part);
                else
                    toggleTrackLanes(h.part);
                return;
            }
            if (h.globalLane >= 0 || h.subLane >= 0)
            {
                const bool glob = h.globalLane >= 0;
                const int track = glob ? -1 : h.part;
                const int lane = glob ? h.globalLane : h.subLane;
                if (e.x < headerW)
                {
                    if (! glob && e.mods.isPopupMenu())
                        showLaneMenu(h.part);
                    return;
                }
                const auto song = st->proc.daw.copySong();
                const DawAutoLane& ln = glob ? song.globals[lane] : song.tracks[h.part].lanes[lane];
                const int pt = hitPoint(ln, e.x, e.y, h.subY);
                if (e.mods.isPopupMenu())
                {
                    if (pt >= 0)
                        st->proc.daw.removeAutoPt(track, lane, pt);
                    repaint();
                    return;
                }
                if (pt >= 0)
                {
                    dragKind = 4;
                    dragTrack = track;
                    dragLane = lane;
                    dragIndex = pt;
                    return;
                }
                const int tick = DawStudio::snapTo(xToTick(e.x), st->snapTicks());
                const float v = juce::jlimit(0.0f, 1.0f, 1.0f - (float) (e.y - h.subY - 4) / (float) (subH - 8));
                dragIndex = st->proc.daw.addAutoPt(track, lane, tick, v);
                dragKind = 4;
                dragTrack = track;
                dragLane = lane;
                repaint();
                return;
            }
            if (h.clip >= 0)
            {
                auto tr = st->proc.daw.copyTrack(h.part);
                const auto& c = tr.clips[(size_t) h.clip];
                if (e.getNumberOfClicks() >= 2)
                {
                    st->proc.daw.setPatternIndex(c.pattern);
                    st->focusClip = h.clip;
                    st->focusTrack = h.part;
                    st->refreshChrome();
                    return;
                }
                if (e.mods.isPopupMenu())
                {
                    st->proc.daw.removeClip(h.part, h.clip);
                    syncSize();
                    repaint();
                    return;
                }
                dragTrack = h.part;
                dragIndex = h.clip;
                dragOffset = xToTick(e.x) - c.start;
                dragOrigStart = c.start;
                dragOrigLen = c.length > 0 ? c.length
                    : st->proc.daw.patternLength(c.pattern);
                if (h.edge < 0)
                    dragKind = 3;
                else if (h.edge > 0)
                    dragKind = 2;
                else
                    dragKind = 1;
                return;
            }
            if (h.part >= 0 && e.x >= headerW && ! e.mods.isPopupMenu())
            {
                const int tick = DawStudio::snapTo(xToTick(e.x), kDawBar);
                st->proc.daw.addClip(h.part, st->proc.daw.getPatternIndex(), tick);
                syncSize();
                repaint();
            }
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (st == nullptr) return;
            if (dragKind == 1 && dragTrack >= 0 && dragIndex >= 0)
            {
                int start = DawStudio::snapTo(xToTick(e.x) - dragOffset, st->snapTicks());
                st->proc.daw.moveClip(dragTrack, dragIndex, start);
                repaint();
            }
            else if ((dragKind == 2 || dragKind == 3) && dragTrack >= 0 && dragIndex >= 0)
            {
                const int tick = DawStudio::snapTo(xToTick(e.x), st->snapTicks());
                if (dragKind == 2)
                {
                    const int len = juce::jmax(st->snapTicks(), tick - dragOrigStart);
                    st->proc.daw.resizeClip(dragTrack, dragIndex, dragOrigStart, len);
                }
                else
                {
                    int start = juce::jmax(0, tick);
                    const int end = dragOrigStart + dragOrigLen;
                    int len = juce::jmax(st->snapTicks(), end - start);
                    start = end - len;
                    st->proc.daw.resizeClip(dragTrack, dragIndex, start, len);
                }
                syncSize();
                repaint();
            }
            else if (dragKind == 4 && dragLane >= 0 && dragIndex >= 0)
            {
                const int tick = DawStudio::snapTo(xToTick(e.x), st->snapTicks());
                Hit h = hitTest(e.getPosition());
                int y0 = h.subY;
                if (y0 <= 0)
                    y0 = e.y - subH / 2;
                const float v = juce::jlimit(0.0f, 1.0f, 1.0f - (float) (e.y - y0 - 4) / (float) (subH - 8));
                dragIndex = st->proc.daw.setAutoPt(dragTrack, dragLane, dragIndex, tick, v);
                repaint();
            }
            else if (dragKind == 5)
            {
                if (auto* vp = findParentComponentOfClass<juce::Viewport>())
                {
                    const auto d = e.getScreenPosition() - panMouse;
                    vp->setViewPosition(juce::jmax(0, panView.x - d.x), juce::jmax(0, panView.y - d.y));
                }
            }
            else if (dragKind == 6)
            {
                st->proc.daw.seekTick((double) xToTick(e.x));
                repaint();
            }
        }

        void mouseUp(const juce::MouseEvent&) override { dragKind = 0; }

        void mouseMove(const juce::MouseEvent& e) override
        {
            Hit h = hitTest(e.getPosition());
            if (h.clip >= 0 && h.edge != 0)
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            else if (h.clip >= 0)
                setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            else
                setMouseCursor(juce::MouseCursor::NormalCursor);
        }

        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (e.mods.isCtrlDown() || e.mods.isCommandDown())
            {
                pxTick = juce::jlimit(0.012, 0.18, pxTick * (w.deltaY > 0 ? 1.12 : 0.9));
                syncSize();
                if (st) st->resized();
                repaint();
                return;
            }
            if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            {
                const bool horiz = e.mods.isShiftDown()
                    || std::abs(w.deltaX) > std::abs(w.deltaY)
                    || e.y < rulerH;
                const float raw = horiz
                    ? (std::abs(w.deltaX) > 0.01f ? w.deltaX : w.deltaY)
                    : w.deltaY;
                auto pos = vp->getViewPosition();
                const int step = (int) std::llround((double) raw * 220.0);
                if (horiz)
                    pos.x = juce::jmax(0, pos.x - step);
                else
                    pos.y = juce::jmax(0, pos.y - step);
                vp->setViewPosition(pos);
            }
        }

        double pxTick { 0.045 };

    private:
        struct Hit
        {
            int part { -1 };
            int clip { -1 };
            int edge { 0 }; // -1 left, +1 right
            bool fold { false };
            bool mute { false };
            bool solo { false };
            bool rec { false };
            bool globalHead { false };
            bool ruler { false };
            int subLane { -1 };
            int globalLane { -1 };
            int subY { 0 };
        };

        int tickX(int tick) const
        {
            return headerW + (int) std::llround((double) tick * pxTick);
        }

        int xToTick(int x) const
        {
            return juce::jmax(0, (int) std::llround((double) (x - headerW) / pxTick));
        }

        int hitPoint(const DawAutoLane& lane, int x, int y, int y0) const
        {
            for (int i = 0; i < (int) lane.pts.size(); ++i)
            {
                const int px = tickX(lane.pts[(size_t) i].tick);
                const int py = y0 + 4 + (int) std::llround((1.0f - lane.pts[(size_t) i].value) * (float) (subH - 8));
                const int dx = px - x;
                const int dy = py - y;
                if (dx * dx + dy * dy <= 64)
                    return i;
            }
            return -1;
        }

        void showLaneMenu(int part)
        {
            juce::PopupMenu m;
            auto tr = st->proc.daw.copyTrack(part);
            for (int L = 0; L < kDawLanes; ++L)
                m.addItem(L + 1, juce::String(kDawLaneName[L]) + "  CC" + juce::String(kDawLaneCc[L]),
                          true, tr.lanes[L].open);
            m.addSeparator();
            m.addItem(20, "Close all");
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                            [this, part](int r)
                            {
                                if (r <= 0)
                                    return;
                                if (r == 20)
                                    st->proc.daw.foldAllLanes(part, false);
                                else
                                    st->proc.daw.toggleLane(part, r - 1);
                                syncSize();
                                if (st) st->resized();
                                repaint();
                            });
        }

        Hit hitTest(juce::Point<int> p)
        {
            Hit h;
            if (st == nullptr)
                return h;
            auto song = st->proc.daw.copySong();
            if (p.y < rulerH)
            {
                h.ruler = true;
                return h;
            }
            if (p.y < rulerH + masterH)
            {
                if (p.x < headerW)
                    h.globalHead = true;
                return h;
            }
            int y = rulerH + masterH;
            if (song.globalOpen)
            {
                for (int L = 0; L < kDawGlobals; ++L)
                {
                    if (p.y >= y && p.y < y + subH)
                    {
                        h.globalLane = L;
                        h.subY = y;
                        return h;
                    }
                    y += subH;
                }
            }
            for (int part = 0; part < kDawTracks; ++part)
            {
                int hh = rowH;
                for (int L = 0; L < kDawLanes; ++L)
                    if (song.tracks[part].lanes[L].open)
                        hh += subH;
                if (p.y >= y && p.y < y + hh)
                {
                    h.part = part;
                    if (p.y < y + rowH)
                    {
                        if (p.x < 22)
                            h.fold = true;
                        else if (p.x < 42)
                            h.mute = true;
                        else if (p.x < 62)
                            h.solo = true;
                        else if (p.x < 82)
                            h.rec = true;
                    }
                    if (p.y >= y && p.y < y + rowH && p.x >= headerW)
                    {
                        const int tick = xToTick(p.x);
                        const auto& clips = song.tracks[part].clips;
                        for (int i = (int) clips.size() - 1; i >= 0; --i)
                        {
                            int plen = kDawBar;
                            const int cpi = clips[(size_t) i].pattern;
                            if (cpi >= 0 && cpi < (int) song.patterns.size())
                                plen = DawEngine::clipSpan(clips[(size_t) i],
                                                           song.patterns[(size_t) cpi]);
                            const int cs = clips[(size_t) i].start;
                            if (tick >= cs && tick < cs + plen)
                            {
                                h.clip = i;
                                const int x0 = tickX(cs);
                                const int x1 = tickX(cs + plen);
                                if (p.x - x0 <= 8)
                                    h.edge = -1;
                                else if (x1 - p.x <= 8)
                                    h.edge = 1;
                                break;
                            }
                        }
                    }
                    int sy = y + rowH;
                    for (int L = 0; L < kDawLanes; ++L)
                    {
                        if (! song.tracks[part].lanes[L].open)
                            continue;
                        if (p.y >= sy && p.y < sy + subH)
                        {
                            h.subLane = L;
                            h.subY = sy;
                        }
                        sy += subH;
                    }
                    return h;
                }
                y += hh;
            }
            return h;
        }

        void toggleTrackLanes(int part)
        {
            auto tr = st->proc.daw.copyTrack(part);
            bool any = false;
            for (int L = 0; L < kDawLanes; ++L)
                if (tr.lanes[L].open)
                    any = true;
            if (any)
            {
                st->proc.daw.foldAllLanes(part, false);
            }
            else
            {
                bool opened = false;
                for (int L = 0; L < kDawLanes; ++L)
                {
                    if (tr.lanes[L].pts.empty())
                        continue;
                    st->proc.daw.setLaneOpen(part, L, true);
                    opened = true;
                }
                if (! opened)
                    st->proc.daw.setLaneOpen(part, 2, true);
            }
            syncSize();
            if (st) st->resized();
            repaint();
        }

        static void drawArrow(juce::Graphics& g, float x, float y, float s, bool open)
        {
            juce::Path p;
            if (open)
                p.addTriangle(x, y + s, x + s, y + s, x + s * 0.5f, y);
            else
                p.addTriangle(x, y, x + s, y, x + s * 0.5f, y + s);
            g.fillPath(p);
        }

        DawStudio* st { nullptr };
        int dragKind { 0 };
        int dragTrack { -1 };
        int dragIndex { -1 };
        int dragLane { -1 };
        int dragOffset { 0 };
        int dragOrigStart { 0 };
        int dragOrigLen { 0 };
        juce::Point<int> panMouse;
        juce::Point<int> panView;
        static const int headerW = 260;
        static const int rowH = 24;
        static const int subH = 36;
        static const int rulerH = 20;
        static const int masterH = 22;
    };

    // ---- piano roll ----
    class Roll : public juce::Component,
                 private juce::ScrollBar::Listener
    {
    public:
        Roll()
        {
            zOut.setButtonText("-");
            zIn.setButtonText("+");
            zOut.setConnectedEdges(juce::Button::ConnectedOnRight);
            zIn.setConnectedEdges(juce::Button::ConnectedOnLeft);
            zOut.onClick = [this] { zoom(0.85); };
            zIn.onClick = [this] { zoom(1.18); };
            addAndMakeVisible(zOut);
            addAndMakeVisible(zIn);
            hbar.addListener(this);
            addAndMakeVisible(hbar);
        }

        ~Roll() override { hbar.removeListener(this); }

        void setStudio(DawStudio* s) { st = s; }

        void applyPalette(const SkinPalette& p)
        {
            hbar.setColour(juce::ScrollBar::thumbColourId, juce::Colour(p.accent));
            hbar.setColour(juce::ScrollBar::trackColourId, juce::Colour(p.surface2));
        }

        void resized() override
        {
            auto top = getLocalBounds().removeFromTop(titleH).removeFromRight(72).reduced(2, 1);
            zIn.setBounds(top.removeFromRight(32));
            zOut.setBounds(top.removeFromRight(32));
            hbar.setBounds(getLocalBounds().removeFromBottom(scrollH).withTrimmedLeft(keyW));
            syncBar();
        }

        void zoom(double f)
        {
            pxTick = juce::jlimit(0.02, 0.55, pxTick * f);
            syncBar();
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            if (st == nullptr) return;
            auto& pal = st->pal;
            auto cBg = juce::Colour(pal.bg);
            auto cSurf = juce::Colour(pal.surface);
            auto cSurf2 = juce::Colour(pal.surface2);
            auto cBord = juce::Colour(pal.border);
            auto cText = juce::Colour(pal.text);
            auto cMut = juce::Colour(pal.muted);
            auto cAcc = juce::Colour(pal.accent);
            g.fillAll(cBg);

            const int part = st->proc.daw.getEditTrack();
            const int pi = st->proc.daw.getPatternIndex();
            const auto pat = st->proc.daw.copyPattern(pi);
            const auto col = DawStudio::patternColour(pi);
            const int grid = st->snapTicks();
            const int plen = juce::jmax(120, pat.length);
            const auto gridR = gridBounds();

            g.setColour(cSurf2);
            g.fillRect(0, 0, getWidth(), titleH);
            g.setColour(cAcc);
            g.fillRect(0, titleH - 2, getWidth(), 2);
            g.setColour(cSurf);
            g.fillRect(0, titleH, getWidth(), rulerH);
            g.setColour(cText);
            g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
            juce::String title = "EDITING  " + pat.name + "  on  " + DawStudio::partTag(part);
            const auto patchName = st->proc.getPartPatchName(part);
            if (title.length() + patchName.length() < 48)
                title += "  " + patchName;
            g.drawText(title, 8, 0, getWidth() - 90, titleH, juce::Justification::centredLeft, true);
            g.setColour(cMut);
            g.setFont(juce::FontOptions(11.0f));
            g.drawText("BARS", 6, titleH, keyW - 8, rulerH, juce::Justification::centredLeft);

            for (int p = 0; p < 128; ++p)
            {
                const int y = pitchY(p);
                if (y + keyH <= headH || y >= velTop())
                    continue;
                const bool black = isBlack(p);
                g.setColour(black ? cBg.brighter(0.08f) : cSurf);
                g.fillRect(keyW, y, getWidth() - keyW, keyH);
                if ((p % 12) == 0)
                {
                    g.setColour(cBord.withAlpha(0.55f));
                    g.drawHorizontalLine(y + keyH - 1, (float) keyW, (float) getWidth());
                }
                auto key = juce::Rectangle<int>(0, y, keyW, keyH);
                g.setColour(black ? juce::Colour(0xff1a1a1a) : juce::Colour(0xfff3efe6));
                g.fillRect(key);
                g.setColour(cBord);
                g.drawRect(key);
                if ((p % 12) == 0 && keyH >= 11)
                {
                    g.setColour(black ? juce::Colours::white : juce::Colours::black);
                    g.setFont(juce::FontOptions(10.0f));
                    g.drawText("C" + juce::String(p / 12 - 1), key.reduced(3, 0),
                               juce::Justification::centredLeft, false);
                }
            }

            g.saveState();
            g.reduceClipRegion(keyW, headH, juce::jmax(0, getWidth() - keyW), juce::jmax(0, velTop() - headH));
            for (int t = 0; t <= plen + grid; t += grid)
            {
                const int x = tickX(t);
                const bool bar = (t % kDawBar) == 0;
                g.setColour(bar ? cBord : cBord.withAlpha(0.35f));
                g.drawVerticalLine(x, (float) headH, (float) velTop());
            }

            const int endX = tickX(plen);
            if (endX < getWidth())
            {
                g.setColour(juce::Colours::black.withAlpha(0.45f));
                g.fillRect(endX, headH, getWidth() - endX, velTop() - headH);
                g.setColour(cAcc);
                g.drawVerticalLine(endX, (float) headH, (float) velTop());
                g.fillRect(endX - 3, velTop() / 2 - 10, 6, 20);
            }

            for (int i = 0; i < (int) pat.notes.size(); ++i)
            {
                const auto& n = pat.notes[(size_t) i];
                if (n.start >= plen)
                    continue;
                auto nr = noteRect(n);
                const bool sel = (i == selNote);
                auto fill = col.withAlpha(0.45f + 0.5f * (n.vel / 127.0f));
                if (n.slide)
                    fill = fill.brighter(0.25f);
                g.setColour(fill);
                g.fillRoundedRectangle(nr.toFloat(), 3.0f);
                g.setColour(sel ? juce::Colours::white : col.brighter(0.3f));
                g.drawRoundedRectangle(nr.toFloat(), 3.0f, sel ? 2.0f : 1.0f);
                if (n.slide)
                {
                    juce::Path ar;
                    ar.addTriangle((float) nr.getRight() - 8.0f, (float) nr.getCentreY() - 5.0f,
                                   (float) nr.getRight() - 2.0f, (float) nr.getCentreY(),
                                   (float) nr.getRight() - 8.0f, (float) nr.getCentreY() + 5.0f);
                    g.setColour(juce::Colours::white);
                    g.fillPath(ar);
                }
                if (sel)
                {
                    g.setColour(juce::Colours::white);
                    g.fillRect(nr.getRight() - 5, nr.getY() + 2, 4, nr.getHeight() - 4);
                }
            }

            g.restoreState();
            int lastNumX = -100;
            for (int t = 0; t <= plen; t += kDawBar)
            {
                const int x = tickX(t);
                if (x < keyW - 8 || x > getWidth() || x - lastNumX < 18)
                    continue;
                g.setColour(cMut);
                g.setFont(juce::FontOptions(11.0f));
                g.drawText(juce::String(t / kDawBar + 1), x + 3, titleH, 28, rulerH,
                           juce::Justification::centredLeft);
                lastNumX = x;
            }

            auto vel = juce::Rectangle<int>(keyW, velTop(), getWidth() - keyW, velH);
            g.setColour(cSurf2);
            g.fillRect(0, velTop(), getWidth(), velH);
            g.setColour(cMut);
            g.setFont(juce::FontOptions(10.0f));
            g.drawText("VEL", 4, velTop() + 4, keyW - 8, 14, juce::Justification::centred);
            for (int i = 0; i < (int) pat.notes.size(); ++i)
            {
                const auto& n = pat.notes[(size_t) i];
                if (n.start >= plen)
                    continue;
                const int x = tickX(n.start);
                const int h = juce::jmax(2, (int) std::llround((n.vel / 127.0f) * (velH - 8)));
                g.setColour(i == selNote ? cAcc : col.withAlpha(0.8f));
                g.fillRect(x, vel.getBottom() - 4 - h, juce::jmax(3, (int) std::llround(n.dur * pxTick * 0.4)), h);
            }

            int playLocal = -1;
            if (! st->proc.daw.isSongMode())
                playLocal = (int) st->proc.daw.getPosTick();
            else
            {
                const double pos = st->proc.daw.getPosTick();
                auto tr = st->proc.daw.copyTrack(part);
                for (const auto& c : tr.clips)
                {
                    if (c.pattern != pi) continue;
                    const int span = c.length > 0 ? c.length : pat.length;
                    if (pos >= (double) c.start && pos < (double) (c.start + span))
                    {
                        playLocal = ((int) pos - c.start) % juce::jmax(1, pat.length);
                        break;
                    }
                }
            }
            if (playLocal >= 0)
            {
                const int px = tickX(playLocal);
                g.setColour(cAcc);
                g.drawVerticalLine(px, (float) headH, (float) (getHeight() - scrollH));
            }

            g.setColour(cBord);
            g.drawRect(gridR);
            juce::ignoreUnused(cSurf);
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (st == nullptr) return;
            dragKind = 0;
            previewPitch = -1;
            const int pi = st->proc.daw.getPatternIndex();
            auto pat = st->proc.daw.copyPattern(pi);
            const int plen = juce::jmax(120, pat.length);

            if (e.mods.isMiddleButtonDown())
            {
                dragKind = 6;
                panX = e.x;
                panY = e.y;
                panScroll = scrollTick;
                panPitch = topPitch;
                return;
            }
            if (e.y < titleH)
                return;
            if (e.y < headH)
            {
                if (e.x >= keyW)
                {
                    dragKind = 7;
                    seekRoll(juce::jlimit(0, plen, xToTick(e.x)));
                    repaint();
                }
                return;
            }

            const int endX = tickX(plen);
            if (std::abs(e.x - endX) <= 6 && e.y < velTop())
            {
                dragKind = 5;
                return;
            }

            if (e.y >= velTop())
            {
                int idx = hitNoteX(pat, e.x);
                if (idx >= 0)
                {
                    selNote = idx;
                    dragKind = 4;
                    dragIndex = idx;
                    mouseDrag(e);
                }
                return;
            }
            if (e.x < keyW)
            {
                const int pitch = yToPitch(e.y);
                previewPitch = pitch;
                st->proc.dawPreview(st->proc.daw.getEditTrack(), pitch, true);
                return;
            }
            if (e.x >= endX)
                return;

            int idx = hitNote(pat, e.getPosition());
            if (idx >= 0)
            {
                selNote = idx;
                auto n = pat.notes[(size_t) idx];
                st->proc.dawPreview(st->proc.daw.getEditTrack(), n.pitch, true);
                previewPitch = n.pitch;
                if (e.mods.isPopupMenu())
                {
                    st->proc.daw.removeNote(pi, idx);
                    selNote = -1;
                    st->proc.dawPreview(st->proc.daw.getEditTrack(), n.pitch, false);
                    previewPitch = -1;
                    repaint();
                    return;
                }
                if (e.mods.isShiftDown())
                {
                    n.slide = ! n.slide;
                    st->proc.daw.setNote(pi, idx, n);
                    repaint();
                    return;
                }
                auto nr = noteRect(n);
                if (e.x >= nr.getRight() - 6)
                    dragKind = 2;
                else
                    dragKind = 1;
                dragIndex = idx;
                dragOffTick = xToTick(e.x) - n.start;
                dragOffPitch = yToPitch(e.y) - n.pitch;
                return;
            }
            if (e.mods.isPopupMenu())
                return;
            DawNote n;
            n.pitch = yToPitch(e.y);
            n.start = DawStudio::snapTo(xToTick(e.x), st->snapTicks());
            if (n.start >= plen)
                return;
            n.dur = juce::jmin(st->snapTicks(), plen - n.start);
            n.vel = 100;
            n.slide = e.mods.isShiftDown();
            selNote = st->proc.daw.addNote(pi, n);
            dragKind = 2;
            dragIndex = selNote;
            st->proc.dawPreview(st->proc.daw.getEditTrack(), n.pitch, true);
            previewPitch = n.pitch;
            repaint();
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (st == nullptr) return;
            if (dragKind == 6)
            {
                scrollTick = juce::jmax(0.0, panScroll - (double) (e.x - panX) / juce::jmax(0.02, pxTick));
                topPitch = juce::jlimit(24, 120, panPitch + (e.y - panY) / juce::jmax(1, keyH));
                syncBar();
                repaint();
                return;
            }
            if (dragKind == 7)
            {
                const int plen = juce::jmax(120, st->proc.daw.patternLength(st->proc.daw.getPatternIndex()));
                seekRoll(juce::jlimit(0, plen, xToTick(e.x)));
                repaint();
                return;
            }
            const int pi = st->proc.daw.getPatternIndex();
            if (dragKind == 5)
            {
                int len = juce::jmax(480, DawStudio::snapTo(xToTick(e.x), st->snapTicks()));
                st->proc.daw.setPatternLength(pi, len);
                repaint();
                return;
            }
            if (dragIndex < 0) return;
            auto pat = st->proc.daw.copyPattern(pi);
            if (dragIndex >= (int) pat.notes.size()) return;
            auto n = pat.notes[(size_t) dragIndex];
            const int plen = juce::jmax(120, pat.length);
            if (dragKind == 1)
            {
                const int pitch = juce::jlimit(0, 127, yToPitch(e.y) - dragOffPitch);
                int start = juce::jmax(0, DawStudio::snapTo(xToTick(e.x) - dragOffTick, st->snapTicks()));
                start = juce::jmin(start, plen - 1);
                if (pitch != n.pitch)
                {
                    if (previewPitch >= 0)
                        st->proc.dawPreview(st->proc.daw.getEditTrack(), previewPitch, false);
                    st->proc.dawPreview(st->proc.daw.getEditTrack(), pitch, true);
                    previewPitch = pitch;
                }
                n.pitch = pitch;
                n.start = start;
                if (n.start + n.dur > plen)
                    n.dur = plen - n.start;
                st->proc.daw.setNote(pi, dragIndex, n);
            }
            else if (dragKind == 2)
            {
                int end = juce::jmax(n.start + 30, DawStudio::snapTo(xToTick(e.x), st->snapTicks()));
                end = juce::jmin(end, plen);
                n.dur = juce::jmax(1, end - n.start);
                st->proc.daw.setNote(pi, dragIndex, n);
            }
            else if (dragKind == 4)
            {
                const int vel = juce::jlimit(1, 127,
                    (int) std::llround((1.0f - (float) (e.y - velTop()) / (float) velH) * 127.0f));
                n.vel = vel;
                st->proc.daw.setNote(pi, dragIndex, n);
            }
            repaint();
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            if (st != nullptr && previewPitch >= 0)
                st->proc.dawPreview(st->proc.daw.getEditTrack(), previewPitch, false);
            previewPitch = -1;
            dragKind = 0;
        }

        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (e.mods.isAltDown() && selNote >= 0 && st != nullptr)
            {
                const int pi = st->proc.daw.getPatternIndex();
                auto pat = st->proc.daw.copyPattern(pi);
                if (selNote < (int) pat.notes.size())
                {
                    auto n = pat.notes[(size_t) selNote];
                    int dv = (int) std::llround((double) w.deltaY * 28.0);
                    if (dv == 0)
                        dv = w.deltaY >= 0.0f ? 1 : -1;
                    dv = juce::jlimit(-16, 16, dv);
                    n.vel = juce::jlimit(1, 127, n.vel + dv);
                    st->proc.daw.setNote(pi, selNote, n);
                    repaint();
                    return;
                }
            }
            if (e.mods.isAltDown())
            {
                keyH = juce::jlimit(8, 28, keyH + (w.deltaY > 0 ? 1 : -1));
                repaint();
                return;
            }
            const bool horiz = e.mods.isShiftDown()
                || std::abs(w.deltaX) > std::abs(w.deltaY)
                || (e.y >= titleH && e.y < headH);
            if (horiz)
            {
                const float raw = std::abs(w.deltaX) > 0.01f ? w.deltaX : w.deltaY;
                scrollTick = juce::jmax(0.0, scrollTick - (double) raw * (double) kDawBar * 0.5);
                syncBar();
                repaint();
                return;
            }
            if (e.mods.isCtrlDown() || e.mods.isCommandDown())
            {
                zoom(w.deltaY > 0 ? 1.12 : 0.9);
                return;
            }
            topPitch = juce::jlimit(24, 120, topPitch + (w.deltaY > 0 ? 2 : -2));
            repaint();
        }

        void mouseMove(const juce::MouseEvent& e) override
        {
            if (st == nullptr) return;
            auto pat = st->proc.daw.copyPattern(st->proc.daw.getPatternIndex());
            const int plen = juce::jmax(120, pat.length);
            if (std::abs(e.x - tickX(plen)) <= 6 && e.y > headH && e.y < velTop())
            {
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                return;
            }
            int idx = hitNote(pat, e.getPosition());
            if (idx >= 0)
            {
                auto nr = noteRect(pat.notes[(size_t) idx]);
                setMouseCursor(e.x >= nr.getRight() - 6 ? juce::MouseCursor::LeftRightResizeCursor
                                                        : juce::MouseCursor::DraggingHandCursor);
            }
            else
                setMouseCursor(juce::MouseCursor::NormalCursor);
        }

    private:
        static bool isBlack(int p)
        {
            const int k = p % 12;
            return k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
        }

        int velTop() const { return getHeight() - velH - scrollH; }

        juce::Rectangle<int> gridBounds() const
        {
            return { keyW, headH, getWidth() - keyW, velTop() - headH };
        }

        int pitchY(int pitch) const
        {
            return headH + (topPitch - pitch) * keyH;
        }

        int yToPitch(int y) const
        {
            return juce::jlimit(0, 127, topPitch - (y - headH) / keyH);
        }

        int tickX(int tick) const
        {
            return keyW + (int) std::llround(((double) tick - scrollTick) * pxTick);
        }

        int xToTick(int x) const
        {
            return juce::jmax(0, (int) std::llround((double) (x - keyW) / pxTick + scrollTick));
        }

        juce::Rectangle<int> noteRect(const DawNote& n) const
        {
            return { tickX(n.start), pitchY(n.pitch) + 1,
                     juce::jmax(6, (int) std::llround(n.dur * pxTick)), keyH - 2 };
        }

        int hitNote(const DawPattern& pat, juce::Point<int> p) const
        {
            for (int i = (int) pat.notes.size() - 1; i >= 0; --i)
                if (noteRect(pat.notes[(size_t) i]).contains(p))
                    return i;
            return -1;
        }

        int hitNoteX(const DawPattern& pat, int x) const
        {
            int best = -1;
            int bestD = 24;
            for (int i = 0; i < (int) pat.notes.size(); ++i)
            {
                const int d = std::abs(tickX(pat.notes[(size_t) i].start) - x);
                if (d < bestD) { bestD = d; best = i; }
            }
            return best;
        }

        void scrollBarMoved(juce::ScrollBar* bar, double) override
        {
            if (bar != &hbar)
                return;
            scrollTick = juce::jmax(0.0, hbar.getCurrentRangeStart());
            repaint();
        }

        void syncBar()
        {
            if (st == nullptr)
                return;
            const int plen = juce::jmax(120, st->proc.daw.patternLength(st->proc.daw.getPatternIndex()));
            const double view = (double) juce::jmax(1, getWidth() - keyW) / juce::jmax(0.02, pxTick);
            hbar.setRangeLimits(0.0, (double) plen, juce::dontSendNotification);
            hbar.setCurrentRange(scrollTick, view, juce::dontSendNotification);
        }

        void seekRoll(int localTick)
        {
            if (st == nullptr)
                return;
            localTick = juce::jmax(0, localTick);
            if (! st->proc.daw.isSongMode())
            {
                st->proc.daw.seekTick((double) localTick);
                return;
            }
            const int part = st->proc.daw.getEditTrack();
            const int pi = st->proc.daw.getPatternIndex();
            auto tr = st->proc.daw.copyTrack(part);
            int base = 0;
            if (st->focusTrack == part && st->focusClip >= 0
                && st->focusClip < (int) tr.clips.size()
                && tr.clips[(size_t) st->focusClip].pattern == pi)
            {
                base = tr.clips[(size_t) st->focusClip].start;
            }
            else
            {
                for (const auto& c : tr.clips)
                {
                    if (c.pattern == pi)
                    {
                        base = c.start;
                        break;
                    }
                }
            }
            st->proc.daw.seekTick((double) (base + localTick));
        }

        DawStudio* st { nullptr };
        juce::TextButton zOut, zIn;
        juce::ScrollBar hbar { false };
        int selNote { -1 };
        int dragKind { 0 };
        int dragIndex { -1 };
        int dragOffTick { 0 };
        int dragOffPitch { 0 };
        int previewPitch { -1 };
        int topPitch { 84 };
        int keyH { 14 };
        int panX { 0 };
        int panY { 0 };
        int panPitch { 84 };
        double panScroll { 0.0 };
        double scrollTick { 0.0 };
        double pxTick { 0.08 };
        static const int keyW = 48;
        static const int velH = 52;
        static const int titleH = 22;
        static const int rulerH = 18;
        static const int headH = titleH + rulerH;
        static const int scrollH = 12;
    };

    ModernEdirolSd80Processor& proc;
    SkinPalette pal { kSkins[0] };
    juce::TextButton playBtn, stopBtn, recBtn, songBtn, patBtn, newBtn, saveBtn, loadBtn, importBtn, exportBtn, plusPatBtn;
    juce::Slider bpmSlider;
    juce::ComboBox snapBox, patBox;
    juce::Label targetL, hintL;
    Arrange arrange;
    Roll roll;
    juce::Viewport arrangeView;
    juce::Component splitBar;
    bool active { false };
    bool splitDrag { false };
    int seenGen { -1 };
    int rollFrac { 45 };
};
