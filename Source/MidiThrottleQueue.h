#pragma once

// Optional inter-message delay for CC / SysEx dumps.
// Default is 0 ms (off): the queue still serialises, but processBlock drains it
// in one gulp. OPTIONS can raise 10-50 ms if a dump ever drops on a 2002 USB box.
// Playlist / mixer CCs at 0 ms match FL MIDI Out and Roland's own editor.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <deque>

enum class MidiPort : std::uint8_t { A = 0, B = 1, Both = 2 };

struct QueuedMidi
{
    juce::MidiMessage message;
    MidiPort port { MidiPort::A };
};

class MidiThrottleQueue
{
public:
    void setDelayMs(int ms) noexcept
    {
        delayMs.store(juce::jlimit(0, 50, ms), std::memory_order_relaxed);
    }

    int getDelayMs() const noexcept
    {
        return delayMs.load(std::memory_order_relaxed);
    }

    bool isOff() const noexcept { return getDelayMs() <= 0; }

    void push(const juce::MidiMessage& m, MidiPort port)
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        queue.push_back({ m, port });
    }

    void pushMany(const std::vector<juce::MidiMessage>& msgs, MidiPort port)
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        for (auto& m : msgs)
            queue.push_back({ m, port });
    }

    void clear()
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        queue.clear();
        pendingSamples = 0;
    }

    int size() const
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        return (int) queue.size();
    }

    // Call from processBlock. At 0 ms pops every call; otherwise one message every delayMs.
    bool popDue(double sampleRate, int numSamples, QueuedMidi& out)
    {
        const int ms = getDelayMs();
        const int gap = (ms <= 0) ? 0 : (int) std::llround(sampleRate * (ms / 1000.0));
        const juce::SpinLock::ScopedLockType sl(lock);
        pendingSamples += numSamples;
        if (queue.empty())
        {
            pendingSamples = juce::jmin(pendingSamples, juce::jmax(0, gap));
            return false;
        }
        if (gap > 0 && pendingSamples < gap && ! firstImmediate)
            return false;
        firstImmediate = false;
        pendingSamples = 0;
        out = std::move(queue.front());
        queue.pop_front();
        return true;
    }

    // Editor-thread / timer path when audio is idle.
    bool popNow(QueuedMidi& out)
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        if (queue.empty())
            return false;
        out = std::move(queue.front());
        queue.pop_front();
        firstImmediate = false;
        return true;
    }

    void resetTiming()
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        pendingSamples = 0;
        firstImmediate = true;
    }

private:
    mutable juce::SpinLock lock;
    std::deque<QueuedMidi> queue;
    std::atomic<int> delayMs { 0 };
    int pendingSamples { 0 };
    bool firstImmediate { true };
};
