#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Skin.h"
#include <cmath>

using namespace sd80;

static juce::String scaleTuneToCsv(const std::uint8_t tune[32][12])
{
    juce::String scale;
    for (int p = 0; p < 32; ++p)
        for (int n = 0; n < 12; ++n)
        {
            if (scale.isNotEmpty())
                scale << ",";
            scale << (int) tune[p][n];
        }
    return scale;
}

static void scaleTuneFromCsv(std::uint8_t tune[32][12], const juce::String& csv)
{
    if (csv.isEmpty())
        return;
    const auto scale = juce::StringArray::fromTokens(csv, ",", "");
    int k = 0;
    for (int p = 0; p < 32 && k < scale.size(); ++p)
        for (int n = 0; n < 12 && k < scale.size(); ++n, ++k)
            tune[p][n] = (std::uint8_t) juce::jlimit(0, 127, scale[k].getIntValue());
}

static void writeKeysXml(juce::XmlElement& xml, bool keysOn, int keysVel, int keysOct, int keysCurve,
                          const std::uint8_t tune[32][12])
{
    xml.setAttribute("keysOn", keysOn ? 1 : 0);
    xml.setAttribute("keysVel", keysVel);
    xml.setAttribute("keysOct", keysOct);
    xml.setAttribute("keysCurve", keysCurve);
    xml.setAttribute("scaleTune", scaleTuneToCsv(tune));
}

static void readKeysXml(const juce::XmlElement& xml, bool& keysOn, int& keysVel, int& keysOct, int& keysCurve,
                         std::uint8_t tune[32][12])
{
    if (xml.hasAttribute("keysOn"))
        keysOn = xml.getBoolAttribute("keysOn", keysOn);
    if (xml.hasAttribute("keysVel"))
        keysVel = juce::jlimit(1, 127, xml.getIntAttribute("keysVel", keysVel));
    if (xml.hasAttribute("keysOct"))
        keysOct = juce::jlimit(0, 8, xml.getIntAttribute("keysOct", keysOct));
    if (xml.hasAttribute("keysCurve"))
        keysCurve = juce::jlimit(0, 3, xml.getIntAttribute("keysCurve", keysCurve));
    if (xml.hasAttribute("scaleTune"))
        scaleTuneFromCsv(tune, xml.getStringAttribute("scaleTune"));
}

juce::String ModernEdirolSd80Processor::pid(int part, const char* key)
{
    return "p" + juce::String(part).paddedLeft('0', 2) + "_" + key;
}

juce::AudioProcessorValueTreeState::ParameterLayout ModernEdirolSd80Processor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto addInt = [&](const juce::String& id, const juce::String& name, int min, int max, int def)
    {
        p.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { id, 1 }, name, min, max, def));
    };
    auto addBool = [&](const juce::String& id, const juce::String& name, bool def)
    {
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { id, 1 }, name, def));
    };

    addInt("mode", "Generator Mode", 0, 3, 0); // Native
    addInt("throttle", "USB Throttle ms", 0, 50, 0);
    addInt("reverbType", "Reverb Type", 0, 8, 4);
    addInt("reverbTime", "Reverb Time", 0, 127, 64);
    addInt("chorusType", "Chorus Type", 0, 5, 2);
    addInt("chorusRate", "Chorus Rate", 0, 127, 3);
    addInt("chorusDepth", "Chorus Depth", 0, 127, 19);
    addInt("chorusFeedback", "Chorus Feedback", 0, 127, 8);
    addInt("chorusToRev", "Chorus To Reverb", 0, 127, 0);
    addInt("masterVol", "Master Volume", 0, 127, 127);
    addInt("mfxAType", "MFX A Type", 0, 90, 0);
    addInt("mfxBType", "MFX B Type", 0, 90, 0);
    addInt("mfxCType", "MFX C Type", 0, 90, 0);
    addBool("mfxAOn", "MFX A", false);
    addBool("mfxBOn", "MFX B", false);
    addBool("mfxCOn", "MFX C", false);
    addBool("hostMirrorA", "Host MIDI mirrors Part A", true);
    addBool("hostMirrorB", "Host MIDI mirrors Part B", false);
    addInt("hostRoute", "Host MIDI route", 0, 2, 1); // 0=Follow SEL, 1=Part A as-played, 2=Part B as-played
    for (int slot = 0; slot < 3; ++slot)
        for (int k = 0; k < 4; ++k)
            addInt(mfxParamId(slot, k),
                   juce::String("MFX ") + char('A' + slot) + " P" + juce::String(k + 1),
                   0, 127, 64);

    for (int i = 0; i < 32; ++i)
    {
        const auto n = juce::String(i + 1);
        const bool drumDefault = (i % 16) == 9; // A10 / B10
        addInt(pid(i, "msb"), "P" + n + " MSB", 0, 127, drumDefault ? 104 : 96);
        addInt(pid(i, "lsb"), "P" + n + " LSB", 0, 127, 0);
        addInt(pid(i, "pc"),  "P" + n + " PC",  0, 127, 0);
        addBool(pid(i, "drum"), "P" + n + " Drum", drumDefault);
        addInt(pid(i, "vol"), "P" + n + " Volume", 0, 127, 100);
        addInt(pid(i, "pan"), "P" + n + " Pan", 0, 127, 64);
        addInt(pid(i, "expr"), "P" + n + " Expression", 0, 127, 127);
        addInt(pid(i, "cut"), "P" + n + " Cutoff", 0, 127, 64);
        addInt(pid(i, "res"), "P" + n + " Resonance", 0, 127, 64);
        addInt(pid(i, "atk"), "P" + n + " Attack", 0, 127, 64);
        addInt(pid(i, "dec"), "P" + n + " Decay", 0, 127, 64);
        addInt(pid(i, "rel"), "P" + n + " Release", 0, 127, 64);
        addInt(pid(i, "vr"),  "P" + n + " Vib Rate", 0, 127, 64);
        addInt(pid(i, "vd"),  "P" + n + " Vib Depth", 0, 127, 64);
        addInt(pid(i, "vdl"), "P" + n + " Vib Delay", 0, 127, 64);
        addInt(pid(i, "rev"), "P" + n + " Reverb Send", 0, 127, 40);
        addInt(pid(i, "cho"), "P" + n + " Chorus Send", 0, 127, 0);
        addInt(pid(i, "mfx"), "P" + n + " MFX Send", 0, 127, 0);
        addInt(pid(i, "mfxSel"), "P" + n + " MFX Select", 0, 2, 0);
        addInt(pid(i, "outAsg"), "P" + n + " Output Assign", 0, 2, 0); // 0=MFX so insertion FX can bite
        addInt(pid(i, "portaT"), "P" + n + " Porta Time", 0, 127, 0);
        addBool(pid(i, "porta"), "P" + n + " Porta", false);
        addInt(pid(i, "coarse"), "P" + n + " Coarse", -24, 24, 0);
        addInt(pid(i, "fine"), "P" + n + " Fine", -64, 63, 0);
        addBool(pid(i, "mute"), "P" + n + " Mute", false);
        addBool(pid(i, "solo"), "P" + n + " Solo", false);
        addInt(pid(i, "map"), "P" + n + " Map", 0, 8, 0);
    }

    return { p.begin(), p.end() };
}

ModernEdirolSd80Processor::ModernEdirolSd80Processor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MESD80", createLayout())
{
    startTimerHz(40);

    juce::PropertiesFile::Options opt;
    opt.applicationName = "ModernEdirolSD80";
    opt.filenameSuffix = "settings";
    opt.osxLibrarySubFolder = "Application Support";
    opt.folderName = "CrimsonRedstone";
    appProps.setStorageParameters(opt);
    for (int p = 0; p < 32; ++p)
        for (int n = 0; n < 12; ++n)
            scaleTune[p][n] = 64;
    restoreAppSettings();
    playlistA.setOutputPort(MidiPort::A);
    playlistB.setOutputPort(MidiPort::B);
    autoDetectUsbPorts();
    requestHardwareDump();

    auto listen = [this](const juce::String& id) { apvts.addParameterListener(id, this); };
    listen("mode"); listen("throttle"); listen("reverbType"); listen("reverbTime");
    listen("chorusType"); listen("chorusRate"); listen("chorusDepth"); listen("chorusFeedback");
    listen("chorusToRev"); listen("mfxAType"); listen("mfxBType"); listen("mfxCType");
    listen("mfxAOn"); listen("mfxBOn"); listen("mfxCOn"); listen("masterVol"); listen("hostRoute");
    for (int slot = 0; slot < 3; ++slot)
        for (int k = 0; k < 4; ++k)
            listen(mfxParamId(slot, k));
    for (int i = 0; i < 32; ++i)
    {
        for (auto key : { "msb", "lsb", "pc", "drum", "vol", "pan", "expr", "cut", "res", "atk",
                          "dec", "rel", "vr", "vd", "vdl", "rev", "cho", "mfx", "mfxSel", "outAsg",
                          "portaT", "porta", "mute", "solo", "map", "coarse", "fine" })
            listen(pid(i, key));
    }
}

ModernEdirolSd80Processor::~ModernEdirolSd80Processor()
{
    stopTimer();
    persistAppSettings();
    auto unlisten = [this](const juce::String& id) { apvts.removeParameterListener(id, this); };
    unlisten("mode"); unlisten("throttle"); unlisten("reverbType"); unlisten("reverbTime");
    unlisten("chorusType"); unlisten("chorusRate"); unlisten("chorusDepth"); unlisten("chorusFeedback");
    unlisten("chorusToRev"); unlisten("mfxAType"); unlisten("mfxBType"); unlisten("mfxCType");
    unlisten("mfxAOn"); unlisten("mfxBOn"); unlisten("mfxCOn"); unlisten("masterVol"); unlisten("hostRoute");
    for (int slot = 0; slot < 3; ++slot)
        for (int k = 0; k < 4; ++k)
            unlisten(mfxParamId(slot, k));
    for (int i = 0; i < 32; ++i)
        for (auto key : { "msb", "lsb", "pc", "drum", "vol", "pan", "expr", "cut", "res", "atk",
                          "dec", "rel", "vr", "vd", "vdl", "rev", "cho", "mfx", "mfxSel", "outAsg",
                          "portaT", "porta", "mute", "solo", "map", "coarse", "fine" })
            unlisten(pid(i, key));
    const juce::ScopedLock sl(deviceLock);
    if (midiInA) midiInA->stop();
    if (midiInB) midiInB->stop();
    midiInA.reset();
    midiInB.reset();
    midiOutA.reset();
    midiOutB.reset();
}

void ModernEdirolSd80Processor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate > 0 ? sampleRate : 44100.0;
    throttle.resetTiming();
    throttle.setDelayMs(paramInt("throttle", 0));
}

