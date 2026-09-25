#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>
#include "AeternaAnimationData.h"
#include "BinaryData.h"
#include "MidiPlayer.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Optional Aeterna on the PLAYER piano roll.
// She stands on a note that is still coming in, and rides that point
// down the highway. On, she hops, dances, and rides the long notes.
// Off, she is hidden. She is not a mouse target.
class AeternaCompanion
{
public:
    // 0 off, 1 on. She follows the notes: hop, dance, ride the long ones.
    void setMode(int m)
    {
        m = juce::jlimit(0, 1, m);
        if (m == mode)
            return;
        mode = m;
        if (mode == 0)
            visible = false;
        clearRide();
    }

    int getMode() const { return mode; }

    // Playlist may keep her up after the first song, even at 0:00.
    void setLinger(bool v) { linger = v; }
    void setBpm(double b) { bpm = juce::jlimit(40.0, 240.0, b); }
    void setDrumMask(std::uint32_t m) { drums = m; }

    void noteScoreChanged() { clearRide(); }

    void noteSeekOrStop() { clearRide(); }

    void tick(double dtSec, double pos, double prevPos, bool playing, bool loaded,
              bool jumped, const std::vector<PlayerNote>& score, std::uint32_t silenced)
    {
        if (mode == 0 || ! loaded)
        {
            visible = false;
            pausedDraw = false;
            if (mode == 0)
                clearRide();
            return;
        }

        if (jumped || pos + 0.05 < prevPos)
            clearRide();

        // Pause freezes her where she is. The keys stay put, so she does too.
        if (! playing)
        {
            pausedDraw = visible;
            return;
        }
        pausedDraw = false;

        if (pos < 0.08 && ! linger)
        {
            visible = false;
            clearRide();
            return;
        }

        double dt = dtSec;
        if (! std::isfinite(dt) || dt < 0.0)
            dt = 0.0;
        if (dt > 0.25)
            dt = 0.016;

        const double speed = juce::jlimit(0.8, 1.55, bpm / 120.0);
        anim.advance(dt * 1000.0 * speed);
        if (sparkle > 0.0f)
            sparkle = juce::jmax(0.0f, sparkle - (float) (dt * 1.6));
        if (cool > 0.0)
            cool -= dt;
        if (impact > 0.0f)
            impact = juce::jmax(0.0f, impact - (float) (dt * 3.4));
        if (drumHold > 0.0)
            drumHold -= dt;
        if (anim.frameName() == "kira_pose_a" && ! sparkleArmed)
        {
            sparkleArmed = true;
            sparkle = 1.0f;
        }
        else if (anim.frameName() != "kira_pose_a")
            sparkleArmed = false;

        pollDrums(pos, prevPos, score, silenced);
        followMelody(dt, pos, score, silenced);
    }

