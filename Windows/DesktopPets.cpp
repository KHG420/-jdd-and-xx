#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellscalingapi.h>
#include <powrprof.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>
#include "Behavior.hpp"

using namespace Gdiplus;
using namespace pets;
namespace fs = std::filesystem;
constexpr wchar_t HostClass[] = L"DeskCompanions.Host";
constexpr wchar_t PetClass[] = L"DeskCompanions.Pet";
constexpr UINT TrayMessage = WM_APP + 1, ReopenMessage = WM_APP + 2;
enum Command : UINT { Hide = 100, Pause, Quiet, Reset, Help, Quit, ModeBoth = 200, ModeGirl, ModeBoy, SizeSmall = 300, SizeNormal, SizeLarge, PoseBase = 1000, PairBase = 2000 };

double clockNow() { return GetTickCount64() / 1000.0; }
RECT windowRect(HWND hwnd) { RECT r{}; GetWindowRect(hwnd, &r); return r; }
Area workArea(HWND hwnd) {
    MONITORINFO info{}; info.cbSize = sizeof(info);
    GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &info);
    return {double(info.rcWork.left), double(info.rcWork.top), double(info.rcWork.right-info.rcWork.left), double(info.rcWork.bottom-info.rcWork.top)};
}
void moveWindow(HWND hwnd, double x, double y) {
    SetWindowPos(hwnd, nullptr, int(std::round(x)), int(std::round(y)), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
void clampWindow(HWND hwnd) {
    const auto r = windowRect(hwnd);
    const auto p = clampOrigin({double(r.left), double(r.top)}, r.right-r.left, r.bottom-r.top, workArea(hwnd));
    moveWindow(hwnd, p.x, p.y);
}

struct Sprite {
    int width = 0, height = 0;
    double referenceWidth = 0;
    std::vector<BYTE> pixels;
    std::unique_ptr<Bitmap> bitmap, scaled;
    int scaledWidth = 0, scaledHeight = 0;
    Bitmap* image(double unit) {
        const int w = std::max(1, int(std::round(width * 208 * unit / referenceWidth)));
        const int h = std::max(1, int(std::round(height * 208 * unit / referenceWidth)));
        if (!scaled || scaledWidth != w || scaledHeight != h) {
            scaled = std::make_unique<Bitmap>(w, h, PixelFormat32bppPARGB);
            Graphics g(scaled.get()); g.SetCompositingMode(CompositingModeSourceCopy);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            g.DrawImage(bitmap.get(), Rect(0, 0, w, h), 0, 0, width, height, UnitPixel);
            scaledWidth = w; scaledHeight = h;
        }
        return scaled.get();
    }
    bool contains(double u, double v) const {
        if (u < 0 || v < 0 || u >= 1 || v >= 1) return false;
        return pixels[(size_t(int(v*height))*width+int(u*width))*4+3] > 30;
    }
};

std::vector<Sprite> loadAtlas(int resource, int columns, const std::vector<double>& cuts, bool paired) {
    const HRSRC found = FindResourceW(nullptr, MAKEINTRESOURCEW(resource), RT_RCDATA);
    if (!found) throw std::runtime_error("Embedded character atlas is missing");
    const HGLOBAL loaded = LoadResource(nullptr, found);
    const auto bytes = static_cast<const BYTE*>(LockResource(loaded));
    IStream* stream = SHCreateMemStream(bytes, SizeofResource(nullptr, found));
    if (!stream) throw std::runtime_error("Cannot read embedded character atlas");
    std::unique_ptr<IStream, void(*)(IStream*)> ownedStream(stream, [](IStream* s) { s->Release(); });
    std::unique_ptr<Bitmap> atlas(Bitmap::FromStream(stream));
    if (!atlas || atlas->GetLastStatus() != Ok) throw std::runtime_error("Cannot decode character PNG");
    std::vector<Sprite> sprites;
    for (size_t row = 0; row+1 < cuts.size(); ++row) for (int col = 0; col < columns; ++col) {
        const int x = int(atlas->GetWidth()*col/columns), nextX = int(atlas->GetWidth()*(col+1)/columns);
        const int y = int(atlas->GetHeight()*cuts[row]), nextY = int(atlas->GetHeight()*cuts[row+1]);
        const int w = nextX-x, h = nextY-y;
        Rect region(x, y, w, h); BitmapData data{};
        if (atlas->LockBits(&region, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) throw std::runtime_error("Cannot read atlas pixels");
        std::vector<BYTE> pixels(size_t(w)*h*4);
        int minX = w, maxX = 0, minY = h, maxY = 0;
        for (int py = 0; py < h; ++py) for (int px = 0; px < w; ++px) {
            const BYTE* src = static_cast<BYTE*>(data.Scan0) + ptrdiff_t(py)*data.Stride + px*4;
            const auto keyed = keyPixel(src[2], src[1], src[0]);
            std::copy(keyed.begin(), keyed.end(), pixels.begin()+(size_t(py)*w+px)*4);
            if (keyed[3] > 30) { minX = std::min(minX, px); maxX = std::max(maxX, px); minY = std::min(minY, py); maxY = std::max(maxY, py); }
        }
        atlas->UnlockBits(&data);
        if (minX >= maxX || minY >= maxY) throw std::runtime_error("Empty character sprite");
        minX = std::max(0, minX-2); minY = std::max(0, minY-2); maxX = std::min(w-1, maxX+2); maxY = std::min(h-1, maxY+2);
        Sprite sprite; sprite.width = maxX-minX+1; sprite.height = maxY-minY+1;
        sprite.referenceWidth = double(atlas->GetWidth()) / columns / (paired ? 2 : 1);
        sprite.pixels.resize(size_t(sprite.width)*sprite.height*4);
        for (int py = 0; py < sprite.height; ++py)
            std::memcpy(sprite.pixels.data()+size_t(py)*sprite.width*4, pixels.data()+(size_t(py+minY)*w+minX)*4, size_t(sprite.width)*4);
        sprite.bitmap = std::make_unique<Bitmap>(sprite.width, sprite.height, sprite.width*4, PixelFormat32bppPARGB, sprite.pixels.data());
        if (sprite.bitmap->GetLastStatus() != Ok) throw std::runtime_error("Cannot create transparent sprite");
        sprites.push_back(std::move(sprite));
    }
    return sprites;
}

void roundRect(GraphicsPath& path, const RectF& r, REAL radius) {
    const REAL d = radius*2;
    path.AddArc(r.X, r.Y, d, d, 180, 90); path.AddArc(r.GetRight()-d, r.Y, d, d, 270, 90);
    path.AddArc(r.GetRight()-d, r.GetBottom()-d, d, d, 0, 90); path.AddArc(r.X, r.GetBottom()-d, d, d, 90, 90); path.CloseFigure();
}

struct App;
struct Panel {
    App* app = nullptr;
    int id;
    HWND hwnd = nullptr;
    Behavior behavior;
    HDC dc = nullptr;
    HBITMAP dib = nullptr;
    HGDIOBJ originalBitmap = nullptr;
    BYTE* surface = nullptr;
    int width = 0, height = 0, spriteIndex = -1, previousIndex = -1;
    double dpi = 1, changedAt = 0, landingAt = -100, lean = 0;
    bool pressed = false, dragging = false, transparent = false;
    POINT dragStart{}, origin{};
    RectF spriteRect{};
    explicit Panel(int index) : id(index), behavior(index == 1 ? Character::boy : Character::girl) {}
    ~Panel() {
        if (dc && originalBitmap) SelectObject(dc, originalBitmap);
        if (dib) DeleteObject(dib);
        if (dc) DeleteDC(dc);
        if (hwnd) DestroyWindow(hwnd);
    }
    void allocate(int w, int h) {
        if (dc && width == w && height == h) return;
        if (!dc) dc = CreateCompatibleDC(nullptr);
        if (originalBitmap) SelectObject(dc, originalBitmap);
        if (dib) DeleteObject(dib);
        width = w; height = h;
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        void* data = nullptr; dib = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &data, nullptr, 0);
        if (!dc || !dib || !data) throw std::runtime_error("Cannot allocate transparent window");
        const auto old = SelectObject(dc, dib); if (!originalBitmap) originalBitmap = old;
        surface = static_cast<BYTE*>(data);
    }
};

struct App {
    HWND host = nullptr;
    std::array<Panel, 3> panels{Panel(0), Panel(1), Panel(2)};
    std::array<std::vector<Sprite>, 3> sprites;
    NOTIFYICONDATAW tray{};
    HICON icon = nullptr;
    HPOWERNOTIFY powerNotice = nullptr;
    UINT taskbarCreated = 0;
    fs::path settingsPath;
    double scale = 1, pausedAt = 0, motionTime = 0, lastTick = 0, pairUntil = 0, nextPair = 0, approachAt = 0;
    bool quiet = false, paused = false, hidden = false, reduced = false, sleeping = false, trackingMenu = false, testing = false;
    int mode = 0, pairIndex = 0;
    std::optional<Together> pair, approaching;
    std::array<pets::Point, 2> approachFrom{}, approachTo{};

    explicit App(bool test = false) : testing(test) {
        sprites[0] = loadAtlas(101, 3, {0, 432.0/1254, 814.0/1254, 1}, false);
        sprites[1] = loadAtlas(102, 3, {0, 435.0/1254, 830.0/1254, 1}, false);
        sprites[2] = loadAtlas(103, 2, {0, 0.5, 1}, true);
    }
    ~App() {
        if (host) KillTimer(host, 1);
        if (powerNotice) UnregisterPowerSettingNotification(powerNotice);
        if (tray.cbSize) Shell_NotifyIconW(NIM_DELETE, &tray);
        if (icon) DestroyIcon(icon);
        if (host) DestroyWindow(host);
    }
    static LRESULT CALLBACK hostProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK petProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    void init();
    void tick(bool force = false);
    void render(Panel& panel, double now);
    void menu(Panel* panel, POINT point);
    void command(UINT id);
    void startPair(Together scene);
    void presentPair(Together scene);
    void finishPair();
    void touch(Panel& panel);
    void land(Panel& panel);
    void save();
    void reset();
    void resize(Panel& panel);
    void visibility();
    void setPaused(bool value);
    void schedule() {
        KillTimer(host, 1);
        if (!sleeping) SetTimer(host, 1, paused || hidden ? 200 : (quiet || reduced ? 100 : 40), nullptr);
    }
    void checkMotion() { BOOL enabled = TRUE; SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0); reduced = !enabled; }
    bool visible(const Panel& panel) const { return !hidden && (panel.id == 2 ? pair.has_value() : !pair && (mode == 0 || mode == panel.id+1)); }
    double unit(const Panel& panel) const { return scale*panel.dpi; }
    Sprite& currentSprite(Panel& panel) { return sprites[panel.id][std::max(0, panel.spriteIndex)]; }
};

void App::resize(Panel& panel) {
    const double u = unit(panel);
    const auto r = windowRect(panel.hwnd);
    const int w = int(std::round((panel.id == 2 ? 465 : 250)*u)), h = int(std::round((panel.id == 2 ? 300 : 290)*u));
    panel.allocate(w, h);
    SetWindowPos(panel.hwnd, nullptr, r.left, r.bottom-h, w, h, SWP_NOACTIVATE | SWP_NOZORDER);
    clampWindow(panel.hwnd);
}
void App::reset() {
    finishPair(); POINT cursor{}; GetCursorPos(&cursor);
    MONITORINFO info{}; info.cbSize = sizeof(info); GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &info);
    for (int i = 0; i < 2; ++i) {
        auto& panel = panels[i];
        UINT xDpi = 96, yDpi = 96; GetDpiForMonitor(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), MDT_EFFECTIVE_DPI, &xDpi, &yDpi);
        panel.dpi = xDpi/96.0; resize(panel);
        const double u = unit(panel);
        moveWindow(panel.hwnd, info.rcWork.right-(i == 0 ? 485 : 260)*u-22, info.rcWork.bottom-panel.height-16*u);
        clampWindow(panel.hwnd);
    }
    save();
}
void App::save() {
    if (testing || settingsPath.empty()) return;
    const auto put = [&](const wchar_t* key, const std::wstring& value) { WritePrivateProfileStringW(L"DesktopPets", key, value.c_str(), settingsPath.c_str()); };
    put(L"size", std::to_wstring(scale)); put(L"mode", std::to_wstring(mode)); put(L"quiet", quiet ? L"1" : L"0");
    if (!pair) for (int i = 0; i < 2; ++i) {
        const auto r = windowRect(panels[i].hwnd);
        put(i == 0 ? L"girlX" : L"boyX", std::to_wstring(r.left)); put(i == 0 ? L"girlY" : L"boyY", std::to_wstring(r.top));
    }
}
void App::visibility() {
    for (auto& panel : panels) ShowWindow(panel.hwnd, visible(panel) ? SW_SHOWNOACTIVATE : SW_HIDE);
}
void App::setPaused(bool value) {
    if (paused == value) return;
    const double now = clockNow();
    if (value) pausedAt = now;
    else {
        const double elapsed = now-pausedAt;
        for (auto& panel : panels) { panel.behavior.shiftTime(elapsed); panel.changedAt += elapsed; panel.landingAt += elapsed; }
        pairUntil += elapsed; nextPair += elapsed; approachAt += elapsed;
    }
    paused = value; schedule();
}
void App::touch(Panel& panel) {
    approaching.reset(); setPaused(false);
    if (panel.id < 2) panel.behavior.touch(clockNow());
    else { finishPair(); for (int i = 0; i < 2; ++i) panels[i].behavior.set(Pose::love, clockNow(), 3, L"也喜欢你。"); }
    nextPair = clockNow()+65; tick(true);
}
void App::land(Panel& panel) {
    panel.dragging = false; panel.pressed = false; clampWindow(panel.hwnd);
    if (panel.id < 2) panel.behavior.land(clockNow());
    panel.landingAt = clockNow(); nextPair = clockNow()+70; save(); tick(true);
}
void App::startPair(Together scene) {
    if (hidden || mode != 0 || panels[0].dragging || panels[1].dragging) return;
    setPaused(false); finishPair();
    if (reduced) { presentPair(scene); return; }
    const auto a = windowRect(panels[0].hwnd), b = windowRect(panels[1].hwnd);
    const bool same = MonitorFromWindow(panels[0].hwnd, MONITOR_DEFAULTTONEAREST) == MonitorFromWindow(panels[1].hwnd, MONITOR_DEFAULTTONEAREST);
    const double center = same ? (a.left+a.right+b.left+b.right)/4.0 : (a.left+a.right)/2.0;
    for (int i = 0; i < 2; ++i) {
        auto& panel = panels[i]; const auto r = windowRect(panel.hwnd);
        approachFrom[i] = {double(r.left), double(r.top)};
        approachTo[i] = {center+(i == 0 ? -94 : 94)*unit(panels[0])-panel.width/2.0, double(a.bottom-panel.height)};
        panel.behavior.set(Pose::wave, clockNow(), 2);
    }
    approachAt = clockNow(); approaching = scene; nextPair = clockNow()+75; visibility();
}
void App::presentPair(Together scene) {
    approaching.reset();
    const auto a = windowRect(panels[0].hwnd), b = windowRect(panels[1].hwnd);
    const bool same = MonitorFromWindow(panels[0].hwnd, MONITOR_DEFAULTTONEAREST) == MonitorFromWindow(panels[1].hwnd, MONITOR_DEFAULTTONEAREST);
    auto& shared = panels[2]; shared.dpi = panels[0].dpi; resize(shared);
    const double center = same ? (a.left+a.right+b.left+b.right)/4.0 : (a.left+a.right)/2.0;
    moveWindow(shared.hwnd, center-shared.width/2.0, (same ? std::max(a.bottom,b.bottom) : a.bottom)-shared.height);
    clampWindow(shared.hwnd); shared.spriteIndex = -1; shared.previousIndex = -1;
    pair = scene; pairUntil = clockNow()+11; visibility();
}
void App::finishPair() {
    approaching.reset(); if (!pair) return;
    const auto frame = windowRect(panels[2].hwnd); pair.reset();
    for (int i = 0; i < 2; ++i) {
        auto& panel = panels[i]; panel.dpi = panels[2].dpi; resize(panel);
        moveWindow(panel.hwnd, (frame.left+frame.right)/2.0+(i == 0 ? -108 : 108)*unit(panel)-panel.width/2.0, frame.bottom-panel.height);
        clampWindow(panel.hwnd); panel.behavior.set(Pose::idle, clockNow(), 0);
    }
    nextPair = clockNow()+75; visibility(); save();
}

