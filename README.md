# CheatiFrame

> 基于 `ImGui + D3D11` 的外部可视化/辅助框架项目，面向 Windows 平台。

线程和资源优化比较烂，主要是写实现功能作学习研究。


## 免责声明

- 本仓库代码仅用于学习 C++ 多线程、内存读写、图形叠加层与工程化组织。
- 请遵守你所在地区法律法规、软件许可协议与游戏平台规则。

---

## 项目简介

`CheatiFrame` 是一个模块化的 C++ 工程，核心由以下部分组成：

- 叠加层窗口与渲染（`Visuals/External.*`）
- 菜单与配置输入（`Visuals/Menu.*`）
- 业务主逻辑（`Cheats/Cheats.*`）
- 偏移加载（`Cheats/OffsetsLoader.*` + `Offsets/*.json`）
- 可视性检测运行时（`Math/VisCheckCS2/*` + `maps/*.opt`）
- 鼠标输入后端抽象（WinAPI / KMBox）

入口在 `main.cpp`，通过窗口类名与窗口标题附着目标窗口，并将主循环回调交给 `Cheats::CheatMain`。

---

## 功能总览

### 1) 视觉模块（ESP）

- 2D/3D 框体绘制
- 骨骼与可视骨骼点绘制
- 血量、距离、准心、FOV 圆
- C4 信息显示（站点、是否拆包）

### 2) 瞄准模块

- 自瞄 / 扳机 / 后座补偿
- 可配置热键
- 曲线控制（速度、平滑、X/Y 比例）
- 可选智能部位选择

### 3) 投掷物辅助（Grenade Helper）

- 按地图加载点位（`GrenadeData/*.json`）
- 站位点与瞄点引导
- 点位录制与列表管理
- 站位聚类后的文字列表显示
- 前后缓冲渲染数据发布（渲染线程仅消费）

### 4) 可视性解析（VPK/OPT）

- 通过 `maps/*.opt` 做点位可视性检测
- 异步地图加载与状态展示（Loaded/Loading/Not Found）

### 5) 输入后端

- WinAPI
- KMBox Net（IP:Port + UUID）
- KMBox B+ Pro（串口 COM）（未测试）
- 采用消费者线程队列异步发送，避免主逻辑阻塞

### 6) 配置系统

- `Configs/*.json` 保存/加载
- 启动自动加载上次配置（`Configs/last_loaded.txt`）

---

## 架构与线程模型

项目采用“采集-计算-绘制”分层设计，核心线程包括：

- `ReadWorker`：读取原始状态（实体、矩阵、本地玩家、武器、骨骼等）
- `EspWorker`：世界坐标到屏幕数据转换，构建可绘制数据
- `AimWorker`：目标选择、平滑计算、输入命令下发
- UI/Render 线程：只负责 `ImGui` 与 D3D11 绘制

共享数据通过 `SharedState` 管理，使用 `shared_mutex`/`atomic` 协调。

### 当前性能优化策略（已落地）

- `ReadProcessMemory` 结构体读取与批量读取（`ReadInto` / `ReadBuffer`）
- 页缓存读取（`ReadCached`，4KB 页）
- 骨骼块读（减少高频小 RPM）
- 地图名低频缓存
- GrenadeHelper 可绘制数据前后缓冲发布（`grenadeBack -> grenadeFront`）
- 渲染线程不做重逻辑计算

---

## 目录结构

```text
CheatiFrame/
├─ main.cpp                      # 程序入口
├─ Cheats/
│  ├─ Cheats.cpp/.h              # 核心逻辑与线程调度
│  ├─ GameState.h                # 线程共享状态定义
│  ├─ MouseController.cpp/.h     # 输入后端抽象与异步命令队列
│  ├─ OffsetsLoader.cpp/.h       # 偏移JSON加载
│  ├─ OffsetsData.h              # 偏移字段结构
│  └─ AimCurves.cpp/.h           # 瞄准曲线计算
├─ Visuals/
│  ├─ External.cpp/.h            # Overlay窗口、D3D11、ImGui、消息循环
│  └─ Menu.cpp/.h                # UI菜单与用户配置输入
├─ Math/
│  ├─ Vector.h                   # 向量与数学工具
│  └─ VisCheckCS2/               # 地图可视检测解析与运行时
├─ KmBox/
│  ├─ kmboxNet.cpp/.h            # KMBox Net 对接
│  └─ KmboxB*.h / my_enc.cpp     # KMBox B+ Pro 相关
├─ Offsets/
│  ├─ offsets.json
│  ├─ buttons.json
│  └─ client_dll.json
├─ GrenadeData/                  # 地图点位JSON
├─ maps/                         # 对应地图 .opt 可视文件
├─ ImGui/                        # Dear ImGui 源码
└─ thirdparty/rapidjson/include/ # RapidJSON
```

