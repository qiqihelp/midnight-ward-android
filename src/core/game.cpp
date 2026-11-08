// game.cpp — Game.java 的 C++ 移植。
#include "game.h"
#include <algorithm>
#include <cmath>
#include <cstring>

Sprite::Sprite(int floor_, float x_, float y_, int type_)
    : x(x_), y(y_), floor(floor_), type(type_) {
    switch (type_) {
        case TYPE_KEY:
        case TYPE_BKEY:
            height = 0.26f; aspect = 1.0f; pickable = true; break;
        case TYPE_CARD:
            height = 0.20f; aspect = 1.0f; pickable = true; break;
        case TYPE_TORCH:
            height = 0.22f; aspect = 1.0f; pickable = true; break;
        case TYPE_CROWBAR:
            height = 0.30f; aspect = 1.0f; pickable = true; break;
        case TYPE_BED:
            height = 0.42f; aspect = 1.6f; break;
        case TYPE_GHOST:
            height = 0.92f; aspect = 0.667f; break;
        default:
            height = 0.3f; aspect = 1.0f; break;
    }
}

void Game::reset() {
    player.reset(lm);
    for (auto& kv : lm.doors) {
        kv.second.openness = 0.f;
        kv.second.opening = false;
    }
    boards = 2;
    gateOpen = false;
    gateOpenness = 0.f;
    sprites.clear();
    // 物品布置（与 Java ITEM_DEFS 一致）
    static const int ITEM_DEFS[][4] = {
        {lm::F2, 2, 5, Sprite::TYPE_KEY},       // 病房钥匙（房内桌上）
        {lm::F2, 4, 1, Sprite::TYPE_BED},       // 病床（陈设）
        {lm::F2, 5, 14, Sprite::TYPE_CARD},     // 房卡（二楼值班室）
        {lm::F1, 4, 3, Sprite::TYPE_TORCH},     // 手电筒（一楼药房）
        {lm::F1, 12, 3, Sprite::TYPE_BKEY},     // 地下室钥匙（院长办公室）
        {lm::B1, 16, 11, Sprite::TYPE_CROWBAR}, // 撬棍（地下室深处）
    };
    for (const auto& d : ITEM_DEFS)
        sprites.emplace_back(d[0], d[1] + 0.5f, d[2] + 0.5f, d[3]);
    ghost.reset();
    ghostLife = 0.f;
    messages.clear();
    timePlayed = 0.f;
    state = ST_INTRO;
    introPage = 0;
    fadeAlpha = 0.f;
    fadeDir = 0;
    hasPendingTransition = false;
    pendingTransition = lm::Transition{0, 0, 0, 0};
    ghostTimer = 18.f + rand01() * 15.f;
    lightningTimer = 10.f + rand01() * 14.f;
    beatTimer = 0.f;
    flashFrames = 0;
    interactLabel.clear();
    interactKind = 0;
    hasTarget = false;
}

void Game::startPlay() { state = ST_PLAY; }

void Game::addMsg(const std::string& s) {
    messages.emplace_back(s, 3.8f);
    if (messages.size() > 4) messages.erase(messages.begin());
}

void Game::update(float dt) {
    if (flashFrames > 0) flashFrames--;
    if (state != ST_PLAY) return;
    timePlayed += dt;

    updateFade(dt);
    updateDoors(dt);
    checkAutoDoors();
    checkTransition();
    checkPickups();
    checkWin();
    updateEvents(dt);
    updateInteract();

    for (size_t i = 0; i < messages.size();) {
        messages[i].life -= dt;
        if (messages[i].life <= 0.f) messages.erase(messages.begin() + i);
        else i++;
    }
}

void Game::updateFade(float dt) {
    if (fadeDir == 1) {
        fadeAlpha += dt * 2.8f;
        if (fadeAlpha >= 1.f) {
            fadeAlpha = 1.f;
            doTeleport();
            fadeDir = 2;
        }
    } else if (fadeDir == 2) {
        fadeAlpha -= dt * 2.4f;
        if (fadeAlpha <= 0.f) {
            fadeAlpha = 0.f;
            fadeDir = 0;
        }
    }
}

void Game::updateDoors(float dt) {
    for (auto& kv : lm.doors) {
        lm::DoorState& d = kv.second;
        if (d.opening && d.openness < 1.f)
            d.openness = std::min(1.f, d.openness + dt * 1.7f);
    }
    if (gateOpen && gateOpenness < 1.f)
        gateOpenness = std::min(1.f, gateOpenness + dt * 1.2f);
}

void Game::checkAutoDoors() {
    for (auto& kv : lm.doors) {
        lm::DoorState& d = kv.second;
        if (!d.isAutoOpen() || d.floor != player.floor || d.openness >= 1.f) continue;
        float dx = d.x + 0.5f - player.x;
        float dy = d.y + 0.5f - player.y;
        if (dx * dx + dy * dy < 1.5f * 1.5f) d.opening = true;
    }
}