void ModernEdirolSd80Processor::releaseResources() {}

bool ModernEdirolSd80Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto main = layouts.getMainOutputChannelSet();
    return main == juce::AudioChannelSet::stereo()
        || main == juce::AudioChannelSet::mono();
}

int ModernEdirolSd80Processor::paramInt(const juce::String& id, int fallback) const
{
    if (auto* p = apvts.getRawParameterValue(id))
        return (int) std::lround(p->load());
    return fallback;
}

bool ModernEdirolSd80Processor::paramBool(const juce::String& id) const
{
    return paramInt(id, 0) >= 1;
}

void ModernEdirolSd80Processor::setParamInt(const juce::String& id, int v)
{
    if (isLocked(id))
        return;
    if (auto* p = apvts.getParameter(id))
    {
        const auto ranged = p->getNormalisableRange();
        p->beginChangeGesture();
        p->setValueNotifyingHost(ranged.convertTo0to1((float) v));
        p->endChangeGesture();
    }
}

juce::String ModernEdirolSd80Processor::mfxParamId(int slot0to2, int param0to3)
{
    return juce::String("mfx") + char('A' + juce::jlimit(0, 2, slot0to2))
         + "_p" + juce::String(juce::jlimit(0, 3, param0to3));
}

void ModernEdirolSd80Processor::setSelectedPart(int p)
{
    p = juce::jlimit(0, 31, p);
    selectedPart.store(p);
    visibleGroup.store(p >= 16 ? 1 : 0);
    selectedMask.store(1u << p);
}

bool ModernEdirolSd80Processor::isPartSelected(int p) const
{
    p = juce::jlimit(0, 31, p);
    return (selectedMask.load() & (1u << p)) != 0;
}

void ModernEdirolSd80Processor::togglePartSelected(int p, bool exclusive)
{
    p = juce::jlimit(0, 31, p);
    if (exclusive)
    {
        setSelectedPart(p);
        return;
    }
    auto mask = selectedMask.load();
    mask ^= (1u << p);
    if (mask == 0)
        mask = (1u << p);
    selectedMask.store(mask);
    selectedPart.store(p);
    visibleGroup.store(p >= 16 ? 1 : 0);
}

bool ModernEdirolSd80Processor::isPartSilenced(int part) const
{
    part = juce::jlimit(0, 31, part);
    if (paramBool(pid(part, "mute")))
        return true;
    bool anySolo = false;
    for (int i = 0; i < 32; ++i)
        if (paramBool(pid(i, "solo"))) { anySolo = true; break; }
    return anySolo && ! paramBool(pid(part, "solo"));
}

void ModernEdirolSd80Processor::muteAll(bool shouldMute)
{
    suppressOutgoing = true;
    for (int i = 0; i < 32; ++i)
        setParamInt(pid(i, "mute"), shouldMute ? 1 : 0);
    suppressOutgoing = false;
    for (int i = 0; i < 32; ++i)
        enqueuePartMix(i);
}

void ModernEdirolSd80Processor::unsoloAll()
{
    suppressOutgoing = true;
    for (int i = 0; i < 32; ++i)
        setParamInt(pid(i, "solo"), 0);
    suppressOutgoing = false;
    for (int i = 0; i < 32; ++i)
        enqueuePartMix(i);
}

void ModernEdirolSd80Processor::autoDetectUsbPorts()
{
    auto isSd80 = [](const juce::String& name)
    {
        auto n = name.toLowerCase();
        return n.contains("sd-80") || n.contains("sd80") || n.contains("edirol")
            || n.contains("studio canvas") || n.contains("studio-canvas");
    };

    if (outAName.isEmpty() || outBName.isEmpty())
    {
        auto devices = juce::MidiOutput::getAvailableDevices();
        juce::Array<int> hits;
        for (int i = 0; i < devices.size(); ++i)
            if (isSd80(devices[i].name))
                hits.add(i);
        if (hits.size() > 0 && outAName.isEmpty())
            setMidiOutputA(hits[0]);
        if (outBName.isEmpty())
        {
            if (hits.size() >= 2)
                setMidiOutputB(hits[1]);
            else if (hits.size() == 1 && hits[0] + 1 < devices.size())
                setMidiOutputB(hits[0] + 1);
        }
    }

    auto ins = juce::MidiInput::getAvailableDevices();
    juce::Array<int> inHits;
    for (int i = 0; i < ins.size(); ++i)
        if (isSd80(ins[i].name))
            inHits.add(i);
    if (inHits.size() > 0 && inAName.isEmpty())
        openNamedInput(midiInA, inAName, inAIndex, ins[inHits[0]].name);
    if (inBName.isEmpty())
    {
        if (inHits.size() >= 2)
            openNamedInput(midiInB, inBName, inBIndex, ins[inHits[1]].name);
        else if (inHits.size() == 1 && inHits[0] + 1 < ins.size())
            openNamedInput(midiInB, inBName, inBIndex, ins[inHits[0] + 1].name);
    }
}

void ModernEdirolSd80Processor::sendMasterVolume()
{
    throttle.push(gmMasterVolume(paramInt("masterVol", 127)), MidiPort::Both);
}

void ModernEdirolSd80Processor::refillPlaylistQueueUnlocked()
{
    playlistQueue.clear();
    for (int i = 0; i < playlistLibrary.size(); ++i)
        playlistQueue.add(playlistLibrary.getReference(i));
}

void ModernEdirolSd80Processor::applyPlaylistLoopToEngines()
{
    const int mode = playlistLoopMode.load();
    playlistA.setLooping(mode == 2 && playlistA.isLoaded() && ! playlistSpentA.load());
    const bool bLoop = partBOn.load() && playlistB.isLoaded() && mode != 0;
    playlistB.setLooping(bLoop);
}

void ModernEdirolSd80Processor::playlistArm(int side)
{
    side = side ? 1 : 0;
    juce::File f;
    bool didRefill = false;
    for (;;)
    {
        const juce::ScopedLock sl(playlistLock);
        if (playlistQueue.isEmpty() && ! didRefill && playlistLoopMode.load() == 1
            && playlistLibrary.size() > 0)
        {
            refillPlaylistQueueUnlocked();
            didRefill = true;
        }
        if (playlistQueue.isEmpty())
        {
            if (side)
            {
                playlistB.unload();
                playlistSpentB.store(true);
            }
            else
            {
                playlistA.unload();
                playlistSpentA.store(true);
            }
            return;
        }
        f = playlistQueue.getFirst();
        playlistQueue.remove(0);
        if (f.existsAsFile())
            break;
    }
    applyMidiFile(f, side);
    if (side)
    {
        playlistB.setOutputPort(MidiPort::B);
        playlistB.load(f);
        playlistSpentB.store(false);
    }
    else
    {
        playlistA.setOutputPort(MidiPort::A);
        playlistA.load(f);
        playlistSpentA.store(false);
    }
    applyPlaylistLoopToEngines();
}

void ModernEdirolSd80Processor::playlistAdd(const juce::File& f)
{
    if (! (f.hasFileExtension(".mid") || f.hasFileExtension(".midi")))
        return;
    if (! f.existsAsFile())
        return;
    {
        const juce::ScopedLock sl(playlistLock);
        playlistLibrary.add(f);
        playlistQueue.add(f);
    }
    playlistOwnsTransport.store(true);
    player.stop();
    if (! playlistA.isLoaded())
        playlistArm(0);
}

void ModernEdirolSd80Processor::loadPartB(const juce::File& f)
{
    if (! (f.hasFileExtension(".mid") || f.hasFileExtension(".midi")))
        return;
    if (! f.existsAsFile())
        return;
    applyMidiFile(f, 1);
    playlistB.setOutputPort(MidiPort::B);
    playlistB.load(f);
    playlistSpentB.store(false);
    applyPlaylistLoopToEngines();
    if (partBOn.load() && (playlistA.isPlaying() || player.isPlaying()))
        playlistB.play();
}

void ModernEdirolSd80Processor::setPartBEnabled(bool on)
{
    partBOn.store(on);
    if (! on)
    {
        if (playlistB.isPlaying())
            playlistB.stop();
        return;
    }
    applyPlaylistLoopToEngines();
    if (playlistB.isLoaded() && (playlistA.isPlaying() || player.isPlaying()))
    {
        playlistSpentB.store(false);
        playlistB.play();
    }
}

void ModernEdirolSd80Processor::partBFollow(int cmd)
{
    if (cmd == 0 || ! partBOn.load() || ! playlistB.isLoaded())
    {
        if (cmd == 0 && playlistB.isPlaying())
            playlistB.stop();
        return;
    }
    if (cmd == 2)
    {
        if (playlistB.isPlaying())
            playlistB.pause();
        return;
    }
    playlistB.setOutputPort(MidiPort::B);
    playlistSpentB.store(false);
    applyPlaylistLoopToEngines();
    playlistB.play();
}

void ModernEdirolSd80Processor::playlistPlay()
{
    daw.stop();
    player.stop();
    playlistOwnsTransport.store(true);

    if (playlistPaused.load())
    {
        const bool aMid = playlistA.isLoaded() && ! playlistSpentA.load()
            && playlistA.getPosition() > 0.05
            && playlistA.getPosition() + 0.08 < playlistA.getLength();
        const bool bMid = partBOn.load() && playlistB.isLoaded() && ! playlistSpentB.load()
            && playlistB.getPosition() > 0.05
            && playlistB.getPosition() + 0.08 < playlistB.getLength();
        if (aMid || bMid)
        {
            playlistPaused.store(false);
            playlistActive.store(true);
            applyPlaylistLoopToEngines();
            if (aMid)
                playlistA.play();
            if (bMid)
                playlistB.play();
            return;
        }
        playlistPaused.store(false);
    }

    if (! playlistA.isLoaded())
    {
        {
            const juce::ScopedLock sl(playlistLock);
            refillPlaylistQueueUnlocked();
        }
        playlistArm(0);
    }
    playlistA.stop();
    if (! playlistA.isLoaded() && ! (partBOn.load() && playlistB.isLoaded()))
        return;
    playlistActive.store(true);
    playlistPaused.store(false);
    applyPlaylistLoopToEngines();
    if (playlistA.isLoaded())
    {
        playlistSpentA.store(false);
        playlistA.play();
    }
    if (partBOn.load() && playlistB.isLoaded())
        playlistB.stop();
    partBFollow(1);
}

void ModernEdirolSd80Processor::playlistPause()
{
    if (! playlistA.isPlaying() && ! playlistB.isPlaying() && ! player.isPlaying())
        return;
    playlistActive.store(false);
    playlistPaused.store(true);
    if (playlistA.isPlaying())
        playlistA.pause();
    if (playlistB.isPlaying())
        playlistB.pause();
}

