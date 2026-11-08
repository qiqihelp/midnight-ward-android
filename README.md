# 🏥 午夜病栋：手电筒之夜 (Midnight Ward)

> 一款**纯 Java + 原生 Android Framework**（零第三方依赖、零素材文件）的恐怖游戏 APK。
> 推送到 GitHub 即由 **GitHub Actions** 自动编译出可直接安装的 APK。

![Build APK](https://github.com/qiqihelp/midnight-ward-android/actions/workflows/build-apk.yml/badge.svg)

## 📖 玩法

你被困在 1963 年废弃的青山精神病院。整栋楼一片漆黑，只有手中的一支手电筒。

- **按住并拖动屏幕** —— 移动手电筒光圈，照亮病房
- 光照到**符纸**（黄纸红纹）即收下；集齐当夜全部符纸即可撑过一夜
- 光照到**电池**（绿色）可补充手电筒电量
- **「她」会循着光而来**——只在光圈内、闪电的瞬间、或余光一瞥时可见
- 电池会持续消耗：电量越低光圈越小，她移动越快；电量耗尽后果自负
- 被她碰到光心即被抓住……
- **连撑三夜**，等到黎明 → 真结局

## 🎮 结局

| 结局 | 条件 |
|---|---|
| ★ 真结局 · 超度 | 连续撑过三夜 |
| ☠ 被抓 · 长眠 | 被她碰到光心 |

每夜符纸数量 3 / 4 / 5 递增，光圈半径递减，她的速度递增。

## 📲 安装 APK

**方式一（推荐）：下载 GitHub Actions 编译产物**

1. 打开本仓库 → **Actions** → 最近一次成功的 **Build APK** 运行
2. 页面底部 **Artifacts** 下载 `MidnightWard-APK`
3. 解压得到 `MidnightWard-v1.0.apk`，传到手机安装（需允许"安装未知来源应用"）

**方式二：本地编译**

```bash
./gradlew assembleDebug
# 产物：app/build/outputs/apk/debug/app-debug.apk
adb install app/build/outputs/apk/debug/app-debug.apk
```

要求：JDK 17+、Android SDK（compileSdk 34）。首次构建 Gradle 会自动下载依赖。

## 🤖 GitHub Actions 自动编译

`.github/workflows/build-apk.yml` 会在每次 push 到 main（或手动触发）时自动：

1. ✅ 检出代码 → 安装 JDK 17（GitHub 托管 runner 自带 Android SDK）
2. ✅ `./gradlew assembleDebug` 编译出 APK
3. ✅ 用 `aapt2 dump badging` 校验 APK 元信息
4. ✅ 上传 `MidnightWard-v1.0.apk` 为 Artifact（保留 30 天）

## 📁 项目结构

```
midnight-ward-android/
├── .github/workflows/build-apk.yml          # APK 编译流水线
├── app/
│   ├── build.gradle                          # 应用模块（compileSdk 34 / minSdk 21）
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/com/midnight/ward/
│       │   ├── MainActivity.java             # 全屏入口
│       │   └── GameView.java                 # 游戏主体（Canvas 渲染 + 状态机）
│       └── res/
│           ├── drawable/ic_launcher.xml      # 矢量图标（残月 + 红十字）
│           └── values/（strings / styles）
├── gradle/wrapper/  +  gradlew               # Gradle 8.7 wrapper
├── build.gradle / settings.gradle            # AGP 8.5.2
└── README.md
```

## 🔧 技术说明

- **零依赖**：不引入 AndroidX / 第三方库，直接继承 `android.view.View` 用 Canvas 手绘全部画面（病房、门牌、月光、床、输液架、血迹、怨灵、惊吓脸……）
- 声音使用系统 `ToneGenerator`，震动使用 `Vibrator`，无任何素材文件
- 游戏循环：`Handler.postDelayed` 16ms 定时刷新，状态机驱动 6 个场景
- 兼容 Android 5.0（API 21）及以上