void App::render(Panel& panel, double now) {
    const int index = panel.id == 2 ? int(pair.value_or(Together::read)) : int(panel.behavior.displayedPose(now));
    if (panel.spriteIndex != index) {
        panel.previousIndex = panel.spriteIndex; panel.spriteIndex = index; panel.changedAt = now;
        const auto label = panel.id == 2 ? title(pair.value_or(Together::read)) : name(panel.behavior.character)+L" · "+title(panel.behavior.pose, panel.behavior.character);
        SetWindowTextW(panel.hwnd, label.c_str());
    }
    const double u = unit(panel);
    const Pose pose = panel.id == 2 ? (pair == Together::heart ? Pose::love : Pose::idle) : panel.behavior.pose;
    const double breath = paused || reduced ? 0 : std::sin(motionTime*(pose == Pose::sleep ? 1.3 : 2.1))*(quiet ? 0.6 : 1.1)*u;
    double bounce = 0;
    if (!paused && !reduced) {
        if (pose == Pose::wave || pose == Pose::love) bounce = std::abs(std::sin(motionTime*4))*3*u;
        if (panel.dragging) bounce = std::sin(motionTime*5)*3*u;
    }
    const double elapsed = now-panel.landingAt;
    const double squash = elapsed >= 0 && elapsed < 0.45 && !reduced ? std::sin(elapsed/0.45*3.141592653589793)*0.075 : 0;
    const double angle = paused || reduced ? 0 : (panel.dragging ? std::sin(motionTime*4)*4 : (pose == Pose::grumpy ? std::sin(motionTime*7)*1.2 : panel.lean*0.25));
    auto& sprite = currentSprite(panel); Bitmap* image = sprite.image(u);
    const auto rectFor = [&](Bitmap* bitmap) {
        const REAL w = REAL(bitmap->GetWidth()*(1+squash)), h = REAL(bitmap->GetHeight()*(1-squash)+breath);
        return RectF(REAL((panel.width-w)/2+panel.lean*u), REAL(panel.height-16*u-bounce-h), w, h);
    };
    panel.spriteRect = rectFor(image);
    Bitmap surface(panel.width, panel.height, panel.width*4, PixelFormat32bppPARGB, panel.surface);
    Graphics g(&surface); g.Clear(Color(0, 0, 0, 0));
    g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetInterpolationMode(InterpolationModeBilinear);
    if (!panel.dragging) {
        SolidBrush shadow(Color(16, 40, 40, 40));
        g.FillEllipse(&shadow, REAL(panel.width/2.0-panel.spriteRect.Width*0.26), REAL(panel.height-18*u), REAL(panel.spriteRect.Width*0.52), REAL(7*u));
    }
    const double mix = reduced ? 1 : std::clamp((now-panel.changedAt)/0.14, 0.0, 1.0);
    const auto draw = [&](Bitmap* bitmap, double opacity) {
        const auto rect = rectFor(bitmap); const auto state = g.Save();
        g.TranslateTransform(rect.X+rect.Width/2, rect.Y+rect.Height/2); g.RotateTransform(REAL(angle)); g.TranslateTransform(-rect.X-rect.Width/2, -rect.Y-rect.Height/2);
        ColorMatrix matrix = {{{1,0,0,0,0},{0,1,0,0,0},{0,0,1,0,0},{0,0,0,REAL(opacity),0},{0,0,0,0,1}}};
        ImageAttributes attributes; attributes.SetColorMatrix(&matrix);
        g.DrawImage(bitmap, rect, 0, 0, REAL(bitmap->GetWidth()), REAL(bitmap->GetHeight()), UnitPixel, &attributes);
        g.Restore(state);
    };
    if (panel.previousIndex >= 0 && mix < 1) draw(sprites[panel.id][panel.previousIndex].image(u), 1-mix);
    draw(image, mix);
    std::wstring text;
    if (!quiet) {
        if (panel.id == 2 && pair && now <= pairUntil-6) text = message(*pair);
        else if (panel.id < 2 && now < panel.behavior.bubbleUntil) text = panel.behavior.bubble;
    }
    if (!text.empty()) {
        Font preferred(L"Microsoft YaHei UI", REAL(12.5*u), FontStyleRegular, UnitPixel);
        Font fallback(FontFamily::GenericSansSerif(), REAL(12.5*u), FontStyleRegular, UnitPixel);
        const Font* font = preferred.GetLastStatus() == Ok ? &preferred : &fallback;
        RectF measure;
        if (g.MeasureString(text.c_str(), -1, font, PointF(0, 0), &measure) != Ok)
            throw std::runtime_error("Cannot render system-font speech bubble");
        const REAL w = std::min(REAL(panel.width-12*u), measure.Width+REAL(24*u));
        const RectF box(REAL((panel.width-w)/2), std::max(REAL(5*u), panel.spriteRect.Y-REAL(43*u)), w, REAL(31*u));
        SolidBrush cream(Color(250, 255, 249, 232)), ink(Color(255, 74, 56, 46));
        GraphicsPath shape; roundRect(shape, box, REAL(12*u)); g.FillPath(&cream, &shape);
        PointF triangle[]{{box.X+w/2-REAL(5*u),box.GetBottom()-1},{box.X+w/2,box.GetBottom()+REAL(5*u)},{box.X+w/2+REAL(5*u),box.GetBottom()-1}};
        g.FillPolygon(&cream, triangle, 3);
        StringFormat format; format.SetAlignment(StringAlignmentCenter); format.SetLineAlignment(StringAlignmentCenter);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit); g.DrawString(text.c_str(), -1, font, box, &format, &ink);
    }
    g.Flush(FlushIntentionSync);
    const auto position = windowRect(panel.hwnd); POINT dest{position.left, position.top}, source{}; SIZE size{panel.width, panel.height};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    if (!UpdateLayeredWindow(panel.hwnd, nullptr, &dest, &size, panel.dc, &source, 0, &blend, ULW_ALPHA))
        throw std::runtime_error("Cannot update transparent desktop window");
}