    void paint(juce::Graphics& g, double pos,
               int spanLo, int span, const float keyX[128], const float keyWf[128],
               juce::Rectangle<float> keys, float zFar, float hitY, float highwayH,
               float vanishX, juce::Rectangle<float> plot)
    {
        if (mode == 0 || ! visible || ! placed)
            return;
        ensureAtlas();
        if (! atlas.isValid())
            return;

        const auto fr = anim.frame();
        const Support at = project(pos + (double) shownAhead, shownNote, pos, zFar, hitY, highwayH,
                                   vanishX, spanLo, span, keyX, keyWf, keys);
        float air = 0.0f;
        float spinDeg = 0.0f;
        const float gap = std::abs(goalNote - shownNote);
        if (gestureKind == 3 && leapSpan > 1.0f)
        {
            const float u = juce::jlimit(0.0f, 1.0f, 1.0f - gap / leapSpan);
            const float power = 0.65f + 0.7f * jumpPower;
            const float H = juce::jlimit(16.0f, 58.0f, (14.0f + leapSpan * 1.6f) * power);
            air = 4.0f * H * u * (1.0f - u);
            if (doFlip)
                spinDeg = (float) anim.rotationDegrees();
        }
        else if (gestureKind == 1)
            air = std::sin(bob * 7.2f) * 4.0f;
        else if (gestureKind == 2)
            air = std::abs(std::sin(bob * 10.0f)) * 5.0f;

        const float minP = pausedDraw ? 0.02f : 0.08f;
        if (at.p < minP || at.p > 0.88f)
            return;

        const auto clip = anim.stateName();
        const bool uprightSpin = clip == "spin" || clip == "spin_enter" || clip == "spin_exit"
                                 || clip == "spin_once";
        // A step smaller than 1.8.11, docked and fullscreen, so she sits on the note instead of covering the lane.
        const float h = plot.getHeight();
        const float t = juce::jlimit(0.0f, 1.0f, (h - 300.0f) / 520.0f);
        const float eased = t * t * (3.0f - 2.0f * t);
        float scale = (0.27f + 0.30f * eased) * (1.0f - 0.12f * at.p);
        scale = juce::jlimit(0.21f, 0.58f, scale);
        const float rootX = at.x;
        float dip = 0.0f;
        if (impact > 0.02f)
            dip = std::sin(impact * 3.14159265f) * 9.0f;
        const float rootY = at.y - air + dip;
        const bool mirror = facingLeft && ! uprightSpin;

        juce::Graphics::ScopedSaveState saved(g);
        auto allow = plot.withTop(plot.getY() - 200.0f);
        allow.setBottom(keys.getY() + 6.0f);
        g.reduceClipRegion(allow.toNearestInt());

        if (mirror || std::abs(spinDeg) > 0.5f)
        {
            auto xform = juce::AffineTransform::translation(-rootX, -rootY);
            if (mirror)
                xform = xform.followedBy(juce::AffineTransform::scale(-1.0f, 1.0f));
            if (std::abs(spinDeg) > 0.5f)
            {
                const float ox = (128.0f - (float) fr.pivotX) * scale;
                const float oy = (140.0f - (float) fr.pivotY) * scale;
                xform = xform.followedBy(juce::AffineTransform::rotation(
                    juce::degreesToRadians(spinDeg), ox, oy));
            }
            g.addTransform(xform.followedBy(juce::AffineTransform::translation(rootX, rootY)));
        }

        const float left = rootX - (float) fr.pivotX * scale;
        const float top = rootY - (float) fr.pivotY * scale;
        g.setOpacity(0.98f);
        g.drawImage(atlas,
                    (int) std::floor(left), (int) std::floor(top),
                    juce::jmax(1, (int) std::ceil(256.0f * scale)),
                    juce::jmax(1, (int) std::ceil(256.0f * scale)),
                    fr.x, fr.y, fr.w, fr.h);

        if (sparkle > 0.02f)
        {
            const float dir = mirror ? -1.0f : 1.0f;
            const float hx = rootX + dir * 18.0f * (scale / 0.52f);
            const float hy = rootY - 158.0f * scale;
            g.setColour(juce::Colour(0xfffff2c4).withAlpha(juce::jlimit(0.0f, 0.9f, sparkle)));
            g.fillEllipse(hx - 2.5f, hy - 2.5f, 5.0f, 5.0f);
            g.drawLine(hx - 6.0f, hy, hx + 6.0f, hy, 1.0f);
            g.drawLine(hx, hy - 6.0f, hx, hy + 6.0f, 1.0f);
        }
    }

private:
    struct Support
    {
        float x { 0 }, y { 0 }, p { 0 };
    };

    struct Ride
    {
        bool valid { false };
        int index { -1 };
        int note { 60 };
        int vel { 100 };
        double start { 0 };
        double anchor { 0 };
        double end { 0 };
    };

    static float perspectiveOf(float z, float zFar)
    {
        if (zFar <= 0.001f)
            return 0.0f;
        const float u = juce::jmax(0.0f, z / zFar);
        const float k = 2.6f;
        const float persp = 1.0f - 1.0f / (1.0f + k * u);
        const float perspFar = 1.0f - 1.0f / (1.0f + k);
        return persp / perspFar;
    }

    static float centerOf(float note, int spanLo, int span, const float keyX[128],
                          const float keyWf[128], juce::Rectangle<float> keys)
    {
        const int n0 = juce::jlimit(0, 127, (int) std::floor(note));
        const int n1 = juce::jmin(127, n0 + 1);
        const float frac = juce::jlimit(0.0f, 1.0f, note - (float) n0);
        const float x0 = centerAt(n0, spanLo, span, keyX, keyWf, keys);
        const float x1 = centerAt(n1, spanLo, span, keyX, keyWf, keys);
        return x0 + (x1 - x0) * frac;
    }

    static float centerAt(int note, int spanLo, int span, const float keyX[128],
                          const float keyWf[128], juce::Rectangle<float> keys)
    {
        if (note >= 0 && note < 128 && keyWf[note] > 0.5f)
            return keyX[note] + keyWf[note] * 0.5f;
        const float t = (float) (note - spanLo) / (float) juce::jmax(1, span);
        return keys.getX() + t * keys.getWidth();
    }

