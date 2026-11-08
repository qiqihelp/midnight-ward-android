# 🏥 午夜病栋 3D (Midnight Ward 3D)

> 一款**纯 Java + 原生 Android Framework** 的第一人称 3D 恐怖解谜游戏。
> 自研光线投射（Raycasting）引擎，零第三方依赖、零素材文件，推送到 GitHub 即由 **GitHub Actions** 自动编译出可直接安装的 APK。

![Build APK](https://github.com/qiqihelp/midnight-ward-android/actions/workflows/build-apk.yml/badge.svg)

## 📖 剧情与流程

你在 1963 年废弃的青山病院的病床上醒来，什么都不记得，只知道自己必须**逃出去**：

1. 🛏️ **下床**，在病房里找到**钥匙**，打开反锁的房门
2. 🗝️ 探索二楼：在**值班室**找到**房卡**
3. 🪜 从楼梯下到一楼
4. 🔦 在**药房**拿到**手电筒**、在**院长办公室**找到**地下室钥匙**
5. ⬇️ 打开一楼尽头的**铁门**，进入漆黑的**地下室**（记得点亮手电筒——那里只有你的光）
6. 🔧 拿到**撬棍**（小心身后……）
7. 🚪 回到一楼大门：用**撬棍撬下两根木板** → 用**房卡在读卡器上刷卡** → **推门逃出**

地图共三层：二楼病房区、一楼大厅（正门）、地下储物区。怨灵会不时在走廊深处显形；雷雨之夜，闪电会照亮整条走廊——包括她。

## 🕹️ 操作

| 操作 | 方式 |
|---|---|
| 移动 | 左半屏按住拖动（虚拟摇杆：前进/后退/左右平移） |
| 转身 | 右半屏左右滑动 |
| 互动 | 右下大按钮（开门 / 撬开 / 刷卡，靠近目标自动变化） |
| 手电筒 | 右上手电按钮开关（拾取后出现） |
| 拾取 | 走近物品自动拾取 |
| 楼层 | 楼梯门靠近自动打开，走进即换层 |

## 📲 安装 APK

1. 打开本仓库 → **Actions** → 最近一次成功的 **Build APK** 运行
2. 页面底部 **Artifacts** 下载 `MidnightWard-APK`
3. 解压得到 `MidnightWard-v1.1-3D.apk`，传到手机安装（需允许"安装未知来源应用"）

本地编译：`./gradlew assembleDebug`（要求 JDK 17+ / Android SDK 34）

## 🤖 GitHub Actions 流水线

`.github/workflows/build-apk.yml` 每次 push（或手动触发）自动执行：

1. ✅ 检出代码 → 安装 JDK 17
2. ✅ `./gradlew assembleDebug` 编译出 APK
3. ✅ `aapt2 dump badging` 校验 APK 元信息
4. ✅ **无头逻辑测试**：纯 Java 驱动游戏状态机，验证任务链完整可通关
5. ✅ 上传 `MidnightWard-v1.1-3D.apk` 为 Artifact（保留 30 天）

## 📁 项目结构

```
midnight-ward-android/
├── .github/workflows/build-apk.yml     # APK 编译流水线
├── app/src/main/java/com/midnight/ward/
│   ├── MainActivity.java               # 全屏入口
│   ├── GameView.java                   # 帧循环 / 触摸操控 / HUD / 过场
│   ├── Raycaster.java                  # 光线投射 3D 引擎（DDA + 薄门 + billboard + zbuffer）
│   ├── Textures.java                   # 程序化贴图（墙面/门/物品/怨灵，全 Canvas 手绘）
│   ├── Game.java                       # 任务状态机（纯 Java，可无头测试）
│   ├── LevelMap.java                   # 三层地图 / 门锁 / 楼层传送
│   ├── Player.java                     # 玩家与碰撞
│   └── Sprite.java                     # 精灵模型
├── app/src/main/res/                   # 主题 / 图标 / 字符串
└── gradle wrapper + 构建脚本            # Gradle 8.7 + AGP 8.5.2
```

## 🔧 技术说明

- **3D 引擎**：经典 lodev 式 Raycasting——逐列 DDA 网格求交、薄门（滑门）半格平面求交、
  billboard 精灵按深度排序 + zbuffer 逐列裁剪；离屏 300px 宽低分辨率渲染后放大（性能 + 复古质感）
- **光照**：环境光距离衰减 + 手电筒锥形光束（含火苗抖动）+ N/S 侧面调暗；地下室环境光趋近于零
- **贴图**：全部运行时用 Canvas 程序化生成（涂料墙 / 瓷砖墙裙 / 血迹墙 / 木门 / B1 铁门 / 楼梯 /
  带木板和读卡器的大门 / 钥匙 / 房卡 / 手电筒 / 撬棍 / 病床 / 怨灵），APK 内没有任何图片文件
- **游戏逻辑与渲染完全解耦**：`Game.java` 无 Android 依赖，CI 中直接 `javac + java` 无头跑通任务链
- 兼容 Android 5.0（API 21）及以上