void App::tick(bool force) {
    const double now = clockNow(), dt = std::clamp(now-lastTick, 0.0, 0.1); lastTick = now;
    if (sleeping || trackingMenu) return;
    if (!paused && !hidden) motionTime += dt;
    if (approaching && !paused && !hidden) {
        const double progress = std::clamp((now-approachAt-0.35)/0.65, 0.0, 1.0), ease = 1-std::pow(1-progress, 3);
        for (int i = 0; i < 2; ++i) moveWindow(panels[i].hwnd, approachFrom[i].x+(approachTo[i].x-approachFrom[i].x)*ease, approachFrom[i].y+(approachTo[i].y-approachFrom[i].y)*ease);
        if (progress >= 1) presentPair(*approaching);
    }
    if (pair && !paused && !hidden && now >= pairUntil && !panels[2].dragging) finishPair();
    SYSTEMTIME local{}; GetLocalTime(&local);
    for (auto& panel : panels) {
        if (panel.id < 2 && !paused && !hidden && !pair) panel.behavior.tick(now, local.wHour, quiet);
        if (!visible(panel)) continue;
        POINT cursor{}; GetCursorPos(&cursor); ScreenToClient(panel.hwnd, &cursor);
        const auto r = panel.spriteRect;
        bool hit = false;
        if (panel.spriteIndex >= 0 && r.Width > 0 && r.Height > 0)
            hit = currentSprite(panel).contains((cursor.x-r.X)/r.Width, (cursor.y-r.Y)/r.Height);
        const bool pass = !panel.pressed && !panel.dragging && !hit;
        if (pass != panel.transparent) {
            LONG_PTR style = GetWindowLongPtrW(panel.hwnd, GWL_EXSTYLE);
            SetWindowLongPtrW(panel.hwnd, GWL_EXSTYLE, pass ? style | WS_EX_TRANSPARENT : style & ~WS_EX_TRANSPARENT);
            panel.transparent = pass;
        }
        const bool cursorNearby = std::abs(cursor.x-panel.width/2) < 180*unit(panel) && cursor.y >= 0 && cursor.y < panel.height;
        const double target = cursorNearby && !quiet && !paused && !reduced && !panel.dragging && panel.behavior.pose != Pose::sleep ? std::clamp((cursor.x-panel.width/2.0)/(55*unit(panel)), -2.0, 2.0) : 0;
        panel.lean += (target-panel.lean)*0.12;
        if (force || !paused) render(panel, paused ? pausedAt : now);
    }
    if (!paused && !hidden && !quiet && mode == 0 && !pair && !approaching && now >= nextPair) {
        nextPair = now+75; const auto a = windowRect(panels[0].hwnd), b = windowRect(panels[1].hwnd);
        if (!panels[0].dragging && !panels[1].dragging && std::abs((a.left+a.right-b.left-b.right)/2) < 300*unit(panels[0]) && std::abs(a.bottom-b.bottom) < 65*unit(panels[0]))
            startPair(static_cast<Together>(pairIndex++ % 4));
    }
}

