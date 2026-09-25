#pragma once

// Local user-patch model for the PATCH tab.
// SD-90 MIDI Implementation (model 00 48, Dec 2001): temporary patch of part 1
// is 11 00 00 00, each next part is +00 20 00 00 in 7-bit address space
// (part 32 = 18 60 00 00). Patch Tone 1 sits at +00 20 00.
// Wave group/number are NOT written — the base sound's factory patch owns the PCM.
// SD-80 user slots U-001..U-128 store MFX from the panel Write Patch, not this tone block.

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "SD80Sysex.h"

static const int kUserPatchSlots = 144; // 128 inst + 16 drum, local librarian

enum class PatchTarget { SD90 = 0, SD80 = 1, SD20 = 2 };

enum class PatchPush { All, Common, Tmt, ToneHead, EnvTvf, EnvTva, CcOnly };

struct UserTone
{
    bool on { true };
    int level { 100 };
    int coarse { 64 }; // sent clamped 16..112, 64 = 0
    int fine { 64 };   // sent clamped 14..114, 64 = 0
    int pan { 64 };
    int keyLo { 0 };
    int keyHi { 127 };
    int velLo { 1 };
    int velHi { 127 };
    int outMfx { 0 }; // 0 = MFX, 1 = direct (hardware Tone Output Assign)
    int chorus { 20 };
    int reverb { 30 };
    int tvfType { 1 }; // 0 OFF, 1 LPF, 2 BPF, 3 HPF, 4 PKG, 5 LPF2, 6 LPF3
    int cutoff { 90 };
    int reso { 12 };
    int envDepth { 96 }; // 64 = 0 semitone-style centre
    int tvfT[4] { 8, 50, 70, 40 };
    int tvfL[5] { 0, 127, 70, 50, 20 }; // L0..L4
    int tvaT[4] { 6, 40, 50, 35 };
    int tvaL[3] { 127, 96, 80 };
    int lfoWave { 0 };
};

struct UserPatch
{
    juce::String name { "User Patch" };
    int target { (int) PatchTarget::SD80 };
    int part { 0 };
    int level { 110 };
    int pan { 64 };
    int coarse { 64 };
    int fine { 64 };
    int baseMsb { 96 };
    int baseLsb { 0 };
    int basePc { 0 };
    bool baseDrum { false };
    juce::String baseName { "Piano 1" };
    UserTone tones[4];

    int audibleTones() const
    {
        return target == (int) PatchTarget::SD20 ? 2 : 4;
    }
};

struct UserPatchSlot
{
    bool used { false };
    UserPatch patch;
};

inline int clamp7(int v) { return juce::jlimit(0, 127, v); }

inline void addrAdd7(std::uint8_t b0, std::uint8_t b1, std::uint8_t b2, std::uint8_t b3,
                     int o0, int o1, int o2, int o3,
                     std::uint8_t& r0, std::uint8_t& r1, std::uint8_t& r2, std::uint8_t& r3)
{
    int c = (int) b3 + o3;
    r3 = (std::uint8_t) (c % 128); c /= 128;
    c += (int) b2 + o2;
    r2 = (std::uint8_t) (c % 128); c /= 128;
    c += (int) b1 + o1;
    r1 = (std::uint8_t) (c % 128); c /= 128;
    c += (int) b0 + o0;
    r0 = (std::uint8_t) (c % 128);
}

inline void tempPatchBase(int part, std::uint8_t& a0, std::uint8_t& a1, std::uint8_t& a2, std::uint8_t& a3)
{
    part = juce::jlimit(0, 31, part);
    addrAdd7(0x11, 0x00, 0x00, 0x00, 0, part * 0x20, 0, 0, a0, a1, a2, a3);
}

inline juce::MidiMessage patchDt1(std::uint8_t b0, std::uint8_t b1, std::uint8_t b2, std::uint8_t b3,
                                  int o0, int o1, int o2, int o3,
                                  const std::vector<std::uint8_t>& data)
{
    std::uint8_t r0, r1, r2, r3;
    addrAdd7(b0, b1, b2, b3, o0, o1, o2, o3, r0, r1, r2, r3);
    return sd80::dt1(r0, r1, r2, r3, data);
}