    static Support project(double timeSec, float note, double pos, float zFar, float hitY,
                           float highwayH, float vanishX, int spanLo, int span,
                           const float keyX[128], const float keyWf[128],
                           juce::Rectangle<float> keys)
    {
        float z = (float) (timeSec - pos);
        if (z < 0.0f)
            z = 0.0f;
        if (z > zFar)
            z = zFar;
        const float p = juce::jlimit(0.0f, 1.0f, perspectiveOf(z, zFar));
        const float xNear = centerOf(note, spanLo, span, keyX, keyWf, keys);
        Support s;
        s.x = vanishX + (xNear - vanishX) * (1.0f - 0.70f * p);
        s.y = hitY - p * highwayH;
        s.p = p;
        return s;
    }

    void ensureAtlas()
    {
        if (atlas.isValid() || atlasFailed)
            return;
        atlas = juce::ImageFileFormat::loadFrom(BinaryData::aeterna_atlas_png,
                                                BinaryData::aeterna_atlas_pngSize);
        atlasFailed = ! atlas.isValid();
    }

    void clearRide()
    {
        ride = {};
        gestureKind = 0;
        placed = false;
        doFlip = false;
        leapSpan = 1.0f;
        moveCharge = 0.0;
        sparkle = 0.0f;
        sparkleArmed = false;
        anim.play("idle");
    }

    static bool isLoop(std::string_view c)
    {
        return c == "walk" || c == "dash" || c == "dance" || c == "sit" || c == "ballet"
            || c == "spin" || c == "kira_hold" || c == "social_dance" || c == "refined_dance";
    }

    void playExit(std::string_view c)
    {
        if (c == "dance") anim.play("dance_exit");
        else if (c == "ballet") anim.play("ballet_exit");
        else if (c == "social_dance") anim.play("social_exit");
        else if (c == "refined_dance") anim.play("refined_exit");
        else if (c == "walk" || c == "dash") anim.play("walk_stop");
        else if (c == "sit") anim.play("get_up");
        else if (c == "kira_hold") anim.play("kira_exit");
        else if (c == "spin")
        {
            const auto fr = anim.frameName();
            if (fr == "spin_front" || fr == "spin_front_left" || fr == "spin_front_right")
                anim.play("spin_exit");
            else
                loopLeft = 0.12;
        }
    }

    static bool noteOk(const PlayerNote& hit, std::uint32_t silenced)
    {
        if (hit.note < 0 || hit.note > 127)
            return false;
        if (hit.channel < 1 || hit.channel > 16)
            return false;
        if ((silenced & (1u << (hit.channel - 1))) != 0)
            return false;
        return hit.endSec > hit.startSec + 0.015;
    }

    bool isDrum(const PlayerNote& hit) const
    {
        if (hit.channel < 1 || hit.channel > 16)
            return false;
        if (hit.channel == 10)
            return true;
        return (drums & (1u << (hit.channel - 1))) != 0;
    }

    int pickNote(double pos, const std::vector<PlayerNote>& score, std::uint32_t silenced) const
    {
        const double line = pos + 0.85;
        int ids[48];
        int n = 0;
        auto take = [&](int i)
        {
            if (n >= 48)
                return;
            ids[n++] = i;
        };
        for (int i = 0; i < (int) score.size(); ++i)
        {
            const auto& hit = score[(size_t) i];
            if (! noteOk(hit, silenced) || isDrum(hit))
                continue;
            if (hit.startSec > line || hit.endSec < line + 0.06)
                continue;
            take(i);
        }
        if (n < 1)
        {
            for (int i = 0; i < (int) score.size(); ++i)
            {
                const auto& hit = score[(size_t) i];
                if (! noteOk(hit, silenced) || isDrum(hit))
                    continue;
                if (hit.endSec < pos + 0.35 || hit.startSec > pos + 1.35)
                    continue;
                take(i);
            }
        }
        if (n < 1)
            return -1;
        int pitches[48];
        for (int i = 0; i < n; ++i)
            pitches[i] = score[(size_t) ids[i]].note;
        for (int a = 0; a < n; ++a)
            for (int b = a + 1; b < n; ++b)
                if (pitches[b] < pitches[a])
                {
                    const int tp = pitches[a];
                    pitches[a] = pitches[b];
                    pitches[b] = tp;
                }
        const int median = pitches[n / 2];
        int best = ids[0];
        int bestDist = 128;
        double bestLen = -1.0;
        for (int i = 0; i < n; ++i)
        {
            const auto& hit = score[(size_t) ids[i]];
            const int dist = hit.note > median ? hit.note - median : median - hit.note;
            const double len = hit.endSec - hit.startSec;
            if (dist < bestDist || (dist == bestDist && len > bestLen))
            {
                best = ids[i];
                bestDist = dist;
                bestLen = len;
            }
        }
        return best;
    }

