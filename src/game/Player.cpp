#include "Player.h"
#include <algorithm>

namespace OS {

void Player::TakeDamage(float dmg, int type) {
    if (!alive || dmg <= 0) return;

    // Armor absorption (CS 1.6: armor absorbs 50% of bullet damage, 20% of blast)
    if (armor > 0 && (type & (DMG_BULLET | DMG_SLASH))) {
        float armorFrac = 0.5f;
        float armorDmg = dmg * armorFrac;
        if (armorDmg > armor) {
            dmg -= armor;
            armor = 0;
        } else {
            armor -= armorDmg;
            dmg *= (1.0f - armorFrac);
        }
        // Helmet reduces headshot damage further (handled by weapon system)
    }

    health -= dmg;
    if (health <= 0) {
        health = 0;
        alive  = false;
        deaths++;
    }
}

void Player::Respawn() {
    health        = 100.0f;
    alive         = true;
    physState     = {};
    physState.velocity = {};
    killsThisRound = 0;
    isDefusing    = false;
    defuseTimer   = 0;
    // money preserved across rounds (CS 1.6 economy)
}

} // namespace OS