void Game::checkTransition() {
    if (hasPendingTransition || fadeDir != 0) return;
    int cx = (int)std::floor(player.x);
    int cy = (int)std::floor(player.y);
    auto it = lm.transitions.find(LevelMap::key(player.floor, cx, cy));
    if (it == lm.transitions.end()) return;
    auto dit = lm.doors.find(LevelMap::key(player.floor, cx, cy));
    if (dit != lm.doors.end() && dit->second.openness > 0.75f) {
        pendingTransition = it->second;
        hasPendingTransition = true;
        fadeDir = 1; // 渐黑后传送
    }
}

void Game::doTeleport() {
    if (!hasPendingTransition) return;
    player.floor = pendingTransition.floor;
    player.x = pendingTransition.x;
    player.y = pendingTransition.y;
    player.angle = pendingTransition.angle;
    hasPendingTransition = false;
}

void Game::checkPickups() {
    for (auto& s : sprites) {
        if (!s.alive || !s.pickable || s.floor != player.floor) continue;
        float dx = s.x - player.x;
        float dy = s.y - player.y;
        if (dx * dx + dy * dy > 0.62f * 0.62f) continue;
        s.alive = false;
        switch (s.type) {
            case Sprite::TYPE_KEY:
                player.hasKey = true;
                addMsg(str::MSG_KEY); break;
            case Sprite::TYPE_CARD:
                player.hasCard = true;
                addMsg(str::MSG_CARD); break;
            case Sprite::TYPE_TORCH:
                player.hasFlashlight = true;
                player.flashlightOn = true;
                addMsg(str::MSG_TORCH); break;
            case Sprite::TYPE_BKEY:
                player.hasBasementKey = true;
                addMsg(str::MSG_BKEY); break;
            case Sprite::TYPE_CROWBAR:
                player.hasCrowbar = true;
                addMsg(str::MSG_CROWBAR);
                scare(); // 撬棍惊吓：背后显形
                break;
            default: break;
        }
    }
}

void Game::checkWin() {
    if (player.floor == lm::F1 && (int)std::floor(player.x) == 11
        && (int)std::floor(player.y) == 17) {
        state = ST_WIN;
    }
}

void Game::updateEvents(float dt) {
    if (player.floor != lm::B1) {
        lightningTimer -= dt;
        if (lightningTimer <= 0.f) {
            flashFrames = 6;
            lightningTimer = 14.f + rand01() * 16.f;
        }
    } else {
        beatTimer -= dt;
        if (beatTimer <= 0.f) beatTimer = 1.2f; // 心跳节拍（无音频，仅节奏状态）
    }

    if (!ghost) {
        ghostTimer -= dt;
        if (ghostTimer <= 0.f) {
            ghostTimer = 22.f + rand01() * 22.f;
            float gx, gy;
            if (findGhostSpot(gx, gy)) {
                ghost = std::make_unique<Sprite>(player.floor, gx, gy, Sprite::TYPE_GHOST);
                ghostLife = 2.4f;
            }
        }
    } else {
        ghostLife -= dt;
        float t = 1.f - std::max(0.f, ghostLife) / 2.4f;
        float a = std::sin(3.14159265f * std::min(1.f, std::max(0.f, t)));
        ghost->alpha = a * 0.8f;
        if (ghostLife <= 0.f) ghost.reset();
    }
}

bool Game::findGhostSpot(float& ox, float& oy) const {
    float dx = std::cos(player.angle);
    float dy = std::sin(player.angle);
    for (int d = 3; d <= 8; d++) {
        float fx = player.x + dx * d;
        float fy = player.y + dy * d;
        int cx = (int)std::floor(fx);
        int cy = (int)std::floor(fy);
        if (lm.cellAt(player.floor, cx, cy) == lm::CELL_FLOOR) {
            ox = fx; oy = fy;
            return true;
        }
    }
    return false;
}

void Game::scare() {
    float bx = player.x - std::cos(player.angle) * 1.4f;
    float by = player.y - std::sin(player.angle) * 1.4f;
    if (!lm.blocked(this, player.floor, bx, by)) {
        ghost = std::make_unique<Sprite>(player.floor, bx, by, Sprite::TYPE_GHOST);
        ghostLife = 2.6f;
    }
}