    void followMelody(double dt, double pos, const std::vector<PlayerNote>& score, std::uint32_t silenced)
    {
        bob += (float) dt;
        const int next = pickNote(pos, score, silenced);
        if (next < 0)
        {
            if (! placed)
            {
                visible = false;
                return;
            }
            visible = true;
            settleDance(dt, nullptr, pos);
            return;
        }

        const auto& hit = score[(size_t) next];
        goalNote = (float) hit.note;
        ride.valid = true;
        ride.index = next;
        ride.note = hit.note;
        ride.vel = hit.velocity;
        ride.start = hit.startSec;
        ride.end = hit.endSec;
        ride.anchor = pos + (double) shownAhead;

        if (! placed)
        {
            shownNote = goalNote;
            shownAhead = 1.05f;
            placed = true;
            gestureKind = 0;
            moveCharge = 0.0;
            visible = true;
            cool = 0.4;
            anim.play("ballet_enter");
            loopLeft = 2.8;
            return;
        }

        float aheadTarget = 0.9f;
        const float bodyA = (float) (hit.startSec - pos);
        const float bodyB = (float) (hit.endSec - pos - 0.04);
        if (bodyB > bodyA + 0.02f)
        {
            const float mid = 0.5f * (bodyA + bodyB);
            aheadTarget = juce::jlimit(bodyA + 0.02f, bodyB, juce::jlimit(0.55f, 1.15f, mid));
        }
        const float depthK = (float) (1.0 - std::exp(-dt / 0.14));
        shownAhead += (aheadTarget - shownAhead) * depthK;
        if (shownAhead < 0.20f)
            shownAhead = 0.20f;
        if (shownAhead > 1.8f)
            shownAhead = 1.8f;

        const float delta = goalNote - shownNote;
        const float ad = std::abs(delta);
        if (ad > 0.35f)
        {
            const float dur = ad >= 12.0f ? 0.32f : ad >= 7.0f ? 0.26f : 0.18f;
            const float rate = juce::jlimit(10.0f, 72.0f, ad / dur);
            const float cap = (float) dt * rate;
            float step = delta;
            if (step > cap)
                step = cap;
            if (step < -cap)
                step = -cap;
            shownNote += step;
        }
        else
        {
            shownNote = goalNote;
        }

        visible = true;
        const float gap = std::abs(goalNote - shownNote);
        if (gestureKind == 0)
        {
            // A couple of semitones is still the same pose. Only a real move starts a new one.
            if (gap >= 4.0f)
                beginGesture(gap, hit.velocity);
            else
                settleDance(dt, &hit, pos);
        }
        else if (gap < 0.75f)
        {
            arrive(hit.velocity);
        }
        else
        {
            beginGesture(gap, hit.velocity);
        }
    }

    void beginGesture(float gap, int velocity)
    {
        const int kind = gap >= 10.0f ? 3 : gap >= 6.0f ? 2 : 1;
        if (kind <= gestureKind)
            return;
        const auto cur = anim.stateName();
        // Let a loop finish. A leap still cuts in. A step or a walk does not.
        if (kind < 3 && isLoop(cur) && loopLeft > 0.45)
            return;
        gestureKind = kind;
        facingLeft = goalNote < shownNote;
        moveCharge = 0.0;
        jumpPower = juce::jlimit(0.2f, 1.0f, (float) velocity / 127.0f);
        if (kind == 1)
        {
            doFlip = false;
            if (cur != "ballet" && cur != "ballet_enter")
                anim.play("ballet_enter");
            loopLeft = juce::jmax(loopLeft, 2.4);
            return;
        }
        if (kind == 2)
        {
            doFlip = false;
            if (cur != "walk" && cur != "walk_start" && cur != "dash")
                anim.play(gap >= 7.0f ? "dash" : "walk_start");
            loopLeft = juce::jmax(loopLeft, 2.2);
            return;
        }
        leapSpan = juce::jmax(gap, 1.0f);
        doFlip = gap >= 12.0f || velocity > 112;
        if (doFlip)
            jumpPower = juce::jmin(1.0f, jumpPower + 0.25f);
        anim.play(doFlip ? "somersault" : "jump_start");
    }

