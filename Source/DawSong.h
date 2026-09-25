#pragma once

// STUDIO song: 32 tracks = SD-80 parts, FL-style patterns/clips, Ableton-style CC lanes.
// PPQ 480. Playback emits notes + pitch-bend slides + thinned CCs onto Part A/B USB.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "MidiThrottleQueue.h"
#include "SD80Sysex.h"

static const int kDawPpq = 480;
static const int kDawTracks = 32;
static const int kDawBar = 1920; // 4/4 bar
static const int kDawLanes = 7;
static const int kDawGlobals = 5;

static const int kDawLaneCc[kDawLanes] = { 74, 71, 7, 10, 91, 93, 94 };
static const char* const kDawLaneName[kDawLanes] = {
    "Cutoff", "Reso", "Vol", "Pan", "Rev", "Cho", "MFX"
};
static const char* const kDawGlobalName[kDawGlobals] = {
    "Tempo", "Master Vol", "Rev Time", "Cho Rate", "Cho Depth"
};

struct DawNote
{
    int pitch { 60 };
    int start { 0 };
    int dur { 120 };
    int vel { 100 };
    int pan { 0 };
    bool slide { false };
};

struct DawPattern
{
    juce::String name { "P1" };
    int length { kDawBar };
    std::vector<DawNote> notes;
};

struct DawClip
{
    int pattern { 0 };
    int start { 0 };
    int length { 0 }; // 0 = use pattern length
};

struct DawAutoPt
{
    int tick { 0 };
    float value { 0.5f };
};

struct DawAutoLane
{
    int cc { 74 };
    bool open { false };
    std::vector<DawAutoPt> pts;
};

struct DawTrack
{
    std::vector<DawClip> clips;
    DawAutoLane lanes[kDawLanes];
};

struct DawVoice
{
    int part { 0 };
    int pitch { 60 };
    int offTick { 0 };
    int startTick { 0 };
    int slideTo { -1 };
    int slideEnd { 0 };
};

struct DawSong
{
    juce::String title { "Untitled" };
    double bpm { 120.0 };
    int ppq { kDawPpq };
    int loopEnd { kDawBar * 4 };
    bool globalOpen { false };
    std::vector<DawPattern> patterns;
    DawTrack tracks[kDawTracks];
    DawAutoLane globals[kDawGlobals];
};

class DawEngine
{
public:
    DawEngine() { resetSong(); }

    static int clipSpan(const DawClip& c, const DawPattern& pat)
    {
        const int plen = juce::jmax(1, pat.length);
        return c.length > 0 ? c.length : plen;
    }

    static int laneForCc(int cc)
    {
        for (int i = 0; i < kDawLanes; ++i)
            if (kDawLaneCc[i] == cc)
                return i;
        return -1;
    }

    static float laneAt(const DawAutoLane& lane, int tick)
    {
        if (lane.pts.empty())
            return -1.0f;
        if (lane.pts.size() == 1 || tick <= lane.pts.front().tick)
            return lane.pts.front().value;
        if (tick >= lane.pts.back().tick)
            return lane.pts.back().value;
        for (int i = 1; i < (int) lane.pts.size(); ++i)
        {
            if (tick < lane.pts[(size_t) i].tick)
            {
                const int i0 = juce::jmax(0, i - 2);
                const int i1 = i - 1;
                const int i2 = i;
                const int i3 = juce::jmin((int) lane.pts.size() - 1, i + 1);
                const auto& a = lane.pts[(size_t) i1];
                const auto& b = lane.pts[(size_t) i2];
                const float u = (float) (tick - a.tick) / (float) juce::jmax(1, b.tick - a.tick);
                return catmull(lane.pts[(size_t) i0].value, a.value, b.value,
                               lane.pts[(size_t) i3].value, juce::jlimit(0.0f, 1.0f, u));
            }
        }
        return lane.pts.back().value;
    }

    void resetSong()
    {
        const juce::ScopedLock sl(lock);
        song.title = "Untitled";
        song.bpm = 120.0;
        song.ppq = kDawPpq;
        song.loopEnd = kDawBar * 4;
        song.globalOpen = false;
        song.patterns.clear();
        DawPattern p;
        p.name = "P1";
        p.length = kDawBar;
        song.patterns.push_back(p);
        for (int t = 0; t < kDawTracks; ++t)
        {
            song.tracks[t].clips.clear();
            for (int L = 0; L < kDawLanes; ++L)
            {
                song.tracks[t].lanes[L].cc = kDawLaneCc[L];
                song.tracks[t].lanes[L].open = false;
                song.tracks[t].lanes[L].pts.clear();
            }
        }
        for (int g = 0; g < kDawGlobals; ++g)
        {
            song.globals[g].cc = g;
            song.globals[g].open = false;
            song.globals[g].pts.clear();
        }
        posTick = 0.0;
        playing = false;
        songMode = true;
        patternIndex = 0;
        editTrack = 0;
        voices.clear();
        dirty = true;
        panic = true;
        lastAutoTick = 0;
        for (int t = 0; t < kDawTracks; ++t)
            for (int L = 0; L < kDawLanes; ++L)
                lastCc[t][L] = -1;
        for (int g = 0; g < kDawGlobals; ++g)
            lastG[g] = -1;
        ++gen;
    }

    juce::CriticalSection& getLock() { return lock; }

    juce::String getTitle() const { juce::ScopedLock sl(lock); return song.title; }
    void setTitle(const juce::String& t)
    {
        juce::ScopedLock sl(lock);
        song.title = t;
        dirty = true;
    }

    double getBpm() const { juce::ScopedLock sl(lock); return song.bpm; }
    void setBpm(double b)
    {
        juce::ScopedLock sl(lock);
        song.bpm = juce::jlimit(20.0, 300.0, b);
        dirty = true;
    }

    bool isPlaying() const { juce::ScopedLock sl(lock); return playing; }
    bool isSongMode() const { juce::ScopedLock sl(lock); return songMode; }
    void setSongMode(bool v) { juce::ScopedLock sl(lock); songMode = v; }

    int getEditTrack() const { juce::ScopedLock sl(lock); return editTrack; }
    void setEditTrack(int t)
    {
        juce::ScopedLock sl(lock);
        editTrack = juce::jlimit(0, kDawTracks - 1, t);
    }

    int getPatternIndex() const { juce::ScopedLock sl(lock); return patternIndex; }
    void setPatternIndex(int i)
    {
        juce::ScopedLock sl(lock);
        if (song.patterns.empty())
            return;
        patternIndex = juce::jlimit(0, (int) song.patterns.size() - 1, i);
    }

    int numPatterns() const { juce::ScopedLock sl(lock); return (int) song.patterns.size(); }