inline void pushCcPatch(std::vector<juce::MidiMessage>& out, const UserPatch& p)
{
    const int ch = (juce::jlimit(0, 31, p.part) % 16) + 1;
    const auto& t = p.tones[0];
    out.push_back(sd80::makeCc(ch, sd80::cc::cutoff, clamp7(t.cutoff)));
    out.push_back(sd80::makeCc(ch, sd80::cc::resonance, clamp7(t.reso)));
    out.push_back(sd80::makeCc(ch, sd80::cc::attack, clamp7(t.tvaT[0])));
    out.push_back(sd80::makeCc(ch, sd80::cc::decay, clamp7(t.tvaT[1])));
    out.push_back(sd80::makeCc(ch, sd80::cc::release, clamp7(t.tvaT[3])));
    const int n = p.audibleTones();
    for (int i = 0; i < 4; ++i)
    {
        const int lv = (i < n && p.tones[i].on) ? clamp7(p.tones[i].level) : 0;
        out.push_back(sd80::makeCc(ch, sd80::cc::gp5Tone1 + i, lv));
    }
}

inline std::vector<std::uint8_t> toneHeadBytes(const UserTone& t)
{
    return {
        (std::uint8_t) clamp7(t.level),
        (std::uint8_t) juce::jlimit(16, 112, t.coarse),
        (std::uint8_t) juce::jlimit(14, 114, t.fine)
    };
}

inline std::vector<juce::MidiMessage> userPatchMessages(const UserPatch& p, PatchPush what, int toneIndex, bool withBase)
{
    std::vector<juce::MidiMessage> out;
    const int part = juce::jlimit(0, 31, p.part);
    const int ch = (part % 16) + 1;

    if (p.target == (int) PatchTarget::SD20)
    {
        pushCcPatch(out, p);
        return out;
    }

    if (what == PatchPush::CcOnly)
    {
        pushCcPatch(out, p);
        return out;
    }

    if (withBase && what == PatchPush::All)
    {
        sd80::appendPatchSelect(out, ch,
                                (std::uint8_t) clamp7(p.baseMsb),
                                (std::uint8_t) clamp7(p.baseLsb),
                                (std::uint8_t) clamp7(p.basePc));
    }

    std::uint8_t b0, b1, b2, b3;
    tempPatchBase(part, b0, b1, b2, b3);

    auto add = [&](int o0, int o1, int o2, int o3, const std::vector<std::uint8_t>& d)
    {
        if (! d.empty())
            out.push_back(patchDt1(b0, b1, b2, b3, o0, o1, o2, o3, d));
    };

    if (what == PatchPush::All || what == PatchPush::Common)
    {
        std::vector<std::uint8_t> name(12, 0x20);
        const auto ascii = p.name.toRawUTF8();
        for (int i = 0; i < 12 && ascii[i] != 0; ++i)
        {
            const unsigned c = (unsigned char) ascii[i];
            name[(size_t) i] = (std::uint8_t) (c >= 32 && c < 127 ? c : 0x20);
        }
        add(0x00, 0x00, 0x00, 0x00, name);
        add(0x00, 0x00, 0x00, 0x0E, { (std::uint8_t) clamp7(p.level) });
        add(0x00, 0x00, 0x00, 0x0F, { (std::uint8_t) clamp7(p.pan) });
        add(0x00, 0x00, 0x00, 0x11, { (std::uint8_t) juce::jlimit(16, 112, p.coarse) });
        add(0x00, 0x00, 0x00, 0x12, { (std::uint8_t) juce::jlimit(14, 114, p.fine) });
    }

    if (what == PatchPush::All || what == PatchPush::Tmt)
    {
        std::vector<std::uint8_t> tmt;
        tmt.reserve(36);
        const int nAud = p.audibleTones();
        for (int i = 0; i < 4; ++i)
        {
            const auto& t = p.tones[i];
            const bool on = t.on && i < nAud;
            int lo = clamp7(t.keyLo), hi = clamp7(t.keyHi);
            if (hi < lo) std::swap(lo, hi);
            int vlo = juce::jlimit(1, 127, t.velLo), vhi = juce::jlimit(1, 127, t.velHi);
            if (vhi < vlo) std::swap(vlo, vhi);
            tmt.push_back(on ? 1 : 0);
            tmt.push_back((std::uint8_t) lo);
            tmt.push_back((std::uint8_t) hi);
            tmt.push_back(0);
            tmt.push_back(0);
            tmt.push_back((std::uint8_t) vlo);
            tmt.push_back((std::uint8_t) vhi);
            tmt.push_back(0);
            tmt.push_back(0);
        }
        // TMT block is patch + 00 10 00, tone 1 switch at +00 05
        add(0x00, 0x10, 0x00, 0x05, tmt);
    }

    auto pushTone = [&](int i, bool head, bool tvf, bool tva)
    {
        const auto& t = p.tones[i];
        const int toneOff = 0x20 + i * 0x02; // 00 20 / 22 / 24 / 26
        if (head)
        {
            add(0x00, toneOff, 0x00, 0x00, toneHeadBytes(t));
            add(0x00, toneOff, 0x00, 0x04, { (std::uint8_t) clamp7(t.pan) });
            add(0x00, toneOff, 0x00, 0x0C, {
                127,
                (std::uint8_t) clamp7(t.chorus),
                (std::uint8_t) clamp7(t.reverb),
                (std::uint8_t) clamp7(t.chorus),
                (std::uint8_t) clamp7(t.reverb),
                (std::uint8_t) (t.outMfx ? 1 : 0)
            });
            add(0x00, toneOff, 0x00, 0x48, { (std::uint8_t) juce::jlimit(0, 6, t.tvfType) });
            add(0x00, toneOff, 0x00, 0x49, { (std::uint8_t) clamp7(t.cutoff) });
            add(0x00, toneOff, 0x00, 0x4D, { (std::uint8_t) clamp7(t.reso) });
            add(0x00, toneOff, 0x00, 0x4F, { (std::uint8_t) juce::jlimit(1, 127, t.envDepth) });
            add(0x00, toneOff, 0x00, 0x6D, { (std::uint8_t) juce::jlimit(0, 10, t.lfoWave) });
        }
        if (tvf)
        {
            std::vector<std::uint8_t> env;
            for (int k = 0; k < 4; ++k) env.push_back((std::uint8_t) clamp7(t.tvfT[k]));
            for (int k = 0; k < 5; ++k) env.push_back((std::uint8_t) clamp7(t.tvfL[k]));
            add(0x00, toneOff, 0x00, 0x55, env);
        }
        if (tva)
        {
            std::vector<std::uint8_t> env;
            for (int k = 0; k < 4; ++k) env.push_back((std::uint8_t) clamp7(t.tvaT[k]));
            for (int k = 0; k < 3; ++k) env.push_back((std::uint8_t) clamp7(t.tvaL[k]));
            add(0x00, toneOff, 0x00, 0x66, env);
        }
    };

    if (what == PatchPush::All)
    {
        for (int i = 0; i < 4; ++i)
            pushTone(i, true, true, true);
        // CC 74/71/72/73/75 are relative offsets (64 = unchanged). SysEx already
        // wrote the absolute cutoff. Do not stack those CCs on top.
    }
    else if (what == PatchPush::ToneHead)
    {
        pushTone(juce::jlimit(0, 3, toneIndex), true, false, false);
    }
    else if (what == PatchPush::EnvTvf)
    {
        pushTone(juce::jlimit(0, 3, toneIndex), false, true, false);
    }
    else if (what == PatchPush::EnvTva)
    {
        pushTone(juce::jlimit(0, 3, toneIndex), false, false, true);
    }

    return out;
}