void ModernEdirolSd80Processor::playlistStop()
{
    playlistActive.store(false);
    playlistPaused.store(false);
    playlistHeard.store(false);
    playlistA.stop();
    playlistB.stop();
}

void ModernEdirolSd80Processor::disarmPlaylist()
{
    playlistStop();
    playlistOwnsTransport.store(false);
    playlistHeard.store(false);
}

void ModernEdirolSd80Processor::dawPlay()
{
    player.stop();
    playlistStop();
    const auto song = daw.copySong();
    if (daw.isSongMode())
    {
        for (int p = 0; p < 32; ++p)
            if (! song.tracks[p].clips.empty())
            {
                enqueuePartPatch(p);
                enqueuePartMix(p);
                enqueuePartDeep(p);
            }
    }
    const int t = daw.getEditTrack();
    enqueuePartPatch(t);
    enqueuePartMix(t);
    enqueuePartDeep(t);
    sendMasterVolume();
    studioHold = true;
    daw.play();
    syncStudioDisplay();
}

void ModernEdirolSd80Processor::dawPause()
{
    studioHold = true;
    daw.pause();
    syncStudioDisplay();
}

void ModernEdirolSd80Processor::dawStop()
{
    studioHold = false;
    daw.stop();
    syncStudioDisplay();
}

void ModernEdirolSd80Processor::dawPreview(int part, int pitch, bool on, int velocity)
{
    part = juce::jlimit(0, 31, part);
    pitch = juce::jlimit(0, 127, pitch);
    const int ch = channelForPart(part);
    const MidiPort port = portForPart(part);
    if (on)
        throttle.push(juce::MidiMessage::noteOn(ch, pitch, (juce::uint8) shapedVelocity(velocity)), port);
    else
        throttle.push(juce::MidiMessage::noteOff(ch, pitch), port);
    daw.recordLive(part, on, pitch, on ? shapedVelocity(velocity) : 0);
}

int ModernEdirolSd80Processor::shapedVelocity(int velocity) const
{
    const int v = juce::jlimit(1, 127, velocity);
    if (keysCurve == 3)
        return juce::jlimit(1, 127, screenKeysVel);
    if (keysCurve == 1)
    {
        const float u = (float) v / 127.0f;
        return juce::jlimit(1, 127, (int) std::lround(u * u * 127.0));
    }
    if (keysCurve == 2)
    {
        const float u = (float) v / 127.0f;
        return juce::jlimit(1, 127, (int) std::lround(std::sqrt(u) * 127.0));
    }
    return v;
}

void ModernEdirolSd80Processor::setKeysCurve(int v)
{
    v = juce::jlimit(0, 3, v);
    if (v == keysCurve)
        return;
    keysCurve = v;
    persistAppSettings();
}

int ModernEdirolSd80Processor::getScaleTune(int part, int semitone) const
{
    part = juce::jlimit(0, 31, part);
    semitone = juce::jlimit(0, 11, semitone);
    return scaleTune[part][semitone];
}

void ModernEdirolSd80Processor::setScaleTune(int part, int semitone, int cents64)
{
    part = juce::jlimit(0, 31, part);
    semitone = juce::jlimit(0, 11, semitone);
    scaleTune[part][semitone] = (std::uint8_t) juce::jlimit(0, 127, cents64);
    throttle.push(sd80::gm2ScaleOctave(channelForPart(part), scaleTune[part]), portForPart(part));
    persistAppSettings();
}

void ModernEdirolSd80Processor::resetScaleTune(int part)
{
    part = juce::jlimit(0, 31, part);
    for (int n = 0; n < 12; ++n)
        scaleTune[part][n] = 64;
    throttle.push(sd80::gm2ScaleOctave(channelForPart(part), scaleTune[part]), portForPart(part));
    persistAppSettings();
}

void ModernEdirolSd80Processor::setScreenKeysOn(bool v)
{
    screenKeysOn = v;
    persistAppSettings();
}

void ModernEdirolSd80Processor::setScreenKeysVel(int v)
{
    v = juce::jlimit(1, 127, v);
    if (v == screenKeysVel)
        return;
    screenKeysVel = v;
    persistAppSettings();
}

void ModernEdirolSd80Processor::setScreenKeysOct(int v)
{
    screenKeysOct = juce::jlimit(0, 8, v);
    persistAppSettings();
}

bool ModernEdirolSd80Processor::dawSaveSong(const juce::File& file)
{
    auto xml = daw.toXml();
    if (! xml)
        return false;
    if (file.replaceWithText(xml->toString()))
    {
        persistAppSettings();
        return true;
    }
    return false;
}

bool ModernEdirolSd80Processor::dawLoadSong(const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || ! daw.fromXml(*xml))
        return false;
    persistAppSettings();
    return true;
}

bool ModernEdirolSd80Processor::dawImportMidi(const juce::File& file, int track)
{
    if (! daw.importMidi(file, track))
        return false;
    applyMidiFile(file); // mixer patches + CCs, then SYNC
    persistAppSettings();
    return true;
}

bool ModernEdirolSd80Processor::dawExportMidi(const juce::File& fileA)
{
    auto fileB = fileA.getSiblingFile(fileA.getFileNameWithoutExtension() + "-B.mid");
    return daw.exportMidi(fileA, fileB, [this](int part, juce::MidiMessageSequence& seq)
    {
        const int ch = channelForPart(part);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 0, paramInt(pid(part, "msb"))), 0);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 32, paramInt(pid(part, "lsb"))), 0);
        seq.addEvent(juce::MidiMessage::programChange(ch, paramInt(pid(part, "pc"))), 0);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 7, paramInt(pid(part, "vol"), 100)), 0);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 10, paramInt(pid(part, "pan"), 64)), 0);
    });
}

void ModernEdirolSd80Processor::playlistClear()
{
    playlistStop();
    playlistOwnsTransport.store(false);
    playlistLoopMode.store(0);
    {
        const juce::ScopedLock sl(playlistLock);
        playlistQueue.clear();
        playlistLibrary.clear();
    }
    playlistA.unload();
    playlistB.unload();
    playlistSpentA.store(true);
    playlistSpentB.store(true);
    partBOn.store(false);
    applyPlaylistLoopToEngines();
}

void ModernEdirolSd80Processor::playlistService()
{
    if (! playlistOwnsTransport.load())
    {
        playlistNeedArm.store(0);
        playlistNeedPlay.store(false);
        return;
    }
    if (! playlistA.isLoaded())
    {
        playlistArm(0);
        playlistNeedArm.fetch_and(~1);
    }

    const int need = playlistNeedArm.exchange(0);
    if ((need & 1) != 0)
        playlistArm(0);

    if (playlistNeedPlay.exchange(false) && playlistOwnsTransport.load())
    {
        if (! playlistA.isPlaying() && playlistA.isLoaded() && ! playlistSpentA.load())
        {
            applyPlaylistLoopToEngines();
            playlistActive.store(true);
            playlistA.play();
        }
        else if (! playlistA.isLoaded() && ! playlistB.isPlaying())
            playlistActive.store(false);
    }
}

bool ModernEdirolSd80Processor::playlistIsArmed() const
{
    if (! playlistOwnsTransport.load())
        return false;
    if (playlistA.isLoaded() || playlistB.isLoaded())
        return true;
    const juce::ScopedLock sl(playlistLock);
    return playlistQueue.size() > 0 || playlistLibrary.size() > 0;
}

int ModernEdirolSd80Processor::cyclePlaylistLoop()
{
    int m = playlistLoopMode.load();
    m = (m + 1) % 3;
    playlistLoopMode.store(m);
    applyPlaylistLoopToEngines();
    return m;
}

MidiPlayerEngine& ModernEdirolSd80Processor::displayEngine()
{
    if (studioFollowing)
        return studioView;
    if (playlistOwnsTransport.load())
    {
        if (playlistA.isLoaded())
            return playlistA;
        if (partBOn.load() && playlistB.isLoaded())
            return playlistB;
    }
    return player;
}

int ModernEdirolSd80Processor::displayPartGroup()
{
    if (studioFollowing)
        return studioSide;
    return (&displayEngine() == &playlistB) ? 1 : 0;
}

void ModernEdirolSd80Processor::applyListedPatch(int part, int map, bool drum, int msb, int lsb, int pc)
{
    part = juce::jlimit(0, 31, part);
    const bool was = suppressOutgoing.load();
    suppressOutgoing = true;
    setParamInt(pid(part, "map"), juce::jlimit(0, 8, map));
    setParamInt(pid(part, "drum"), drum ? 1 : 0);
    setParamInt(pid(part, "msb"), juce::jlimit(0, 127, msb));
    setParamInt(pid(part, "lsb"), juce::jlimit(0, 127, lsb));
    setParamInt(pid(part, "pc"), juce::jlimit(0, 127, pc));
    suppressOutgoing = was;
    enqueuePartPatch(part);
}