void Game::updateInteract() {
    interactLabel.clear();
    interactKind = 0;
    hasTarget = false;

    float dx = std::cos(player.angle);
    float dy = std::sin(player.angle);
    for (float t = 0.2f; t <= 1.6f; t += 0.12f) {
        int cx = (int)std::floor(player.x + dx * t);
        int cy = (int)std::floor(player.y + dy * t);
        int cell = lm.cellAt(player.floor, cx, cy);
        if (cell == lm::CELL_FLOOR) continue;
        if (cell == lm::CELL_GATE) {
            if (!gateOpen) {
                if (boards > 0) {
                    interactLabel = player.hasCrowbar ? str::IT_PRY : str::IT_BOARDS;
                    interactKind = 2;
                } else {
                    interactLabel = player.hasCard ? str::IT_SWIPE : str::IT_READER;
                    interactKind = 3;
                }
                interactTX = cx; interactTY = cy;
                hasTarget = true;
            }
            return;
        }
        if (cell == lm::CELL_DOOR || cell == lm::CELL_METAL || cell == lm::CELL_STAIR) {
            auto it = lm.doors.find(LevelMap::key(player.floor, cx, cy));
            if (it != lm.doors.end() && it->second.openness < 1.f) {
                if (it->second.lock == lm::LOCK_ROOM && !player.hasKey) {
                    interactLabel = str::IT_LOCKED;
                } else if (it->second.lock == lm::LOCK_BASEMENT && !player.hasBasementKey) {
                    interactLabel = str::IT_LOCKED;
                } else {
                    interactLabel = str::IT_OPEN;
                }
                interactKind = 1;
                interactTX = cx; interactTY = cy;
                hasTarget = true;
            }
            return;
        }
        return; // 实心墙：探测终止
    }
}

void Game::interact() {
    if (state != ST_PLAY || !hasTarget) return;
    int cx = interactTX, cy = interactTY;
    int cell = lm.cellAt(player.floor, cx, cy);

    if (cell == lm::CELL_GATE) {
        if (boards > 0) {
            if (!player.hasCrowbar) {
                addMsg(str::MSG_NEED_CROWBAR);
                return;
            }
            boards--;
            if (boards == 1) addMsg(str::MSG_BOARD_1);
            else addMsg(str::MSG_BOARD_0);
            return;
        }
        if (!gateOpen) {
            if (!player.hasCard) {
                addMsg(str::MSG_NEED_CARD);
                return;
            }
            gateOpen = true;
            addMsg(str::MSG_GATE_OPEN);
        }
        return;
    }

    // 门
    auto it = lm.doors.find(LevelMap::key(player.floor, cx, cy));
    if (it == lm.doors.end() || it->second.openness >= 1.f) return;
    lm::DoorState& d = it->second;
    if (d.lock == lm::LOCK_ROOM && !player.hasKey) {
        addMsg(str::MSG_LOCK_ROOM);
        return;
    }
    if (d.lock == lm::LOCK_BASEMENT && !player.hasBasementKey) {
        addMsg(str::MSG_LOCK_B1);
        return;
    }
    d.opening = true;
    if (d.lock == lm::LOCK_ROOM) addMsg(str::MSG_OPEN_ROOM);
    else if (d.lock == lm::LOCK_BASEMENT) addMsg(str::MSG_OPEN_B1);
}

const char* Game::questText() const {
    if (state == ST_WIN) return str::QUEST_DONE;
    if (!player.hasKey) return str::QUEST_0;
    auto it = lm.doors.find(LevelMap::key(lm::F2, 3, 7));
    if (it == lm.doors.end() || it->second.openness < 1.f) return str::QUEST_1;
    if (!player.hasCard) return str::QUEST_2;
    if (!player.hasFlashlight) return str::QUEST_3;
    if (!player.hasBasementKey) return str::QUEST_4;
    if (!player.hasCrowbar) return str::QUEST_5;
    if (boards > 0) return str::QUEST_6;
    if (!gateOpen) return str::QUEST_7;
    return str::QUEST_8;
}

// ---------------------------------------------------------------------------
// 存档
// ---------------------------------------------------------------------------

static void wU32(std::vector<uint8_t>& o, uint32_t v) {
    for (int i = 0; i < 4; i++) o.push_back((uint8_t)(v >> (i * 8)));
}
static void wF32(std::vector<uint8_t>& o, float v) {
    uint32_t u;
    std::memcpy(&u, &v, 4);
    wU32(o, u);
}
static bool rU32(const std::vector<uint8_t>& in, size_t& p, uint32_t& v) {
    if (p + 4 > in.size()) return false;
    v = 0;
    for (int i = 0; i < 4; i++) v |= (uint32_t)in[p++] << (i * 8);
    return true;
}
static bool rF32(const std::vector<uint8_t>& in, size_t& p, float& v) {
    uint32_t u;
    if (!rU32(in, p, u)) return false;
    std::memcpy(&v, &u, 4);
    return true;
}

