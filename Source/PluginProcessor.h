#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>
#include "SD80PatchData.h"
#include "SD80Sysex.h"
#include "MidiThrottleQueue.h"
#include "MidiFileImporter.h"
#include "MidiPlayer.h"
#include "DawSong.h"
#include "UserPatch.h"

class ModernEdirolSd80Processor : public juce::AudioProcessor,
                                   public juce::Timer,
                                   private juce::AudioProcessorValueTreeState::Listener,
                                   private juce::MidiInputCallback
{
public:
    ModernEdirolSd80Processor();
    ~ModernEdirolSd80Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    MidiPlayerEngine player;
    MidiPlayerEngine playlistA;
    MidiPlayerEngine playlistB;
    MidiPlayerEngine studioView;
    DawEngine daw;
    UserPatch userPatch;
    std::array<UserPatchSlot, kUserPatchSlots> userSlots;
    int userSlot { 0 };

    const UserPatch& getUserPatch() const { return userPatch; }
    int getUserSlot() const { return userSlot; }
    void setUserSlot(int i) { userSlot = juce::jlimit(0, kUserPatchSlots - 1, i); }
    void setUserPatch(const UserPatch& p, bool persist);
    void sendUserPatch(PatchPush what, int tone, bool withBase);
    bool saveUserPatchFile(const juce::File&);
    bool loadUserPatchFile(const juce::File&);
    bool exportUserPatchSyx(const juce::File&);
    void storeUserSlot(int index);
    void recallUserSlot(int index);
    bool userSlotUsed(int index) const;

    juce::StringArray midiOutputNames() const;
    juce::StringArray midiInputNames() const;
    void setMidiOutputA(int deviceIndex);
    void setMidiOutputB(int deviceIndex);
    void setMidiInput(int deviceIndex);
    int getMidiOutputAIndex() const { return outAIndex; }
    int getMidiOutputBIndex() const { return outBIndex; }
    juce::String getMidiOutputAName() const { return outAName; }
    juce::String getMidiOutputBName() const { return outBName; }

    void enqueuePartPatch(int part);
    void enqueuePartMix(int part);
    void enqueuePartDeep(int part);
    void enqueueMfxBlock();
    void syncHardwarePush();
    void requestHardwareDump();
    void applyMidiFile(const juce::File&, int group = -1);
    void setGeneratorMode(sd80::GeneratorMode m);
    sd80::GeneratorMode getGeneratorMode() const;

    bool saveMesd80Preset(const juce::File&);
    bool loadMesd80Preset(const juce::File&);

    int getSelectedPart() const { return selectedPart.load(); }
    void setSelectedPart(int p);
    bool isPartSelected(int p) const;
    void togglePartSelected(int p, bool exclusive);
    std::uint32_t getSelectedMask() const { return selectedMask.load(); }

    int getVisibleGroup() const { return visibleGroup.load(); }
    void setVisibleGroup(int g) { visibleGroup.store(g ? 1 : 0); }

    juce::String getPartPatchName(int part) const;
    bool isPartDrum(int part) const;
    bool isPartSilenced(int part) const;
    int queueDepth() const { return throttle.size(); }

    bool isLocked(const juce::String& paramId) const;
    void setLocked(const juce::String& paramId, bool shouldLock);
    juce::StringArray getLockedIds() const;

    int getSkinIndex() const { return skinIndex; }
    void setSkinIndex(int i);

    // 0 off, 1 on. Piano-roll companion only.
    int getAeternaMode() const { return aeternaMode; }
    void setAeternaMode(int mode);

    void muteAll(bool shouldMute);
    void unsoloAll();
    void autoDetectUsbPorts();
    void factoryResetHardware();
    void silenceForQuit();
    void resetEffectsToDefault();
    void pullFromHardware();
    void sendMasterVolume();
    void playlistAdd(const juce::File&);
    void loadPartB(const juce::File&);
    void setPartBEnabled(bool on);
    bool partBEnabled() const { return partBOn.load(); }
    void partBFollow(int cmd);
    void playlistPlay();
    void playlistPause();
    void playlistStop();
    void playlistClear();
    void disarmPlaylist();
    void playlistService();
    bool playlistIsActive() const { return playlistActive.load(); }
    bool playlistIsArmed() const;
    int getPlaylistLoop() const { return playlistLoopMode.load(); }
    int cyclePlaylistLoop();
    MidiPlayerEngine& displayEngine();
    int displayPartGroup();
    bool studioDisplayOn() const { return studioFollowing; }
    void syncStudioDisplay();
    void applyListedPatch(int part, int map, bool drum, int msb, int lsb, int pc);
    bool playlistSlotLoaded(int side) const;
    bool playlistSlotPlaying(int side) const;
    bool playlistSlotSpent(int side) const;
    juce::String playlistSlotName(int side) const;
    juce::StringArray playlistQueueNames() const;
    juce::String playlistStatus() const;
    void dawPlay();
    void dawPause();
    void dawStop();
    void dawPreview(int part, int pitch, bool on, int velocity = 100);
    int getScaleTune(int part, int semitone) const;
    void setScaleTune(int part, int semitone, int cents64);
    void resetScaleTune(int part);
    bool playlistMayLinger() const { return playlistOwnsTransport.load() && playlistHeard.load(); }
    bool getScreenKeysOn() const { return screenKeysOn; }
    void setScreenKeysOn(bool v);
    int getScreenKeysVel() const { return screenKeysVel; }
    void setScreenKeysVel(int v);
    int getScreenKeysOct() const { return screenKeysOct; }
    void setScreenKeysOct(int v);
    int getKeysCurve() const { return keysCurve; }
    void setKeysCurve(int v);
    int shapedVelocity(int velocity) const;
    bool dawSaveSong(const juce::File&);
    bool dawLoadSong(const juce::File&);
    bool dawImportMidi(const juce::File&, int track);
    bool dawExportMidi(const juce::File&);
    bool consumeDumpDirty() { return dumpDirty.exchange(false); }
    bool skipFxResetWarning() const { return skipFxWarn; }
    void setSkipFxResetWarning(bool v);

    static juce::String mfxParamId(int slot0to2, int param0to3);

    std::function<void()> onImportFinished;
    std::function<void()> onSkinChanged;
    std::function<void()> onAeternaChanged;
    juce::String lastImportSummary;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::String pid(int part, const char* key);

    void setParamInt(const juce::String& id, int v);

private:
    void timerCallback() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void sendQueued(const QueuedMidi&, juce::MidiBuffer* hostOut, int sampleOffset);
    void openNamedOutput(std::unique_ptr<juce::MidiOutput>& slot, juce::String& stored, int& index, const juce::String& name);
    int paramInt(const juce::String& id, int fallback = 0) const;
    bool paramBool(const juce::String& id) const;
    MidiPort portForPart(int part) const { return part < 16 ? MidiPort::A : MidiPort::B; }
    int channelForPart(int part) const { return (part % 16) + 1; }
    void persistAppSettings();
    void restoreAppSettings();
    void loadUserLibraryXml(const juce::XmlElement&);
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    void applyDumpMessage(const juce::MidiMessage&);
    void openNamedInput(std::unique_ptr<juce::MidiInput>& slot, juce::String& stored,
                        int& index, const juce::String& name);

    MidiThrottleQueue throttle;
    std::unique_ptr<juce::MidiOutput> midiOutA, midiOutB;
    std::unique_ptr<juce::MidiInput> midiInA, midiInB;
    juce::String outAName, outBName, inAName, inBName;
    int outAIndex { -1 }, outBIndex { -1 }, inAIndex { -1 }, inBIndex { -1 };
    double currentSampleRate { 44100.0 };
    std::atomic<int> selectedPart { 0 };
    std::atomic<int> visibleGroup { 0 };
    std::atomic<std::uint32_t> selectedMask { 1u };
    juce::CriticalSection deviceLock;
    std::atomic<bool> suppressOutgoing { false };

    juce::Array<juce::MidiMessage> dumpInbox;
    juce::CriticalSection dumpLock;
    int persistTicker { 0 };
    std::atomic<bool> dumpDirty { false };

    juce::StringArray lockedIds;
    int skinIndex { 0 };
    int aeternaMode { 1 };
    bool skipFxWarn { false };
    bool screenKeysOn { false };
    int screenKeysVel { 100 };
    int screenKeysOct { 4 };
    int keysCurve { 0 };
    std::uint8_t scaleTune[32][12] {};
    juce::ApplicationProperties appProps;

    juce::Array<juce::File> playlistQueue;
    juce::Array<juce::File> playlistLibrary;
    juce::CriticalSection playlistLock;
    std::atomic<bool> playlistActive { false };
    std::atomic<bool> playlistOwnsTransport { false };
    std::atomic<bool> partBOn { false };
    std::atomic<bool> playlistPaused { false };
    std::atomic<bool> playlistHeard { false };
    std::atomic<int> playlistLoopMode { 0 }; // 0 off, 1 whole playlist, 2 this song
    std::atomic<int> playlistNeedArm { 0 }; // bit0=A, bit1=B
    std::atomic<bool> playlistNeedPlay { false };
    std::atomic<bool> playlistSpentA { false };
    std::atomic<bool> playlistSpentB { false };
    bool studioFollowing { false };
    bool studioHold { false };
    int studioSide { 0 };
    int studioSeenGen { -1 };
    int studioSeenMode { -1 };
    int studioSeenEdit { -1 };
    int studioSeenPat { -1 };
    int studioSeenBpm { -1 };
    double studioSeenSec { -1.0 };
    void playlistArm(int side);
    void applyPlaylistLoopToEngines();
    void refillPlaylistQueueUnlocked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModernEdirolSd80Processor)
};
