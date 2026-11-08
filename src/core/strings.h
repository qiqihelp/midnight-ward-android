// strings.h — 《午夜病栋》全部游戏文案（唯一来源）。
// scripts/gen_font_sdf.py 会扫描本文件里的所有字符串字面量生成 SDF 字体图集，
// 因此新增文案必须写在这里（或本文件已覆盖到的字符集内）。
#pragma once

namespace str {

// ---------------- 标题 / 过场 ----------------
constexpr const char* GAME_TITLE    = "午 夜 病 栋";
constexpr const char* GAME_SUBTITLE = "MIDNIGHT WARD · 3D";
constexpr const char* VERSION_TAG   = "v2.1 · 纯 C++ 引擎";
constexpr const char* TOUCH_CONTINUE = "—— 触摸继续 ——";

constexpr const char* INTRO_0_0 = "……消毒水的气味。";
constexpr const char* INTRO_0_1 = "你在青山病院的病床上醒来。";
constexpr const char* INTRO_0_2 = "头很沉——你想不起自己为什么在这里。";
constexpr const char* INTRO_1_0 = "房门锁着。";
constexpr const char* INTRO_1_1 = "走廊尽头，有什么东西";
constexpr const char* INTRO_1_2 = "在轻轻地、轻轻地拖行。";
constexpr const char* INTRO_2_0 = "逃出去。";
constexpr const char* INTRO_2_1 = "（左手拖动 移动 · 右手滑动 环顾四周）";
constexpr const char* INTRO_2_2 = "（左下：档案 · 右下：互动 / 手电筒）";

// ---------------- 继续 / 重新开始 ----------------
constexpr const char* CHOOSE_HINT  = "检测到上次未完成的逃脱。";
constexpr const char* CHOOSE_RESUME = "继续游戏";
constexpr const char* CHOOSE_RESTART = "重新开始";
constexpr const char* MSG_RESUMED = "——进度已恢复——";

// ---------------- 楼层名（小地图下方 / 方位感） ----------------
constexpr const char* FLOOR_NAME_F2 = "二楼 · 住院区";
constexpr const char* FLOOR_NAME_F1 = "一楼 · 门诊层";
constexpr const char* FLOOR_NAME_B1 = "地下层 · 停用";

// ---------------- 动态目标 ----------------
constexpr const char* QUEST_DONE  = "你逃出来了。";
constexpr const char* QUEST_0 = "目标：在病房里找到钥匙";
constexpr const char* QUEST_1 = "目标：用钥匙打开病房门";
constexpr const char* QUEST_2 = "目标：去二楼值班室找房卡";
constexpr const char* QUEST_3 = "目标：一楼药房找手电筒";
constexpr const char* QUEST_4 = "目标：一楼院长办公室找地下室钥匙";
constexpr const char* QUEST_5 = "目标：下地下室取撬棍（记得开手电筒）";
constexpr const char* QUEST_6 = "目标：回一楼大厅，撬开大门的木板";
constexpr const char* QUEST_7 = "目标：用房卡在读卡器上刷卡";
constexpr const char* QUEST_8 = "目标：逃出去！！";

// ---------------- 拾取 / 互动消息 ----------------
constexpr const char* MSG_KEY     = "拿到了一把锈迹斑斑的钥匙。";
constexpr const char* MSG_CARD    = "一张房卡，卡面印着『青山病院 · 001』。";
constexpr const char* MSG_TORCH   = "一支军用手电筒——「咔哒」，光柱刺破黑暗。";
constexpr const char* MSG_BKEY    = "一把沉重的黄铜钥匙，标签上刻着：B1。";
constexpr const char* MSG_CROWBAR = "撬棍到手——身后忽然传来指甲刮擦玻璃的声音……快走。";
constexpr const char* MSG_NEED_CROWBAR = "大门被两根木板钉死了——需要一根撬棍。";
constexpr const char* MSG_BOARD_1 = "「嘎吱——」撬下一根木板，还剩一根。";
constexpr const char* MSG_BOARD_0 = "两根木板都撬掉了。门禁读卡器露了出来，红灯闪烁。";
constexpr const char* MSG_NEED_CARD = "读卡器红灯一闪——你需要一张房卡。";
constexpr const char* MSG_GATE_OPEN = "「嘀——」绿灯亮起，门锁弹开了！快走！";
constexpr const char* MSG_LOCK_ROOM = "门锁着。钥匙应该就在这间病房里。";
constexpr const char* MSG_LOCK_B1  = "沉重的铁门纹丝不动——锁孔是黄铜的，需要对应的钥匙。";
constexpr const char* MSG_OPEN_ROOM = "钥匙转动，「咔哒」——病房门开了。";
constexpr const char* MSG_OPEN_B1  = "黄铜钥匙转动，铁门吱呀开启。下面一片漆黑。";

// ---------------- HUD ----------------
constexpr const char* LABEL_TORCH  = "手电";
constexpr const char* LABEL_DOT    = "·";
constexpr const char* WARN_DARK    = "太黑了——打开手电筒！";
constexpr const char* IT_PRY       = "撬开";
constexpr const char* IT_BOARDS    = "木板";
constexpr const char* IT_SWIPE     = "刷卡";
constexpr const char* IT_READER    = "读卡器";
constexpr const char* IT_LOCKED    = "上锁";
constexpr const char* IT_OPEN      = "开门";

// ---------------- 字条系统 ----------------
constexpr const char* ARCHIVE_BTN   = "档案";
constexpr const char* ARCHIVE_TITLE = "已收集的字条";
constexpr const char* ARCHIVE_COUNT = "已收集 %d / 8";
constexpr const char* NOTE_CLOSE_HINT = "—— 点击任意处继续 ——";

// 字条 1：病房（起点 → 钥匙提示）
constexpr const char* NOTE_1_TITLE = "揉皱的便签";
constexpr const char* NOTE_1_L0 = "302 的病人：";
constexpr const char* NOTE_1_L1 = "夜里走廊不安全，别乱跑。";
constexpr const char* NOTE_1_L2 = "房门钥匙落在房里——";
constexpr const char* NOTE_1_L3 = "好像在床边的柜子附近。";
constexpr const char* NOTE_1_L4 = "—— 值班护士 留";

// 字条 2：二楼走廊（→ 值班室 / 办公室方位）
constexpr const char* NOTE_2_TITLE = "值班交接单";
constexpr const char* NOTE_2_L0 = "各病房备用房卡，统一存放在";
constexpr const char* NOTE_2_L1 = "二楼值班室（走廊南侧）。";
constexpr const char* NOTE_2_L2 = "其余钥匙由一楼院长办公室";
constexpr const char* NOTE_2_L3 = "统一保管，闲人勿动。";
constexpr const char* NOTE_2_L4 = "—— 10 月 14 日 夜班";

// 字条 3：值班室（→ 药房手电筒 / 地下层无灯）
constexpr const char* NOTE_3_TITLE = "值班室的留言";
constexpr const char* NOTE_3_L0 = "夜巡的同事：";
constexpr const char* NOTE_3_L1 = "手电筒在一楼药房的架子上，";
constexpr const char* NOTE_3_L2 = "记得取——地下层的照明";
constexpr const char* NOTE_3_L3 = "总闸坏了，一直没有修。";
constexpr const char* NOTE_3_L4 = "—— 老陈";

// 字条 4：一楼大厅（→ 一楼布局指引）
constexpr const char* NOTE_4_TITLE = "楼层指引";
constexpr const char* NOTE_4_L0 = "一楼：药房（西北侧）、";
constexpr const char* NOTE_4_L1 = "院长办公室（正北侧）。";
constexpr const char* NOTE_4_L2 = "东走廊尽头通往地下层，";
constexpr const char* NOTE_4_L3 = "南侧是住院部大门。";

// 字条 5：院长办公室（→ 地下室钥匙 + 剧情）
constexpr const char* NOTE_5_TITLE = "院长的日记";
constexpr const char* NOTE_5_L0 = "她还在下面。";
constexpr const char* NOTE_5_L1 = "夜里总听见滴水声，";
constexpr const char* NOTE_5_L2 = "和指甲刮擦铁门的声音……";
constexpr const char* NOTE_5_L3 = "地下层的钥匙我收在办公室";
constexpr const char* NOTE_5_L4 = "抽屉里，绝不能再让人下去。";

// 字条 6：地下室入口（→ 撬棍在东侧 / 需光源）
constexpr const char* NOTE_6_TITLE = "管线检修记录";
constexpr const char* NOTE_6_L0 = "地下层东侧的储藏间里";
constexpr const char* NOTE_6_L1 = "存放着撬棍等检修工具。";
constexpr const char* NOTE_6_L2 = "照明总闸位于地面一层，";
constexpr const char* NOTE_6_L3 = "地下层作业请自备光源。";

// 字条 7：撬棍间（剧情残页）
constexpr const char* NOTE_7_TITLE = "撕碎的病历（残页）";
constexpr const char* NOTE_7_L0 = "患者：林晚秋，女，19 岁。";
constexpr const char* NOTE_7_L1 = "深夜被发现于地下层走廊，";
constexpr const char* NOTE_7_L2 = "已无生命体征。手指磨损";
constexpr const char* NOTE_7_L3 = "严重，像是徒手抓挠过";
constexpr const char* NOTE_7_L4 = "什么东西……";
constexpr const char* NOTE_7_L5 = "（后半页被撕去了）";

// 字条 8：大门旁（→ 终局流程）
constexpr const char* NOTE_8_TITLE = "贴在大门上的封条";
constexpr const char* NOTE_8_L0 = "住院部大门已按规定封闭。";
constexpr const char* NOTE_8_L1 = "拆除封板需使用撬棍；";
constexpr const char* NOTE_8_L2 = "门禁断电后，请用房卡";
constexpr const char* NOTE_8_L3 = "在读卡器上刷卡放行。";
constexpr const char* NOTE_8_L4 = "—— 院务科";

// ---------------- 胜利 ----------------
constexpr const char* WIN_TITLE  = "你 推 开 了 大 门";
constexpr const char* WIN_0 = "冷风灌进领口。";
constexpr const char* WIN_1 = "身后的病栋在晨雾里沉默着——你逃出来了。";
constexpr const char* WIN_TIME_PREFIX = "逃脱用时  ";
constexpr const char* WIN_TIME_MIN = " 分 ";
constexpr const char* WIN_TIME_SEC = " 秒";
constexpr const char* WIN_AGAIN = "—— 触摸屏幕 · 再来一次 ——";

} // namespace str