    void arrive(int velocity)
    {
        const int prev = gestureKind;
        gestureKind = 0;
        moveCharge = 0.0;
        cool = 1.1;
        const bool flipped = doFlip;
        doFlip = false;
        if (prev >= 3)
            anim.play("land");
        else if (prev == 2)
            anim.play("walk_stop");
        else if (anim.stateName() == "idle")
            anim.play(velocity > 104 ? "cheer" : "refined_enter");
        if (flipped)
            cool = 0.7;
        loopLeft = 2.6;
    }

    void settleDance(double dt, const PlayerNote* hit, double pos)
    {
        const auto clip = anim.stateName();
        if (clip == "jump_start" || clip == "jump_air" || clip == "somersault")
        {
            anim.play("land");
            return;
        }
        if (isLoop(clip))
        {
            if (loopLeft > 0.0)
                loopLeft -= dt;
            else
            {
                playExit(clip);
                cool = 1.3;
            }
            return;
        }
        if (clip != "idle")
            return;
        if (cool > 0.0)
            return;
        const double left = hit != nullptr ? hit->endSec - pos : 1.4;
        const int role = drumHold > 0.0 ? 1 : danceRole(hit, pos);
        const bool longHold = role == 0 && left > 0.55;
        if (longHold)
            startGrace(left);
        else if (role == 1)
            startGroove(left);
        else
            startLead(left);
    }

    static int danceRole(const PlayerNote* hit, double pos)
    {
        if (hit == nullptr)
            return 2;
        const double len = hit->endSec - hit->startSec;
        if (len >= 0.75 && (hit->endSec - pos) > 0.45)
            return 0;
        return 2;
    }

    void playCycle(const char* const* names, int n, double timeLeft)
    {
        if (n < 1)
            return;
        anim.play(names[actIx % n]);
        ++actIx;
        loopLeft = juce::jlimit(2.6, 4.8, juce::jmax(timeLeft, 2.6));
        cool = 1.5;
    }

    void playFlash(double timeLeft)
    {
        static const char* kFlash[] = {
            "cheer", "dab", "kira_flourish", "spin_once",
            "curtsy", "arabesque", "social_finish"
        };
        playCycle(kFlash, (int) (sizeof(kFlash) / sizeof(kFlash[0])), timeLeft);
    }

    void startGroove(double timeLeft)
    {
        static const char* kGroove[] = {
            "kira_enter", "social_enter", "dance_enter", "refined_enter"
        };
        playCycle(kGroove, 4, timeLeft);
    }

    void startLead(double timeLeft)
    {
        static const char* kLead[] = {
            "dance_enter", "spin_enter", "social_enter", "refined_enter"
        };
        playCycle(kLead, 4, timeLeft);
    }

    void startGrace(double timeLeft)
    {
        static const char* kGrace[] = {
            "ballet_enter", "refined_enter", "spin_enter", "social_enter"
        };
        if (timeLeft > 2.4 && (actIx % 5) == 4)
        {
            anim.play("sit_down");
            loopLeft = juce::jmin(3.2, juce::jmax(2.4, timeLeft));
            cool = 1.2;
            ++actIx;
            return;
        }
        playCycle(kGrace, 4, timeLeft);
    }

    void pollDrums(double pos, double prev, const std::vector<PlayerNote>& score, std::uint32_t silenced)
    {
        if (pos <= prev)
            return;
        for (const auto& hit : score)
        {
            if (! isDrum(hit) || ! noteOk(hit, silenced))
                continue;
            if (hit.startSec <= prev || hit.startSec > pos)
                continue;
            impact = 1.0f;
            drumHold = 0.45;
            ++drumIx;
            // A nod only. Do not change her note, and do not sit her down.
            if (gestureKind == 0 && anim.stateName() == "idle" && (drumIx % 12) == 0)
            {
                anim.play("cheer");
                cool = 0.45;
            }
            return;
        }
    }

    int mode { 1 };
    bool linger { false };
    bool visible { false };
    bool pausedDraw { false };
    bool placed { false };
    double bpm { 120.0 };
    float impact { 0.0f };
    double drumHold { 0.0 };
    float jumpPower { 0.5f };
    float shownNote { 60.0f };
    float shownAhead { 1.05f };
    float goalNote { 60.0f };
    float leapSpan { 1.0f };
    float bob { 0.0f };
    std::uint32_t drums { 0 };
    int drumIx { 0 };
    int actIx { 0 };
    int gestureKind { 0 };
    aeterna::Player anim;
    Ride ride;
    double moveCharge { 0.0 };
    double cool { 0.2 };
    double loopLeft { 0.0 };
    bool doFlip { false };
    bool facingLeft { false };
    bool atlasFailed { false };
    bool sparkleArmed { false };
    float sparkle { 0.0f };
    juce::Image atlas;
};
