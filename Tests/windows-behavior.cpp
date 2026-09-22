#include "../Windows/Behavior.hpp"
#include <cstdlib>
#include <iostream>

using namespace pets;
int checks = 0;
void check(bool ok, const char* reason) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << reason << '\n'; std::exit(1); }
}
int main() {
    Behavior girl(Character::girl, 100);
    girl.set(Pose::sleep, 100, 20); girl.touch(101);
    check(girl.pose == Pose::blink, "sleep wakes gradually");
    girl.tick(101.8, 12, false); check(girl.pose == Pose::wave, "wake waves");
    girl.tick(105, 12, false); check(girl.pose == Pose::idle, "wave settles");
    for (int i = 0; i < 4; ++i) girl.touch(106 + i * 0.2);
    check(girl.pose == Pose::grumpy, "repeated taps protest");
    girl.tick(110, 12, false); check(girl.pose == Pose::idle, "protest recovers");
    girl.lift(111); girl.tick(400, 12, false);
    check(girl.held && girl.pose == Pose::lifted, "held cannot expire");
    girl.land(401); girl.tick(401.5, 12, false);
    check(!girl.held && girl.pose == Pose::idle, "landing recovers");
    girl.set(Pose::busy, 402, 20); girl.touch(403);
    check(girl.pose == Pose::idle, "puts down prop before waving");
    girl.tick(403.4, 12, false); check(girl.pose == Pose::wave, "prop transition continues");
    girl.shiftTime(1000); girl.tick(1404, 12, false);
    check(girl.pose == Pose::wave, "pause retains response");
    Behavior boy(Character::boy); boy.tick(20, 1, false);
    check(boy.pose == Pose::sleep, "night sleep");
    Behavior calm(Character::girl);
    for (int time = 0; time <= 600; ++time) {
        calm.tick(time, 12, true);
        check(calm.pose != Pose::wave && calm.pose != Pose::love && calm.pose != Pose::grumpy, "quiet does not seek attention");
    }
    const auto p = clampOrigin({-100, 999}, 250, 290, {0, 0, 1200, 800});
    check(p.x == 0 && p.y == 510, "clamp whole window");
    check(clampOrigin({-1700, 30}, 250, 290, {-1920, 0, 1920, 1080}).x == -1700, "negative monitor coordinates");
    check(keyPixel(0, 255, 0)[3] == 0, "green is transparent");
    check(keyPixel(255, 248, 220) == std::array<uint8_t, 4>{220, 248, 255, 255}, "cream skin preserved as BGRA");
    check(keyPixel(10, 10, 10)[3] == 255, "black hair preserved");
    const auto edge = keyPixel(100, 150, 90);
    check(edge[3] > 0 && edge[3] < 255 && edge[1] <= edge[2], "partial edge de-spilled and premultiplied");
    std::cout << "PASS: " << checks << " Windows behavior, geometry and chroma-key assertions\n";
}