void ModernEdirolSd80Processor::syncStudioDisplay()
{
    const bool playing = daw.isPlaying();
    const double posTick = daw.getPosTick();
    if (! playing && ! studioHold && posTick < 1.0)
    {
        if (studioFollowing)
        {
            studioFollowing = false;
            studioView.unload();
            studioSeenGen = -1;
            studioSeenMode = -1;
            studioSeenEdit = -1;
            studioSeenPat = -1;
            studioSeenBpm = -1;
            studioSeenSec = -1.0;
        }
        return;
    }
    if (! playing && ! studioFollowing)
        return;

    const double bpm = juce::jmax(1.0, daw.getBpm());
    const double secPerTick = 60.0 / (bpm * (double) kDawPpq);
    const double posSec = juce::jmax(0.0, posTick * secPerTick);

    // Pause keeps the notes and the keys that were already on screen.
    if (! playing && studioFollowing)
    {
        studioView.setDisplayClock(posSec, false);
        return;
    }

    const int gen = daw.getGen();
    const int mode = daw.isSongMode() ? 1 : 0;
    const int edit = daw.getEditTrack();
    const int pat = daw.getPatternIndex();
    const int bpm10 = (int) std::llround(bpm * 10.0);
    const bool windowMoved = std::abs(posSec - studioSeenSec) > 2.0;
    const bool rebuild = ! studioFollowing
                      || gen != studioSeenGen
                      || mode != studioSeenMode
                      || edit != studioSeenEdit
                      || pat != studioSeenPat
                      || bpm10 != studioSeenBpm
                      || windowMoved;

    studioFollowing = true;
    if (! rebuild)
    {
        studioView.setDisplayClock(posSec, playing);
        return;
    }

    studioSeenGen = gen;
    studioSeenMode = mode;
    studioSeenEdit = edit;
    studioSeenPat = pat;
    studioSeenBpm = bpm10;
    studioSeenSec = posSec;

    const auto song = daw.copySong();

    auto sideHasNotes = [&song](int side) -> bool
    {
        const int a = side * 16;
        for (int p = a; p < a + 16; ++p)
        {
            for (const auto& c : song.tracks[p].clips)
            {
                if (c.pattern < 0 || c.pattern >= (int) song.patterns.size())
                    continue;
                if (! song.patterns[(size_t) c.pattern].notes.empty())
                    return true;
            }
        }
        return false;
    };

    if (mode == 0)
        studioSide = edit < 16 ? 0 : 1;
    else
    {
        const int prefer = edit < 16 ? 0 : 1;
        if (! sideHasNotes(prefer) && sideHasNotes(1 - prefer))
            studioSide = 1 - prefer;
        else
            studioSide = prefer;
    }

    const int tick0 = (int) std::floor((posSec - 0.6) / secPerTick);
    const int tick1 = (int) std::ceil((posSec + 10.0) / secPerTick);
    std::vector<PlayerNote> notes;
    notes.reserve(512);
    int lo = 127;
    int hi = 0;
    std::uint32_t mask = 0;
    auto pushNote = [&](int tickOn, int tickOff, int pitch, int channel, int vel)
    {
        if (tickOff < tick0 || tickOn > tick1)
            return;
        if ((int) notes.size() >= 8000)
            return;
        pitch = juce::jlimit(0, 127, pitch);
        channel = juce::jlimit(1, 16, channel);
        PlayerNote pn;
        pn.startSec = (double) tickOn * secPerTick;
        pn.endSec = (double) juce::jmax(tickOn + 1, tickOff) * secPerTick;
        if (pn.endSec < pn.startSec + 0.02)
            pn.endSec = pn.startSec + 0.02;
        pn.note = pitch;
        pn.channel = channel;
        pn.velocity = juce::jlimit(1, 127, vel);
        notes.push_back(pn);
        lo = juce::jmin(lo, pitch);
        hi = juce::jmax(hi, pitch);
        mask |= (1u << (channel - 1));
    };

    if (mode == 0)
    {
        if (pat >= 0 && pat < (int) song.patterns.size())
        {
            const auto& pattern = song.patterns[(size_t) pat];
            const int ch = (edit % 16) + 1;
            const int plen = juce::jmax(1, pattern.length);
            for (const auto& n : pattern.notes)
            {
                if (n.start < 0 || n.start >= plen)
                    continue;
                pushNote(n.start, n.start + juce::jmax(1, n.dur), n.pitch, ch, n.vel);
            }
        }
    }
    else
    {
        const int a = studioSide * 16;
        for (int p = a; p < a + 16; ++p)
        {
            const int ch = (p % 16) + 1;
            for (const auto& c : song.tracks[p].clips)
            {
                if (c.pattern < 0 || c.pattern >= (int) song.patterns.size())
                    continue;
                const auto& pattern = song.patterns[(size_t) c.pattern];
                const int plen = juce::jmax(1, pattern.length);
                const int span = DawEngine::clipSpan(c, pattern);
                const int clipEnd = c.start + juce::jmax(1, span);
                for (const auto& n : pattern.notes)
                {
                    if (n.start < 0 || n.start >= plen)
                        continue;
                    for (int loop = 0; loop * plen < span; ++loop)
                    {
                        const int on = c.start + loop * plen + n.start;
                        if (on >= clipEnd)
                            break;
                        pushNote(on, juce::jmin(on + juce::jmax(1, n.dur), clipEnd), n.pitch, ch, n.vel);
                        if ((int) notes.size() >= 8000)
                            break;
                    }
                    if ((int) notes.size() >= 8000)
                        break;
                }
            }
        }
    }

    if (notes.empty())
    {
        lo = 36;
        hi = 84;
    }

    int endTick = song.loopEnd;
    for (int t = 0; t < kDawTracks; ++t)
    {
        for (const auto& c : song.tracks[t].clips)
        {
            int len = kDawBar;
            if (c.pattern >= 0 && c.pattern < (int) song.patterns.size())
                len = DawEngine::clipSpan(c, song.patterns[(size_t) c.pattern]);
            endTick = juce::jmax(endTick, c.start + len);
        }
    }
    if (mode == 0 && pat >= 0 && pat < (int) song.patterns.size())
        endTick = juce::jmax(endTick, song.patterns[(size_t) pat].length);
    endTick = juce::jmax(kDawBar, endTick);

    juce::String title("STUDIO");
    if (mode == 0 && pat >= 0 && pat < (int) song.patterns.size())
        title << "  " << song.patterns[(size_t) pat].name;
    else if (song.title.isNotEmpty())
        title << "  " << song.title;

    studioView.setDisplayScore(notes, (double) endTick * secPerTick, title, lo, hi, mask);
    studioView.setDisplayClock(posTick * secPerTick, playing);
}

bool ModernEdirolSd80Processor::playlistSlotLoaded(int side) const
{
    return side ? playlistB.isLoaded() : playlistA.isLoaded();
}

bool ModernEdirolSd80Processor::playlistSlotPlaying(int side) const
{
    return side ? playlistB.isPlaying() : playlistA.isPlaying();
}

bool ModernEdirolSd80Processor::playlistSlotSpent(int side) const
{
    return side ? playlistSpentB.load() : playlistSpentA.load();
}

juce::String ModernEdirolSd80Processor::playlistSlotName(int side) const
{
    return side ? playlistB.getName() : playlistA.getName();
}

juce::StringArray ModernEdirolSd80Processor::playlistQueueNames() const
{
    const juce::ScopedLock sl(playlistLock);
    juce::StringArray names;
    for (int i = 0; i < playlistQueue.size(); ++i)
        names.add(playlistQueue.getReference(i).getFileName());
    return names;
}

juce::String ModernEdirolSd80Processor::playlistStatus() const
{
    juce::String loop;
    const int mode = playlistLoopMode.load();
    if (mode == 1)
        loop = "  |  LOOP ALL";
    else if (mode == 2)
        loop = "  |  LOOP 1";

    if (playlistActive.load())
    {
        juce::String s = playlistA.isPlaying() ? "Part A playing" : "Playlist";
        if (partBOn.load() && playlistB.isPlaying())
            s << "  +  Part B";
        else if (partBOn.load() && playlistB.isLoaded())
            s << "  |  Part B on";
        return s + loop;
    }
    if (playlistPaused.load())
        return juce::String("Paused") + loop;
    if (playlistA.isLoaded() || playlistQueue.size() > 0 || playlistLibrary.size() > 0)
        return juce::String("Armed. PLAY starts Part A. Part B is a second file.") + loop;
    if (partBOn.load() && playlistB.isLoaded())
        return juce::String("Part B is on. Add files to arm Part A.") + loop;
    return "Drop .mid files to arm Part A. Part B plays a second file with it.";
}

void ModernEdirolSd80Processor::pullFromHardware()
{
    requestHardwareDump();
}

void ModernEdirolSd80Processor::setSkipFxResetWarning(bool v)
{
    skipFxWarn = v;
    persistAppSettings();
}

void ModernEdirolSd80Processor::resetEffectsToDefault()
{
    suppressOutgoing = true;
    setParamInt("reverbType", 4);
    setParamInt("reverbTime", 64);
    setParamInt("chorusType", 2);
    setParamInt("chorusRate", 3);
    setParamInt("chorusDepth", 19);
    setParamInt("chorusFeedback", 8);
    setParamInt("chorusToRev", 0);
    setParamInt("mfxAType", 0);
    setParamInt("mfxBType", 0);
    setParamInt("mfxCType", 0);
    setParamInt("mfxAOn", 0);
    setParamInt("mfxBOn", 0);
    setParamInt("mfxCOn", 0);
    for (int slot = 0; slot < 3; ++slot)
        for (int k = 0; k < 4; ++k)
            setParamInt(mfxParamId(slot, k), 64);
    for (int i = 0; i < 32; ++i)
    {
        setParamInt(pid(i, "rev"), 40);
        setParamInt(pid(i, "cho"), 0);
        setParamInt(pid(i, "mfx"), 0);
        setParamInt(pid(i, "mfxSel"), 0);
        setParamInt(pid(i, "outAsg"), 0);
    }
    suppressOutgoing = false;
    syncHardwarePush();
}

void ModernEdirolSd80Processor::factoryResetHardware()
{
    lockedIds.clear();
    skipFxWarn = false;
    skinIndex = 0;
    keysCurve = 0;
    for (int p = 0; p < 32; ++p)
        for (int n = 0; n < 12; ++n)
            scaleTune[p][n] = 64;
    selectedPart.store(0);
    visibleGroup.store(0);
    selectedMask.store(1u);
    suppressOutgoing = true;
    for (auto* p : getParameters())
        if (p != nullptr)
            p->setValueNotifyingHost(p->getDefaultValue());
    suppressOutgoing = false;

    for (int ch = 1; ch <= 16; ++ch)
    {
        throttle.push(juce::MidiMessage::allNotesOff(ch), MidiPort::Both);
        throttle.push(juce::MidiMessage::allSoundOff(ch), MidiPort::Both);
        throttle.push(juce::MidiMessage::controllerEvent(ch, 121, 0), MidiPort::Both);
    }
    throttle.push(nativeOn(), MidiPort::Both);
    throttle.push(gsReset(), MidiPort::Both);
    throttle.push(nativeOn(), MidiPort::Both);
    persistAppSettings();
    syncHardwarePush();
}

void ModernEdirolSd80Processor::silenceForQuit()
{
    player.stop();
    playlistStop();
    daw.stop();

    const juce::ScopedLock sl(deviceLock);
    juce::MidiOutput* ports[2] = { midiOutA.get(), midiOutB.get() };
    for (int i = 0; i < 2; ++i)
    {
        juce::MidiOutput* out = ports[i];
        if (out == nullptr)
            continue;
        for (int ch = 1; ch <= 16; ++ch)
        {
            out->sendMessageNow(juce::MidiMessage::allNotesOff(ch));
            out->sendMessageNow(juce::MidiMessage::allSoundOff(ch));
            out->sendMessageNow(juce::MidiMessage::controllerEvent(ch, 121, 0));
        }
    }
}

bool ModernEdirolSd80Processor::isLocked(const juce::String& paramId) const
{
    return lockedIds.contains(paramId);
}

void ModernEdirolSd80Processor::setLocked(const juce::String& paramId, bool shouldLock)
{
    if (paramId.isEmpty())
        return;
    if (shouldLock)
    {
        if (! lockedIds.contains(paramId))
            lockedIds.add(paramId);
    }
    else
    {
        lockedIds.removeString(paramId);
    }
    persistAppSettings();
}

