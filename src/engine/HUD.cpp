#include "HUD.h"
#include "../game/Player.h"
#include "../game/Weapon.h"
#include "../game/GameRules.h"
#include <cmath>
#include <algorithm>

namespace OS {

// Seven-segment layout:
//    _0_
//  5|   |1
//   |_6_|
//  4|   |2
//   |_3_|
static const uint8_t kDigitSegments[10] = {
    0b0111111, // 0: segments 0,1,2,3,4,5
    0b0000110, // 1: 1,2
    0b1011011, // 2: 0,1,3,4,6
    0b1001111, // 3: 0,1,2,3,6
    0b1100110, // 4: 1,2,5,6
    0b1101101, // 5: 0,2,3,5,6
    0b1111101, // 6: 0,2,3,4,5,6
    0b0000111, // 7: 0,1,2
    0b1111111, // 8: all
    0b1101111, // 9: 0,1,2,3,5,6
};

void HUD::DrawSegment(int seg, Vec2 pos, float h, Vec4 color) {
    float w  = h * 0.55f;       // digit width
    float t  = std::max(2.0f, h * 0.12f); // segment thickness
    float hw = w - t;           // horizontal segment length
    float vh = (h - 3*t) * 0.5f; // vertical segment length

    switch (seg) {
    case 0: m_r.Draw2DRect({pos.x + t,      pos.y},                {hw, t}, color); break;
    case 1: m_r.Draw2DRect({pos.x + w,      pos.y + t},            {t, vh}, color); break;
    case 2: m_r.Draw2DRect({pos.x + w,      pos.y + 2*t + vh},     {t, vh}, color); break;
    case 3: m_r.Draw2DRect({pos.x + t,      pos.y + 2*t + 2*vh},   {hw, t}, color); break;
    case 4: m_r.Draw2DRect({pos.x,          pos.y + 2*t + vh},     {t, vh}, color); break;
    case 5: m_r.Draw2DRect({pos.x,          pos.y + t},            {t, vh}, color); break;
    case 6: m_r.Draw2DRect({pos.x + t,      pos.y + t + vh},       {hw, t}, color); break;
    }
}

void HUD::DrawDigit(int digit, Vec2 pos, float h, Vec4 color) {
    if (digit < 0 || digit > 9) return;
    uint8_t mask = kDigitSegments[digit];
    for (int s = 0; s < 7; s++)
        if (mask & (1 << s))
            DrawSegment(s, pos, h, color);
}

void HUD::DrawNumber(int value, Vec2 pos, float digitH, Vec4 color, int minDigits) {
    if (value < 0) value = 0;

    int digits[10];
    int count = 0;
    int v = value;
    do {
        digits[count++] = v % 10;
        v /= 10;
    } while (v > 0 && count < 10);
    while (count < minDigits) digits[count++] = 0;

    float step = digitH * 0.55f + digitH * 0.25f;
    for (int i = 0; i < count; i++) {
        Vec2 p = { pos.x + (count - 1 - i) * step, pos.y };
        DrawDigit(digits[i], p, digitH, color);
    }
}

void HUD::DrawCrosshair(float gap, float len, Vec4 color) {
    float cx = m_w * 0.5f;
    float cy = m_h * 0.5f;
    float t  = 2.0f; // line thickness

    m_r.Draw2DRect({cx - gap - len, cy - t*0.5f}, {len, t}, color); // left
    m_r.Draw2DRect({cx + gap,       cy - t*0.5f}, {len, t}, color); // right
    m_r.Draw2DRect({cx - t*0.5f, cy - gap - len}, {t, len}, color); // top
    m_r.Draw2DRect({cx - t*0.5f, cy + gap},       {t, len}, color); // bottom
}

void HUD::DrawHealthArmor(float health, float armor) {
    float h = 26.0f;
    Vec4 color = health > 25 ? HUDColors::Main() : HUDColors::Danger();

    // Health (bottom-left)
    Vec2 hpPos = { 24.0f, m_h - h - 20.0f };
    // Cross icon: two rects
    m_r.Draw2DRect({hpPos.x, hpPos.y + h*0.33f}, {h*0.8f, h*0.33f}, color);
    m_r.Draw2DRect({hpPos.x + h*0.27f, hpPos.y}, {h*0.27f, h}, color);
    DrawNumber((int)std::ceil(health), {hpPos.x + h + 12, hpPos.y}, h, color, 1);

    // Armor next to health
    if (armor > 0) {
        Vec2 apPos = { hpPos.x + h + 12 + 4 * (h*0.8f), hpPos.y };
        // Shield icon: simple block
        m_r.Draw2DRect({apPos.x, apPos.y}, {h*0.6f, h*0.85f}, HUDColors::Main());
        DrawNumber((int)std::ceil(armor), {apPos.x + h*0.6f + 12, apPos.y}, h,
                   HUDColors::Main(), 1);
    }
}

void HUD::DrawAmmo(int clip, int reserve) {
    float h = 26.0f;
    // Bottom-right: "clip / reserve"
    float baseX = m_w - 24.0f;
    Vec2 reservePos = { baseX - 2 * (h*0.8f), m_h - h - 20.0f };
    DrawNumber(reserve, reservePos, h * 0.7f, HUDColors::Main(), 1);

    Vec2 clipPos = { reservePos.x - 3.5f * (h*0.8f), m_h - h - 20.0f };
    DrawNumber(clip, clipPos, h, HUDColors::Main(), 1);

    // Divider
    m_r.Draw2DRect({reservePos.x - h*0.55f, m_h - h - 18.0f}, {3, h - 4}, HUDColors::Main());
}

void HUD::DrawMoney(int money) {
    float h = 22.0f;
    // Top-right area, below timer
    Vec2 pos = { m_w - 24.0f - 5.5f * (h*0.8f), 64.0f };
    // $ symbol approximated by S-like seven segment (digit 5) + strikethrough
    DrawDigit(5, {pos.x - h*0.8f, pos.y}, h, HUDColors::Main());
    m_r.Draw2DRect({pos.x - h*0.8f + h*0.2f, pos.y - 3}, {2, h + 6}, HUDColors::Main());
    DrawNumber(money, pos, h, HUDColors::Main(), 1);
}

void HUD::DrawTimer(float seconds) {
    if (seconds < 0) seconds = 0;
    int mins = (int)seconds / 60;
    int secs = (int)seconds % 60;

    float h = 24.0f;
    float cx = m_w * 0.5f;

    // mm : ss centered at top
    Vec2 minPos = { cx - 3.2f * (h*0.55f), 18.0f };
    DrawNumber(mins, minPos, h, HUDColors::Main(), 2);

    // Colon dots
    m_r.Draw2DRect({cx - h*0.1f, 18.0f + h*0.25f}, {4, 4}, HUDColors::Main());
    m_r.Draw2DRect({cx - h*0.1f, 18.0f + h*0.65f}, {4, 4}, HUDColors::Main());

    Vec2 secPos = { cx + h*0.35f, 18.0f };
    DrawNumber(secs, secPos, h, HUDColors::Main(), 2);
}

void HUD::Draw(const Player& player, const WeaponState& weapon,
               const GameState& game, float crosshairGap) {
    if (!player.alive) return;

    DrawCrosshair(crosshairGap, 7.0f, HUDColors::Cross());
    DrawHealthArmor(player.health, player.armor);

    const WeaponDef* def = WeaponDatabase::Get(weapon.id);
    if (def && def->clipSize > 0)
        DrawAmmo(weapon.clip, weapon.reserve);

    DrawMoney(player.money);
    DrawTimer(game.timer);
}

} // namespace OS