    juce::String patternName(int i) const
    {
        juce::ScopedLock sl(lock);
        if (i < 0 || i >= (int) song.patterns.size())
            return {};
        return song.patterns[(size_t) i].name;
    }

    int patternLength(int i) const
    {
        juce::ScopedLock sl(lock);
        if (i < 0 || i >= (int) song.patterns.size())
            return kDawBar;
        return song.patterns[(size_t) i].length;
    }

    void setPatternLength(int i, int len)
    {
        juce::ScopedLock sl(lock);
        if (i < 0 || i >= (int) song.patterns.size())
            return;
        song.patterns[(size_t) i].length = juce::jmax(480, len);
        dirty = true;
        ++gen;
    }

    int addPattern()
    {
        juce::ScopedLock sl(lock);
        DawPattern p;
        p.name = "P" + juce::String((int) song.patterns.size() + 1);
        p.length = kDawBar;
        song.patterns.push_back(p);
        patternIndex = (int) song.patterns.size() - 1;
        dirty = true;
        ++gen;
        return patternIndex;
    }

    double getPosTick() const { juce::ScopedLock sl(lock); return posTick; }
    int getLoopEnd() const { juce::ScopedLock sl(lock); return song.loopEnd; }
    void setLoopEnd(int t)
    {
        juce::ScopedLock sl(lock);
        song.loopEnd = juce::jmax(kDawBar, t);
        dirty = true;
    }
    int getGen() const { juce::ScopedLock sl(lock); return gen; }
    bool consumeDirty() { juce::ScopedLock sl(lock); bool d = dirty; dirty = false; return d; }
    bool isGlobalOpen() const { juce::ScopedLock sl(lock); return song.globalOpen; }
    void setGlobalOpen(bool v)
    {
        juce::ScopedLock sl(lock);
        song.globalOpen = v;
        dirty = true;
        ++gen;
    }

    void play()
    {
        juce::ScopedLock sl(lock);
        playing = true;
        panic = true;
        if (posTick >= (double) songLengthUnlocked() - 1.0)
            posTick = 0.0;
    }

    void pause()
    {
        juce::ScopedLock sl(lock);
        flushRecUnlocked();
        playing = false;
        panic = true;
    }

    void stop()
    {
        juce::ScopedLock sl(lock);
        flushRecUnlocked();
        playing = false;
        posTick = 0.0;
        panic = true;
        voices.clear();
    }

    void seekTick(double t)
    {
        juce::ScopedLock sl(lock);
        posTick = juce::jlimit(0.0, (double) songLengthUnlocked(), t);
        panic = true;
        voices.clear();
    }

    DawSong copySong() const { juce::ScopedLock sl(lock); return song; }

    DawPattern copyPattern(int i) const
    {
        juce::ScopedLock sl(lock);
        if (i < 0 || i >= (int) song.patterns.size())
            return {};
        return song.patterns[(size_t) i];
    }

    void replacePattern(int i, const DawPattern& p)
    {
        juce::ScopedLock sl(lock);
        if (i < 0 || i >= (int) song.patterns.size())
            return;
        song.patterns[(size_t) i] = p;
        dirty = true;
        ++gen;
    }

    DawTrack copyTrack(int t) const
    {
        juce::ScopedLock sl(lock);
        return song.tracks[juce::jlimit(0, kDawTracks - 1, t)];
    }

    void replaceTrack(int t, const DawTrack& tr)
    {
        juce::ScopedLock sl(lock);
        t = juce::jlimit(0, kDawTracks - 1, t);
        song.tracks[t] = tr;
        dirty = true;
        ++gen;
    }

    void addClip(int track, int pattern, int start)
    {
        juce::ScopedLock sl(lock);
        track = juce::jlimit(0, kDawTracks - 1, track);
        if (song.patterns.empty())
            return;
        DawClip c;
        c.pattern = juce::jlimit(0, (int) song.patterns.size() - 1, pattern);
        c.start = juce::jmax(0, start);
        c.length = song.patterns[(size_t) c.pattern].length;
        song.tracks[track].clips.push_back(c);
        editTrack = track;
        dirty = true;
        ++gen;
    }

    void removeClip(int track, int index)
    {
        juce::ScopedLock sl(lock);
        track = juce::jlimit(0, kDawTracks - 1, track);
        auto& clips = song.tracks[track].clips;
        if (index < 0 || index >= (int) clips.size())
            return;
        clips.erase(clips.begin() + index);
        dirty = true;
        ++gen;
    }

    void moveClip(int track, int index, int start)
    {
        juce::ScopedLock sl(lock);
        track = juce::jlimit(0, kDawTracks - 1, track);
        auto& clips = song.tracks[track].clips;
        if (index < 0 || index >= (int) clips.size())
            return;
        clips[(size_t) index].start = juce::jmax(0, start);
        dirty = true;
        ++gen;
    }

    void resizeClip(int track, int index, int start, int length)
    {
        juce::ScopedLock sl(lock);
        track = juce::jlimit(0, kDawTracks - 1, track);
        auto& clips = song.tracks[track].clips;
        if (index < 0 || index >= (int) clips.size())
            return;
        clips[(size_t) index].start = juce::jmax(0, start);
        clips[(size_t) index].length = juce::jmax(120, length);
        dirty = true;
        ++gen;
    }

    int addNote(int pattern, DawNote n)
    {
        juce::ScopedLock sl(lock);
        if (pattern < 0 || pattern >= (int) song.patterns.size())
            return -1;
        auto& pat = song.patterns[(size_t) pattern];
        const int plen = juce::jmax(1, pat.length);
        n.pitch = juce::jlimit(0, 127, n.pitch);
        n.start = juce::jlimit(0, plen - 1, n.start);
        n.dur = juce::jlimit(1, plen - n.start, n.dur);
        n.vel = juce::jlimit(1, 127, n.vel);
        pat.notes.push_back(n);
        dirty = true;
        ++gen;
        return (int) pat.notes.size() - 1;
    }

    bool isRecording() const { juce::ScopedLock sl(lock); return recording; }

    void setRecording(bool v)
    {
        juce::ScopedLock sl(lock);
        if (! v)
            flushRecUnlocked();
        recording = v;
        ++gen;
    }

    bool isRecArmed(int part) const
    {
        juce::ScopedLock sl(lock);
        part = juce::jlimit(0, kDawTracks - 1, part);
        return (recArm & (1u << part)) != 0;
    }

    void toggleRecArm(int part)
    {
        juce::ScopedLock sl(lock);
        part = juce::jlimit(0, kDawTracks - 1, part);
        recArm ^= (1u << part);
        ++gen;
    }