void addItem(HMENU menu, UINT id, const std::wstring& text, bool checked = false) { AppendMenuW(menu, MF_STRING | (checked ? MF_CHECKED : 0), id, text.c_str()); }
void separator(HMENU menu) { AppendMenuW(menu, MF_SEPARATOR, 0, nullptr); }
HMENU actions(Character c) {
    HMENU menu = CreatePopupMenu();
    for (auto p : {Pose::wave, Pose::busy, Pose::snack, Pose::sleep, Pose::love, Pose::grumpy}) addItem(menu, PoseBase+UINT(c)*100+UINT(p), title(p, c));
    return menu;
}
HMENU pairedActions() { HMENU menu = CreatePopupMenu(); for (int i = 0; i < 4; ++i) addItem(menu, PairBase+i, title(static_cast<Together>(i))); return menu; }
void subMenu(HMENU root, const wchar_t* title, HMENU child) { AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(child), title); }
void App::menu(Panel* panel, POINT point) {
    HMENU root = panel ? (panel->id < 2 ? actions(panel->behavior.character) : pairedActions()) : CreatePopupMenu();
    if (!panel) { AppendMenuW(root, MF_STRING | MF_GRAYED, 0, L"桌边的你们"); addItem(root, Hide, hidden ? L"叫他们出来" : L"暂时藏起来"); }
    else { separator(root); if (mode == 0) subMenu(root, L"和对方一起…", pairedActions()); }
    addItem(root, Pause, paused ? L"继续活动" : L"暂停动作", paused); addItem(root, Quiet, L"安静陪伴", quiet);
    if (!panel) {
        separator(root); HMENU who = CreatePopupMenu(), size = CreatePopupMenu();
        addItem(who, ModeBoth, L"两个人一起", mode == 0); addItem(who, ModeGirl, L"只留红衣女孩", mode == 1); addItem(who, ModeBoy, L"只留白衣男孩", mode == 2);
        addItem(size, SizeSmall, L"小小只 · 80%", scale == 0.8); addItem(size, SizeNormal, L"刚刚好 · 100%", scale == 1); addItem(size, SizeLarge, L"大一点 · 125%", scale == 1.25);
        subMenu(root, L"谁来陪我", who); subMenu(root, L"人物大小", size);
        subMenu(root, L"红衣女孩", actions(Character::girl)); subMenu(root, L"白衣男孩", actions(Character::boy)); subMenu(root, L"两个人的时光", pairedActions());
    }
    addItem(root, Reset, L"回到桌面右下角");
    if (panel) addItem(root, Hide, L"暂时藏起来");
    separator(root); addItem(root, Help, L"怎么玩"); addItem(root, Quit, L"退出桌边的你们");
    trackingMenu = true; SetForegroundWindow(host);
    const UINT chosen = TrackPopupMenuEx(root, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, point.x, point.y, host, nullptr);
    DestroyMenu(root); PostMessageW(host, WM_NULL, 0, 0); trackingMenu = false;
    nextPair = std::max(nextPair, clockNow()+15); if (pair) pairUntil = std::max(pairUntil, clockNow()+3);
    if (chosen) command(chosen);
}
void App::command(UINT id) {
    if (id >= PoseBase && id < PoseBase+200) {
        const int c = (id-PoseBase)/100, raw = (id-PoseBase)%100;
        if (raw > int(Pose::love)) return;
        finishPair(); hidden = false; if (mode != 0) mode = c+1; setPaused(false);
        const auto pose = static_cast<Pose>(raw);
        panels[c].behavior.set(pose, clockNow(), pose == Pose::sleep ? 40 : (pose == Pose::busy || pose == Pose::snack ? 24 : 4));
        nextPair = clockNow()+65;
    } else if (id >= PairBase && id < PairBase+4) {
        hidden = false; mode = 0; startPair(static_cast<Together>(id-PairBase));
    } else if (id >= ModeBoth && id <= ModeBoy) { finishPair(); mode = id-ModeBoth; hidden = false; }
    else if (id >= SizeSmall && id <= SizeLarge) { scale = std::array<double, 3>{0.8, 1, 1.25}[id-SizeSmall]; for (auto& panel : panels) resize(panel); }
    else switch (id) {
    case Hide: hidden = !hidden; break;
    case Pause: setPaused(!paused); break;
    case Quiet: quiet = !quiet; nextPair = clockNow()+75; break;
    case Reset: hidden = false; reset(); break;
    case Help:
        MessageBoxW(host, L"点击人物：打招呼，连戳会有小脾气。\n按住拖动：换个位置，放下后自动保存。\n右键人物：选择读书、吃饭、比心、小睡或双人互动。\n\n把两个人放近一些，偶尔会招呼对方、挪近，再一起过小日子。\n\n任务栏右下角的双人图标，可以调整大小、单人显示、安静陪伴和暂停。图标可能在托盘的折叠菜单中。\n\n程序离线运行，不读取屏幕或键盘内容。", L"很高兴，住进你的桌面。", MB_OK | MB_ICONINFORMATION); break;
    case Quit: save(); PostQuitMessage(0); return;
    default: return;
    }
    visibility(); save(); schedule(); tick(true);
}

