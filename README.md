# 午夜病栋 · 纯 C++ 原生版（Midnight Ward — Pure C++ Native）

第一人称 3D 恐怖逃脱游戏，**全部逻辑与渲染均为 C++**（NativeActivity + 软件光投射渲染器），
APK 内 **不含一行 Java、不含任何 DEX**（`android:hasCode="false"`）。

> 任务链：病床上醒来 → 找钥匙开病房门 → 二楼值班室拿房卡 → 一楼药房拿手电筒 →
> 院长办公室拿地下室钥匙 → 铁门下地下室（开手电筒）→ 深处取撬棍 →
> 回大门撬掉两根木板 → 刷房卡 → 逃出生天。
> 沿途 **8 张字条**指引方向（也可在左下「档案」里随时回看）。

## v2.1 更新（相对 v2.0）

| 改进 | 说明 |
| --- | --- |
| 真·3D 观感 | 地板/天花板**透视贴图**（不再是纯色平面）、墙面**双线性采样**、
              内部分辨率 300→**720px + 双线性放大**（摆脱复古大颗粒像素）、
              距离雾 + 手电筒锥形光 + **加性暖光**、屏幕暗角、Bayer 抖动去色带 |
| 俯仰视角 | 右半屏上下滑动即可**抬头/低头**（pitch），环顾更真实 |
| 字条提示系统 | 8 张故事字条布置在必经之路上，捡到即展开阅读（暂停世界）；
              左下「档案」按钮随时回看，不再毫无头绪 |
| 小地图 | 右上角实时小地图：墙体/门/楼梯/大门分色 + **金色目标点** + 楼层名 |
| 单架构瘦身 | 仅保留 **arm64-v8a**（现代安卓标准架构），APK 体积约 **减半**；
              运行内存占用不变 |

## 修复相对旧版（v1.x Java 版）的问题

| 问题 | 旧版 | 本版 |
| --- | --- | --- |
| 启动闪退 | Java UI 层设备敏感 | 零 JVM 参与，核心经 ASan/UBSan 全量模糊测试 |
| 无法横屏 | Manifest 锁死 `portrait` | **强制 `landscape`** + 全屏沉浸 |
| 图标兼容性 | vector XML（老机型启动器易异常） | 全密度 PNG 图标 |
| 进度丢失 | 无存档 | 自动存档 + 「继续游戏」（v2 旧档兼容读取） |

## 技术架构

```
src/
├── core/            平台无关核心（桌面与 Android 共用同一份代码）
│   ├── fb.cpp         RGBA 帧缓冲 + 2D 基元（双线性放大/圆角/多边形/线条）
│   ├── font.cpp       SDF 有向距离场字体（单图集任意字号清晰渲染中文）
│   ├── levelmap.cpp   三层地图 / 门锁 / 楼层传送（Java 版 1:1 移植）
│   ├── player.cpp     移动 + 碰撞 + 俯仰视角
│   ├── game.cpp       任务链状态机 / 字条系统 / 互动 / 氛围事件 / 存档 v3
│   ├── textures.cpp   程序化贴图（墙面×9 + 地板/天花板×4 + 精灵×7，零素材）
│   ├── raycaster.cpp  DDA 光投射 + 地板/天花板透视投射 + 薄门滑开
│   │                  + billboard 精灵 + zbuffer + 手电锥形光（加性暖光）
│   │                  + 距离雾 + Bayer 抖动 + 暗角 + 俯仰地平线
│   ├── hud.cpp        HUD / 小地图 / 档案 / 字条阅读 / 摇杆与环顾触摸
│   └── app.cpp        应用控制器（编排 + 自动存档）
├── android/
│   └── native_main.cpp  NativeActivity 入口（事件循环/触摸转发/上屏）
└── desktop/
    └── desktop_main.cpp 桌面模拟器 + 测试总成（与 Android 共用 core）
```

- 字体：`scripts/gen_font_sdf.py` 扫描 `src/core/strings.h` 生成 SDF 图集 `assets/font_sdf.bin`
- 图标：`scripts/gen_icons.py` 生成全密度 PNG

## 测试（防闪退的三道关）

```bash
bash scripts/build_desktop.sh asan          # ASan + UBSan 构建
./build-desktop/mw --logic                  # 完整任务链通关（含 8 张字条断言 + 档案流程）
./build-desktop/mw --saveload               # 存档往返 / v2 旧档兼容 / 损坏存档防护
./build-desktop/mw --fuzz --min 2 --seed 7  # 随机触摸 + 传送 + 叠层模糊测试
./build-desktop/mw --sim --dir /tmp/sim     # 带渲染通关 + 里程碑截图
```

## 构建 APK

```bash
# 本地（需 NDK / build-tools / platform-34）
export NDK=... BT=... PLAT=... JAVA_HOME=...
bash scripts/build_android.sh   # 仅 arm64-v8a（现代安卓标准架构）
bash scripts/package_apk.sh     # aapt2 + zipalign + apksigner（不经 Gradle）
```

GitHub Actions 推送即构建：`.github/workflows/build-apk.yml`
（桌面回归测试 → NDK 编译 → 打包 → badging/横屏/单架构/无 DEX 断言 → 上传产物）。

## 安装

1. **卸载旧版**（v1.x 与本版调试签名不同，需先卸载；v2.0 → v2.1 同签名可直接覆盖安装）
2. 下载 Actions 产物 `midnight-ward-cpp-apk`，安装 `MidnightWard-v2.1-cpp.apk`
3. 需要 Android 5.0+ **arm64 手机**（2016 年后主流机型均满足）；无任何权限申请
4. 若极旧 32 位机型无法安装，说明设备不支持 arm64（可改回 armeabi-v7a 重新构建）

## 操作

- 左半屏拖动 = 移动（虚拟摇杆）
- 右半屏滑动 = 转身 + **抬头/低头**
- 右下大按钮 = 开门 / 撬板 / 刷卡；其上方 = 手电筒开关（地下室必备）
- 左下按钮 = **档案**（回看已收集的字条）
- 走近地上的白纸 = 自动捡起并展开阅读（点击任意处收起）
- 返回键：先关字条/档案，再防误退；主界面按返回正常退出

## 目录说明

- `legacy-java3d/`：旧版 Java 工程存档（仅作参照，不参与构建）