    // Live keyboard / host MIDI. Only armed tracks, and only while REC and PLAY are on.
    void recordLive(int part, bool noteOn, int pitch, int velocity)
    {
        juce::ScopedLock sl(lock);
        if (! recording || ! playing)
        {
            if (! recording)
                pendingRec.clear();
            return;
        }
        part = juce::jlimit(0, kDawTracks - 1, part);
        if ((recArm & (1u << part)) == 0)
            return;
        pitch = juce::jlimit(0, 127, pitch);
        if (! noteOn)
        {
            for (int i = 0; i < (int) pendingRec.size(); ++i)
            {
                if (pendingRec[(size_t) i].part != part || pendingRec[(size_t) i].pitch != pitch)
                    continue;
                commitRecUnlocked(pendingRec[(size_t) i], (int) posTick);
                pendingRec.erase(pendingRec.begin() + i);
                return;
            }
            return;
        }
        for (int i = 0; i < (int) pendingRec.size(); ++i)
        {
            if (pendingRec[(size_t) i].part == part && pendingRec[(size_t) i].pitch == pitch)
            {
                commitRecUnlocked(pendingRec[(size_t) i], (int) posTick);
                pendingRec.erase(pendingRec.begin() + i);
                break;
            }
        }
        RecHold hold;
        hold.part = part;
        hold.pitch = pitch;
        hold.vel = juce::jlimit(1, 127, velocity);
        hold.songTick = juce::jmax(0, (int) posTick);
        if (! prepareRecUnlocked(hold))
            return;
        pendingRec.push_back(hold);
    }

    void removeNote(int pattern, int index)
    {
        juce::ScopedLock sl(lock);
        if (pattern < 0 || pattern >= (int) song.patterns.size())
            return;
        auto& notes = song.patterns[(size_t) pattern].notes;
        if (index < 0 || index >= (int) notes.size())
            return;
        notes.erase(notes.begin() + index);
        dirty = true;
        ++gen;
    }

    void setNote(int pattern, int index, const DawNote& src)
    {
        juce::ScopedLock sl(lock);
        if (pattern < 0 || pattern >= (int) song.patterns.size())
            return;
        auto& notes = song.patterns[(size_t) pattern].notes;
        if (index < 0 || index >= (int) notes.size())
            return;
        auto n = src;
        const int plen = juce::jmax(1, song.patterns[(size_t) pattern].length);
        n.pitch = juce::jlimit(0, 127, n.pitch);
        n.start = juce::jlimit(0, plen - 1, n.start);
        n.dur = juce::jlimit(1, plen - n.start, n.dur);
        n.vel = juce::jlimit(1, 127, n.vel);
        notes[(size_t) index] = n;
        dirty = true;
        ++gen;
    }

    void toggleLane(int track, int which)
    {
        juce::ScopedLock sl(lock);
        track = juce::jlimit(0, kDawTracks - 1, track);
        which = juce::jlimit(0, kDawLanes - 1, which);
        song.tracks[track].lanes[which].open = ! song.tracks[track].lanes[which].open;
        dirty = true;
        ++gen;
    }

    void setLaneOpen(int track, int which, bool open)
    {
        juce::ScopedLock sl(lock);
        track = juce::jlimit(0, kDawTracks - 1, track);
        which = juce::jlimit(0, kDawLanes - 1, which);
        song.tracks[track].lanes[which].open = open;
        dirty = true;
        ++gen;
    }

    void foldAllLanes(int track, bool openCutoffOnly)
    {
        juce::ScopedLock sl(lock);
        track = juce::jlimit(0, kDawTracks - 1, track);
        bool any = false;
        for (int L = 0; L < kDawLanes; ++L)
            if (song.tracks[track].lanes[L].open)
                any = true;
        for (int L = 0; L < kDawLanes; ++L)
            song.tracks[track].lanes[L].open = false;
        if (! any && openCutoffOnly)
            song.tracks[track].lanes[0].open = true;
        dirty = true;
        ++gen;
    }

    int addAutoPt(int track, int which, int tick, float value)
    {
        juce::ScopedLock sl(lock);
        auto* lane = laneRef(track, which);
        if (lane == nullptr)
            return -1;
        DawAutoPt p;
        p.tick = juce::jmax(0, tick);
        p.value = juce::jlimit(0.0f, 1.0f, value);
        lane->pts.push_back(p);
        std::sort(lane->pts.begin(), lane->pts.end(),
                  [](const DawAutoPt& a, const DawAutoPt& b) { return a.tick < b.tick; });
        dirty = true;
        ++gen;
        int idx = 0;
        for (int i = 0; i < (int) lane->pts.size(); ++i)
            if (lane->pts[(size_t) i].tick == p.tick && lane->pts[(size_t) i].value == p.value)
                idx = i;
        return idx;
    }

    int setAutoPt(int track, int which, int index, int tick, float value)
    {
        juce::ScopedLock sl(lock);
        auto* lane = laneRef(track, which);
        if (lane == nullptr || index < 0 || index >= (int) lane->pts.size())
            return index;
        tick = juce::jmax(0, tick);
        value = juce::jlimit(0.0f, 1.0f, value);
        lane->pts[(size_t) index].tick = tick;
        lane->pts[(size_t) index].value = value;
        std::sort(lane->pts.begin(), lane->pts.end(),
                  [](const DawAutoPt& a, const DawAutoPt& b) { return a.tick < b.tick; });
        dirty = true;
        ++gen;
        int idx = 0;
        for (int i = 0; i < (int) lane->pts.size(); ++i)
            if (lane->pts[(size_t) i].tick == tick)
                idx = i;
        return idx;
    }

    void removeAutoPt(int track, int which, int index)
    {
        juce::ScopedLock sl(lock);
        auto* lane = laneRef(track, which);
        if (lane == nullptr || index < 0 || index >= (int) lane->pts.size())
            return;
        lane->pts.erase(lane->pts.begin() + index);
        dirty = true;
        ++gen;
    }

    int songLengthUnlocked() const
    {
        int end = song.loopEnd;
        for (int t = 0; t < kDawTracks; ++t)
            for (const auto& c : song.tracks[t].clips)
            {
                int len = kDawBar;
                if (c.pattern >= 0 && c.pattern < (int) song.patterns.size())
                    len = clipSpan(c, song.patterns[(size_t) c.pattern]);
                end = juce::jmax(end, c.start + len);
            }
        if (! songMode && patternIndex >= 0 && patternIndex < (int) song.patterns.size())
            end = juce::jmax(end, song.patterns[(size_t) patternIndex].length);
        return juce::jmax(kDawBar, end);
    }