juce::StringArray ModernEdirolSd80Processor::getLockedIds() const
{
    return lockedIds;
}

void ModernEdirolSd80Processor::loadUserLibraryXml(const juce::XmlElement& lib)
{
    userSlot = juce::jlimit(0, kUserPatchSlots - 1, lib.getIntAttribute("slot", userSlot));
    for (int i = 0; i < kUserPatchSlots; ++i)
        userSlots[(size_t) i] = {};
    for (auto* c = lib.getFirstChildElement(); c != nullptr; c = c->getNextElement())
    {
        if (! c->hasTagName("mesd80patch"))
            continue;
        const int i = c->getIntAttribute("slot", -1);
        if (i < 0 || i >= kUserPatchSlots)
            continue;
        userSlots[(size_t) i].used = true;
        userSlots[(size_t) i].patch = readPatchXml(*c);
    }
}

void ModernEdirolSd80Processor::setUserPatch(const UserPatch& p, bool persist)
{
    userPatch = p;
    if (persist)
        persistAppSettings();
}

void ModernEdirolSd80Processor::sendUserPatch(PatchPush what, int tone, bool withBase)
{
    const auto msgs = userPatchMessages(userPatch, what, tone, withBase);
    const auto port = portForPart(juce::jlimit(0, 31, userPatch.part));
    for (const auto& m : msgs)
        throttle.push(m, port);
}

bool ModernEdirolSd80Processor::saveUserPatchFile(const juce::File& file)
{
    juce::XmlElement xml("mesd80patch");
    writePatchXml(xml, userPatch);
    return xml.writeTo(file);
}

bool ModernEdirolSd80Processor::loadUserPatchFile(const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || ! xml->hasTagName("mesd80patch"))
        return false;
    userPatch = readPatchXml(*xml);
    persistAppSettings();
    return true;
}

bool ModernEdirolSd80Processor::exportUserPatchSyx(const juce::File& file)
{
    UserPatch copy = userPatch;
    if (copy.target == (int) PatchTarget::SD20)
        copy.target = (int) PatchTarget::SD80;
    const auto msgs = userPatchMessages(copy, PatchPush::All, 0, true);
    juce::MemoryOutputStream out;
    for (const auto& m : msgs)
        if (m.isSysEx())
            out.write(m.getRawData(), (size_t) m.getRawDataSize());
    if (out.getDataSize() == 0)
        return false;
    return file.replaceWithData(out.getData(), out.getDataSize());
}

void ModernEdirolSd80Processor::storeUserSlot(int index)
{
    index = juce::jlimit(0, kUserPatchSlots - 1, index);
    userSlot = index;
    userSlots[(size_t) index].used = true;
    userSlots[(size_t) index].patch = userPatch;
    persistAppSettings();
}

void ModernEdirolSd80Processor::recallUserSlot(int index)
{
    index = juce::jlimit(0, kUserPatchSlots - 1, index);
    userSlot = index;
    if (! userSlots[(size_t) index].used)
        return;
    userPatch = userSlots[(size_t) index].patch;
    persistAppSettings();
}

bool ModernEdirolSd80Processor::userSlotUsed(int index) const
{
    if (index < 0 || index >= kUserPatchSlots)
        return false;
    return userSlots[(size_t) index].used;
}

void ModernEdirolSd80Processor::setSkinIndex(int i)
{
    i = juce::jlimit(0, kNumSkins - 1, i);
    const bool changed = (i != skinIndex);
    skinIndex = i;
    persistAppSettings();
    if (changed && onSkinChanged)
        juce::MessageManager::callAsync([this] { if (onSkinChanged) onSkinChanged(); });
}

void ModernEdirolSd80Processor::setAeternaMode(int mode)
{
    mode = juce::jlimit(0, 1, mode);
    const bool changed = (mode != aeternaMode);
    aeternaMode = mode;
    persistAppSettings();
    if (changed && onAeternaChanged)
        juce::MessageManager::callAsync([this] { if (onAeternaChanged) onAeternaChanged(); });
}

void ModernEdirolSd80Processor::persistAppSettings()
{
    if (auto* f = appProps.getUserSettings())
    {
        f->setValue("skin", skinIndex);
        f->setValue("aeterna", aeternaMode);
        f->setValue("keysOn", screenKeysOn);
        f->setValue("keysVel", screenKeysVel);
        f->setValue("keysOct", screenKeysOct);
        f->setValue("keysCurve", keysCurve);
        f->setValue("scaleTune", scaleTuneToCsv(scaleTune));
        f->setValue("locks", lockedIds.joinIntoString(","));
        f->setValue("skipFxWarn", skipFxWarn);
        f->setValue("routeAsPlayed", true);
        if (auto xml = apvts.copyState().createXml())
            f->setValue("sessionXml", xml->toString());
        if (auto d = daw.toXml())
            f->setValue("dawXml", d->toString());
        {
            juce::XmlElement patchXml("mesd80patch");
            writePatchXml(patchXml, userPatch);
            f->setValue("userPatchXml", patchXml.toString());
            juce::XmlElement lib("mesd80lib");
            lib.setAttribute("slot", userSlot);
            for (int i = 0; i < kUserPatchSlots; ++i)
            {
                if (! userSlots[(size_t) i].used)
                    continue;
                auto* c = lib.createNewChildElement("mesd80patch");
                c->setAttribute("slot", i);
                writePatchXml(*c, userSlots[(size_t) i].patch);
            }
            f->setValue("userLibXml", lib.toString());
        }
        f->saveIfNeeded();
    }
}

void ModernEdirolSd80Processor::restoreAppSettings()
{
    if (auto* f = appProps.getUserSettings())
    {
        skinIndex = juce::jlimit(0, kNumSkins - 1, f->getIntValue("skin", 0));
        aeternaMode = juce::jlimit(0, 1, f->getIntValue("aeterna", 1));
        screenKeysOn = f->getBoolValue("keysOn", false);
        screenKeysVel = juce::jlimit(1, 127, f->getIntValue("keysVel", 100));
        screenKeysOct = juce::jlimit(0, 8, f->getIntValue("keysOct", 4));
        keysCurve = juce::jlimit(0, 3, f->getIntValue("keysCurve", 0));
        scaleTuneFromCsv(scaleTune, f->getValue("scaleTune"));
        lockedIds = juce::StringArray::fromTokens(f->getValue("locks"), ",", "");
        lockedIds.trim();
        lockedIds.removeEmptyStrings();
        skipFxWarn = f->getBoolValue("skipFxWarn", false);
        const auto sess = f->getValue("sessionXml");
        if (sess.isNotEmpty())
        {
            if (auto xml = juce::XmlDocument::parse(sess))
            {
                auto tree = juce::ValueTree::fromXml(*xml);
                if (tree.isValid())
                {
                    suppressOutgoing = true;
                    apvts.replaceState(tree);
                    suppressOutgoing = false;
                }
            }
        }
        // v1.4 stored Follow SEL as 0 (including old "Part A" sessions).
        // Once per settings file, put Live MIDI back on Part A as-played.
        if (! f->getBoolValue("routeAsPlayed", false))
        {
            if (paramInt("hostRoute", 0) == 0)
                setParamInt("hostRoute", 1);
            f->setValue("routeAsPlayed", true);
            if (auto xml = apvts.copyState().createXml())
                f->setValue("sessionXml", xml->toString());
            f->saveIfNeeded();
        }
        // v1.6.6: USB throttle defaults to off. Old sessions stored 30 ms.
        if (! f->getBoolValue("throttleOffDefault", false))
        {
            setParamInt("throttle", 0);
            f->setValue("throttleOffDefault", true);
            if (auto xml = apvts.copyState().createXml())
                f->setValue("sessionXml", xml->toString());
            f->saveIfNeeded();
        }
        const auto dawS = f->getValue("dawXml");
        if (dawS.isNotEmpty())
            daw.fromXmlString(dawS);
        const auto patchS = f->getValue("userPatchXml");
        if (patchS.isNotEmpty())
            if (auto xml = juce::XmlDocument::parse(patchS))
                if (xml->hasTagName("mesd80patch"))
                    userPatch = readPatchXml(*xml);
        const auto libS = f->getValue("userLibXml");
        if (libS.isNotEmpty())
            if (auto lib = juce::XmlDocument::parse(libS))
                loadUserLibraryXml(*lib);
    }
}

sd80::GeneratorMode ModernEdirolSd80Processor::getGeneratorMode() const
{
    return static_cast<sd80::GeneratorMode>(juce::jlimit(0, 3, paramInt("mode")));
}

void ModernEdirolSd80Processor::setGeneratorMode(sd80::GeneratorMode m)
{
    setParamInt("mode", (int) m);
    throttle.push(modeMessage(m), MidiPort::Both);
    if (m == GeneratorMode::Native || m == GeneratorMode::GM2)
    {
        for (int part = 0; part < 32; ++part)
            enqueuePartPatch(part);
    }
}

juce::String ModernEdirolSd80Processor::getPartPatchName(int part) const
{
    part = juce::jlimit(0, 31, part);
    const auto msb = (std::uint8_t) paramInt(pid(part, "msb"), 96);
    const auto lsb = (std::uint8_t) paramInt(pid(part, "lsb"));
    const auto pc  = (std::uint8_t) paramInt(pid(part, "pc"));
    const bool drum = paramBool(pid(part, "drum"));
    return patchName(msb, lsb, pc, drum);
}

bool ModernEdirolSd80Processor::isPartDrum(int part) const
{
    return paramBool(pid(juce::jlimit(0, 31, part), "drum"));
}

void ModernEdirolSd80Processor::enqueuePartPatch(int part)
{
    part = juce::jlimit(0, 31, part);
    const int ch = channelForPart(part);
    const auto port = portForPart(part);
    const auto mode = getGeneratorMode();
    auto msb = (std::uint8_t) paramInt(pid(part, "msb"), 96);
    const auto lsb = (std::uint8_t) paramInt(pid(part, "lsb"));
    const auto pc  = (std::uint8_t) paramInt(pid(part, "pc"));
    const bool drum = paramBool(pid(part, "drum"));

    if (mode == GeneratorMode::GM2)
        msb = drum ? kGm2DrumMsb : kGm2InstMsb;

    std::vector<juce::MidiMessage> msgs;
    if (mode == GeneratorMode::GM2)
    {
        const int map = paramInt(pid(part, "map"));
        if (map <= 3)
            msgs.push_back(gm2InstrumentSetSelect(part, (std::uint8_t) map));
    }
    appendPatchSelect(msgs, ch, msb, lsb, pc);
    throttle.pushMany(msgs, port);
}