---

## 核心数据流

1. `main.cpp` 调用 `Visual::external.AttachWindow(...)`。
2. `External::MessageLoop()` 每帧驱动 `Cheats::CheatMain()`。
3. `CheatMain()` 处理热键与菜单，再进入 `Game::CheatTick()`。
4. `CheatTick()` 负责：
   - 初始化（进程/模块/偏移/线程）
   - 配置快照更新
   - 地图状态刷新与点位管理请求处理
   - 渲染层消费 `esp/aim/grenade` 前台数据
5. 工作线程持续生产数据并发布到共享状态。

---

## 构建说明

### 环境要求

- Windows 10/11
- Visual Studio 2022（MSVC v143）
- Windows SDK（工程中目标为 `10.0`）
- C++17

### 依赖

- D3D11（系统库）
- WinSock2（系统库）
- ImGui（仓库内置）
- RapidJSON（`thirdparty/rapidjson/include`）

### 构建步骤

1. 用 VS2022 打开 `CheatiFrame.sln`
2. 选择 `Debug|x64` 或 `Release|x64`
3. 直接生成 `CheatiFrame` 项目

---

## 运行说明

1. 启动目标应用窗口（默认查找类名 `SDL_app`、标题 `Counter-Strike 2`）
2. 运行本程序
3. 默认热键：
   - `Insert`：显示/隐藏菜单
   - `End`：退出程序

字体默认从程序当前目录读取 `font.otf`；若缺失，会回退到 ImGui 默认字体。

---

## 配置与数据文件

### Offsets

- 位置：`Offsets/`
- 文件：`offsets.json`、`buttons.json`、`client_dll.json`
- 用途：运行时读取偏移并填充 `OffsetsData`

### 投掷物点位

- 位置：`GrenadeData/*.json`
- 用途：按地图加载站位/瞄点数据

### 可视性地图

- 位置：`maps/*.opt`
- 用途：供 `VisCheckRuntime` 做可视检测

### 用户配置

- 位置：`Configs/*.json`
- 最近配置：`Configs/last_loaded.txt`

---

## 常见问题

### 1) 叠加层不显示

- 检查目标窗口类名/标题是否匹配
- 检查是否创建了 D3D11 设备（日志输出）

### 2) Map Status 为 Not Found

- 确认 `maps/` 下存在对应地图 `.opt` 文件
- 确认地图名规范（不含 `maps/` 前缀与 `.bsp` 后缀）

### 3) 编译出现 UTF-8/编码告警

- 工程已开启 `/utf-8`
- 若第三方文件存在异常字符，请保存为 UTF-8（无 BOM 亦可）

### 4) 偏移加载失败

- 检查 `Offsets/` 三个 JSON 是否存在且字段完整

---

## 开发建议

- 将新逻辑优先放入工作线程，渲染线程保持“纯绘制”
- 高频路径避免动态分配与多次小块 RPM
- 统一通过 `SettingsSnapshot` 传播菜单配置，减少跨线程直接访问

## 预览
![menu1](https://github.com/TalkItNextTime/CheatiFrame/blob/CS2/pics/menu1.jpg)
![menu2](https://github.com/TalkItNextTime/CheatiFrame/blob/CS2/pics/menu2.jpg)
![menu3](https://github.com/TalkItNextTime/CheatiFrame/blob/CS2/pics/menu3.jpg)
![menu4](https://github.com/TalkItNextTime/CheatiFrame/blob/CS2/pics/menu4.jpg)
![throwhelper](https://github.com/TalkItNextTime/CheatiFrame/blob/CS2/pics/throwhelper.jpg)