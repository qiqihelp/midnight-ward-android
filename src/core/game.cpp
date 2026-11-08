// game.cpp — Game.java 的 C++ 移植。
#include "game.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace notes {
// 字条内容表（与 str::NOTE_x 一一对应；顺序即 noteId）
const NoteDef ALL[Game::NOTE_COUNT] = {
    {str::NOTE_1_TITLE, {str::NOTE_1_L0, str::NOTE_1_L1, str::NOTE_1_L2,
                          str::NOTE_1_L3, str::NOTE_1_L4, nullptr}},
    {str::NOTE_2_TITLE, {str::NOTE_2_L0, str::NOTE_2_L1, str::NOTE_2_L2,
                          str::NOTE_2_L3, str::NOTE_2_L4, nullptr}},
    {str::NOTE_3_TITLE, {str::NOTE_3_L0, str::NOTE_3_L1, str::NOTE_3_L2,
                          str::NOTE_3_L3, str::NOTE_3_L4, nullptr}},
    {str::NOTE_4_TITLE, {str::NOTE_4_L0, str::NOTE_4_L1, str::NOTE_4_L2,
                          str::NOTE_4_L3, nullptr, nullptr}},
    {str::NOTE_5_TITLE, {str::NOTE_5_L0, str::NOTE_5_L1, str::NOTE_5_L2,
                          str::NOTE_5_L3, str::NOTE_5_L4, nullptr}},
    {str::NOTE_6_TITLE, {str::NOTE_6_L0, str::NOTE_6_L1, str::NOTE_6_L2,
                          str::NOTE_6_L3, nullptr, nullptr}},
    {str::NOTE_7_TITLE, {str::NOTE_7_L0, str::NOTE_7_L1, str::NOTE_7_L2,
                          str::NOTE_7_L3, str::NOTE_7_L4, str::NOTE_7_L5}},
    {str::NOTE_8_TITLE, {str::NOTE_8_L0, str::NOTE_8_L1, str::NOTE_8_L2,
                          str::NOTE_8_L3, str::NOTE_8_L4, nullptr}},
};
} // namespace notes

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
        case TYPE_NOTE:
            height = 0.17f; aspect = 0.82f; pickable = true; break;
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
    // 字条布置（坐标落在必经之路上，走向目标即可看到；详见 README 任务链）
    static const float NOTE_DEFS[][3] = {
        {lm::F2, 3.1f, 4.3f},   // 1 病房内：醒来→钥匙的路上
        {lm::F2, 4.0f, 9.4f},   // 2 二楼走廊：出病房去值班室的路上
        {lm::F2, 5.0f, 13.4f},  // 3 值班室内：去房卡的路上
        {lm::F1, 12.0f, 8.6f},  // 4 一楼大厅：大厅中央
        {lm::F1, 13.5f, 4.8f},  // 5 院长办公室：去 B1 钥匙的路上
        {lm::B1, 5.0f, 7.5f},   // 6 地下室入口：下梯后的路上
        {lm::B1, 15.0f, 11.5f}, // 7 撬棍间：去撬棍的路上
        {lm::F1, 14.1f, 14.75f},// 8 大门前：穿大厅去大门的路上
    };
    noteRead.assign(NOTE_COUNT, false);
    readingNote = -1;
    archiveOpen = false;
    for (int i = 0; i < NOTE_COUNT; i++) {
        sprites.emplace_back((int)NOTE_DEFS[i][0], NOTE_DEFS[i][1],
                             NOTE_DEFS[i][2], Sprite::TYPE_NOTE);
        sprites.back().noteId = i;
    }
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
    // 阅读字条 / 档案打开时暂停世界（时间、门、怨灵、事件全部冻结）
    if (readingNote >= 0 || archiveOpen) return;
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
            case Sprite::TYPE_NOTE:
                if (s.noteId >= 0 && s.noteId < NOTE_COUNT) {
                    noteRead[s.noteId] = true;
                    readingNote = s.noteId; // 展开阅读（暂停世界）
                }
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

// 与 questText 同一决策链：返回当前目标的世界落点（小地图标记）
void Game::questTarget(int& floor, float& x, float& y) const {
    if (!player.hasKey) { floor = lm::F2; x = 2.5f; y = 5.5f; return; }
    auto it = lm.doors.find(LevelMap::key(lm::F2, 3, 7));
    if (it == lm.doors.end() || it->second.openness < 1.f) {
        floor = lm::F2; x = 3.5f; y = 7.5f; return;
    }
    if (!player.hasCard)   { floor = lm::F2; x = 5.5f; y = 14.5f; return; }
    if (!player.hasFlashlight) { floor = lm::F1; x = 4.5f; y = 3.5f; return; }
    if (!player.hasBasementKey) { floor = lm::F1; x = 12.5f; y = 3.5f; return; }
    if (!player.hasCrowbar) { floor = lm::B1; x = 16.5f; y = 11.5f; return; }
    floor = lm::F1; x = 11.5f; y = 17.0f; // 大门（撬板 / 刷卡 / 逃出）
}

int Game::notesReadCount() const {
    int n = 0;
    for (int i = 0; i < NOTE_COUNT; i++)
        if (i < (int)noteRead.size() && noteRead[i]) n++;
    return n;
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
    wU32(out, 3); // 版本：3 = 含字条精灵（14 个）；2 = 旧版（6 个，兼容读取）
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
    if (!rU32(in, p, ver) || (ver != 2 && ver != 3)) return false;

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
    if (!rU32(in, p, nSpr)) return false;
    // v3：全部 14 个精灵（6 物品 + 8 字条）；v2 旧档：仅 6 个物品
    size_t expected = (ver == 3) ? fresh.sprites.size() : 6;
    if (nSpr != expected) return false;

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

    // 按顺序索引应用（reset() 的精灵顺序恒定，避免同类型多精灵歧义）
    for (uint32_t i = 0; i < nSpr; i++) {
        uint32_t ty, alv;
        if (!rU32(in, p, ty) || !rU32(in, p, alv) || alv > 1) return false;
        if (i >= fresh.sprites.size()) return false;
        if ((uint32_t)fresh.sprites[i].type != ty) return false; // 顺序即身份
        fresh.sprites[i].alive = alv == 1;
    }
    // 字条已读状态由字条精灵派生（v2 旧档无字条：全部未读）
    for (const auto& s : fresh.sprites)
        if (s.type == Sprite::TYPE_NOTE && s.noteId >= 0
            && s.noteId < Game::NOTE_COUNT)
            fresh.noteRead[s.noteId] = !s.alive;
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
    noteRead = fresh.noteRead;
    readingNote = -1;   // 阅读态不存档
    archiveOpen = false;
    for (auto& kv : lm.doors) {
        auto it2 = fresh.lm.doors.find(kv.first);
        if (it2 != fresh.lm.doors.end()) {
            kv.second.openness = it2->second.openness;
            kv.second.opening = it2->second.opening;
        }
    }
    return true;
}
