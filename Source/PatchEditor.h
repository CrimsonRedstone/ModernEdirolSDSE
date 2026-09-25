#pragma once

// PATCH tab: 4-tone editor, drag envelopes, local 144-slot library.
// SEND (SD-80 / SD-90) writes the temporary patch of one part in Native mode.
// SD-20 has no user flash. SAVE / LOAD only, plus live CC offsets.

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "Skin.h"
#include "UserPatch.h"

class PatchInfoOverlay : public juce::Component
{
public:
    PatchInfoOverlay()
    {
        body.setMultiLine(true);
        body.setReadOnly(true);
        body.setScrollbarsShown(true);
        body.setCaretVisible(false);
        body.setPopupMenuEnabled(false);
        body.setText(
            "A patch starts from a factory sound. Pick a map, then a category, then a name. "
            "That sound's waves stay. Everything you change sits on top of them.\n\n"
            "LAYERS\n"
            "Up to four layers can play at once. An SD-20 only plays layers 1 and 2. "
            "ON / OFF mutes a layer without deleting it.\n"
            "Loud: how loud this layer is next to the others.\n"
            "Tune: pitch in semitones. The middle matches the factory sound.\n"
            "Pan: stereo position. The middle is the center.\n"
            "Keys: which playing strength uses this layer. Drag the left end for soft notes and the right end for hard notes.\n"
            "Through effect runs the layer through the part's Multi FX. Straight out skips that effect.\n\n"
            "FILTER AND VOLUME\n"
            "FILTER is the brightness curve. VOLUME is the loudness curve. "
            "Drag a dot up for brighter or louder, and sideways to make that stage longer.\n"
            "Cutoff: how open the filter is. Higher is brighter.\n"
            "Resonance: a peak at the cutoff. Higher is sharper.\n"
            "Envelope: how far the curve moves the filter. The middle, 0, means the curve does nothing.\n"
            "Type: low pass gets darker as cutoff falls. High pass thins the bass. Off is no filter.\n"
            "LFO: a repeating wobble. Sine is smooth, square is on and off.\n\n"
            "CHORUS AND REVERB\n"
            "Chorus makes the layer wider and washier. Reverb puts it further into the room. "
            "Both use the shared chorus and reverb on the mixer.\n\n"
            "SEND, SAVE, STORE\n"
            "LIVE sends each drag while you move. Turn it off to edit quietly, then SEND.\n"
            "SEND (SD-80 and SD-90) writes this sound onto the selected part as a temporary patch. "
            "The module keeps it until you pick another sound or power off. It does not write the module's user memory. "
            "The module's own user slots only keep multi-effects from the panel Write Patch.\n"
            "SAVE / LOAD writes a .mesd80patch file on this computer. That works for an SD-20 too.\n"
            "STORE keeps a copy in one of the 144 slots on this screen. Those slots live in the plugin, not on the module.\n"
            "SYX writes a SysEx file a DAW can play into an SD-80 or SD-90.\n\n"
            "SD-20\n"
            "SEND stays off. Layers 3 and 4 are saved in the file but not played. "
            "Live edits use controller offsets. The middle of cutoff, resonance, and envelope means leave the factory sound alone.");
        addAndMakeVisible(body);
        closeBtn.setButtonText("CLOSE");
        closeBtn.onClick = [this] { setVisible(false); };
        addAndMakeVisible(closeBtn);
    }

    void setColours(juce::Colour background, juce::Colour panel, juce::Colour bodyCol,
                    juce::Colour dim, juce::Colour mark)
    {
        bg = background;
        surface = panel;
        text = bodyCol;
        muted = dim;
        accent = mark;
        body.setColour(juce::TextEditor::backgroundColourId, panel);
        body.setColour(juce::TextEditor::textColourId, bodyCol);
        body.setColour(juce::TextEditor::outlineColourId, mark);
        closeBtn.setColour(juce::TextButton::buttonColourId, mark);
        closeBtn.setColour(juce::TextButton::textColourOffId, background);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(bg.withAlpha(0.78f));
        g.setColour(surface);
        g.fillRoundedRectangle(panelR.toFloat(), 10.0f);
        g.setColour(accent);
        g.drawRoundedRectangle(panelR.toFloat(), 10.0f, 1.5f);
        g.setColour(text);
        g.setFont(juce::FontOptions(17.0f).withStyle("Bold"));
        g.drawText("How a patch is built", titleR, juce::Justification::centredLeft);
    }

    void resized() override
    {
        panelR = getLocalBounds().reduced(juce::jmax(16, getWidth() / 12), juce::jmax(16, getHeight() / 14));
        auto in = panelR.reduced(14, 10);
        auto bar = in.removeFromTop(28);
        titleR = bar.removeFromLeft(bar.getWidth() - 84);
        closeBtn.setBounds(bar.removeFromRight(76).reduced(0, 2));
        in.removeFromTop(6);
        body.setBounds(in);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (! panelR.contains(e.getPosition()))
            setVisible(false);
    }

private:
    juce::TextEditor body;
    juce::TextButton closeBtn;
    juce::Colour bg { 0xff101218 }, surface { 0xff1a1d27 }, text { 0xffece8df };
    juce::Colour muted { 0xff8b8f9c }, accent { 0xffe8a317 };
    juce::Rectangle<int> panelR, titleR;
};