LRESULT CALLBACK App::petProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Panel* p = reinterpret_cast<Panel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) { p = static_cast<Panel*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); p->hwnd = hwnd; SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(p)); }
    if (!p || !p->app) return DefWindowProcW(hwnd, msg, wp, lp);
    auto& app = *p->app;
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps{}; BeginPaint(hwnd, &ps); EndPaint(hwnd, &ps); return 0; }
    case WM_LBUTTONDOWN: {
        p->pressed = true; p->dragging = false; p->dragStart = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; ClientToScreen(hwnd, &p->dragStart);
        const auto r = windowRect(hwnd); p->origin = {r.left, r.top}; SetCapture(hwnd); return 0;
    }
    case WM_MOUSEMOVE:
        if (p->pressed) {
            POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; ClientToScreen(hwnd, &point);
            if (!p->dragging && std::hypot(point.x-p->dragStart.x, point.y-p->dragStart.y) > 3*app.unit(*p)) {
                p->dragging = true; app.approaching.reset(); app.setPaused(false); if (p->id < 2) p->behavior.lift(clockNow());
            }
            if (p->dragging) { moveWindow(hwnd, p->origin.x+point.x-p->dragStart.x, p->origin.y+point.y-p->dragStart.y); app.nextPair = clockNow()+70; }
        }
        return 0;
    case WM_LBUTTONUP:
        if (p->pressed) { const bool dragged = p->dragging; p->pressed = false; p->dragging = false; ReleaseCapture(); if (dragged) app.land(*p); else app.touch(*p); }
        return 0;
    case WM_CAPTURECHANGED: if (p->pressed) { p->pressed = false; if (p->dragging) app.land(*p); } return 0;
    case WM_CONTEXTMENU: {
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (point.x == -1 && point.y == -1) { const auto r = windowRect(hwnd); point = {(r.left+r.right)/2, (r.top+r.bottom)/2}; }
        app.menu(p, point); return 0;
    }
    case WM_DPICHANGED: {
        p->dpi = HIWORD(wp)/96.0; const RECT suggested = *reinterpret_cast<RECT*>(lp);
        moveWindow(hwnd, suggested.left, suggested.top); app.resize(*p); app.tick(true); return 0;
    }
    case WM_DISPLAYCHANGE: clampWindow(hwnd); return 0;
    default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}