inline void writePatchXml(juce::XmlElement& xml, const UserPatch& p)
{
    xml.setAttribute("name", p.name);
    xml.setAttribute("target", p.target);
    xml.setAttribute("part", p.part);
    xml.setAttribute("level", p.level);
    xml.setAttribute("pan", p.pan);
    xml.setAttribute("coarse", p.coarse);
    xml.setAttribute("fine", p.fine);
    xml.setAttribute("baseMsb", p.baseMsb);
    xml.setAttribute("baseLsb", p.baseLsb);
    xml.setAttribute("basePc", p.basePc);
    xml.setAttribute("baseDrum", p.baseDrum ? 1 : 0);
    xml.setAttribute("baseName", p.baseName);
    for (int i = 0; i < 4; ++i)
    {
        const auto& t = p.tones[i];
        auto* tn = xml.createNewChildElement("tone");
        tn->setAttribute("i", i);
        tn->setAttribute("on", t.on ? 1 : 0);
        tn->setAttribute("level", t.level);
        tn->setAttribute("coarse", t.coarse);
        tn->setAttribute("fine", t.fine);
        tn->setAttribute("pan", t.pan);
        tn->setAttribute("keyLo", t.keyLo);
        tn->setAttribute("keyHi", t.keyHi);
        tn->setAttribute("velLo", t.velLo);
        tn->setAttribute("velHi", t.velHi);
        tn->setAttribute("outMfx", t.outMfx);
        tn->setAttribute("chorus", t.chorus);
        tn->setAttribute("reverb", t.reverb);
        tn->setAttribute("tvfType", t.tvfType);
        tn->setAttribute("cutoff", t.cutoff);
        tn->setAttribute("reso", t.reso);
        tn->setAttribute("envDepth", t.envDepth);
        tn->setAttribute("lfo", t.lfoWave);
        tn->setAttribute("tvfT", juce::String(t.tvfT[0]) + "," + juce::String(t.tvfT[1]) + ","
                                 + juce::String(t.tvfT[2]) + "," + juce::String(t.tvfT[3]));
        tn->setAttribute("tvfL", juce::String(t.tvfL[0]) + "," + juce::String(t.tvfL[1]) + ","
                                 + juce::String(t.tvfL[2]) + "," + juce::String(t.tvfL[3]) + ","
                                 + juce::String(t.tvfL[4]));
        tn->setAttribute("tvaT", juce::String(t.tvaT[0]) + "," + juce::String(t.tvaT[1]) + ","
                                 + juce::String(t.tvaT[2]) + "," + juce::String(t.tvaT[3]));
        tn->setAttribute("tvaL", juce::String(t.tvaL[0]) + "," + juce::String(t.tvaL[1]) + ","
                                 + juce::String(t.tvaL[2]));
    }
}

