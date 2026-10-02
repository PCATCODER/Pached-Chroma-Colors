#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <algorithm>
#include <climits>
#include <cmath>

using namespace geode::prelude;

namespace {
    float g_hue = 0.f;
    float g_syncTimer = 0.f;
    int g_lastPaletteId = -1;
    std::vector<WeakRef<SimplePlayer>> g_tracked;

    template <class GM>
    void setGlowId(GM* gm, int id) {
        if constexpr (requires { gm->m_playerGlowColor; }) {
            gm->m_playerGlowColor = id;
        } else if constexpr (requires { gm->setPlayerColor3(id); }) {
            gm->setPlayerColor3(id);
        }
    }

    ccColor3B hueToRGB(float h) {
        h = std::fmod(h, 1.f);
        if (h < 0.f) h += 1.f;

        float r = std::fabs(h * 6.f - 3.f) - 1.f;
        float g = 2.f - std::fabs(h * 6.f - 2.f);
        float b = 2.f - std::fabs(h * 6.f - 4.f);
        auto cl = [](float v) { return std::clamp(v, 0.f, 1.f); };

        return {
            static_cast<GLubyte>(cl(r) * 255.f),
            static_cast<GLubyte>(cl(g) * 255.f),
            static_cast<GLubyte>(cl(b) * 255.f)
        };
    }

    int nearestPaletteId(ccColor3B c) {
        auto gm = GameManager::get();
        int best = 0;
        int bestDist = INT_MAX;

        for (int i = 0; i < 107; i++) {
            auto p = gm->colorForIdx(i);
            int dr = int(p.r) - int(c.r);
            int dg = int(p.g) - int(c.g);
            int db = int(p.b) - int(c.b);
            int d = dr * dr + dg * dg + db * db;

            if (d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
        return best;
    }

    void trackPlayer(SimplePlayer* sp) {
        if (!sp) return;

        // Avoid accumulating duplicate references when a page is refreshed.
        auto found = std::find_if(g_tracked.begin(), g_tracked.end(), [sp](auto const& w) {
            return w.lock() == sp;
        });

        if (found == g_tracked.end())
            g_tracked.push_back(WeakRef<SimplePlayer>(sp));
    }

    void collectPlayers(CCNode* node) {
        if (!node) return;

        if (auto sp = typeinfo_cast<SimplePlayer*>(node))
            trackPlayer(sp);

        if (auto kids = node->getChildren()) {
            for (auto kid : CCArrayExt<CCNode*>(kids))
                collectPlayers(kid);
        }
    }

    // Applies the selected rainbow targets to a SimplePlayer preview.
    void paintSimplePlayer(SimplePlayer* sp, ccColor3B col) {
        auto mod = Mod::get();
        int target = mod->getSettingValue<int>("target");

        // 0 = Color 1, 1 = Color 2, 2 = Glow, 3 = Outline, 4 = All
        if (target == 0 || target == 4) {
            // setColors updates both layers, so preserve the existing Color 2
            // by changing only the first layer directly when possible.
            if (sp->m_firstLayer)
                sp->m_firstLayer->setColor(col);
        }

        if (target == 1 || target == 4)
            sp->setSecondColor(col);

        if (target == 2 || target == 4) {
            sp->enableCustomGlowColor(col);
            if (sp->m_robotSprite && sp->m_robotSprite->isVisible()) {
                sp->m_robotSprite->showGlow();
                sp->m_robotSprite->updateGlowColor(col, false);
            } else if (sp->m_spiderSprite && sp->m_spiderSprite->isVisible()) {
                sp->m_spiderSprite->showGlow();
                sp->m_spiderSprite->updateGlowColor(col, false);
            }
        }

        if (target == 3 || target == 4) {
            if (sp->m_outlineSprite) {
                sp->m_outlineSprite->setVisible(true);
                sp->m_outlineSprite->setColor(col);
            }
            sp->setGlowOutline(col);
        }
    }

    void restoreOriginalColors() {
        auto mod = Mod::get();
        auto gm = GameManager::get();

        if (!mod->getSavedValue<bool>("has-orig", false))
            return;

        gm->setPlayerColor2(mod->getSavedValue<int>("orig-color2"));
        setGlowId(gm, mod->getSavedValue<int>("orig-glow-color"));
        gm->setPlayerGlow(mod->getSavedValue<bool>("orig-glow"));
        mod->setSavedValue("has-orig", false);
        g_lastPaletteId = -1;
    }

    void tick(float dt) {
        auto mod = Mod::get();

        if (!mod->getSettingValue<bool>("enabled"))
            return;

        float speed = static_cast<float>(mod->getSettingValue<double>("speed"));
        g_hue += dt * speed;
        g_hue = std::fmod(g_hue, 1.f);

        ccColor3B col = hueToRGB(g_hue);

        std::erase_if(g_tracked, [](auto& w) { return !w.lock(); });
        for (auto& w : g_tracked) {
            if (auto sp = w.lock())
                paintSimplePlayer(sp, col);
        }

        bool sync = mod->getSettingValue<bool>("palette-sync");
        bool hasOrig = mod->getSavedValue<bool>("has-orig", false);
        auto gm = GameManager::get();

        if (sync) {
            if (!hasOrig) {
                mod->setSavedValue("orig-color2", gm->getPlayerColor2());
                mod->setSavedValue("orig-glow-color", gm->getPlayerGlowColor());
                mod->setSavedValue("orig-glow", gm->getPlayerGlow());
                mod->setSavedValue("has-orig", true);
            }

            g_syncTimer += dt;
            if (g_syncTimer >= 0.1f) {
                g_syncTimer = 0.f;
                int id = nearestPaletteId(col);

                if (id != g_lastPaletteId) {
                    g_lastPaletteId = id;
                    gm->setPlayerColor2(id);
                    setGlowId(gm, id);
                    gm->setPlayerGlow(true);
                }
            }
        } else if (hasOrig) {
            restoreOriginalColors();
        }
    }
}

class $modify(RainbowScheduler, CCScheduler) {
    void update(float dt) {
        CCScheduler::update(dt);
        tick(dt);
    }
};

class $modify(RainbowProfile, ProfilePage) {
    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);

        if (score && score->m_accountID == GJAccountManager::get()->m_accountID)
            collectPlayers(this);
    }
};

class $modify(RainbowGarage, GJGarageLayer) {
    bool init() {
        if (!GJGarageLayer::init())
            return false;

        trackPlayer(this->m_playerObject);
        return true;
    }
};

class $modify(RainbowPlayer, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);

        auto mod = Mod::get();
        if (!mod->getSettingValue<bool>("enabled"))
            return;

        auto pl = PlayLayer::get();
        if (!pl)
            return;

        if (this != pl->m_player1 && this != pl->m_player2)
            return;

        ccColor3B col = hueToRGB(g_hue);
        int target = mod->getSettingValue<int>("target");

        if (target == 1 || target == 4)
            this->setSecondColor(col);

        if (target == 2 || target == 4) {
            if (!this->m_hasGlow) {
                this->m_hasGlow = true;
                this->updatePlayerGlow();
            }
            this->enableCustomGlowColor(col);
            this->updateGlowColor();
        }
    }
};

$on_mod(Loaded) {
    auto mod = Mod::get();

    if (!mod->getSettingValue<bool>("palette-sync") &&
        mod->getSavedValue<bool>("has-orig", false)) {
        restoreOriginalColors();
    }
}