LRESULT CALLBACK App::hostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) { app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app)); }
    if (!app) return DefWindowProcW(hwnd, msg, wp, lp);
    if (msg == app->taskbarCreated && app->taskbarCreated) { Shell_NotifyIconW(NIM_ADD, &app->tray); Shell_NotifyIconW(NIM_SETVERSION, &app->tray); return 0; }
    switch (msg) {
    case WM_TIMER: app->tick(); return 0;
    case TrayMessage:
        if (LOWORD(lp) == WM_CONTEXTMENU || LOWORD(lp) == NIN_SELECT || LOWORD(lp) == NIN_KEYSELECT || LOWORD(lp) == WM_RBUTTONUP) {
            POINT point{}; GetCursorPos(&point); app->menu(nullptr, point);
        }
        return 0;
    case ReopenMessage: app->hidden = false; app->reset(); app->visibility(); app->tick(true); return 0;
    case WM_SETTINGCHANGE: app->checkMotion(); app->schedule(); for (auto& p : app->panels) clampWindow(p.hwnd); return 0;
    case WM_DISPLAYCHANGE: for (auto& p : app->panels) clampWindow(p.hwnd); app->save(); return 0;
    case WM_POWERBROADCAST:
        if (wp == PBT_APMSUSPEND) app->sleeping = true;
        else if (wp == PBT_APMRESUMEAUTOMATIC) app->sleeping = false;
        else if (wp == PBT_POWERSETTINGCHANGE) {
            const auto setting = reinterpret_cast<POWERBROADCAST_SETTING*>(lp);
            if (IsEqualGUID(setting->PowerSetting, GUID_CONSOLE_DISPLAY_STATE) && setting->DataLength >= sizeof(DWORD)) {
                DWORD state = 0; std::memcpy(&state, setting->Data, sizeof(state)); app->sleeping = state == 0;
            }
        }
        app->lastTick = clockNow(); app->schedule(); return TRUE;
    case WM_QUERYENDSESSION: app->save(); return TRUE;
    default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void App::init() {
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW hostType{}; hostType.lpfnWndProc = hostProc; hostType.hInstance = instance; hostType.lpszClassName = HostClass;
    RegisterClassW(&hostType);
    WNDCLASSW petType{}; petType.lpfnWndProc = petProc; petType.hInstance = instance; petType.lpszClassName = PetClass; petType.hCursor = LoadCursorW(nullptr, IDC_HAND);
    RegisterClassW(&petType);
    host = CreateWindowExW(WS_EX_TOOLWINDOW, HostClass, L"桌边的你们", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, this);
    if (!host) throw std::runtime_error("Cannot create desktop controller");
    std::array<std::optional<pets::Point>, 2> saved;
    if (!testing) {
        PWSTR folder = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &folder))) { settingsPath = fs::path(folder)/L"DeskCompanions"/L"settings.ini"; CoTaskMemFree(folder); fs::create_directories(settingsPath.parent_path()); }
        const auto read = [&](const wchar_t* key) { wchar_t value[80]{}; GetPrivateProfileStringW(L"DesktopPets", key, L"", value, 80, settingsPath.c_str()); return std::wstring(value); };
        const auto size = read(L"size"); if (size == L"0.800000") scale = 0.8; else if (size == L"1.250000") scale = 1.25;
        quiet = read(L"quiet") == L"1"; const auto selected = read(L"mode"); mode = selected == L"1" ? 1 : (selected == L"2" ? 2 : 0);
        for (int i = 0; i < 2; ++i) {
            const auto x = read(i == 0 ? L"girlX" : L"boyX"), y = read(i == 0 ? L"girlY" : L"boyY");
            if (!x.empty() && !y.empty()) { wchar_t *endX = nullptr, *endY = nullptr; const double px = std::wcstod(x.c_str(), &endX), py = std::wcstod(y.c_str(), &endY); if (*endX == 0 && *endY == 0 && std::isfinite(px) && std::isfinite(py)) saved[i] = pets::Point{px,py}; }
        }
    }
    const double now = clockNow();
    for (auto& panel : panels) {
        panel.app = this; panel.behavior = Behavior(panel.id == 1 ? Character::boy : Character::girl, now);
        panel.hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, PetClass, L"桌边的你们", WS_POPUP, 0, 0, 250, 290, nullptr, nullptr, instance, &panel);
        if (!panel.hwnd) throw std::runtime_error("Cannot create transparent pet window");
        panel.dpi = GetDpiForWindow(panel.hwnd)/96.0; resize(panel);
    }
    reset();
    for (int i = 0; i < 2; ++i) {
        if (saved[i]) { moveWindow(panels[i].hwnd, saved[i]->x, saved[i]->y); clampWindow(panels[i].hwnd); }
        panels[i].behavior.set(Pose::wave, now, 3.5, i == 0 ? L"我来陪你啦。" : L"右键找我玩，拖动搬个家。");
    }
    checkMotion(); lastTick = now; nextPair = now+65;
    {
        // A native, small two-person tray glyph; character art stays in the sprites.
        Bitmap glyph(32,32,PixelFormat32bppPARGB); Graphics g(&glyph); g.Clear(Color(0,0,0,0)); g.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush cream(Color(255,255,245,218)), red(Color(255,222,64,62)), blue(Color(255,76,68,175)); Pen outline(Color(255,90,69,55),1.5f);
        g.FillEllipse(&red, 2,17,14,12); g.FillEllipse(&blue,16,17,14,12);
        g.FillEllipse(&cream,3,3,12,13); g.DrawEllipse(&outline,3,3,12,13); g.FillEllipse(&cream,17,3,12,13); g.DrawEllipse(&outline,17,3,12,13);
        g.Flush(FlushIntentionSync);
        if (glyph.GetHICON(&icon) != Ok || !icon) throw std::runtime_error("Cannot create tray icon");
    }
    if (!testing) {
        tray.cbSize = sizeof(tray); tray.hWnd = host; tray.uID = 1; tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; tray.uCallbackMessage = TrayMessage; tray.hIcon = icon;
        wcscpy_s(tray.szTip, L"桌边的你们 · 点击管理小伙伴");
        if (!Shell_NotifyIconW(NIM_ADD, &tray)) throw std::runtime_error("Cannot add system tray icon");
        tray.uVersion = NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION, &tray);
        taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
        powerNotice = RegisterPowerSettingNotification(host, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);
    }
    visibility(); schedule(); tick(true); save();
}