inline void readInts(const juce::String& s, int* dst, int n, const int* fallback)
{
    auto parts = juce::StringArray::fromTokens(s, ",", "");
    for (int i = 0; i < n; ++i)
        dst[i] = (i < parts.size()) ? parts[i].getIntValue() : fallback[i];
}

inline UserPatch readPatchXml(const juce::XmlElement& xml)
{
    UserPatch p;
    p.name = xml.getStringAttribute("name", p.name);
    p.target = juce::jlimit(0, 2, xml.getIntAttribute("target", p.target));
    p.part = juce::jlimit(0, 31, xml.getIntAttribute("part", 0));
    p.level = clamp7(xml.getIntAttribute("level", p.level));
    p.pan = clamp7(xml.getIntAttribute("pan", p.pan));
    p.coarse = clamp7(xml.getIntAttribute("coarse", p.coarse));
    p.fine = clamp7(xml.getIntAttribute("fine", p.fine));
    p.baseMsb = clamp7(xml.getIntAttribute("baseMsb", p.baseMsb));
    p.baseLsb = clamp7(xml.getIntAttribute("baseLsb", p.baseLsb));
    p.basePc = clamp7(xml.getIntAttribute("basePc", p.basePc));
    p.baseDrum = xml.getIntAttribute("baseDrum", 0) != 0;
    p.baseName = xml.getStringAttribute("baseName", p.baseName);
    for (auto* tn = xml.getFirstChildElement(); tn != nullptr; tn = tn->getNextElement())
    {
        if (! tn->hasTagName("tone"))
            continue;
        const int i = juce::jlimit(0, 3, tn->getIntAttribute("i", 0));
        auto& t = p.tones[i];
        const UserTone fb;
        t.on = tn->getIntAttribute("on", 1) != 0;
        t.level = clamp7(tn->getIntAttribute("level", t.level));
        t.coarse = clamp7(tn->getIntAttribute("coarse", t.coarse));
        t.fine = clamp7(tn->getIntAttribute("fine", t.fine));
        t.pan = clamp7(tn->getIntAttribute("pan", t.pan));
        t.keyLo = clamp7(tn->getIntAttribute("keyLo", t.keyLo));
        t.keyHi = clamp7(tn->getIntAttribute("keyHi", t.keyHi));
        t.velLo = juce::jlimit(1, 127, tn->getIntAttribute("velLo", t.velLo));
        t.velHi = juce::jlimit(1, 127, tn->getIntAttribute("velHi", t.velHi));
        t.outMfx = tn->getIntAttribute("outMfx", 0) ? 1 : 0;
        t.chorus = clamp7(tn->getIntAttribute("chorus", t.chorus));
        t.reverb = clamp7(tn->getIntAttribute("reverb", t.reverb));
        t.tvfType = juce::jlimit(0, 6, tn->getIntAttribute("tvfType", t.tvfType));
        t.cutoff = clamp7(tn->getIntAttribute("cutoff", t.cutoff));
        t.reso = clamp7(tn->getIntAttribute("reso", t.reso));
        t.envDepth = juce::jlimit(1, 127, tn->getIntAttribute("envDepth", t.envDepth));
        t.lfoWave = juce::jlimit(0, 10, tn->getIntAttribute("lfo", t.lfoWave));
        readInts(tn->getStringAttribute("tvfT"), t.tvfT, 4, fb.tvfT);
        readInts(tn->getStringAttribute("tvfL"), t.tvfL, 5, fb.tvfL);
        readInts(tn->getStringAttribute("tvaT"), t.tvaT, 4, fb.tvaT);
        readInts(tn->getStringAttribute("tvaL"), t.tvaL, 3, fb.tvaL);
    }
    return p;
}

inline const char* kTvfTypeName[] = { "OFF", "LPF", "BPF", "HPF", "PKG", "LPF2", "LPF3" };
inline const char* kLfoWaveName[] = {
    "SIN", "TRI", "SAW-UP", "SAW-DW", "SQR", "RND", "BEND-UP", "BEND-DW", "TRP", "S&H", "CHS"
};