    int songLength() const { juce::ScopedLock sl(lock); return songLengthUnlocked(); }

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        juce::ScopedLock sl(lock);
        auto xml = std::make_unique<juce::XmlElement>("mesd80song");
        xml->setAttribute("title", song.title);
        xml->setAttribute("bpm", song.bpm);
        xml->setAttribute("ppq", song.ppq);
        xml->setAttribute("loopEnd", song.loopEnd);
        xml->setAttribute("songMode", songMode ? 1 : 0);
        xml->setAttribute("pattern", patternIndex);
        xml->setAttribute("editTrack", editTrack);
        xml->setAttribute("globalOpen", song.globalOpen ? 1 : 0);
        for (int i = 0; i < (int) song.patterns.size(); ++i)
        {
            const auto& p = song.patterns[(size_t) i];
            auto* pe = xml->createNewChildElement("pattern");
            pe->setAttribute("name", p.name);
            pe->setAttribute("length", p.length);
            for (const auto& n : p.notes)
            {
                auto* ne = pe->createNewChildElement("n");
                ne->setAttribute("p", n.pitch);
                ne->setAttribute("s", n.start);
                ne->setAttribute("d", n.dur);
                ne->setAttribute("v", n.vel);
                ne->setAttribute("pan", n.pan);
                ne->setAttribute("sl", n.slide ? 1 : 0);
            }
        }
        for (int t = 0; t < kDawTracks; ++t)
        {
            auto* te = xml->createNewChildElement("track");
            te->setAttribute("i", t);
            for (const auto& c : song.tracks[t].clips)
            {
                auto* ce = te->createNewChildElement("c");
                ce->setAttribute("pat", c.pattern);
                ce->setAttribute("s", c.start);
                ce->setAttribute("len", c.length);
            }
            writeAuto(*te, "cutoff", song.tracks[t].lanes[0]);
            writeAuto(*te, "reso", song.tracks[t].lanes[1]);
            writeAuto(*te, "vol", song.tracks[t].lanes[2]);
            writeAuto(*te, "pan", song.tracks[t].lanes[3]);
            writeAuto(*te, "rev", song.tracks[t].lanes[4]);
            writeAuto(*te, "cho", song.tracks[t].lanes[5]);
            writeAuto(*te, "mfx", song.tracks[t].lanes[6]);
        }
        auto* ge = xml->createNewChildElement("globals");
        writeAuto(*ge, "tempo", song.globals[0]);
        writeAuto(*ge, "masvol", song.globals[1]);
        writeAuto(*ge, "revtime", song.globals[2]);
        writeAuto(*ge, "chorate", song.globals[3]);
        writeAuto(*ge, "chodepth", song.globals[4]);
        return xml;
    }

    bool fromXml(const juce::XmlElement& xml)
    {
        if (! xml.hasTagName("mesd80song"))
            return false;
        juce::ScopedLock sl(lock);
        song.title = xml.getStringAttribute("title", "Untitled");
        song.bpm = xml.getDoubleAttribute("bpm", 120.0);
        song.ppq = xml.getIntAttribute("ppq", kDawPpq);
        song.loopEnd = xml.getIntAttribute("loopEnd", kDawBar * 4);
        songMode = xml.getIntAttribute("songMode", 1) != 0;
        song.globalOpen = xml.getIntAttribute("globalOpen") != 0;
        song.patterns.clear();
        for (int t = 0; t < kDawTracks; ++t)
        {
            song.tracks[t].clips.clear();
            for (int L = 0; L < kDawLanes; ++L)
            {
                song.tracks[t].lanes[L].cc = kDawLaneCc[L];
                song.tracks[t].lanes[L].open = false;
                song.tracks[t].lanes[L].pts.clear();
            }
        }
        for (int g = 0; g < kDawGlobals; ++g)
        {
            song.globals[g].cc = g;
            song.globals[g].open = false;
            song.globals[g].pts.clear();
        }
        for (auto* pe : xml.getChildWithTagNameIterator("pattern"))
        {
            DawPattern p;
            p.name = pe->getStringAttribute("name", "P");
            p.length = pe->getIntAttribute("length", kDawBar);
            for (auto* ne : pe->getChildWithTagNameIterator("n"))
            {
                DawNote n;
                n.pitch = juce::jlimit(0, 127, ne->getIntAttribute("p", 60));
                n.start = juce::jmax(0, ne->getIntAttribute("s"));
                n.dur = juce::jmax(1, ne->getIntAttribute("d", 120));
                n.vel = juce::jlimit(1, 127, ne->getIntAttribute("v", 100));
                n.pan = juce::jlimit(-64, 63, ne->getIntAttribute("pan"));
                n.slide = ne->getIntAttribute("sl") != 0;
                p.notes.push_back(n);
            }
            song.patterns.push_back(p);
        }
        if (song.patterns.empty())
        {
            DawPattern p;
            p.name = "P1";
            song.patterns.push_back(p);
        }
        patternIndex = juce::jlimit(0, (int) song.patterns.size() - 1, xml.getIntAttribute("pattern"));
        editTrack = juce::jlimit(0, 31, xml.getIntAttribute("editTrack"));
        for (auto* te : xml.getChildWithTagNameIterator("track"))
        {
            const int t = juce::jlimit(0, 31, te->getIntAttribute("i"));
            for (auto* ce : te->getChildWithTagNameIterator("c"))
            {
                DawClip c;
                c.pattern = ce->getIntAttribute("pat");
                c.start = ce->getIntAttribute("s");
                c.length = ce->getIntAttribute("len");
                song.tracks[t].clips.push_back(c);
            }
            readAuto(*te, "cutoff", song.tracks[t].lanes[0]);
            readAuto(*te, "reso", song.tracks[t].lanes[1]);
            readAuto(*te, "vol", song.tracks[t].lanes[2]);
            readAuto(*te, "pan", song.tracks[t].lanes[3]);
            readAuto(*te, "rev", song.tracks[t].lanes[4]);
            readAuto(*te, "cho", song.tracks[t].lanes[5]);
            readAuto(*te, "mfx", song.tracks[t].lanes[6]);
        }
        if (auto* ge = xml.getChildByName("globals"))
        {
            readAuto(*ge, "tempo", song.globals[0]);
            readAuto(*ge, "masvol", song.globals[1]);
            readAuto(*ge, "revtime", song.globals[2]);
            readAuto(*ge, "chorate", song.globals[3]);
            readAuto(*ge, "chodepth", song.globals[4]);
        }
        posTick = 0.0;
        playing = false;
        panic = true;
        voices.clear();
        dirty = false;
        lastAutoTick = 0;
        for (int t = 0; t < kDawTracks; ++t)
            for (int L = 0; L < kDawLanes; ++L)
                lastCc[t][L] = -1;
        for (int g = 0; g < kDawGlobals; ++g)
            lastG[g] = -1;
        ++gen;
        return true;
    }

    bool fromXmlString(const juce::String& s)
    {
        if (s.isEmpty())
            return false;
        if (auto xml = juce::XmlDocument::parse(s))
            return fromXml(*xml);
        return false;
    }

    bool importMidi(const juce::File& file, int /*destTrack*/)
    {
        juce::FileInputStream in(file);
        if (! in.openedOk())
            return false;
        juce::MidiFile midi;
        if (! midi.readFrom(in))
            return false;
        midi.convertTimestampTicksToSeconds();

        double bpmNow = 120.0;
        {
            juce::ScopedLock sl(lock);
            bpmNow = song.bpm;
        }
        auto secToTick = [&](double sec) -> int
        {
            return juce::jmax(0, (int) std::llround(sec * (bpmNow / 60.0) * (double) kDawPpq));
        };

        struct On
        {
            int pitch;
            int vel;
            double t;
        };
        std::vector<On> ons[32];
        std::vector<DawNote> notes[32];
        std::vector<DawAutoPt> autos[32][kDawLanes];
        int lastCcIn[32][kDawLanes];
        for (int p = 0; p < 32; ++p)
            for (int L = 0; L < kDawLanes; ++L)
                lastCcIn[p][L] = -1;
        std::vector<DawAutoPt> tempoPts;
        std::vector<DawAutoPt> volPts;
        int lastTempoV = -1;
        int lastVolV = -1;

        for (int t = 0; t < midi.getNumTracks(); ++t)
        {
            auto* tr = midi.getTrack(t);
            if (tr == nullptr)
                continue;
            int port = 0;
            for (int i = 0; i < tr->getNumEvents(); ++i)
            {
                const auto& m = tr->getEventPointer(i)->message;
                if (m.isMetaEvent() && m.getMetaEventType() == 0x21)
                {
                    const auto* raw = m.getRawData();
                    const int sz = m.getRawDataSize();
                    if (sz >= 1)
                        port = raw[sz - 1] >= 1 ? 1 : 0;
                }
                if (m.isTempoMetaEvent())
                {
                    const double spq = m.getTempoSecondsPerQuarterNote();
                    if (spq > 1.0e-6)
                    {
                        bpmNow = juce::jlimit(20.0, 300.0, 60.0 / spq);
                        const int tick = secToTick(m.getTimeStamp());
                        const int iv = (int) std::llround((bpmNow - 20.0) / 280.0 * 127.0);
                        if (iv != lastTempoV)
                        {
                            DawAutoPt pt;
                            pt.tick = tick;
                            pt.value = (float) ((bpmNow - 20.0) / 280.0);
                            tempoPts.push_back(pt);
                            lastTempoV = iv;
                        }
                    }
                }
                if (m.isSysEx())
                {
                    const juce::uint8* d = m.getSysExData();
                    const int n = m.getSysExDataSize();
                    // Universal master volume F0 7F 7F 04 01 ll vv F7
                    if (n >= 6 && d[0] == 0x7F && d[2] == 0x04 && d[3] == 0x01)
                    {
                        const int vv = d[5];
                        if (vv != lastVolV)
                        {
                            DawAutoPt pt;
                            pt.tick = secToTick(m.getTimeStamp());
                            pt.value = (float) vv / 127.0f;
                            volPts.push_back(pt);
                            lastVolV = vv;
                        }
                    }
                }

                const int ch = m.getChannel();
                if (ch < 1 || ch > 16)
                    continue;
                const int part = juce::jlimit(0, 31, (port >= 1 ? 16 : 0) + (ch - 1));

                if (m.isController())
                {
                    const int lane = laneForCc(m.getControllerNumber());
                    if (lane < 0)
                        continue;
                    const int cv = m.getControllerValue();
                    if (cv == lastCcIn[part][lane])
                        continue;
                    lastCcIn[part][lane] = cv;
                    DawAutoPt pt;
                    pt.tick = secToTick(m.getTimeStamp());
                    pt.value = (float) cv / 127.0f;
                    autos[part][lane].push_back(pt);
                    continue;
                }

                if (m.isNoteOn() && m.getVelocity() > 0)
                {
                    On o;
                    o.pitch = m.getNoteNumber();
                    o.vel = m.getVelocity();
                    o.t = m.getTimeStamp();
                    ons[part].push_back(o);
                }
                else if (m.isNoteOff() || (m.isNoteOn() && m.getVelocity() == 0))
                {
                    const int pitch = m.getNoteNumber();
                    const double tOff = m.getTimeStamp();
                    auto& stack = ons[part];
                    for (int k = (int) stack.size() - 1; k >= 0; --k)
                    {
                        if (stack[(size_t) k].pitch != pitch)
                            continue;
                        DawNote n;
                        n.pitch = pitch;
                        n.start = secToTick(stack[(size_t) k].t);
                        n.dur = juce::jmax(1, secToTick(tOff) - n.start);
                        n.vel = stack[(size_t) k].vel;
                        notes[part].push_back(n);
                        stack.erase(stack.begin() + k);
                        break;
                    }
                }
            }
        }

        juce::ScopedLock sl(lock);
        song.bpm = bpmNow;
        const juce::String stem = file.getFileNameWithoutExtension();
        bool any = false;
        for (int part = 0; part < kDawTracks; ++part)
        {
            bool emptyAuto = true;
            for (int L = 0; L < kDawLanes; ++L)
                if (! autos[part][L].empty())
                    emptyAuto = false;
            if (notes[part].empty() && emptyAuto)
                continue;
            DawPattern p;
            const char bank = part < 16 ? 'A' : 'B';
            p.name = juce::String::charToString(bank)
                         + juce::String((part % 16) + 1).paddedLeft('0', 2)
                         + " " + stem;
            p.length = kDawBar;
            p.notes = notes[part];
            for (const auto& n : p.notes)
                p.length = juce::jmax(p.length, n.start + n.dur);
            p.length = juce::jmax(kDawBar, ((p.length + kDawBar - 1) / kDawBar) * kDawBar);
            song.patterns.push_back(p);
            const int pi = (int) song.patterns.size() - 1;
            DawClip c;
            c.pattern = pi;
            c.start = 0;
            c.length = p.length;
            song.tracks[part].clips.push_back(c);
            for (int L = 0; L < kDawLanes; ++L)
            {
                if (autos[part][L].empty())
                    continue;
                song.tracks[part].lanes[L].pts = autos[part][L];
                song.tracks[part].lanes[L].open = false;
            }
            if (! any)
            {
                patternIndex = pi;
                editTrack = part;
                any = true;
            }
        }
        if (! any)
        {
            DawPattern p;
            p.name = stem;
            p.length = kDawBar;
            song.patterns.push_back(p);
            patternIndex = (int) song.patterns.size() - 1;
        }
        if (! tempoPts.empty())
        {
            song.globals[0].pts = tempoPts;
            song.globals[0].open = false;
            song.globalOpen = false;
        }
        if (! volPts.empty())
        {
            song.globals[1].pts = volPts;
            song.globals[1].open = false;
            song.globalOpen = false;
        }
        dirty = true;
        ++gen;
        return true;
    }

    bool exportMidi(const juce::File& fileA, const juce::File& fileB,
                    const std::function<void(int part, juce::MidiMessageSequence&)>& stampPatch)
    {
        const juce::ScopedLock sl(lock);
        auto writeSide = [&](const juce::File& file, int base) -> bool
        {
            juce::MidiFile midi;
            midi.setTicksPerQuarterNote(kDawPpq);
            auto* tempo = new juce::MidiMessageSequence();
            tempo->addEvent(juce::MidiMessage::tempoMetaEvent((int) std::llround(60000000.0 / song.bpm)));
            midi.addTrack(*tempo);
            delete tempo;
            bool any = false;
            for (int t = 0; t < 16; ++t)
            {
                const int part = base + t;
                juce::MidiMessageSequence seq;
                if (stampPatch)
                    stampPatch(part, seq);
                const int ch = t + 1;
                for (const auto& c : song.tracks[part].clips)
                {
                    if (c.pattern < 0 || c.pattern >= (int) song.patterns.size())
                        continue;
                    const auto& pat = song.patterns[(size_t) c.pattern];
                    const int span = clipSpan(c, pat);
                    const int plen = juce::jmax(1, pat.length);
                    for (int loop = 0; loop * plen < span; ++loop)
                    {
                        for (const auto& n : pat.notes)
                        {
                            const int on = c.start + loop * plen + n.start;
                            if (n.start >= plen || on >= c.start + span)
                                continue;
                            any = true;
                            const int off = juce::jmin(on + n.dur, c.start + span);
                            seq.addEvent(juce::MidiMessage::noteOn(ch, n.pitch, (juce::uint8) n.vel),
                                         (double) on);
                            seq.addEvent(juce::MidiMessage::noteOff(ch, n.pitch), (double) off);
                        }
                    }
                    for (int L = 0; L < kDawLanes; ++L)
                        for (const auto& pt : song.tracks[part].lanes[L].pts)
                            seq.addEvent(juce::MidiMessage::controllerEvent(
                                             ch, song.tracks[part].lanes[L].cc,
                                             juce::jlimit(0, 127, (int) std::llround(pt.value * 127.0f))),
                                         (double) (c.start + pt.tick));
                }
                seq.updateMatchedPairs();
                midi.addTrack(seq);
            }
            if (! any && base == 16)
                return true;
            juce::FileOutputStream out(file);
            if (! out.openedOk())
                return false;
            out.setPosition(0);
            out.truncate();
            return midi.writeTo(out);
        };
        if (! writeSide(fileA, 0))
            return false;
        bool hasB = false;
        for (int t = 16; t < 32 && ! hasB; ++t)
            if (! song.tracks[t].clips.empty())
                hasB = true;
        if (hasB && fileB != juce::File())
            writeSide(fileB, 16);
        return true;
    }

    void render(double sampleRate, int numSamples,
                const std::function<void(const juce::MidiMessage&, int, MidiPort)>& emit)
    {
        juce::ScopedLock sl(lock);
        if (sampleRate <= 0.0)
            return;

        if (panic)
        {
            for (int ch = 1; ch <= 16; ++ch)
            {
                emit(juce::MidiMessage::allNotesOff(ch), 0, MidiPort::A);
                emit(juce::MidiMessage::allNotesOff(ch), 0, MidiPort::B);
                emit(juce::MidiMessage::pitchWheel(ch, 8192), 0, MidiPort::A);
                emit(juce::MidiMessage::pitchWheel(ch, 8192), 0, MidiPort::B);
            }
            voices.clear();
            panic = false;
            bendArmed = false;
            lastAutoTick = 0;
            for (int t = 0; t < kDawTracks; ++t)
                for (int L = 0; L < kDawLanes; ++L)
                    lastCc[t][L] = -1;
            for (int g = 0; g < kDawGlobals; ++g)
                lastG[g] = -1;
        }

        if (! playing)
            return;

        if (! bendArmed)
        {
            for (int ch = 1; ch <= 16; ++ch)
            {
                emit(juce::MidiMessage::controllerEvent(ch, 101, 0), 0, MidiPort::Both);
                emit(juce::MidiMessage::controllerEvent(ch, 100, 0), 0, MidiPort::Both);
                emit(juce::MidiMessage::controllerEvent(ch, 6, 12), 0, MidiPort::Both);
                emit(juce::MidiMessage::controllerEvent(ch, 38, 0), 0, MidiPort::Both);
            }
            bendArmed = true;
        }

        double bpmNow = song.bpm;
        const float tv = laneAt(song.globals[0], (int) posTick);
        if (tv >= 0.0f)
            bpmNow = 20.0 + (double) tv * 280.0;
        const double ticksPerSec = (bpmNow / 60.0) * (double) kDawPpq;
        const double blockSec = (double) numSamples / sampleRate;
        const double t0 = posTick;
        const double t1 = posTick + blockSec * ticksPerSec;
        const int loop = juce::jmax(kDawBar, songLengthUnlocked());

        collectOns((int) std::floor(t0), (int) std::ceil(t1), emit, sampleRate, t0, ticksPerSec, numSamples);
        expireVoices((int) std::floor(t1), emit, sampleRate, t0, ticksPerSec, numSamples);
        emitSlides((int) std::floor(t1), emit);
        emitAutos((int) std::floor(t1), emit);

        posTick = t1;
        if (posTick >= (double) loop)
        {
            posTick = 0.0;
            panic = true;
            voices.clear();
        }
    }

    void markDirty() { juce::ScopedLock sl(lock); dirty = true; ++gen; }