int selfTest(const fs::path& directory) {
    fs::create_directories(directory); std::ofstream report(directory/L"windows-self-test.txt");
    int checks = 0;
    const auto check = [&](bool ok, const char* reason) { ++checks; if (!ok) throw std::runtime_error(reason); };
    try {
        App app(true); app.init(); KillTimer(app.host, 1); app.reduced = true;
        check(app.icon != nullptr, "native tray icon renders");
        HMENU characterMenu = actions(Character::girl), sharedMenu = pairedActions();
        check(GetMenuItemCount(characterMenu) == 6 && GetMenuItemCount(sharedMenu) == 4, "native action menus contain expected actions");
        DestroyMenu(characterMenu); DestroyMenu(sharedMenu);
        check(app.sprites[0].size() == 9 && app.sprites[1].size() == 9 && app.sprites[2].size() == 4, "22 embedded sprites");
        for (const auto& list : app.sprites) for (const auto& sprite : list) {
            bool transparent = false, opaque = false;
            for (size_t i = 3; i < sprite.pixels.size(); i += 4) { transparent |= sprite.pixels[i] == 0; opaque |= sprite.pixels[i] == 255; }
            check(transparent && opaque && sprite.width > 150 && sprite.height > 150, "valid keyed sprite");
        }
        check(IsWindowVisible(app.panels[0].hwnd) && IsWindowVisible(app.panels[1].hwnd), "two windows launch");
        check((GetWindowLongPtrW(app.panels[0].hwnd,GWL_EXSTYLE) & (WS_EX_LAYERED | WS_EX_NOACTIVATE)) == (WS_EX_LAYERED | WS_EX_NOACTIVATE), "layered nonactivating window");
        for (int i = 0; i < 4; ++i) { app.command(PairBase+i); check(app.pair == static_cast<Together>(i) && IsWindowVisible(app.panels[2].hwnd) && !IsWindowVisible(app.panels[0].hwnd), "paired scene displays without duplicates"); app.finishPair(); }
        app.command(ModeGirl); check(IsWindowVisible(app.panels[0].hwnd) && !IsWindowVisible(app.panels[1].hwnd), "girl only");
        app.command(ModeBoy); check(!IsWindowVisible(app.panels[0].hwnd) && IsWindowVisible(app.panels[1].hwnd), "boy only");
        app.command(SizeLarge); check(app.scale == 1.25, "size changes"); app.command(SizeNormal);
        app.command(Quiet); check(app.quiet, "quiet toggles"); app.command(Quiet);
        app.command(Pause); check(app.paused, "pause"); app.command(Pause); check(!app.paused, "resume");
        app.command(Hide); check(app.hidden && !IsWindowVisible(app.panels[1].hwnd), "hide"); app.command(Hide);
        app.command(PoseBase+int(Pose::snack)); check(app.mode == 1 && app.panels[0].behavior.pose == Pose::snack, "character action switches single mode");
        app.command(ModeBoth); auto& panel = app.panels[0]; const auto before = windowRect(panel.hwnd);
        SendMessageW(panel.hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(120,120));
        SendMessageW(panel.hwnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(80,70));
        check(panel.dragging && panel.behavior.held, "drag holds character"); const auto after = windowRect(panel.hwnd);
        check(after.left == before.left-40 && after.top == before.top-50, "drag moves in event coordinates");
        SendMessageW(panel.hwnd,WM_LBUTTONUP,0,MAKELPARAM(80,70)); check(!panel.dragging && !panel.behavior.held, "release lands");
        app.reduced = false; app.startPair(Together::heart); check(app.approaching.has_value() && !app.pair, "approach precedes scene");
        app.approachAt -= 2; app.trackingMenu = true; app.tick(); check(!app.pair, "menu holds transition"); app.trackingMenu = false; app.tick(); check(app.pair == Together::heart, "approach completes"); app.finishPair();
        app.startPair(Together::read); app.touch(panel); check(!app.approaching, "touch cancels approach");
        app.reduced = true;
        for (int c = 0; c < 2; ++c) for (int pose = 0; pose < 9; ++pose) { app.panels[c].behavior.set(static_cast<Pose>(pose),clockNow(),10); app.render(app.panels[c],clockNow()); }
        check(true, "all native sprite renders succeed");
        UINT count = 0, bytes = 0; GetImageEncodersSize(&count,&bytes); std::vector<BYTE> info(bytes); GetImageEncoders(count,bytes,reinterpret_cast<ImageCodecInfo*>(info.data()));
        CLSID png{}; bool found = false; for (UINT i = 0; i < count; ++i) { const auto& codec = reinterpret_cast<ImageCodecInfo*>(info.data())[i]; if (wcscmp(codec.MimeType,L"image/png") == 0) { png = codec.Clsid; found = true; } }
        check(found, "PNG encoder available");
        app.panels[0].behavior.set(Pose::wave,clockNow(),4,L"我在呀。");
        app.render(app.panels[0],clockNow());
        Bitmap windowImage(app.panels[0].width,app.panels[0].height,app.panels[0].width*4,PixelFormat32bppPARGB,app.panels[0].surface);
        check(windowImage.Save((directory/L"windows-window.png").c_str(),&png,nullptr) == Ok, "actual transparent window frame exported");
        Bitmap preview(1100,520,PixelFormat32bppPARGB); Graphics graphics(&preview); graphics.Clear(Color(255,247,242,232));
        for (int c = 0; c < 2; ++c) for (int pose = 0; pose < 9; ++pose) {
            Bitmap* sprite = app.sprites[c][pose].image(0.5); graphics.DrawImage(sprite,REAL(10+pose*121),REAL(15+c*155));
        }
        SolidBrush dark(Color(255,38,52,49)); graphics.FillRectangle(&dark,0,310,1100,210);
        for (int scene = 0; scene < 4; ++scene) { Bitmap* sprite = app.sprites[2][scene].image(0.75); graphics.DrawImage(sprite,REAL(10+scene*274),REAL(325)); }
        check(preview.Save((directory/L"windows-preview.png").c_str(),&png,nullptr) == Ok, "native preview exported");
        KillTimer(app.host,1); report << "PASS: " << checks << " Win32 native integration assertions.\n22 embedded PNG sprites, menus, windows, drag, DPI scale and shared scenes.\n";
        return 0;
    } catch (const std::exception& e) { report << "FAIL: " << e.what() << "\nAfter " << checks << " checks.\n"; return 1; }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    GdiplusStartupInput startup; ULONG_PTR token = 0;
    if (GdiplusStartup(&token,&startup,nullptr) != Ok) return 1;
    int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(),&argc);
    const bool test = argc >= 2 && wcscmp(argv[1],L"--self-test") == 0;
    const fs::path output = argc >= 3 ? fs::path(argv[2]) : fs::path(L"windows-test-output");
    LocalFree(argv);
    int result = 0; HANDLE mutex = nullptr;
    try {
        if (test) result = selfTest(output);
        else {
            mutex = CreateMutexW(nullptr,TRUE,L"Local\\DeskCompanions-v1");
            if (!mutex) throw std::runtime_error("Cannot create application lock");
            if (GetLastError() == ERROR_ALREADY_EXISTS) { if (HWND existing = FindWindowW(HostClass,nullptr)) PostMessageW(existing,ReopenMessage,0,0); }
            else {
                App app; app.init(); MSG message{};
                while (true) { const BOOL read = GetMessageW(&message,nullptr,0,0); if (read == 0) { result = int(message.wParam); break; } if (read == -1) throw std::runtime_error("Windows message loop failed"); TranslateMessage(&message); DispatchMessageW(&message); }
            }
        }
    } catch (const std::exception& e) {
        const int n = MultiByteToWideChar(CP_UTF8,0,e.what(),-1,nullptr,0); std::wstring error(n,L'\0'); MultiByteToWideChar(CP_UTF8,0,e.what(),-1,error.data(),n);
        MessageBoxW(nullptr,(L"小伙伴暂时没能出门。\n\n"+error).c_str(),L"桌边的你们",MB_OK | MB_ICONERROR); result = 1;
    }
    if (mutex) CloseHandle(mutex);
    GdiplusShutdown(token); if (SUCCEEDED(com)) CoUninitialize(); return result;
}