class PatchEditor : public juce::Component,
                    private juce::Timer,
                    private juce::Button::Listener,
                    private juce::ComboBox::Listener,
                    private juce::TextEditor::Listener
{
public:
    explicit PatchEditor(ModernEdirolSd80Processor& p) : proc(p)
    {
        setOpaque(false);
        nameEd.setText(proc.getUserPatch().name, false);
        nameEd.setJustification(juce::Justification::centredLeft);
        nameEd.setInputRestrictions(12);
        nameEd.addListener(this);
        nameEd.setTooltip("Name of this sound, 12 characters. SEND writes it onto the part.");
        addAndMakeVisible(nameEd);

        deviceBox.addItem("SD-90", 1);
        deviceBox.addItem("SD-80", 2);
        deviceBox.addItem("SD-20", 3);
        deviceBox.setTooltip("Which module this patch is for. SD-20 can be saved here, but SEND stays off.");
        deviceBox.addListener(this);
        addAndMakeVisible(deviceBox);

        for (int i = 0; i < 32; ++i)
        {
            const juce::String tag = (i < 16 ? "A" : "B") + juce::String(i % 16 + 1).paddedLeft('0', 2);
            partBox.addItem(tag, i + 1);
        }
        partBox.addListener(this);
        partBox.setTooltip("Which of the 32 parts receives this sound.");
        addAndMakeVisible(partBox);

        for (int i = 0; i < 7; ++i)
            tvfBox.addItem(kTvfTypeName[i], i + 1);
        tvfBox.addListener(this);
        tvfBox.setTooltip("Filter type. Low pass gets darker as cutoff falls. High pass thins the bass. Off is no filter.");
        addAndMakeVisible(tvfBox);

        for (int i = 0; i < 11; ++i)
            lfoBox.addItem(kLfoWaveName[i], i + 1);
        lfoBox.addListener(this);
        lfoBox.setTooltip("How the layer wobbles. Sine is smooth, square is on-off, random jumps.");
        addAndMakeVisible(lfoBox);

        liveBtn.setClickingTogglesState(true);
        liveBtn.setToggleState(true, juce::dontSendNotification);
        liveBtn.setTooltip("On: every drag is sent to the module while you move. Off: edit quietly, then SEND.");
        sendBtn.setTooltip("Push the whole sound to this part now. SD-80 and SD-90 only. The module forgets it at the next sound change or power-off.");
        saveBtn.setTooltip("Save a .mesd80patch file on this computer. Works for an SD-20 too.");
        loadBtn.setTooltip("Open a .mesd80patch file.");
        syxBtn.setTooltip("Write a SysEx file a DAW can play into the module.");
        storeBtn.setTooltip("Keep this sound in the highlighted library slot at the bottom.");
        hearBtn.setTooltip("Play middle C on this part so you can hear the edit.");
        tvfBtn.setTooltip("Filter curve. Drag a dot up for brighter, sideways for a longer stage.");
        tvaBtn.setTooltip("Loudness curve. Drag a dot up for louder, sideways for a longer stage.");
        for (auto* b : { &liveBtn, &sendBtn, &saveBtn, &loadBtn, &syxBtn, &storeBtn, &hearBtn, &tvfBtn, &tvaBtn, &infoBtn })
        {
            b->addListener(this);
            addAndMakeVisible(*b);
        }
        tvfBtn.setClickingTogglesState(true);
        tvaBtn.setClickingTogglesState(true);
        tvfBtn.setRadioGroupId(81);
        tvaBtn.setRadioGroupId(81);
        tvfBtn.setToggleState(true, juce::dontSendNotification);
        infoBtn.setTooltip("Explains layers, the filter, chorus, reverb, SEND, SAVE, and STORE.");

        for (int i = 0; i <= (int) sd80::SoundMap::User; ++i)
            mapBox.addItem(sd80::soundMapName((sd80::SoundMap) i), i + 1);
        mapBox.setSelectedId(1, juce::dontSendNotification);
        mapBox.addListener(this);
        mapBox.setTooltip("Factory map. Classical, Contemporary, Solo, Enhanced, Special, or User.");
        addAndMakeVisible(mapBox);

        baseSearch.setTextToShowWhenEmpty("filter", juce::Colours::transparentBlack);
        baseSearch.setTooltip("Optional. Narrows this category. Leave it empty to see every sound in it.");
        baseSearch.addListener(this);
        addAndMakeVisible(baseSearch);

        catModel.owner = this;
        soundModel.owner = this;
        catList.setModel(&catModel);
        soundList.setModel(&soundModel);
        catList.setRowHeight(22);
        soundList.setRowHeight(22);
        catList.setOutlineThickness(1);
        soundList.setOutlineThickness(1);
        addAndMakeVisible(catList);
        addAndMakeVisible(soundList);
        addChildComponent(infoOverlay);

        pullFromProc();
        startTimerHz(20);
    }

    ~PatchEditor() override
    {
        stopTimer();
        catList.setModel(nullptr);
        soundList.setModel(nullptr);
    }

    void applyPalette(const SkinPalette& s)
    {
        bg = juce::Colour(s.bg);
        surface = juce::Colour(s.surface);
        surface2 = juce::Colour(s.surface2);
        border = juce::Colour(s.border);
        text = juce::Colour(s.text);
        muted = juce::Colour(s.muted);
        accent = juce::Colour(s.accent);
        teal = juce::Colour(s.accent2);
        muteC = juce::Colour(s.mute);
        auto style = [&](juce::TextButton& b)
        {
            b.setColour(juce::TextButton::buttonColourId, surface2);
            b.setColour(juce::TextButton::buttonOnColourId, accent);
            b.setColour(juce::TextButton::textColourOffId, text);
            b.setColour(juce::TextButton::textColourOnId, bg);
        };
        style(liveBtn); style(sendBtn); style(saveBtn); style(loadBtn);
        style(syxBtn); style(storeBtn); style(hearBtn); style(tvfBtn); style(tvaBtn); style(infoBtn);
        nameEd.setColour(juce::TextEditor::backgroundColourId, surface2);
        nameEd.setColour(juce::TextEditor::textColourId, text);
        nameEd.setColour(juce::TextEditor::outlineColourId, border);
        nameEd.setTextToShowWhenEmpty("Patch name", muted);
        baseSearch.setColour(juce::TextEditor::backgroundColourId, surface2);
        baseSearch.setColour(juce::TextEditor::textColourId, text);
        baseSearch.setColour(juce::TextEditor::outlineColourId, border);
        baseSearch.setTextToShowWhenEmpty("filter", muted);
        for (auto* c : { &deviceBox, &partBox, &tvfBox, &lfoBox, &mapBox })
        {
            c->setColour(juce::ComboBox::backgroundColourId, surface2);
            c->setColour(juce::ComboBox::textColourId, text);
            c->setColour(juce::ComboBox::outlineColourId, border);
        }
        catList.setColour(juce::ListBox::backgroundColourId, surface);
        catList.setColour(juce::ListBox::outlineColourId, border);
        soundList.setColour(juce::ListBox::backgroundColourId, surface);
        soundList.setColour(juce::ListBox::outlineColourId, border);
        infoOverlay.setColours(bg, surface, text, muted, accent);
        catList.repaint();
        soundList.repaint();
        repaint();
    }

    void loadDropped(const juce::File& f)
    {
        if (proc.loadUserPatchFile(f))
            pullFromProc();
    }

private:
    void pullFromProc()
    {
        patch = proc.getUserPatch();
        slot = proc.getUserSlot();
        nameEd.setText(patch.name, false);
        deviceBox.setSelectedId(patch.target + 1, juce::dontSendNotification);
        partBox.setSelectedId(patch.part + 1, juce::dontSendNotification);
        syncToneBoxes();
        sendBtn.setEnabled(patch.target != (int) PatchTarget::SD20);
        syncBrowserToPatch();
        repaint();
    }

    void syncToneBoxes()
    {
        const auto& t = patch.tones[tone];
        tvfBox.setSelectedId(t.tvfType + 1, juce::dontSendNotification);
        lfoBox.setSelectedId(t.lfoWave + 1, juce::dontSendNotification);
    }

    void timerCallback() override
    {
        if (noteOffAt > 0 && juce::Time::getMillisecondCounter() >= noteOffAt)
        {
            proc.dawPreview(patch.part, 60, false);
            noteOffAt = 0;
        }
        if (! dirty)
            return;
        dirty = false;
        proc.setUserPatch(patch, false);
        if (liveBtn.getToggleState())
            proc.sendUserPatch(dirtyWhat, tone, dirtyBase);
        dirtyBase = false;
        dirtyWhat = PatchPush::ToneHead;
    }

    void mark(PatchPush what, bool withBase = false)
    {
        dirty = true;
        dirtyWhat = what;
        if (withBase)
            dirtyBase = true;
        repaint();
    }

    void buttonClicked(juce::Button* b) override
    {
        if (b == &liveBtn)
        {
            hoverHint = liveBtn.getToggleState()
                ? "LIVE is on. Each drag is sent to the module as you move."
                : "LIVE is off. Nothing is sent until you press SEND.";
            repaint();
            return;
        }
        if (b == &sendBtn)
        {
            if (patch.target == (int) PatchTarget::SD20)
                return;
            proc.setUserPatch(patch, true);
            proc.sendUserPatch(PatchPush::All, tone, true);
            hoverHint = "Sent to this part. It stays until you pick another sound or power off. STORE keeps a copy here.";
            repaint();
            return;
        }
        if (b == &saveBtn)
        {
            auto chooser = std::make_shared<juce::FileChooser>("Save patch", juce::File(), "*.mesd80patch");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this, chooser](const juce::FileChooser& fc)
                                  {
                                      auto f = fc.getResult();
                                      if (f == juce::File()) return;
                                      if (! f.hasFileExtension("mesd80patch"))
                                          f = f.withFileExtension("mesd80patch");
                                      proc.setUserPatch(patch, true);
                                      proc.saveUserPatchFile(f);
                                  });
            return;
        }
        if (b == &loadBtn)
        {
            auto chooser = std::make_shared<juce::FileChooser>("Load patch", juce::File(), "*.mesd80patch");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this, chooser](const juce::FileChooser& fc)
                                  {
                                      auto f = fc.getResult();
                                      if (f == juce::File()) return;
                                      if (proc.loadUserPatchFile(f))
                                          pullFromProc();
                                  });
            return;
        }
        if (b == &syxBtn)
        {
            auto chooser = std::make_shared<juce::FileChooser>("Export SysEx", juce::File(), "*.syx");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this, chooser](const juce::FileChooser& fc)
                                  {
                                      auto f = fc.getResult();
                                      if (f == juce::File()) return;
                                      if (! f.hasFileExtension("syx"))
                                          f = f.withFileExtension("syx");
                                      proc.setUserPatch(patch, true);
                                      proc.exportUserPatchSyx(f);
                                  });
            return;
        }
        if (b == &storeBtn)
        {
            proc.setUserPatch(patch, false);
            proc.storeUserSlot(slot);
            hoverHint = "Stored in slot " + juce::String(slot + 1) + ". Double-click that cell to load it later.";
            repaint();
            return;
        }
        if (b == &hearBtn)
        {
            if (noteOffAt > 0)
                proc.dawPreview(patch.part, 60, false);
            proc.dawPreview(patch.part, 60, true);
            noteOffAt = juce::Time::getMillisecondCounter() + 420;
            hoverHint = "Playing middle C on this part.";
            repaint();
            return;
        }
        if (b == &infoBtn)
        {
            infoOverlay.setColours(bg, surface, text, muted, accent);
            infoOverlay.setVisible(true);
            infoOverlay.toFront(false);
            return;
        }
        if (b == &tvfBtn || b == &tvaBtn)
        {
            hoverHint = (b == &tvfBtn)
                ? "Filter curve. Drag a dot up for a brighter stage, sideways to make that stage longer."
                : "Loudness curve. Drag a dot up for louder, sideways to make that stage longer. It starts and ends silent.";
            repaint();
            return;
        }
    }

    void comboBoxChanged(juce::ComboBox* c) override
    {
        if (c == &deviceBox)
        {
            patch.target = deviceBox.getSelectedId() - 1;
            sendBtn.setEnabled(patch.target != (int) PatchTarget::SD20);
            proc.setUserPatch(patch, true);
            repaint();
            return;
        }
        if (c == &partBox)
        {
            patch.part = partBox.getSelectedId() - 1;
            mark(PatchPush::All, true);
            return;
        }
        if (c == &tvfBox)
        {
            patch.tones[tone].tvfType = tvfBox.getSelectedId() - 1;
            mark(PatchPush::ToneHead);
            return;
        }
        if (c == &mapBox)
        {
            catPick = 0;
            rebuildCats();
            hoverHint = juce::String(sd80::soundMapName((sd80::SoundMap) juce::jlimit(0, 6, mapBox.getSelectedId() - 1)))
                + ". Pick a category, then a sound.";
            repaint();
            return;
        }
        if (c == &lfoBox)
        {
            patch.tones[tone].lfoWave = lfoBox.getSelectedId() - 1;
            mark(PatchPush::ToneHead);
            return;
        }
    }

    void textEditorTextChanged(juce::TextEditor& e) override
    {
        if (&e == &nameEd)
        {
            patch.name = nameEd.getText().substring(0, 12);
            if (patch.target == (int) PatchTarget::SD20)
                proc.setUserPatch(patch, false);
            else
                mark(PatchPush::Common);
            return;
        }
        if (&e == &baseSearch)
            rebuildSounds();
    }

    void layout() const
    {
        auto r = getLocalBounds().reduced(10);
        chrome = r.removeFromTop(34);
        hintR = r.removeFromTop(28);
        browserR = r.removeFromTop(150);
        r.removeFromTop(6);
        cardsR = r.removeFromTop(156);
        r.removeFromTop(6);
        libR = r.removeFromBottom(70);
        r.removeFromBottom(6);
        envR = r.removeFromLeft(juce::jmax(340, (int) (r.getWidth() * 0.62f)));
        r.removeFromLeft(8);
        flowR = r;

        for (int i = 0; i < 4; ++i)
        {
            const int w = cardsR.getWidth() / 4;
            cardR[i] = juce::Rectangle<int>(cardsR.getX() + i * w, cardsR.getY(), w - 6, cardsR.getHeight());
        }

        auto envIn = envR.reduced(12, 8);
        envHead = envIn.removeFromTop(28);
        envFoot = envIn.removeFromBottom(52);
        envLegend = envIn.removeFromBottom(16);
        plotR = envIn.reduced(2, 4);

        auto foot = envFoot;
        tvfComboR = foot.removeFromLeft(86);
        foot.removeFromLeft(8);
        const int third = juce::jmax(40, foot.getWidth() / 3);
        cutR = foot.removeFromLeft(third);
        resR = foot.removeFromLeft(third);
        depR = foot;

        auto head = envHead;
        head.removeFromLeft(168);
        lfoComboR = head.removeFromRight(108);
        lfoLabR = head.removeFromRight(36);
    }

    void resized() override
    {
        layout();
        auto c = chrome.reduced(0, 2);
        nameEd.setBounds(c.removeFromLeft(168));
        c.removeFromLeft(6);
        deviceBox.setBounds(c.removeFromLeft(84));
        c.removeFromLeft(4);
        partBox.setBounds(c.removeFromLeft(68));
        c.removeFromLeft(8);
        liveBtn.setBounds(c.removeFromLeft(56));
        sendBtn.setBounds(c.removeFromLeft(64));
        hearBtn.setBounds(c.removeFromLeft(58));
        c.removeFromLeft(8);
        saveBtn.setBounds(c.removeFromLeft(58));
        loadBtn.setBounds(c.removeFromLeft(58));
        syxBtn.setBounds(c.removeFromLeft(52));
        storeBtn.setBounds(c.removeFromLeft(68));
        c.removeFromLeft(6);
        infoBtn.setBounds(c.removeFromLeft(56));

        auto bar = browserR;
        auto head = bar.removeFromTop(28);
        head.removeFromLeft(78);
        mapBox.setBounds(head.removeFromLeft(150).reduced(0, 3));
        head.removeFromLeft(6);
        baseSearch.setBounds(head.removeFromLeft(140).reduced(0, 3));
        bar.removeFromTop(2);
        catList.setBounds(bar.removeFromLeft(168).reduced(0, 2));
        bar.removeFromLeft(6);
        soundList.setBounds(bar.reduced(0, 2));

        infoOverlay.setBounds(getLocalBounds());
        if (infoOverlay.isVisible())
            infoOverlay.toFront(false);

        auto h = envHead;
        tvfBtn.setBounds(h.removeFromLeft(78).reduced(0, 2));
        tvaBtn.setBounds(h.removeFromLeft(86).reduced(0, 2));
        auto typeR = tvfComboR;
        tvfBox.setBounds(typeR.removeFromBottom(22).reduced(0, 1));
        lfoBox.setBounds(lfoComboR.reduced(0, 2));
    }

    juce::String defaultHint() const
    {
        if (patch.target == (int) PatchTarget::SD20)
            return "Drag to design the sound. SAVE and STORE keep it in the plugin. An SD-20 cannot take SEND, and it only plays layers 1 and 2.";
        return "Drag a control. LIVE sends it as you move. SEND puts the whole sound on this part. Power-off clears the module - STORE keeps a copy here.";
    }

    juce::String hintFor(juce::Point<int> p) const
    {
        for (int i = 0; i < 4; ++i)
        {
            if (onBox[i].contains(p))
                return "Click to mute this layer. It stays in the file, it just makes no sound.";
            if (levelBox[i].contains(p))
                return "How loud this layer is next to the others. Drag right for louder.";
            if (tuneBox[i].contains(p))
                return "Pitch of this layer, in semitones. The middle matches the factory sound.";
            if (panBox[i].contains(p))
                return "Stereo position. The middle is center. Drag left or right.";
            if (velBox[i].contains(p))
                return "Which playing strength uses this layer. Drag the left end for soft notes, the right end for hard notes.";
            if (mfxBox[i].contains(p))
                return patch.tones[i].outMfx == 0
                    ? "This layer runs through the part effect. Click to send it straight out instead."
                    : "This layer skips the part effect. Click to run it through Multi FX.";
            if (cardR[i].contains(p))
            {
                juce::String s = "Layer " + juce::String(i + 1);
                if (i >= patch.audibleTones())
                    s += " is kept in the file but not played on an SD-20.";
                else if (i == tone)
                    s += " is selected. Its curve and sends are on the right.";
                else
                    s += ". Click it to edit the curve and the sends.";
                return s;
            }
        }
        if (plotR.contains(p))
            return tvfBtn.getToggleState()
                ? "Filter of layer " + juce::String(tone + 1) + ". Drag a dot up for a brighter moment, sideways for a longer one."
                : "Loudness of layer " + juce::String(tone + 1) + ". Drag a dot up for louder, sideways for a longer stage.";
        if (cutR.contains(p))
            return "Cutoff. Drag right to open the filter. The layer gets brighter.";
        if (resR.contains(p))
            return "Resonance. Drag right for a sharper peak at the cutoff. Higher is more nasal, more whistle.";
        if (depR.contains(p))
            return tvfBtn.getToggleState()
                ? "How far this curve moves the filter. The middle means the curve does nothing."
                : "How far the filter curve moves the brightness. Switch to FILTER to see that curve. The middle is no movement.";
        if (choR.contains(p))
            return "Chorus. Drag right and this layer gets wider and washier.";
        if (revR.contains(p))
            return "Reverb. Drag right and this layer sits further back in the room.";
        if (routeBig.contains(p))
            return "Click to run the selected layer through the part effect, or straight out.";
        if (slotStrip.contains(p))
            return "Library on this computer. Click a cell, STORE to keep the sound, double-click a filled cell to load it.";
        if (browserR.contains(p))
            return "Pick a map and a category, then click a sound. It supplies the waves. Filter narrows that list. INFO explains every control.";
        return defaultHint();
    }

    void paint(juce::Graphics& g) override
    {
        layout();
        const juce::String hint = hoverHint.isNotEmpty() ? hoverHint : defaultHint();
        g.setColour(hoverHint.isNotEmpty() ? text : muted);
        g.setFont(juce::FontOptions(14.0f));
        g.drawFittedText(hint, hintR, juce::Justification::centredLeft, 2);

        g.setColour(muted);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("Start from", browserR.getX(), browserR.getY(), 74, 28, juce::Justification::centredLeft);
        auto used = browserR.withHeight(28);
        used.removeFromLeft(78 + 150 + 6 + 140 + 10);
        g.setColour(text);
        g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
        g.drawText(patch.baseName, used.removeFromLeft(juce::jmin(200, used.getWidth())), juce::Justification::centredLeft);
        g.setColour(muted);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("waves stay, edits sit on top", used, juce::Justification::centredLeft);

        const int nAud = patch.audibleTones();
        for (int i = 0; i < 4; ++i)
            paintCard(g, i, i == tone, i < nAud);

        paintEnv(g);
        paintFlow(g);
        paintLib(g);
    }

    void paintBar(juce::Graphics& g, juce::Rectangle<int> r, int value, bool bipolar, juce::Colour fillCol) const
    {
        g.setColour(bg);
        g.fillRoundedRectangle(r.toFloat(), 3.0f);
        if (r.getWidth() < 2)
            return;
        if (bipolar)
        {
            const int mid = r.getX() + r.getWidth() / 2;
            const int x = r.getX() + r.getWidth() * clamp7(value) / 127;
            const int left = juce::jmin(mid, x);
            const int span = x >= mid ? x - mid : mid - x;
            g.setColour(fillCol);
            g.fillRoundedRectangle((float) left, (float) r.getY(), (float) juce::jmax(2, span), (float) r.getHeight(), 3.0f);
            g.setColour(text.withAlpha(0.45f));
            g.fillRect(mid, r.getY() - 1, 1, r.getHeight() + 2);
        }
        else
        {
            auto f = r.withWidth(juce::jmax(2, r.getWidth() * clamp7(value) / 127));
            g.setColour(fillCol);
            g.fillRoundedRectangle(f.toFloat(), 3.0f);
        }
    }

    void paintCard(juce::Graphics& g, int i, bool sel, bool audible) const
    {
        auto r = cardR[i].toFloat();
        g.setColour(sel ? surface2 : surface);
        g.fillRoundedRectangle(r, 8.0f);
        g.setColour(sel ? accent : border);
        g.drawRoundedRectangle(r, 8.0f, sel ? 2.0f : 1.0f);

        const auto& t = patch.tones[i];
        auto in = cardR[i].reduced(8, 6);
        g.setColour(audible ? text : muted);
        g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
        g.drawText("Layer " + juce::String(i + 1), in.removeFromTop(18), juce::Justification::centredLeft);

        auto onR = juce::Rectangle<int>(cardR[i].getRight() - 52, cardR[i].getY() + 6, 42, 18);
        onBox[i] = onR;
        g.setColour(t.on && audible ? teal : muteC);
        g.fillRoundedRectangle(onR.toFloat(), 4.0f);
        g.setColour(t.on && audible ? bg : juce::Colours::white);
        g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        g.drawText(t.on ? "ON" : "OFF", onR, juce::Justification::centred);

        auto sliderRow = [&](const juce::String& name, juce::Rectangle<int>& hit, int value, bool bipolar,
                             juce::Colour col, const juce::String& read)
        {
            auto line = in.removeFromTop(22);
            g.setColour(muted);
            g.setFont(juce::FontOptions(12.0f));
            g.drawText(name, line.removeFromLeft(40), juce::Justification::centredLeft);
            auto num = line.removeFromRight(42);
            g.setColour(text);
            g.drawText(read, num, juce::Justification::centredRight);
            auto bar = line.reduced(4, 6);
            hit = bar;
            paintBar(g, bar, value, bipolar, audible ? col : muted);
        };

        sliderRow("Loud", levelBox[i], t.level, false, teal, juce::String(t.level));
        const int semi = t.coarse - 64;
        sliderRow("Tune", tuneBox[i], t.coarse, true, accent,
                  (semi > 0 ? "+" : "") + juce::String(semi));
        const int pan = t.pan - 64;
        const juce::String panTxt = pan == 0 ? "C" : (pan < 0 ? "L" + juce::String(-pan) : "R" + juce::String(pan));
        sliderRow("Pan", panBox[i], t.pan, true, accent, panTxt);

        auto keys = in.removeFromTop(22);
        g.setColour(muted);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Keys", keys.removeFromLeft(40), juce::Justification::centredLeft);
        auto kn = keys.removeFromRight(42);
        g.setColour(text);
        int lo = t.velLo, hi = t.velHi;
        if (hi < lo) std::swap(lo, hi);
        g.drawText(juce::String(lo) + "-" + juce::String(hi), kn, juce::Justification::centredRight);
        auto vel = keys.reduced(4, 6);
        velBox[i] = vel;
        g.setColour(bg);
        g.fillRoundedRectangle(vel.toFloat(), 3.0f);
        if (vel.getWidth() > 2)
        {
            const int x0 = vel.getX() + vel.getWidth() * lo / 127;
            const int x1 = vel.getX() + vel.getWidth() * hi / 127;
            g.setColour(audible ? accent : muted);
            g.fillRoundedRectangle((float) x0, (float) vel.getY(), (float) juce::jmax(3, x1 - x0), (float) vel.getHeight(), 3.0f);
        }

        in.removeFromTop(4);
        auto mfx = in.removeFromTop(22);
        mfxBox[i] = mfx;
        g.setColour(t.outMfx == 0 ? teal : surface);
        g.fillRoundedRectangle(mfx.toFloat(), 4.0f);
        g.setColour(t.outMfx == 0 ? bg : text);
        g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
        g.drawText(t.outMfx == 0 ? "Through effect" : "Straight out", mfx, juce::Justification::centred);
        if (! audible)
        {
            g.setColour(muted);
            g.setFont(juce::FontOptions(11.0f));
            g.drawText("Saved, not played on SD-20",
                       cardR[i].getX(), cardR[i].getBottom() - 14, cardR[i].getWidth(), 12,
                       juce::Justification::centred);
        }
    }

    void paintEnv(juce::Graphics& g) const
    {
        auto r = envR.toFloat();
        g.setColour(surface);
        g.fillRoundedRectangle(r, 8.0f);
        g.setColour(border);
        g.drawRoundedRectangle(r, 8.0f, 1.0f);

        const bool tvf = tvfBtn.getToggleState();
        auto cap = envHead.reduced(0, 4);
        cap.removeFromLeft(170);
        cap.removeFromRight(150);
        g.setColour(muted);
        g.setFont(juce::FontOptions(12.0f));
        g.drawFittedText(tvf ? "Drag a dot. Up is brighter, sideways is longer."
                             : "Drag a dot. Up is louder, sideways is longer.",
                         cap, juce::Justification::centredLeft, 2);
        g.drawText("LFO", lfoLabR, juce::Justification::centredRight);

        const auto& t = patch.tones[tone];
        int times[4];
        int levels[5];
        if (tvf)
        {
            for (int i = 0; i < 4; ++i) times[i] = clamp7(t.tvfT[i]);
            for (int i = 0; i < 5; ++i) levels[i] = clamp7(t.tvfL[i]);
        }
        else
        {
            for (int i = 0; i < 4; ++i) times[i] = clamp7(t.tvaT[i]);
            levels[0] = 0;
            levels[1] = clamp7(t.tvaL[0]);
            levels[2] = clamp7(t.tvaL[1]);
            levels[3] = clamp7(t.tvaL[2]);
            levels[4] = 0;
        }
        int acc[5] { 0, 0, 0, 0, 0 };
        for (int i = 0; i < 4; ++i)
            acc[i + 1] = acc[i] + juce::jmax(1, times[i]);
        const float total = (float) juce::jmax(1, acc[4]);
        auto plot = plotR;
        juce::Point<float> pts[5];
        for (int i = 0; i < 5; ++i)
        {
            const float x = (float) plot.getX() + (float) plot.getWidth() * ((float) acc[i] / total);
            const float y = (float) plot.getBottom() - (float) plot.getHeight() * ((float) levels[i] / 127.0f);
            pts[i] = { x, y };
            dot[i] = pts[i];
        }
        juce::Path fill;
        fill.startNewSubPath(pts[0].x, (float) plot.getBottom());
        for (int i = 0; i < 5; ++i)
            fill.lineTo(pts[i]);
        fill.lineTo(pts[4].x, (float) plot.getBottom());
        fill.closeSubPath();
        g.setColour((tvf ? accent : teal).withAlpha(0.22f));
        g.fillPath(fill);
        juce::Path curve;
        curve.startNewSubPath(pts[0]);
        for (int i = 1; i < 5; ++i)
            curve.lineTo(pts[i]);
        g.setColour(tvf ? accent : teal);
        g.strokePath(curve, juce::PathStrokeType(2.4f));
        for (int i = 0; i < 5; ++i)
        {
            g.setColour(bg);
            g.fillEllipse(pts[i].x - 7, pts[i].y - 7, 14, 14);
            g.setColour(text);
            g.drawEllipse(pts[i].x - 7, pts[i].y - 7, 14, 14, 1.6f);
        }
        g.setColour(muted);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(tvf ? "start          attack          decay          sustain          release"
                       : "silence        attack          body           sustain          silence",
                   envLegend, juce::Justification::centred);

        const int depth = t.envDepth - 64;
        paintMini(g, cutR, "Cutoff", juce::String(t.cutoff), t.cutoff, false, accent);
        paintMini(g, resR, "Resonance", juce::String(t.reso), t.reso, false, accent);
        paintMini(g, depR, "Envelope", (depth > 0 ? "+" : "") + juce::String(depth), t.envDepth, true, teal);
        g.setColour(muted);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Type", tvfComboR.withHeight(14), juce::Justification::centredLeft);
    }

    void paintMini(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
                   const juce::String& value, int v, bool bipolar, juce::Colour col) const
    {
        g.setColour(muted);
        g.setFont(juce::FontOptions(12.0f));
        auto top = area.removeFromTop(14);
        g.drawText(title, top.removeFromLeft(top.getWidth() - 36), juce::Justification::centredLeft);
        g.setColour(text);
        g.drawText(value, top, juce::Justification::centredRight);
        auto bar = area.reduced(0, 6).withHeight(10);
        paintBar(g, bar, v, bipolar, col);
    }

    void paintFlow(juce::Graphics& g) const
    {
        auto r = flowR.toFloat();
        g.setColour(surface);
        g.fillRoundedRectangle(r, 8.0f);
        g.setColour(border);
        g.drawRoundedRectangle(r, 8.0f, 1.0f);
        auto in = flowR.reduced(12, 10);
        g.setColour(text);
        g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
        g.drawText("Layer " + juce::String(tone + 1) + " output", in.removeFromTop(18), juce::Justification::centredLeft);
        g.setColour(muted);
        g.setFont(juce::FontOptions(12.0f));
        g.drawFittedText("Effect uses the Multi FX on this part in the mixer. Chorus and reverb are the shared room.",
                         in.removeFromTop(32), juce::Justification::centredLeft, 2);
        in.removeFromTop(6);
        routeBig = in.removeFromTop(32);
        const bool thru = patch.tones[tone].outMfx == 0;
        g.setColour(thru ? teal : surface2);
        g.fillRoundedRectangle(routeBig.toFloat(), 6.0f);
        g.setColour(thru ? bg : text);
        g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
        g.drawText(thru ? "Through the part effect" : "Straight out, no effect", routeBig, juce::Justification::centred);
        in.removeFromTop(12);
        auto cho = in.removeFromTop(40);
        auto gap = in.removeFromTop(8);
        juce::ignoreUnused(gap);
        auto rev = in.removeFromTop(40);
        paintSend(g, cho, "Chorus  -  wider and washier", patch.tones[tone].chorus, true);
        paintSend(g, rev, "Reverb  -  further into the room", patch.tones[tone].reverb, false);
    }

    void paintSend(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& name, int value, bool chorus) const
    {
        auto full = r;
        g.setColour(muted);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(name + "   " + juce::String(value), r.removeFromTop(14), juce::Justification::centredLeft);
        auto bar = r.removeFromTop(12);
        if (chorus) choR = full; else revR = full;
        paintBar(g, bar, value, false, chorus ? teal : accent);
    }

    void paintLib(juce::Graphics& g) const
    {
        g.setColour(surface);
        g.fillRoundedRectangle(libR.toFloat(), 8.0f);
        g.setColour(border);
        g.drawRoundedRectangle(libR.toFloat(), 8.0f, 1.0f);
        auto in = libR.reduced(10, 6);
        g.setColour(text);
        g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
        g.drawText("Saved here", in.removeFromTop(16).removeFromLeft(90), juce::Justification::centredLeft);
        auto sub = libR.reduced(100, 6).removeFromTop(16);
        g.setColour(muted);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Slot " + juce::String(slot + 1)
                       + " of 144.  Click a cell, STORE keeps this sound, double-click a filled cell to load it.",
                   sub, juce::Justification::centredLeft);
        in.removeFromTop(2);
        slotStrip = in.removeFromTop(22);
        const int n = kUserPatchSlots;
        const float gap = 8.0f;
        const float w = ((float) slotStrip.getWidth() - gap) / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float x = (float) slotStrip.getX() + w * (float) i + (i >= 128 ? gap : 0.0f);
            auto cell = juce::Rectangle<float>(x, (float) slotStrip.getY(), juce::jmax(1.0f, w - 0.6f),
                                              (float) slotStrip.getHeight());
            g.setColour(i == slot ? accent : (proc.userSlotUsed(i) ? teal.withAlpha(0.8f) : surface2));
            g.fillRoundedRectangle(cell, 1.5f);
        }
        auto labels = in;
        g.setColour(muted);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("sounds 1-128", labels.removeFromLeft(labels.getWidth() * 128 / 144), juce::Justification::centredLeft);
        g.drawText("drum slots", labels, juce::Justification::centredLeft);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (slotStrip.contains(e.getPosition()) && slotStrip.getWidth() > 0)
        {
            const float gap = 8.0f;
            const float w = ((float) slotStrip.getWidth() - gap) / (float) kUserPatchSlots;
            const float x = (float) (e.getPosition().x - slotStrip.getX());
            int i = (x < 128.0f * w) ? (int) (x / w) : (int) ((x - gap) / w);
            i = juce::jlimit(0, kUserPatchSlots - 1, i);
            slot = i;
            proc.setUserSlot(i);
            hoverHint = proc.userSlotUsed(i)
                ? "Slot " + juce::String(i + 1) + " has a sound. Double-click to load it."
                : "Empty slot " + juce::String(i + 1) + ". STORE keeps the current sound here.";
            if (proc.userSlotUsed(i) && e.getNumberOfClicks() >= 2)
            {
                proc.recallUserSlot(i);
                pullFromProc();
                hoverHint = "Loaded slot " + juce::String(i + 1) + ".";
                return;
            }
            repaint();
            return;
        }

        for (int i = 0; i < 4; ++i)
        {
            if (! cardR[i].contains(e.getPosition()))
                continue;
            tone = i;
            syncToneBoxes();
            if (onBox[i].contains(e.getPosition()))
            {
                patch.tones[i].on = ! patch.tones[i].on;
                mark(PatchPush::Tmt);
                return;
            }
            if (mfxBox[i].contains(e.getPosition()))
            {
                patch.tones[i].outMfx = patch.tones[i].outMfx ? 0 : 1;
                mark(PatchPush::ToneHead);
                return;
            }
            if (levelBox[i].contains(e.getPosition()))
            {
                drag = 1;
                dragTone = i;
                applyLevel(i, e.getPosition().x);
                return;
            }
            if (tuneBox[i].contains(e.getPosition()))
            {
                drag = 6;
                dragTone = i;
                applyTune(i, e.getPosition().x);
                return;
            }
            if (panBox[i].contains(e.getPosition()))
            {
                drag = 7;
                dragTone = i;
                applyPan(i, e.getPosition().x);
                return;
            }
            if (velBox[i].contains(e.getPosition()))
            {
                drag = 2;
                dragTone = i;
                const auto r = velBox[i];
                const int v = r.getWidth() > 0
                    ? juce::jlimit(1, 127, (e.getPosition().x - r.getX()) * 127 / r.getWidth())
                    : 64;
                const int dLo = v >= patch.tones[i].velLo ? v - patch.tones[i].velLo : patch.tones[i].velLo - v;
                const int dHi = v >= patch.tones[i].velHi ? v - patch.tones[i].velHi : patch.tones[i].velHi - v;
                dragUpper = e.mods.isRightButtonDown() || e.mods.isAltDown() || dHi <= dLo;
                applyVel(i, e.getPosition().x, dragUpper);
                return;
            }
            hoverHint = hintFor(e.getPosition());
            repaint();
            return;
        }

        if (routeBig.contains(e.getPosition()))
        {
            patch.tones[tone].outMfx = patch.tones[tone].outMfx ? 0 : 1;
            hoverHint = patch.tones[tone].outMfx == 0
                ? "This layer now runs through the part effect."
                : "This layer now goes straight out, past the effect.";
            mark(PatchPush::ToneHead);
            return;
        }
        if (choR.contains(e.getPosition()))
        {
            drag = 3;
            applySend(true, e.getPosition().x);
            return;
        }
        if (revR.contains(e.getPosition()))
        {
            drag = 4;
            applySend(false, e.getPosition().x);
            return;
        }
        if (cutR.contains(e.getPosition()))
        {
            drag = 8;
            applyCut(e.getPosition().x);
            return;
        }
        if (resR.contains(e.getPosition()))
        {
            drag = 9;
            applyRes(e.getPosition().x);
            return;
        }
        if (depR.contains(e.getPosition()))
        {
            drag = 10;
            applyDepth(e.getPosition().x);
            return;
        }

        if (plotR.contains(e.getPosition()))
        {
            int best = -1;
            float bestD = 18.0f;
            for (int i = 0; i < 5; ++i)
            {
                const float d = e.position.getDistanceFrom(dot[i]);
                if (d < bestD) { bestD = d; best = i; }
            }
            if (best >= 0)
            {
                drag = 5;
                dragPt = best;
                dragX0 = e.position.x;
                const bool tvf = tvfBtn.getToggleState();
                dragTime0 = (best == 0) ? 0 : (tvf ? patch.tones[tone].tvfT[best - 1] : patch.tones[tone].tvaT[best - 1]);
                return;
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (drag == 1) applyLevel(dragTone, e.getPosition().x);
        else if (drag == 2) applyVel(dragTone, e.getPosition().x, dragUpper);
        else if (drag == 3) applySend(true, e.getPosition().x);
        else if (drag == 4) applySend(false, e.getPosition().x);
        else if (drag == 5) applyEnv(e);
        else if (drag == 6) applyTune(dragTone, e.getPosition().x);
        else if (drag == 7) applyPan(dragTone, e.getPosition().x);
        else if (drag == 8) applyCut(e.getPosition().x);
        else if (drag == 9) applyRes(e.getPosition().x);
        else if (drag == 10) applyDepth(e.getPosition().x);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (drag != 0)
            proc.setUserPatch(patch, true);
        drag = 0;
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        if (drag != 0)
            return;
        const auto next = hintFor(e.getPosition());
        const auto shown = hoverHint.isNotEmpty() ? hoverHint : defaultHint();
        if (next != shown)
        {
            hoverHint = (next == defaultHint()) ? juce::String() : next;
            repaint(hintR);
        }
        const auto p = e.getPosition();
        bool slide = choR.contains(p) || revR.contains(p) || cutR.contains(p) || resR.contains(p) || depR.contains(p);
        for (int i = 0; i < 4 && ! slide; ++i)
            slide = levelBox[i].contains(p) || tuneBox[i].contains(p) || panBox[i].contains(p) || velBox[i].contains(p);
        if (plotR.contains(p))
            setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor);
        else if (slide)
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (drag != 0)
            return;
        if (hoverHint.isNotEmpty())
        {
            hoverHint.clear();
            repaint(hintR);
        }
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    int valueFromX(juce::Rectangle<int> r, int x, int lo, int hi) const
    {
        if (r.getWidth() < 1)
            return lo;
        return juce::jlimit(lo, hi, lo + (x - r.getX()) * (hi - lo) / r.getWidth());
    }

    void applyLevel(int i, int x)
    {
        auto r = levelBox[i];
        if (r.getWidth() < 1) return;
        patch.tones[i].level = clamp7((x - r.getX()) * 127 / r.getWidth());
        tone = i;
        hoverHint = "Layer " + juce::String(i + 1) + " loudness " + juce::String(patch.tones[i].level)
            + ". Higher sits louder against the other layers.";
        mark(PatchPush::ToneHead);
    }

    void applyTune(int i, int x)
    {
        patch.tones[i].coarse = valueFromX(tuneBox[i], x, 0, 127);
        tone = i;
        const int semi = patch.tones[i].coarse - 64;
        hoverHint = "Layer " + juce::String(i + 1) + " pitch "
            + juce::String(semi > 0 ? "+" : "") + juce::String(semi)
            + " semitones. Zero matches the factory sound.";
        mark(PatchPush::ToneHead);
    }

    void applyPan(int i, int x)
    {
        patch.tones[i].pan = valueFromX(panBox[i], x, 0, 127);
        tone = i;
        const int p = patch.tones[i].pan - 64;
        hoverHint = p == 0 ? "Layer sits in the center."
                           : (p < 0 ? "Layer moves left." : "Layer moves right.");
        mark(PatchPush::ToneHead);
    }

    void applyVel(int i, int x, bool upper)
    {
        auto r = velBox[i];
        if (r.getWidth() < 1) return;
        int v = juce::jlimit(1, 127, (x - r.getX()) * 127 / r.getWidth());
        if (upper) patch.tones[i].velHi = v;
        else patch.tones[i].velLo = v;
        tone = i;
        int lo = patch.tones[i].velLo, hi = patch.tones[i].velHi;
        if (hi < lo) std::swap(lo, hi);
        hoverHint = "Layer " + juce::String(i + 1) + " plays from strength " + juce::String(lo)
            + " to " + juce::String(hi) + ". Left is a soft key, right is a hard one.";
        mark(PatchPush::Tmt);
    }

    void applySend(bool chorus, int x)
    {
        auto r = chorus ? choR : revR;
        if (r.getWidth() < 1) return;
        const int v = clamp7((x - r.getX()) * 127 / r.getWidth());
        if (chorus) patch.tones[tone].chorus = v;
        else patch.tones[tone].reverb = v;
        hoverHint = chorus
            ? "Chorus " + juce::String(v) + ". More is wider and washier."
            : "Reverb " + juce::String(v) + ". More sits further back in the room.";
        mark(PatchPush::ToneHead);
    }

    void applyCut(int x)
    {
        patch.tones[tone].cutoff = valueFromX(cutR, x, 0, 127);
        hoverHint = "Cutoff " + juce::String(patch.tones[tone].cutoff) + ". Higher is brighter.";
        mark(PatchPush::ToneHead);
    }

    void applyRes(int x)
    {
        patch.tones[tone].reso = valueFromX(resR, x, 0, 127);
        hoverHint = "Resonance " + juce::String(patch.tones[tone].reso) + ". Higher is a sharper peak, more whistle.";
        mark(PatchPush::ToneHead);
    }

    void applyDepth(int x)
    {
        patch.tones[tone].envDepth = valueFromX(depR, x, 1, 127);
        const int d = patch.tones[tone].envDepth - 64;
        hoverHint = d == 0
            ? "Envelope amount is zero, so the filter curve does not move the sound."
            : "Envelope amount " + juce::String(d > 0 ? "+" : "") + juce::String(d)
                  + ". Further from the middle moves the filter more.";
        mark(PatchPush::ToneHead);
    }

    void applyEnv(const juce::MouseEvent& e)
    {
        auto& t = patch.tones[tone];
        const bool tvf = tvfBtn.getToggleState();
        if (plotR.getHeight() < 1) return;
        int level = clamp7((int) std::lround(127.0f * (float) (plotR.getBottom() - e.position.y) / (float) plotR.getHeight()));
        if (tvf)
        {
            if (dragPt == 0) t.tvfL[0] = level;
            else if (dragPt >= 1 && dragPt <= 4) t.tvfL[dragPt] = level;
        }
        else if (dragPt >= 1 && dragPt <= 3)
            t.tvaL[dragPt - 1] = level;

        if (dragPt >= 1)
        {
            const float scale = (float) plotR.getWidth() / 200.0f;
            int dt = (int) std::lround((e.position.x - dragX0) / juce::jmax(0.4f, scale));
            int nv = clamp7(dragTime0 + dt);
            if (tvf) t.tvfT[dragPt - 1] = nv;
            else t.tvaT[dragPt - 1] = nv;
        }
        const char* stageTvf[5] = { "Start", "Attack", "Decay", "Sustain", "Release" };
        const char* stageTva[5] = { "Silence", "Attack", "Body", "Sustain", "Silence" };
        const int pt = juce::jlimit(0, 4, dragPt);
        if (! tvf && (pt == 0 || pt == 4))
            hoverHint = "Loudness starts and ends silent. Drag Attack, Body, or Sustain to shape it.";
        else
            hoverHint = juce::String(tvf ? "Filter " : "Loudness ") + (tvf ? stageTvf[pt] : stageTva[pt])
                + " is at " + juce::String(level) + ". Sideways changes how long this stage lasts.";
        mark(tvf ? PatchPush::EnvTvf : PatchPush::EnvTva);
    }

    sd80::SoundMap currentMap() const
    {
        return (sd80::SoundMap) juce::jlimit(0, (int) sd80::SoundMap::User, mapBox.getSelectedId() - 1);
    }

    void syncBrowserToPatch()
    {
        int id = 1;
        if (! patch.baseDrum)
        {
            const auto m = sd80::mapFromInstMsb(patch.baseMsb);
            if ((int) m <= (int) sd80::SoundMap::User)
                id = (int) m + 1;
        }
        mapBox.setSelectedId(id, juce::dontSendNotification);
        catPick = 0;
        rebuildCats();
        if (auto* e = sd80::findPatch(patch.baseMsb, patch.baseLsb, patch.basePc, false))
        {
            for (int i = 0; i < (int) shownCats.size(); ++i)
            {
                if (shownCats[(size_t) i] == e->category)
                {
                    catPick = i;
                    break;
                }
            }
            catList.selectRow(catPick);
            rebuildSounds();
        }
    }

    void rebuildCats()
    {
        shownCats.clear();
        const auto map = currentMap();
        bool present[16] {};
        const int nCat = juce::jmin(16, (int) sd80::Category::NumCategories);
        for (int i = 0; i < sd80::kNumPatches; ++i)
        {
            const auto& e = sd80::kPatches[i];
            if (e.drum || e.map != map)
                continue;
            const int c = (int) e.category;
            if (c >= 0 && c < nCat)
                present[c] = true;
        }
        for (int c = 0; c < nCat; ++c)
            if (present[c])
                shownCats.push_back((sd80::Category) c);
        if (catPick < 0 || catPick >= (int) shownCats.size())
            catPick = 0;
        catList.updateContent();
        if (! shownCats.empty())
            catList.selectRow(catPick);
        rebuildSounds();
    }

    void rebuildSounds()
    {
        shownPatches.clear();
        int sel = -1;
        if (! shownCats.empty())
        {
            catPick = juce::jlimit(0, (int) shownCats.size() - 1, catPick);
            const auto cat = shownCats[(size_t) catPick];
            const auto q = baseSearch.getText().trim().toLowerCase();
            const auto map = currentMap();
            for (int i = 0; i < sd80::kNumPatches; ++i)
            {
                const auto& e = sd80::kPatches[i];
                if (e.drum || e.map != map || e.category != cat)
                    continue;
                if (q.isNotEmpty() && ! juce::String(e.name).toLowerCase().contains(q))
                    continue;
                if (! patch.baseDrum && e.msb == patch.baseMsb && e.lsb == patch.baseLsb && e.pc == patch.basePc)
                    sel = (int) shownPatches.size();
                shownPatches.push_back(i);
            }
        }
        soundList.updateContent();
        if (sel >= 0)
            soundList.selectRow(sel);
        else
            soundList.deselectAllRows();
        repaint();
    }

    void pickedCategory(int row)
    {
        if (row < 0 || row >= (int) shownCats.size())
            return;
        catPick = row;
        hoverHint = juce::String(sd80::categoryName(shownCats[(size_t) row])) + ". Click a sound to start from it.";
        rebuildSounds();
    }

    void chooseBase(int row)
    {
        if (row < 0 || row >= (int) shownPatches.size())
            return;
        const auto& pe = sd80::kPatches[shownPatches[(size_t) row]];
        patch.baseMsb = pe.msb;
        patch.baseLsb = pe.lsb;
        patch.basePc = pe.pc;
        patch.baseDrum = false;
        patch.baseName = pe.name;
        hoverHint = juce::String(pe.name) + " supplies the waves. Your edits sit on top.";
        mark(PatchPush::All, true);
        proc.setUserPatch(patch, true);
        soundList.selectRow(row);
    }

    class CatModel : public juce::ListBoxModel
    {
    public:
        PatchEditor* owner { nullptr };
        int getNumRows() override
        {
            return owner == nullptr ? 0 : (int) owner->shownCats.size();
        }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (owner == nullptr || row < 0 || row >= getNumRows())
                return;
            g.fillAll(selected ? owner->accent.withAlpha(0.35f) : (row % 2 ? owner->surface2 : owner->bg));
            g.setColour(owner->text);
            g.setFont(juce::FontOptions(13.0f));
            g.drawText(sd80::categoryName(owner->shownCats[(size_t) row]), 8, 0, w - 10, h,
                       juce::Justification::centredLeft, true);
        }
        void listBoxItemClicked(int row, const juce::MouseEvent&) override
        {
            if (owner != nullptr)
                owner->pickedCategory(row);
        }
    };

    class SoundModel : public juce::ListBoxModel
    {
    public:
        PatchEditor* owner { nullptr };
        int getNumRows() override
        {
            return owner == nullptr ? 0 : (int) owner->shownPatches.size();
        }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (owner == nullptr || row < 0 || row >= getNumRows())
                return;
            g.fillAll(selected ? owner->teal.withAlpha(0.35f) : (row % 2 ? owner->surface2 : owner->bg));
            g.setColour(owner->text);
            g.setFont(juce::FontOptions(13.0f));
            const auto& pe = sd80::kPatches[owner->shownPatches[(size_t) row]];
            g.drawText(pe.name, 8, 0, w - 10, h, juce::Justification::centredLeft, true);
        }
        void listBoxItemClicked(int row, const juce::MouseEvent&) override
        {
            if (owner != nullptr)
                owner->chooseBase(row);
        }
    };

    ModernEdirolSd80Processor& proc;
    UserPatch patch;
    int tone { 0 };
    int slot { 0 };
    int drag { 0 };
    int dragTone { 0 };
    int dragPt { 0 };
    int dragTime0 { 0 };
    float dragX0 { 0 };
    bool dragUpper { false };
    bool dirty { false };
    bool dirtyBase { false };
    PatchPush dirtyWhat { PatchPush::ToneHead };
    std::uint32_t noteOffAt { 0 };
    juce::String hoverHint;
    int catPick { 0 };
    std::vector<sd80::Category> shownCats;
    std::vector<int> shownPatches;
    CatModel catModel;
    SoundModel soundModel;

    juce::Colour bg { 0xff101218 }, surface { 0xff1a1d27 }, surface2 { 0xff232734 }, border { 0xff323646 };
    juce::Colour text { 0xffece8df }, muted { 0xff8b8f9c }, accent { 0xffe8a317 }, teal { 0xff3dbaa0 }, muteC { 0xffc4453c };

    juce::TextEditor nameEd, baseSearch;
    juce::ComboBox deviceBox, partBox, tvfBox, lfoBox, mapBox;
    juce::TextButton liveBtn { "LIVE" }, sendBtn { "SEND" }, saveBtn { "SAVE" }, loadBtn { "LOAD" };
    juce::TextButton syxBtn { "SYX" }, storeBtn { "STORE" }, hearBtn { "HEAR" }, infoBtn { "INFO" };
    juce::TextButton tvfBtn { "FILTER" }, tvaBtn { "VOLUME" };
    juce::ListBox catList { "categories", nullptr };
    juce::ListBox soundList { "sounds", nullptr };
    PatchInfoOverlay infoOverlay;

    mutable juce::Rectangle<int> chrome, hintR, browserR, cardsR, envR, flowR, plotR, slotStrip, choR, revR, libR;
    mutable juce::Rectangle<int> envHead, envFoot, envLegend, cutR, resR, depR, lfoLabR, lfoComboR, tvfComboR, routeBig;
    mutable juce::Rectangle<int> cardR[4], onBox[4], levelBox[4], tuneBox[4], panBox[4], velBox[4], mfxBox[4];
    mutable juce::Point<float> dot[5];
};