void ModernEdirolSd80Processor::enqueuePartMix(int part)
{
    part = juce::jlimit(0, 31, part);
    const int ch = channelForPart(part);
    const auto port = portForPart(part);
    const bool mute = paramBool(pid(part, "mute"));
    bool anySolo = false;
    for (int i = 0; i < 32; ++i)
        if (paramBool(pid(i, "solo"))) { anySolo = true; break; }
    const bool silenced = mute || (anySolo && ! paramBool(pid(part, "solo")));
    const int vol = silenced ? 0 : paramInt(pid(part, "vol"), 100);

    throttle.push(makeCc(ch, cc::volume, vol), port);
    throttle.push(makeCc(ch, cc::pan, paramInt(pid(part, "pan"), 64)), port);
    throttle.push(makeCc(ch, cc::expression, paramInt(pid(part, "expr"), 127)), port);
    throttle.push(makeCc(ch, cc::reverb, paramInt(pid(part, "rev"), 40)), port);
    throttle.push(makeCc(ch, cc::chorus, paramInt(pid(part, "cho"))), port);
    throttle.push(makeCc(ch, cc::delay, paramInt(pid(part, "mfx"))), port);
}

void ModernEdirolSd80Processor::enqueuePartDeep(int part)
{
    part = juce::jlimit(0, 31, part);
    const int ch = channelForPart(part);
    const auto port = portForPart(part);
    throttle.push(makeCc(ch, cc::cutoff,    paramInt(pid(part, "cut"), 64)), port);
    throttle.push(makeCc(ch, cc::resonance, paramInt(pid(part, "res"), 64)), port);
    throttle.push(makeCc(ch, cc::attack,    paramInt(pid(part, "atk"), 64)), port);
    throttle.push(makeCc(ch, cc::decay,     paramInt(pid(part, "dec"), 64)), port);
    throttle.push(makeCc(ch, cc::release,   paramInt(pid(part, "rel"), 64)), port);
    throttle.push(makeCc(ch, cc::vibRate,   paramInt(pid(part, "vr"), 64)), port);
    throttle.push(makeCc(ch, cc::vibDepth,  paramInt(pid(part, "vd"), 64)), port);
    throttle.push(makeCc(ch, cc::vibDelay,  paramInt(pid(part, "vdl"), 64)), port);
    throttle.push(makeCc(ch, cc::portaSw,   paramBool(pid(part, "porta")) ? 127 : 0), port);
    throttle.push(makeCc(ch, cc::portaTime, paramInt(pid(part, "portaT"))), port);

    if (getGeneratorMode() == GeneratorMode::Native)
    {
        const int asg = paramInt(pid(part, "outAsg"), 0);
        throttle.push(partOutputAssign(part, asg == 0 ? 0x00 : 0x01), portForPart(part));
        throttle.push(partOutputMfxSelect(part, (std::uint8_t) paramInt(pid(part, "mfxSel"))), port);
    }
}

void ModernEdirolSd80Processor::enqueueMfxBlock()
{
    if (getGeneratorMode() != GeneratorMode::Native)
        return;

    throttle.push(mfxSourceCommon(0, true), MidiPort::A);
    throttle.push(mfxSourceCommon(1, true), MidiPort::A);
    throttle.push(mfxSourceCommon(2, true), MidiPort::A);

    const int types[3] = {
        paramBool("mfxAOn") ? paramInt("mfxAType") : 0,
        paramBool("mfxBOn") ? paramInt("mfxBType") : 0,
        paramBool("mfxCOn") ? paramInt("mfxCType") : 0
    };
    for (int i = 0; i < 3; ++i)
        throttle.push(mfxTypeCommon(i, (std::uint8_t) types[i]), MidiPort::A);

    for (int slot = 0; slot < 3; ++slot)
    {
        if (types[slot] == 0)
            continue;
        for (int k = 0; k < 4; ++k)
        {
            const int raw = paramInt(mfxParamId(slot, k), 64);
            throttle.push(mfxParamCommon(slot, k + 1, (raw - 64) * 128), MidiPort::A);
        }
    }

    for (int part = 0; part < 32; ++part)
        enqueuePartDeep(part);
}

void ModernEdirolSd80Processor::syncHardwarePush()
{
    throttle.setDelayMs(paramInt("throttle", 0));
    throttle.push(modeMessage(getGeneratorMode()), MidiPort::Both);

    throttle.push(gm2ReverbParam(0, (std::uint8_t) paramInt("reverbType", 4)), MidiPort::A);
    throttle.push(gm2ReverbParam(1, (std::uint8_t) paramInt("reverbTime", 64)), MidiPort::A);
    throttle.push(gm2ChorusParam(0, (std::uint8_t) paramInt("chorusType", 2)), MidiPort::A);
    throttle.push(gm2ChorusParam(1, (std::uint8_t) paramInt("chorusRate", 3)), MidiPort::A);
    throttle.push(gm2ChorusParam(2, (std::uint8_t) paramInt("chorusDepth", 19)), MidiPort::A);
    throttle.push(gm2ChorusParam(3, (std::uint8_t) paramInt("chorusFeedback", 8)), MidiPort::A);
    throttle.push(gm2ChorusParam(4, (std::uint8_t) paramInt("chorusToRev")), MidiPort::A);

    if (getGeneratorMode() == GeneratorMode::Native)
        enqueueMfxBlock();

    for (int part = 0; part < 32; ++part)
    {
        enqueuePartPatch(part);
        enqueuePartMix(part);
        enqueuePartDeep(part);
        throttle.push(gm2ScaleOctave(channelForPart(part), scaleTune[part]), portForPart(part));
    }
}

void ModernEdirolSd80Processor::requestHardwareDump()
{
    // Documented Native addresses (Owner's Manual pp.64–68). Bank/PC are MIDI
    // CC + program change — there is no published RQ1 for the sounding patch —
    // so those come from the last session and are not overwritten here.
    throttle.push(rq1(0x10, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01), MidiPort::A);
    throttle.push(rq1(0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x01), MidiPort::A);
    throttle.push(rq1(0x10, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x01), MidiPort::A);
    for (int part = 0; part < 32; ++part)
    {
        const auto pp = partAddress(part);
        const auto port = portForPart(part);
        throttle.push(rq1(0x10, 0x00, pp, 0x1F, 0x00, 0x00, 0x00, 0x01), port); // outAsg
        throttle.push(rq1(0x10, 0x00, pp, 0x20, 0x00, 0x00, 0x00, 0x01), port); // mfxSel
        throttle.push(rq1(0x10, 0x00, pp, 0x3F, 0x00, 0x00, 0x00, 0x01), port); // map
    }
}

void ModernEdirolSd80Processor::applyMidiFile(const juce::File& file, int group)
{
    const auto parsed = parseMidiFileForSd80(file);
    lastImportSummary = parsed.summary;
    suppressOutgoing = true;

    const bool oneGroup = (group == 0 || group == 1);
    const int destBase = oneGroup ? group * 16 : 0;

    auto applyPart = [this](int dest, const ImportedPart& ip)
    {
        if (! ip.used)
            return;
        const bool drumMsb = (ip.msb == kGm2DrumMsb || ip.msb == 86 || ip.msb >= 104);
        setParamInt(pid(dest, "msb"), ip.msb);
        setParamInt(pid(dest, "lsb"), ip.lsb);
        setParamInt(pid(dest, "pc"),  ip.pc);
        setParamInt(pid(dest, "drum"), drumMsb ? 1 : 0);
        setParamInt(pid(dest, "map"), (int) mapFromInstMsb(ip.msb));
        setParamInt(pid(dest, "vol"), ip.volume);
        setParamInt(pid(dest, "pan"), ip.pan);
        setParamInt(pid(dest, "rev"), ip.reverb);
        setParamInt(pid(dest, "cho"), ip.chorus);
        if (! isLocked(pid(dest, "msb")) && ! isLocked(pid(dest, "pc")) && ! isLocked(pid(dest, "lsb")))
            enqueuePartPatch(dest);
        enqueuePartMix(dest);
    };

    if (oneGroup)
    {
        for (int ch = 0; ch < 16; ++ch)
        {
            ImportedPart ip = parsed.parts[(size_t) ch];
            if (! ip.used && parsed.parts[(size_t) (ch + 16)].used)
                ip = parsed.parts[(size_t) (ch + 16)];
            applyPart(destBase + ch, ip);
        }
    }
    else
    {
        for (int i = 0; i < 32; ++i)
            applyPart(i, parsed.parts[(size_t) i]);

        setParamInt("reverbType", 4);
        setParamInt("reverbTime", 64);
        setParamInt("chorusType", 2);
        setParamInt("chorusRate", 3);
        setParamInt("chorusDepth", 19);
        setParamInt("chorusFeedback", 8);
        setParamInt("mfxAType", 0);
        setParamInt("mfxBType", 0);
        setParamInt("mfxCType", 0);
        for (int slot = 0; slot < 3; ++slot)
            for (int k = 0; k < 4; ++k)
                setParamInt(mfxParamId(slot, k), 64);
        for (int i = 0; i < 32; ++i)
            if (! parsed.parts[(size_t) i].used)
            {
                setParamInt(pid(i, "rev"), 40);
                setParamInt(pid(i, "cho"), 0);
                setParamInt(pid(i, "mfx"), 0);
            }
    }

    suppressOutgoing = false;
    if (! oneGroup)
    {
        syncHardwarePush();
        // Mixer drop is the FL piano-roll setup path: keep channels as written.
        setParamInt("hostRoute", 1);
    }

    if (onImportFinished)
        juce::MessageManager::callAsync([this] { if (onImportFinished) onImportFinished(); });
}

bool ModernEdirolSd80Processor::saveMesd80Preset(const juce::File& file)
{
    auto xml = apvts.copyState().createXml();
    if (! xml)
        return false;
    xml->setTagName("mesd80preset");
    xml->setAttribute("version", 1);
    xml->setAttribute("plugin", "Modern Edirol SD80");
    xml->setAttribute("author", "Crimson Redstone");
    xml->setAttribute("outA", outAName);
    xml->setAttribute("outB", outBName);
    xml->setAttribute("skin", skinIndex);
    xml->setAttribute("aeterna", aeternaMode);
    writeKeysXml(*xml, screenKeysOn, screenKeysVel, screenKeysOct, keysCurve, scaleTune);
    xml->setAttribute("locks", lockedIds.joinIntoString(","));
    return xml->writeTo(file);
}

