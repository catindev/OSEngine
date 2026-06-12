#pragma once
// HUD rendering: crosshair, health/armor, ammo, money, round timer.
// Numbers drawn as seven-segment digits via 2D rects (no font assets needed).
// Authentic sprite-based HUD (640hud*.spr from user assets) planned later.

#include "../renderer/Renderer.h"

namespace OS {

struct Player;
struct WeaponState;
struct GameState;

struct HUDColors {
    // CS 1.6 default HUD yellow-orange
    static Vec4 Main()   { return {1.0f, 0.625f, 0.0f, 0.9f}; }
    static Vec4 Danger() { return {1.0f, 0.125f, 0.125f, 0.9f}; }
    static Vec4 Cross()  { return {0.0f, 1.0f, 0.0f, 0.85f}; }
};

class HUD {
public:
    explicit HUD(Renderer& renderer) : m_r(renderer) {}

    void SetScreenSize(int w, int h) { m_w = w; m_h = h; }

    // Draw all HUD elements for the local player
    void Draw(const Player& player, const WeaponState& weapon,
              const GameState& game, float crosshairGap);

    // Individual elements
    void DrawCrosshair(float gap, float len, Vec4 color);
    void DrawNumber(int value, Vec2 pos, float digitH, Vec4 color, int minDigits = 1);
    void DrawHealthArmor(float health, float armor);
    void DrawAmmo(int clip, int reserve);
    void DrawMoney(int money);
    void DrawTimer(float seconds);

private:
    Renderer& m_r;
    int m_w = 1280, m_h = 720;

    void DrawDigit(int digit, Vec2 pos, float h, Vec4 color);
    void DrawSegment(int seg, Vec2 pos, float h, Vec4 color);
};

} // namespace OS