private:
    static float catmull(float p0, float p1, float p2, float p3, float t)
    {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return 0.5f * ((2.0f * p1) + (-p0 + p2) * t
                       + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                       + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    }

    DawAutoLane* laneRef(int track, int which)
    {
        if (track < 0)
        {
            which = juce::jlimit(0, kDawGlobals - 1, which);
            return &song.globals[which];
        }
        track = juce::jlimit(0, kDawTracks - 1, track);
        which = juce::jlimit(0, kDawLanes - 1, which);
        return &song.tracks[track].lanes[which];
    }

    static void writeAuto(juce::XmlElement& te, const char* tag, const DawAutoLane& lane)
    {
        auto* ae = te.createNewChildElement(tag);
        ae->setAttribute("cc", lane.cc);
        ae->setAttribute("open", lane.open ? 1 : 0);
        for (const auto& p : lane.pts)
        {
            auto* pe = ae->createNewChildElement("pt");
            pe->setAttribute("t", p.tick);
            pe->setAttribute("v", (double) p.value);
        }
    }

    static void readAuto(const juce::XmlElement& te, const char* tag, DawAutoLane& lane)
    {
        if (auto* ae = te.getChildByName(tag))
        {
            lane.cc = ae->getIntAttribute("cc", lane.cc);
            lane.open = ae->getIntAttribute("open") != 0;
            for (auto* pe : ae->getChildWithTagNameIterator("pt"))
            {
                DawAutoPt p;
                p.tick = pe->getIntAttribute("t");
                p.value = (float) pe->getDoubleAttribute("v", 0.5);
                lane.pts.push_back(p);
            }
        }
    }

    void collectOns(int t0, int t1,
                    const std::function<void(const juce::MidiMessage&, int, MidiPort)>& emit,
                    double sampleRate, double startTick, double ticksPerSec, int numSamples)
    {
        auto emitAt = [&](int tick, const juce::MidiMessage& m, MidiPort port)
        {
            int sample = (int) std::llround(((double) tick - startTick) / ticksPerSec * sampleRate);
            sample = juce::jlimit(0, numSamples - 1, sample);
            emit(m, sample, port);
        };

        if (! songMode)
        {
            if (patternIndex < 0 || patternIndex >= (int) song.patterns.size())
                return;
            const auto& pat = song.patterns[(size_t) patternIndex];
            const int part = editTrack;
            const int ch = (part % 16) + 1;
            const MidiPort port = part < 16 ? MidiPort::A : MidiPort::B;
            firePatternNotes(pat, 0, pat.length, t0, t1, part, ch, port, emitAt);
            return;
        }

        for (int part = 0; part < kDawTracks; ++part)
        {
            const int ch = (part % 16) + 1;
            const MidiPort port = part < 16 ? MidiPort::A : MidiPort::B;
            for (const auto& c : song.tracks[part].clips)
            {
                if (c.pattern < 0 || c.pattern >= (int) song.patterns.size())
                    continue;
                const auto& pat = song.patterns[(size_t) c.pattern];
                firePatternNotes(pat, c.start, clipSpan(c, pat), t0, t1, part, ch, port, emitAt);
            }
        }
    }

    void firePatternNotes(const DawPattern& pat, int clipStart, int clipLen, int t0, int t1,
                          int part, int ch, MidiPort port,
                          const std::function<void(int, const juce::MidiMessage&, MidiPort)>& emitAt)
    {
        const int plen = juce::jmax(1, pat.length);
        const int clipEnd = clipStart + juce::jmax(1, clipLen);
        for (int i = 0; i < (int) pat.notes.size(); ++i)
        {
            const auto& n = pat.notes[(size_t) i];
            if (n.start < 0 || n.start >= plen)
                continue;
            for (int loop = 0; loop * plen < clipLen; ++loop)
            {
                const int on = clipStart + loop * plen + n.start;
                if (on >= clipEnd)
                    break;
                if (on < t0 || on >= t1)
                    continue;
                emitAt(on, juce::MidiMessage::noteOn(ch, n.pitch, (juce::uint8) juce::jlimit(1, 127, n.vel)), port);
                if (n.pan != 0)
                    emitAt(on, juce::MidiMessage::controllerEvent(ch, 10, juce::jlimit(0, 127, n.pan + 64)), port);

                DawVoice v;
                v.part = part;
                v.pitch = n.pitch;
                v.startTick = on;
                v.offTick = juce::jmin(on + n.dur, clipEnd);
                v.slideTo = -1;
                v.slideEnd = v.offTick;
                if (n.slide)
                {
                    int tgt = n.pitch + 4;
                    int nearest = 0x7fffffff;
                    for (int j = 0; j < (int) pat.notes.size(); ++j)
                    {
                        if (j == i)
                            continue;
                        const auto& o = pat.notes[(size_t) j];
                        const int os = clipStart + loop * plen + o.start;
                        if (os > on && os <= on + n.dur + 60)
                        {
                            const int d = os - on;
                            if (d < nearest)
                            {
                                nearest = d;
                                tgt = o.pitch;
                                v.slideEnd = juce::jmin(os, clipEnd);
                            }
                        }
                    }
                    v.slideTo = tgt;
                }
                voices.push_back(v);
            }
        }
    }

    void expireVoices(int tNow,
                      const std::function<void(const juce::MidiMessage&, int, MidiPort)>& emit,
                      double sampleRate, double startTick, double ticksPerSec, int numSamples)
    {
        int w = 0;
        for (int i = 0; i < (int) voices.size(); ++i)
        {
            auto v = voices[(size_t) i];
            if (v.offTick <= tNow)
            {
                const int ch = (v.part % 16) + 1;
                const MidiPort port = v.part < 16 ? MidiPort::A : MidiPort::B;
                int sample = (int) std::llround(((double) v.offTick - startTick) / ticksPerSec * sampleRate);
                sample = juce::jlimit(0, numSamples - 1, sample);
                emit(juce::MidiMessage::noteOff(ch, v.pitch), sample, port);
                if (v.slideTo >= 0)
                    emit(juce::MidiMessage::pitchWheel(ch, 8192), sample, port);
            }
            else
            {
                voices[(size_t) w] = v;
                ++w;
            }
        }
        voices.resize((size_t) w);
    }

    void emitSlides(int tNow, const std::function<void(const juce::MidiMessage&, int, MidiPort)>& emit)
    {
        for (const auto& v : voices)
        {
            if (v.slideTo < 0)
                continue;
            const int span = juce::jmax(1, v.slideEnd - v.startTick);
            const float u = juce::jlimit(0.0f, 1.0f, (float) (tNow - v.startTick) / (float) span);
            const float st = (float) (v.slideTo - v.pitch) * u;
            const int pb = juce::jlimit(0, 16383, 8192 + (int) std::llround(st / 12.0f * 8192.0f));
            const int ch = (v.part % 16) + 1;
            const MidiPort port = v.part < 16 ? MidiPort::A : MidiPort::B;
            emit(juce::MidiMessage::pitchWheel(ch, pb), 0, port);
        }
    }

    void emitAutos(int tNow, const std::function<void(const juce::MidiMessage&, int, MidiPort)>& emit)
    {
        const int minGap = juce::jmax(1, (int) std::llround((song.bpm / 60.0) * (double) kDawPpq * 0.02));
        if (tNow - lastAutoTick < minGap && lastAutoTick != 0)
            return;
        lastAutoTick = tNow;
        for (int part = 0; part < kDawTracks; ++part)
        {
            const int ch = (part % 16) + 1;
            const MidiPort port = part < 16 ? MidiPort::A : MidiPort::B;
            for (int L = 0; L < kDawLanes; ++L)
            {
                const float v = laneAt(song.tracks[part].lanes[L], tNow);
                if (v < 0.0f)
                    continue;
                const int cc = juce::jlimit(0, 127, (int) std::llround(v * 127.0f));
                if (cc == lastCc[part][L])
                    continue;
                lastCc[part][L] = cc;
                emit(juce::MidiMessage::controllerEvent(ch, song.tracks[part].lanes[L].cc, cc), 0, port);
            }
        }
        {
            const float v = laneAt(song.globals[1], tNow);
            if (v >= 0.0f)
            {
                const int iv = juce::jlimit(0, 127, (int) std::llround(v * 127.0f));
                if (iv != lastG[1])
                {
                    lastG[1] = iv;
                    emit(sd80::gmMasterVolume(iv), 0, MidiPort::Both);
                }
            }
        }
        {
            const float v = laneAt(song.globals[2], tNow);
            if (v >= 0.0f)
            {
                const int iv = juce::jlimit(0, 127, (int) std::llround(v * 127.0f));
                if (iv != lastG[2])
                {
                    lastG[2] = iv;
                    emit(sd80::gm2ReverbParam(1, (std::uint8_t) iv), 0, MidiPort::A);
                }
            }
        }
        {
            const float v = laneAt(song.globals[3], tNow);
            if (v >= 0.0f)
            {
                const int iv = juce::jlimit(0, 127, (int) std::llround(v * 127.0f));
                if (iv != lastG[3])
                {
                    lastG[3] = iv;
                    emit(sd80::gm2ChorusParam(1, (std::uint8_t) iv), 0, MidiPort::A);
                }
            }
        }
        {
            const float v = laneAt(song.globals[4], tNow);
            if (v >= 0.0f)
            {
                const int iv = juce::jlimit(0, 127, (int) std::llround(v * 127.0f));
                if (iv != lastG[4])
                {
                    lastG[4] = iv;
                    emit(sd80::gm2ChorusParam(2, (std::uint8_t) iv), 0, MidiPort::A);
                }
            }
        }
    }

    mutable juce::CriticalSection lock;
    DawSong song;
    std::vector<DawVoice> voices;
    double posTick { 0.0 };
    int patternIndex { 0 };
    int editTrack { 0 };
    int gen { 0 };
    int lastAutoTick { 0 };
    int lastCc[kDawTracks][kDawLanes] {};
    int lastG[kDawGlobals] {};
    bool playing { false };
    bool songMode { true };
    bool panic { false };
    bool dirty { false };
    bool bendArmed { false };
    bool recording { false };
    std::uint32_t recArm { 0 };

    struct RecHold
    {
        int part { 0 };
        int pitch { 60 };
        int vel { 100 };
        int songTick { 0 };
        int pattern { -1 };
        int local { 0 };
        int clip { -1 };
    };
    std::vector<RecHold> pendingRec;

    void flushRecUnlocked()
    {
        const int now = juce::jmax(0, (int) posTick);
        for (auto& h : pendingRec)
            commitRecUnlocked(h, now);
        pendingRec.clear();
    }

    bool prepareRecUnlocked(RecHold& h)
    {
        const int tick = h.songTick;
        if (! songMode && h.part == editTrack
            && patternIndex >= 0 && patternIndex < (int) song.patterns.size())
        {
            h.pattern = patternIndex;
            h.clip = -1;
            h.local = juce::jmax(0, tick);
            growPatternUnlocked(h.pattern, h.local + 30);
            auto& pat = song.patterns[(size_t) h.pattern];
            h.local = juce::jlimit(0, juce::jmax(0, pat.length - 1), h.local);
            return true;
        }

        auto& clips = song.tracks[h.part].clips;
        for (int i = (int) clips.size() - 1; i >= 0; --i)
        {
            auto& c = clips[(size_t) i];
            if (c.pattern < 0 || c.pattern >= (int) song.patterns.size())
                continue;
            const auto& pat = song.patterns[(size_t) c.pattern];
            const int span = clipSpan(c, pat);
            if (tick < c.start || tick >= c.start + span)
                continue;
            h.pattern = c.pattern;
            h.clip = i;
            const int plen = juce::jmax(1, pat.length);
            int local = tick - c.start;
            if (local >= plen)
                local = local % plen;
            h.local = juce::jmax(0, local);
            return true;
        }

        DawPattern np;
        np.name = "P" + juce::String((int) song.patterns.size() + 1);
        np.length = kDawBar;
        song.patterns.push_back(std::move(np));
        h.pattern = (int) song.patterns.size() - 1;
        DawClip c;
        c.pattern = h.pattern;
        c.start = (tick / kDawBar) * kDawBar;
        c.length = kDawBar;
        clips.push_back(c);
        h.clip = (int) clips.size() - 1;
        h.local = juce::jlimit(0, kDawBar - 1, tick - c.start);
        return true;
    }

    void growPatternUnlocked(int pattern, int needEnd)
    {
        if (pattern < 0 || pattern >= (int) song.patterns.size())
            return;
        auto& pat = song.patterns[(size_t) pattern];
        if (needEnd <= pat.length)
            return;
        const int bars = ((needEnd + kDawBar - 1) / kDawBar) * kDawBar;
        pat.length = juce::jmin(kDawBar * 32, juce::jmax(pat.length, bars));
    }

    void commitRecUnlocked(const RecHold& h, int nowTick)
    {
        if (h.pattern < 0 || h.pattern >= (int) song.patterns.size())
            return;
        int dur = nowTick - h.songTick;
        if (dur < 40)
            dur = 40;
        growPatternUnlocked(h.pattern, h.local + dur);
        auto& pat = song.patterns[(size_t) h.pattern];
        DawNote n;
        n.pitch = h.pitch;
        n.start = juce::jlimit(0, juce::jmax(0, pat.length - 1), h.local);
        n.dur = juce::jlimit(1, juce::jmax(1, pat.length - n.start), dur);
        n.vel = juce::jlimit(1, 127, h.vel);
        pat.notes.push_back(n);
        if (h.clip >= 0 && h.part >= 0 && h.part < kDawTracks)
        {
            auto& clips = song.tracks[h.part].clips;
            if (h.clip < (int) clips.size())
            {
                auto& c = clips[(size_t) h.clip];
                const int need = n.start + n.dur;
                if (c.length > 0 && need > c.length)
                    c.length = juce::jmin(kDawBar * 32, ((need + kDawBar - 1) / kDawBar) * kDawBar);
            }
        }
        dirty = true;
        ++gen;
    }
};