bool ModernEdirolSd80Processor::loadMesd80Preset(const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(file);
    if (! xml)
        return false;
    auto tree = juce::ValueTree::fromXml(*xml);
    if (! tree.isValid())
        return false;

    std::vector<std::pair<juce::String, float>> kept;
    for (auto& id : lockedIds)
        if (auto* p = apvts.getRawParameterValue(id))
            kept.push_back({ id, p->load() });

    suppressOutgoing = true;
    apvts.replaceState(tree);
    for (auto& kv : kept)
        if (auto* p = apvts.getParameter(kv.first))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(kv.second));
    suppressOutgoing = false;

    if (xml->hasAttribute("skin"))
        setSkinIndex(xml->getIntAttribute("skin", skinIndex));
    if (xml->hasAttribute("aeterna"))
        setAeternaMode(xml->getIntAttribute("aeterna", aeternaMode));
    readKeysXml(*xml, screenKeysOn, screenKeysVel, screenKeysOct, keysCurve, scaleTune);
    persistAppSettings();
    syncHardwarePush();
    return true;
}

void ModernEdirolSd80Processor::getStateInformation(juce::MemoryBlock& destData)
{
    auto xml = apvts.copyState().createXml();
    if (! xml)
        return;
    xml->setAttribute("outA", outAName);
    xml->setAttribute("outB", outBName);
    xml->setAttribute("inA", inAName);
    xml->setAttribute("inB", inBName);
    xml->setAttribute("skin", skinIndex);
    xml->setAttribute("aeterna", aeternaMode);
    writeKeysXml(*xml, screenKeysOn, screenKeysVel, screenKeysOct, keysCurve, scaleTune);
    xml->setAttribute("locks", lockedIds.joinIntoString(","));
    xml->setAttribute("routeAsPlayed", 1);
    if (auto d = daw.toXml())
        xml->addChildElement(d.release());
    {
        auto* el = new juce::XmlElement("mesd80patch");
        writePatchXml(*el, userPatch);
        xml->addChildElement(el);
        auto* lib = new juce::XmlElement("mesd80lib");
        lib->setAttribute("slot", userSlot);
        for (int i = 0; i < kUserPatchSlots; ++i)
        {
            if (! userSlots[(size_t) i].used)
                continue;
            auto* c = lib->createNewChildElement("mesd80patch");
            c->setAttribute("slot", i);
            writePatchXml(*c, userSlots[(size_t) i].patch);
        }
        xml->addChildElement(lib);
    }
    copyXmlToBinary(*xml, destData);
}

void ModernEdirolSd80Processor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (! xml)
        return;
    if (auto* d = xml->getChildByName("mesd80song"))
    {
        daw.fromXml(*d);
        xml->removeChildElement(d, true);
    }
    if (auto* p = xml->getChildByName("mesd80patch"))
    {
        userPatch = readPatchXml(*p);
        xml->removeChildElement(p, true);
    }
    if (auto* lib = xml->getChildByName("mesd80lib"))
    {
        loadUserLibraryXml(*lib);
        xml->removeChildElement(lib, true);
    }
    suppressOutgoing = true;
    auto tree = juce::ValueTree::fromXml(*xml);
    if (tree.isValid())
        apvts.replaceState(tree);
    suppressOutgoing = false;

    outAName = xml->getStringAttribute("outA");
    outBName = xml->getStringAttribute("outB");
    inAName  = xml->getStringAttribute("inA", xml->getStringAttribute("inDev"));
    inBName  = xml->getStringAttribute("inB");
    if (xml->hasAttribute("skin"))
        setSkinIndex(xml->getIntAttribute("skin", 0));
    if (xml->hasAttribute("aeterna"))
        setAeternaMode(xml->getIntAttribute("aeterna", aeternaMode));
    readKeysXml(*xml, screenKeysOn, screenKeysVel, screenKeysOct, keysCurve, scaleTune);
    lockedIds = juce::StringArray::fromTokens(xml->getStringAttribute("locks"), ",", "");
    lockedIds.trim();
    lockedIds.removeEmptyStrings();
    if (xml->getIntAttribute("routeAsPlayed", 0) == 0 && paramInt("hostRoute", 0) == 0)
        setParamInt("hostRoute", 1);
    persistAppSettings();
    if (outAName.isNotEmpty())
        openNamedOutput(midiOutA, outAName, outAIndex, outAName);
    if (outBName.isNotEmpty())
        openNamedOutput(midiOutB, outBName, outBIndex, outBName);
    if (inAName.isNotEmpty())
        openNamedInput(midiInA, inAName, inAIndex, inAName);
    if (inBName.isNotEmpty())
        openNamedInput(midiInB, inBName, inBIndex, inBName);
    autoDetectUsbPorts();
    requestHardwareDump();
}

juce::StringArray ModernEdirolSd80Processor::midiOutputNames() const
{
    juce::StringArray names;
    names.add("(none)");
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        names.add(d.name);
    return names;
}

juce::StringArray ModernEdirolSd80Processor::midiInputNames() const
{
    juce::StringArray names;
    names.add("(none)");
    for (auto& d : juce::MidiInput::getAvailableDevices())
        names.add(d.name);
    return names;
}

void ModernEdirolSd80Processor::openNamedOutput(std::unique_ptr<juce::MidiOutput>& slot,
                                                juce::String& stored, int& index,
                                                const juce::String& name)
{
    const juce::ScopedLock sl(deviceLock);
    slot.reset();
    index = -1;
    stored = name;
    if (name.isEmpty() || name == "(none)")
        return;
    auto devices = juce::MidiOutput::getAvailableDevices();
    for (int i = 0; i < devices.size(); ++i)
    {
        if (devices[i].name == name)
        {
            slot = juce::MidiOutput::openDevice(devices[i].identifier);
            index = i;
            break;
        }
    }
}

void ModernEdirolSd80Processor::setMidiOutputA(int deviceIndex)
{
    auto devices = juce::MidiOutput::getAvailableDevices();
    if (deviceIndex < 0 || deviceIndex >= devices.size())
    {
        const juce::ScopedLock sl(deviceLock);
        midiOutA.reset();
        outAIndex = -1;
        outAName.clear();
        return;
    }
    openNamedOutput(midiOutA, outAName, outAIndex, devices[deviceIndex].name);
}

void ModernEdirolSd80Processor::setMidiOutputB(int deviceIndex)
{
    auto devices = juce::MidiOutput::getAvailableDevices();
    if (deviceIndex < 0 || deviceIndex >= devices.size())
    {
        const juce::ScopedLock sl(deviceLock);
        midiOutB.reset();
        outBIndex = -1;
        outBName.clear();
        return;
    }
    openNamedOutput(midiOutB, outBName, outBIndex, devices[deviceIndex].name);
}

void ModernEdirolSd80Processor::setMidiInput(int deviceIndex)
{
    auto devices = juce::MidiInput::getAvailableDevices();
    if (deviceIndex < 0 || deviceIndex >= devices.size())
    {
        const juce::ScopedLock sl(deviceLock);
        if (midiInA) midiInA->stop();
        midiInA.reset();
        inAIndex = -1;
        inAName.clear();
        return;
    }
    openNamedInput(midiInA, inAName, inAIndex, devices[deviceIndex].name);
}

void ModernEdirolSd80Processor::sendQueued(const QueuedMidi& q, juce::MidiBuffer* hostOut, int sampleOffset)
{
    const juce::ScopedLock sl(deviceLock);
    if (q.port == MidiPort::A || q.port == MidiPort::Both)
        if (midiOutA) midiOutA->sendMessageNow(q.message);
    if (q.port == MidiPort::B || q.port == MidiPort::Both)
        if (midiOutB) midiOutB->sendMessageNow(q.message);

    if (hostOut != nullptr)
    {
        const bool mirrorA = paramBool("hostMirrorA") && (q.port == MidiPort::A || q.port == MidiPort::Both);
        const bool mirrorB = paramBool("hostMirrorB") && (q.port == MidiPort::B || q.port == MidiPort::Both);
        if (mirrorA || mirrorB)
            hostOut->addEvent(q.message, sampleOffset);
    }
}

