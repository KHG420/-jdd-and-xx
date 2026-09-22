#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace pets {
enum class Character { girl, boy };
enum class Pose { idle, blink, wave, busy, snack, sleep, lifted, grumpy, love };
enum class Together { read, heart, comfort, hug };

inline std::wstring name(Character c) { return c == Character::girl ? L"红衣女孩" : L"白衣男孩"; }
inline std::wstring title(Pose pose, Character c) {
    switch (pose) {
    case Pose::idle: return L"陪在你身边";
    case Pose::blink: return L"眨眨眼";
    case Pose::wave: return L"打个招呼";
    case Pose::busy: return c == Character::girl ? L"翻一会儿书" : L"认真敲电脑";
    case Pose::snack: return c == Character::girl ? L"喝口热茶" : L"吃一口饭";
    case Pose::sleep: return L"打个小盹";
    case Pose::lifted: return L"被提起来了";
    case Pose::grumpy: return L"闹一点小脾气";
    case Pose::love: return L"送你一颗心";
    }
    return L"";
}
inline std::wstring title(Together scene) {
    return std::array<std::wstring, 4>{L"一起看书", L"一起比心", L"拍拍头", L"抱一抱"}[static_cast<int>(scene)];
}
inline std::wstring message(Together scene) {
    return std::array<std::wstring, 4>{L"这一页，我们一起看。", L"这颗心，送给你。", L"累了就靠一会儿。", L"抱一下，就好啦。"}[static_cast<int>(scene)];
}

// Match Sources/Behavior.swift. This platform port adds no shared runtime or protocol.
struct Behavior {
    Character character;
    Pose pose = Pose::idle;
    double entered = 0, deadline = 0, bubbleUntil = 0, lastTouch = -100;
    std::wstring bubble;
    int taps = 0;
    bool held = false;
    std::optional<Pose> pending;
    double nextBlink = 4, blinkUntil = 0, nextActivity = 12;
    int activityIndex = 0;

    explicit Behavior(Character c, double now = 0) : character(c), entered(now) {
        nextActivity = now + (c == Character::girl ? 12 : 19);
        nextBlink = now + (c == Character::girl ? 3.7 : 5.1);
    }
    void set(Pose value, double now, double duration, std::wstring text = L"") {
        pose = value; entered = now; deadline = now + duration; pending.reset();
        if (!text.empty()) { bubble = std::move(text); bubbleUntil = now + std::min(duration, 3.5); }
    }
    void touch(double now) {
        taps = now - lastTouch < 2 ? taps + 1 : 1; lastTouch = now;
        if (taps >= 4) {
            set(Pose::grumpy, now, 3, character == Character::girl ? L"再戳，要叉腰啦。" : L"脑袋要被戳扁啦！");
        } else if (pose == Pose::sleep) {
            set(Pose::blink, now, 0.7, L"嗯…你叫我？"); pending = Pose::wave;
        } else if (pose == Pose::busy || pose == Pose::snack) {
            set(Pose::idle, now, 0.35); pending = Pose::wave;
        } else {
            set(taps == 3 ? Pose::love : Pose::wave, now, 2.6,
                character == Character::girl ? L"我在呀。" : L"嘿，我陪着你呢！");
        }
    }
    void lift(double now) {
        held = true;
        set(Pose::lifted, now, std::numeric_limits<double>::infinity(),
            character == Character::girl ? L"慢一点呀～" : L"起飞咯！");
    }
    void land(double now) {
        held = false; set(Pose::blink, now, 0.4, L"稳稳落地。"); pending = Pose::idle; nextActivity = now + 15;
    }
    void tick(double now, int hour, bool quiet) {
        if (held) return;
        if ((pose != Pose::idle || pending.has_value()) && now >= deadline) {
            const auto next = pending.value_or(Pose::idle);
            set(next, now, next == Pose::wave ? 2.5 : 0); nextActivity = now + (quiet ? 45 : 18);
        }
        if (pose == Pose::idle && now >= nextActivity) {
            const std::array<Pose, 6> normal{Pose::busy, Pose::snack, Pose::idle, Pose::sleep, Pose::busy, Pose::love};
            const std::array<Pose, 3> calm{Pose::busy, Pose::sleep, Pose::busy};
            const auto next = hour >= 23 || hour < 7 ? Pose::sleep :
                (quiet ? calm[activityIndex % calm.size()] : normal[activityIndex % normal.size()]);
            ++activityIndex;
            if (next != Pose::idle) set(next, now, next == Pose::sleep ? 38 : (next == Pose::love ? 3 : 24));
            nextActivity = now + (quiet ? 50 : 30);
        }
        if (now >= nextBlink) {
            blinkUntil = now + 0.16; nextBlink = now + (character == Character::girl ? 4.6 : 5.7);
        }
    }
    Pose displayedPose(double now) const { return pose == Pose::idle && now < blinkUntil ? Pose::blink : pose; }
    void shiftTime(double seconds) {
        entered += seconds; deadline += seconds; bubbleUntil += seconds; lastTouch += seconds;
        nextBlink += seconds; blinkUntil += seconds; nextActivity += seconds;
    }
};

struct Point { double x = 0, y = 0; };
struct Area { double x, y, width, height; };
inline Point clampOrigin(Point p, double width, double height, Area area) {
    return {std::min(std::max(p.x, area.x), std::max(area.x, area.x + area.width - width)),
            std::min(std::max(p.y, area.y), std::max(area.y, area.y + area.height - height))};
}

// GDI+ PixelFormat32bppPARGB is BGRA in little-endian memory.
inline std::array<uint8_t, 4> keyPixel(double r, double g, double b) {
    const double dominance = g - std::max(r, b);
    const double alpha = 1 - std::clamp((dominance - 12) / 80, 0.0, 1.0);
    return {static_cast<uint8_t>(b * alpha),
            static_cast<uint8_t>((dominance > 12 ? std::min(g, std::max(r, b)) : g) * alpha),
            static_cast<uint8_t>(r * alpha), static_cast<uint8_t>(std::round(alpha * 255))};
}
}