bool Game::serialize(std::vector<uint8_t>& out) const {
    out.clear();
    out.insert(out.end(), {'M', 'W', 'S', 'V'});
    wU32(out, 2); // 版本
    wF32(out, player.x); wF32(out, player.y); wF32(out, player.angle);
    wU32(out, (uint32_t)player.floor);
    wU32(out, (player.hasKey ? 1 : 0) | (player.hasCard ? 2 : 0)
              | (player.hasFlashlight ? 4 : 0) | (player.flashlightOn ? 8 : 0)
              | (player.hasBasementKey ? 16 : 0) | (player.hasCrowbar ? 32 : 0));
    wF32(out, timePlayed);
    wU32(out, (uint32_t)boards);
    wU32(out, gateOpen ? 1 : 0);
    wF32(out, gateOpenness);
    wU32(out, (uint32_t)sprites.size());
    for (const auto& s : sprites) {
        wU32(out, (uint32_t)s.type);
        wU32(out, s.alive ? 1 : 0);
    }
    wU32(out, (uint32_t)lm.doors.size());
    for (const auto& kv : lm.doors) {
        const lm::DoorState& d = kv.second;
        wU32(out, (uint32_t)((d.floor << 16) | (d.x << 8) | d.y));
        wF32(out, d.openness);
        wU32(out, d.opening ? 1 : 0);
    }
    return true;
}

bool Game::deserialize(const std::vector<uint8_t>& in) {
    if (in.size() < 12 || std::memcmp(in.data(), "MWSV", 4) != 0) return false;
    size_t p = 4;
    uint32_t ver;
    if (!rU32(in, p, ver) || ver != 2) return false;

    Game fresh; // 解析到临时对象，全部成功后再提交（失败安全）
    float x, y, ang, tPlayed, gateOpenness_;
    uint32_t floor_, flags, boards_, gateOpenU, nSpr, nDoor;
    if (!rF32(in, p, x) || !rF32(in, p, y) || !rF32(in, p, ang)) return false;
    if (!rU32(in, p, floor_) || floor_ >= (uint32_t)lm::FLOOR_COUNT) return false;
    if (!rU32(in, p, flags) || flags > 63) return false;
    if (!rF32(in, p, tPlayed) || tPlayed < 0.f || tPlayed > 86400.f) return false;
    if (!rU32(in, p, boards_) || boards_ > 2) return false;
    if (!rU32(in, p, gateOpenU) || gateOpenU > 1) return false;
    if (!rF32(in, p, gateOpenness_) || gateOpenness_ < 0.f || gateOpenness_ > 1.f) return false;
    if (!rU32(in, p, nSpr) || nSpr != fresh.sprites.size()) return false;

    fresh.player.x = x; fresh.player.y = y; fresh.player.angle = ang;
    fresh.player.floor = (int)floor_;
    fresh.player.hasKey = flags & 1;
    fresh.player.hasCard = flags & 2;
    fresh.player.hasFlashlight = flags & 4;
    fresh.player.flashlightOn = flags & 8;
    fresh.player.hasBasementKey = flags & 16;
    fresh.player.hasCrowbar = flags & 32;
    fresh.timePlayed = tPlayed;
    fresh.boards = (int)boards_;
    fresh.gateOpen = gateOpenU == 1;
    fresh.gateOpenness = gateOpenness_;

    for (uint32_t i = 0; i < nSpr; i++) {
        uint32_t ty, alv;
        if (!rU32(in, p, ty) || !rU32(in, p, alv) || alv > 1) return false;
        for (auto& s : fresh.sprites)
            if ((uint32_t)s.type == ty) { s.alive = alv == 1; break; }
    }
    if (!rU32(in, p, nDoor) || nDoor != fresh.lm.doors.size()) return false;
    for (uint32_t i = 0; i < nDoor; i++) {
        uint32_t id, opening;
        float openness;
        if (!rU32(in, p, id) || !rF32(in, p, openness) || !rU32(in, p, opening)) return false;
        if (openness < 0.f || openness > 1.f || opening > 1) return false;
        int fl = (int)(id >> 16) & 0xFF, dx = (int)(id >> 8) & 0xFF, dy = (int)id & 0xFF;
        auto it = fresh.lm.doors.find(LevelMap::key(fl, dx, dy));
        if (it == fresh.lm.doors.end()) return false;
        it->second.openness = openness;
        it->second.opening = opening == 1 && openness < 1.f;
    }
    if (p != in.size()) return false;

    // 提交
    player = fresh.player;
    timePlayed = fresh.timePlayed;
    boards = fresh.boards;
    gateOpen = fresh.gateOpen;
    gateOpenness = fresh.gateOpenness;
    sprites = fresh.sprites;
    for (auto& kv : lm.doors) {
        auto it2 = fresh.lm.doors.find(kv.first);
        if (it2 != fresh.lm.doors.end()) {
            kv.second.openness = it2->second.openness;
            kv.second.opening = it2->second.opening;
        }
    }
    return true;
}