void ModernEdirolSd80Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    throttle.setDelayMs(paramInt("throttle", 0));

    juce::MidiBuffer incoming = midi;
    midi.clear();

    auto remapToPart = [](const juce::MidiMessage& msg, int ch) -> juce::MidiMessage
    {
        const double ts = msg.getTimeStamp();
        juce::MidiMessage remapped = msg;
        if (msg.isNoteOn())
            remapped = juce::MidiMessage::noteOn(ch, msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            remapped = juce::MidiMessage::noteOff(ch, msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isPitchWheel())
            remapped = juce::MidiMessage::pitchWheel(ch, msg.getPitchWheelValue());
        else if (msg.isAftertouch())
            remapped = juce::MidiMessage::aftertouchChange(ch, msg.getNoteNumber(), msg.getAfterTouchValue());
        else if (msg.isChannelPressure())
            remapped = juce::MidiMessage::channelPressureChange(ch, msg.getChannelPressureValue());
        else if (msg.isController())
            remapped = juce::MidiMessage::controllerEvent(ch, msg.getControllerNumber(), msg.getControllerValue());
        remapped.setTimeStamp(ts);
        return remapped;
    };

    auto mask = selectedMask.load();
    if (mask == 0)
        mask = 1u << selectedPart.load();
    const int hostRoute = paramInt("hostRoute", 1);

    for (const auto metadata : incoming)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            const double ts = msg.getTimeStamp();
            msg = juce::MidiMessage::noteOn(msg.getChannel(), msg.getNoteNumber(),
                                            (juce::uint8) shapedVelocity(msg.getVelocity()));
            msg.setTimeStamp(ts);
        }
        if (msg.isSysEx())
        {
            applyDumpMessage(msg);
            continue;
        }
        if (! (msg.isNoteOnOrOff() || msg.isPitchWheel() || msg.isAftertouch()
               || msg.isChannelPressure() || msg.isController() || msg.isProgramChange()))
            continue;

        if (hostRoute == 0) // Follow SEL — live keyboard / single-channel play
        {
            if (msg.isProgramChange())
            {
                MidiPort port = visibleGroup.load() == 1 ? MidiPort::B : MidiPort::A;
                sendQueued({ msg, port }, &midi, metadata.samplePosition);
                continue;
            }
            for (int p = 0; p < 32; ++p)
            {
                if ((mask & (1u << p)) == 0)
                    continue;
                if (isPartSilenced(p) && (msg.isNoteOn() || msg.isNoteOff()
                                          || msg.isPitchWheel() || msg.isAftertouch()
                                          || msg.isChannelPressure()))
                    continue;
                auto out = msg;
                if (msg.getChannel() >= 1)
                    out = remapToPart(msg, channelForPart(p));
                sendQueued({ out, portForPart(p) }, &midi, metadata.samplePosition);
                if (msg.isNoteOnOrOff())
                    daw.recordLive(p, msg.isNoteOn() && msg.getVelocity() > 0,
                                   msg.getNoteNumber(), msg.getVelocity());
            }
        }
        else
        {
            const MidiPort port = hostRoute == 2 ? MidiPort::B : MidiPort::A;
            int part = (hostRoute == 2 ? 16 : 0);
            if (msg.getChannel() >= 1)
                part += msg.getChannel() - 1;
            part = juce::jlimit(0, 31, part);
            if (isPartSilenced(part) && (msg.isNoteOn() || msg.isNoteOff()
                                         || msg.isPitchWheel() || msg.isAftertouch()
                                         || msg.isChannelPressure()))
                continue;
            sendQueued({ msg, port }, &midi, metadata.samplePosition);
            if (msg.isNoteOnOrOff())
                daw.recordLive(part, msg.isNoteOn() && msg.getVelocity() > 0,
                               msg.getNoteNumber(), msg.getVelocity());
        }
    }

    player.render(currentSampleRate, buffer.getNumSamples(),
                  [this, &midi](const juce::MidiMessage& m, int sample, MidiPort port)
                  {
                      int part = 0;
                      if (m.getChannel() >= 1)
                          part = (port == MidiPort::B ? 16 : 0) + (m.getChannel() - 1);
                      if (m.isNoteOnOrOff() || m.isPitchWheel())
                          if (isPartSilenced(juce::jlimit(0, 31, part)))
                              return;
                      sendQueued({ m, port }, &midi, sample);
                  });

    auto emitPl = [this, &midi](const juce::MidiMessage& m, int sample, MidiPort port)
    {
        int part = 0;
        if (m.getChannel() >= 1)
            part = (port == MidiPort::B ? 16 : 0) + (m.getChannel() - 1);
        if (m.isNoteOnOrOff() || m.isPitchWheel())
            if (isPartSilenced(juce::jlimit(0, 31, part)))
                return;
        sendQueued({ m, port }, &midi, sample);
    };
    playlistA.render(currentSampleRate, buffer.getNumSamples(), emitPl);
    if (playlistActive.load() && playlistA.consumeNaturalEnd())
    {
        playlistSpentA.store(true);
        playlistA.setLooping(false);
        bool more = playlistLoopMode.load() == 1;
        if (! more)
        {
            const juce::ScopedLock sl(playlistLock);
            more = playlistQueue.size() > 0;
        }
        playlistNeedArm.fetch_or(1);
        if (more)
            playlistNeedPlay.store(true);
        else if (! (partBOn.load() && playlistB.isPlaying()))
            playlistActive.store(false);
    }
    playlistB.render(currentSampleRate, buffer.getNumSamples(), emitPl);
    if (playlistB.consumeNaturalEnd())
    {
        playlistB.setLooping(false);
        if (partBOn.load() && playlistLoopMode.load() != 0 && playlistB.isLoaded())
        {
            playlistSpentB.store(false);
            playlistB.setLooping(true);
            playlistB.play();
        }
        else
        {
            playlistSpentB.store(true);
            if (! playlistA.isPlaying())
                playlistActive.store(false);
        }
    }
    if ((playlistA.isPlaying() && playlistA.getPosition() > 0.4)
        || (playlistB.isPlaying() && playlistB.getPosition() > 0.4))
        playlistHeard.store(true);

    daw.render(currentSampleRate, buffer.getNumSamples(), emitPl);

    QueuedMidi q;
    int guard = 0;
    const int cap = throttle.isOff() ? 1024 : 8;
    while (guard++ < cap && throttle.popDue(currentSampleRate, buffer.getNumSamples(), q))
        sendQueued(q, &midi, 0);
}

void ModernEdirolSd80Processor::parameterChanged(const juce::String& parameterID, float)
{
    if (suppressOutgoing.load())
        return;

    if (parameterID == "mode")
    {
        throttle.push(modeMessage(getGeneratorMode()), MidiPort::Both);
        if (getGeneratorMode() == GeneratorMode::Native)
            enqueueMfxBlock();
        return;
    }
    if (parameterID == "masterVol")
    {
        sendMasterVolume();
        return;
    }
    if (parameterID == "hostRoute")
        return;
    if (parameterID == "mfxAType" || parameterID == "mfxBType" || parameterID == "mfxCType"
        || parameterID == "mfxAOn" || parameterID == "mfxBOn" || parameterID == "mfxCOn"
        || parameterID.startsWith("mfxA_p") || parameterID.startsWith("mfxB_p") || parameterID.startsWith("mfxC_p"))
    {
        enqueueMfxBlock();
        return;
    }
    if (parameterID.startsWith("reverb") || parameterID.startsWith("chorus"))
    {
        throttle.push(gm2ReverbParam(0, (std::uint8_t) paramInt("reverbType", 4)), MidiPort::A);
        throttle.push(gm2ReverbParam(1, (std::uint8_t) paramInt("reverbTime", 64)), MidiPort::A);
        throttle.push(gm2ChorusParam(0, (std::uint8_t) paramInt("chorusType", 2)), MidiPort::A);
        throttle.push(gm2ChorusParam(1, (std::uint8_t) paramInt("chorusRate", 3)), MidiPort::A);
        throttle.push(gm2ChorusParam(2, (std::uint8_t) paramInt("chorusDepth", 19)), MidiPort::A);
        throttle.push(gm2ChorusParam(3, (std::uint8_t) paramInt("chorusFeedback", 8)), MidiPort::A);
        throttle.push(gm2ChorusParam(4, (std::uint8_t) paramInt("chorusToRev")), MidiPort::A);
        return;
    }

    if (parameterID.length() >= 4 && parameterID[0] == 'p' && parameterID[3] == '_')
    {
        const int part = parameterID.substring(1, 3).getIntValue();
        const auto key = parameterID.fromLastOccurrenceOf("_", false, false);
        if (key == "msb" || key == "lsb" || key == "pc" || key == "drum" || key == "map")
            enqueuePartPatch(part);
        else if (key == "vol" || key == "pan" || key == "expr" || key == "rev" || key == "cho"
                 || key == "mfx" || key == "mute" || key == "solo")
            enqueuePartMix(part);
        else
            enqueuePartDeep(part);
    }
}

void ModernEdirolSd80Processor::timerCallback()
{
    juce::Array<juce::MidiMessage> batch;
    {
        const juce::ScopedLock sl(dumpLock);
        batch.swapWith(dumpInbox);
    }
    for (auto& m : batch)
        applyDumpMessage(m);
    if (batch.size() > 0)
        dumpDirty.store(true);

    if (getCallbackLock().tryEnter())
    {
        QueuedMidi q;
        int n = 0;
        const int cap = throttle.isOff() ? 256 : 1;
        while (n++ < cap && throttle.popNow(q))
            sendQueued(q, nullptr, 0);
        getCallbackLock().exit();
    }

    if (++persistTicker >= 80) // ~2 s at 40 Hz
    {
        persistTicker = 0;
        persistAppSettings();
    }

    playlistService();
    syncStudioDisplay();
}

void ModernEdirolSd80Processor::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (! message.isSysEx())
        return;
    const juce::ScopedLock sl(dumpLock);
    dumpInbox.add(message);
}

void ModernEdirolSd80Processor::applyDumpMessage(const juce::MidiMessage& msg)
{
    if (! msg.isSysEx())
        return;
    auto* d = msg.getSysExData();
    const int n = msg.getSysExDataSize();
    // 41 10 00 48 12 a0 a1 a2 a3 [data...] cs
    if (n < 10 || d[0] != 0x41 || d[2] != 0x00 || d[3] != 0x48 || d[4] != 0x12)
        return;
    const auto a0 = d[5], a1 = d[6], a2 = d[7], a3 = d[8];
    const int dataStart = 9;
    const int dataEnd = n - 1; // checksum
    if (dataEnd <= dataStart)
        return;

    auto applyByte = [this](std::uint8_t aa0, std::uint8_t aa1, std::uint8_t aa2, std::uint8_t aa3, std::uint8_t v)
    {
        if (aa0 != 0x10 || aa1 != 0x00)
            return;
        if (aa2 == 0x06 && aa3 == 0x00)
        {
            setParamInt("mfxAType", v);
            if (v != 0) setParamInt("mfxAOn", 1);
        }
        else if (aa2 == 0x08 && aa3 == 0x00)
        {
            setParamInt("mfxBType", v);
            if (v != 0) setParamInt("mfxBOn", 1);
        }
        else if (aa2 == 0x0A && aa3 == 0x00)
        {
            setParamInt("mfxCType", v);
            if (v != 0) setParamInt("mfxCOn", 1);
        }
        else if (aa2 >= 0x20 && aa2 <= 0x3F)
        {
            const int part = aa2 - 0x20;
            if (aa3 == 0x1F)
                setParamInt(pid(part, "outAsg"), v == 0 ? 0 : 1);
            else if (aa3 == 0x20)
                setParamInt(pid(part, "mfxSel"), juce::jlimit(0, 2, (int) v));
            else if (aa3 == 0x3F)
                setParamInt(pid(part, "map"), juce::jlimit(0, 8, (int) v));
        }
    };

    suppressOutgoing = true;
    int addr = ((int) a0 << 21) | ((int) a1 << 14) | ((int) a2 << 7) | (int) a3;
    for (int i = dataStart; i < dataEnd; ++i)
    {
        const auto aa0 = (std::uint8_t) ((addr >> 21) & 0x7F);
        const auto aa1 = (std::uint8_t) ((addr >> 14) & 0x7F);
        const auto aa2 = (std::uint8_t) ((addr >> 7) & 0x7F);
        const auto aa3 = (std::uint8_t) (addr & 0x7F);
        applyByte(aa0, aa1, aa2, aa3, d[i]);
        ++addr;
    }
    suppressOutgoing = false;
}

void ModernEdirolSd80Processor::openNamedInput(std::unique_ptr<juce::MidiInput>& slot,
                                               juce::String& stored, int& index,
                                               const juce::String& name)
{
    const juce::ScopedLock sl(deviceLock);
    if (slot) slot->stop();
    slot.reset();
    index = -1;
    stored = name;
    if (name.isEmpty() || name == "(none)")
        return;
    auto devices = juce::MidiInput::getAvailableDevices();
    for (int i = 0; i < devices.size(); ++i)
    {
        if (devices[i].name == name)
        {
            slot = juce::MidiInput::openDevice(devices[i].identifier, this);
            if (slot)
            {
                slot->start();
                index = i;
            }
            break;
        }
    }
}

juce::AudioProcessorEditor* ModernEdirolSd80Processor::createEditor()
{
    return new ModernEdirolSd80Editor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ModernEdirolSd80Processor();
}
