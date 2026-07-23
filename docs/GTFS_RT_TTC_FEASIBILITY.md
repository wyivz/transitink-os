# TransitInk OS TTC GTFS-Realtime status and feasibility notes

> **Current branch status (2026-07-23):** TransitInk OS is now a TTC-only
> product. The firmware exposes only `Disabled` and `TtcEta` widget types, uses
> the embedded `data/catalog/ttc/` catalog for route/stop selection, downloads
> `https://bustime.ttc.ca/gtfsrt/trips`, and filters GTFS-Realtime Trip Updates
> on-device by configured `route_id` and `stop_id`. The Hong Kong comparison
> below is retained only as historical design context for the TTC conversion.

本文档分析当前固件的领域分层与数据流，并评估改为（或增补）通用
[GTFS-Realtime](https://gtfs.org/realtime/) 以适配
[TTC GTFS-RT](https://open.toronto.ca/dataset/ttc-gtfs-realtime-gtfs-rt/)
的可行性。评估基于仓库现状与 2026-07-23 对公开端点的实测。

> Older sections may still say "add" or "replace" because they record the
> feasibility investigation. For this branch, those decisions have landed as the
> TTC-only implementation described above.

## 0. 白话：要在这块硬件上做「和现在一样的到站屏」，该怎么做

### 你现在香港版在干什么（一句话）

选好「哪条线 + 哪个站」→ 设备每隔约 1 分钟问服务器「这站下一班还有几分钟」→ 墨水屏显示倒计时。

### TTC 和香港的差别（为什么不能直接改个 URL）

| | 香港（现有） | TTC（官方） |
| --- | --- | --- |
| 问法 | 「只要这一站」 | 「给你全市所有车的预测，自己挑站」 |
| 每次下载 | 大约几百字节 | TripUpdates 大约 **130 KB** |
| 格式 | JSON（ArduinoJson 已支持） | **protobuf**（要另加解析库） |
| 站名路线目录 | 已有精简包 | 完整 Surface GTFS 约 **80 MB**，必须自己裁成小包 |

所以：**同等功能能做**；但不是换个 endpoint，而是要加一层「按站过滤」。

### 推荐做法（对这块 ESP32 最省事）

**在云端或家里跑一个很小的代理**，设备仍像现在一样只请求「一站的 JSON」。

```text
墨水屏设备                         你的小代理（Cloudflare Worker / 小 VPS）
─────────                         ────────────────────────────────────
配置：TTC stop_id + route_id       每 30–60s 拉一次
刷新时 GET /eta?stop=8431&route=306   https://bustime.ttc.ca/gtfsrt/trips
                                    按 stop_id（可选 route_id）过滤
←── 返回约 100–200 字节 JSON ─────── 只留下下一两班的 epoch/分钟数
显示倒计时（复用现有 WidgetSnapshot）
```

2026-07-23 实测：从全市 TripUpdates 里筛出一个站，可得到与香港同量级的响应，例如：

```json
{"stop_id":"8431","arrivals":[{"route_id":"305","epoch":1784793703,"minutes":1},{"route_id":"306","epoch":1784794345,"minutes":12}]}
```

约 **146 字节**。设备侧几乎就是再写一个 `TtcClient`，和现有 `KmbClient` 同级。

**为什么推荐代理而不是设备直接啃 130KB：**

- 省电、省 RAM、少引入 nanopb
- 4 个槽位共享一次全市下载（代理做一次即可）
- 固件仍保持「站点级小 JSON」架构，改动最小

### 备选做法（不依赖你自己的服务器）

设备直连 `https://bustime.ttc.ca/gtfsrt/trips`：

1. 每刷新周期**只下载一次**（四个槽共用）
2. 用 nanopb **边下边筛** 自己配置的 `stop_id`
3. 转成现有 `WidgetSnapshot` 倒计时

能做，但固件更重；适合想完全离线自托管、不愿养代理的人。

### 不推荐当主路径的数据源

| 来源 | 原因 |
| --- | --- |
| 旧 NextBus / UmoIQ XML | 官方要求迁走，已不适合新产品 |
| Clever `bustime/api/v3/getpredictions` | 需要 API key，公开 TEST key 无效 |
| `ttc.ca/ttcapi/.../GetNextBuses` | 非公开契约，易变；实测常返回 `[]` |

**结论：官方可靠实时源就是 BusTime GTFS-RT；「更好用」的方式是把它变成站点级小 JSON（代理或机上过滤），而不是另找未公开 API。**

### 设备上还要补的两块（和香港一样）

1. **选站目录**：从 TTC Surface GTFS 裁出 routes/stops/站序小包，塞进固件或 LittleFS（不能塞 80MB 原包）。  
2. **TLS**：信任 `*.ttc.ca` 的 GlobalSign（现固件只钉了香港邮证 CA）。

地铁完整到站不在同一套 surface GTFS-RT 里；首版按 **巴士 + 有轨电车** 做，才能和「香港巴士 ETA」对等。

### 不想跑代理：GitHub + 设备按需，是否可行？

**可行，但要把两件事分开：**

| 数据 | 是什么 | GitHub 行不行 | 设备直连行不行 |
| --- | --- | --- | --- |
| 站点/路线目录 | 站名、`stop_id`、路线列表，用来选站 | **很合适** | 也可，但 80MB 原包不行，要预裁小包 |
| 实时到站 ETA | 「还有几分钟」 | **不合适当主路径** | **合适（推荐无代理方案）** |

#### 为什么实时 ETA 不要指望 GitHub

- GitHub Actions 定时任务通常至少数分钟一级，且有排队；Pages/Release 还有 CDN 缓存。
- 到站预测几十秒就过期；用 GitHub 当「全市过滤后的 ETA 托管」等于又养了一个**又慢又不实时的代理**。
- Actions 分钟级跑一次全市 protobuf，还吃 CI 分钟数与滥用风险。

#### 无代理时推荐的具体架构

```text
【低频 · 选站】
GitHub Actions（你推送或每周）
  下载 TTC Surface GTFS → 裁成 stops/routes 小包
  → Release 或 repo 内 data/catalog/（与现有香港目录同模式）
设备配置门户：按需下载/嵌入该小包，用户选 stop_id

【高频 · 到站】
设备每次刷新（四槽共用一次）：
  HTTPS GET https://bustime.ttc.ca/gtfsrt/trips   (~130KB)
  nanopb 边收边筛：只保留已配置的 stop_id（最多 4 个站）
  → 写成 WidgetSnapshot 倒计时
```

这与「额外跑代理」的差别只在于：**过滤发生在设备上，而不是云上**。没有常驻服务器。

#### 设备端按需抓取：哪些按需、哪些不要按需

| 按需可以 | 不要每次按需 |
| --- | --- |
| 打开门户时加载路线/站牌小包（或从固件 PROGMEM 读） | 不要为每个槽位各下一遍全市 TripUpdates |
| 用户点「更新目录」时从 GitHub Release 拉新站牌包 | 不要从 GitHub 拉实时 ETA |
| 首次选某路线时缓存该路线站序到 LittleFS | 不要下载完整 80MB SurfaceGTFS.zip 到板子 |

实时路径必须：**一周期一下载 + 内存里筛 4 个 stop_id**（实测约 130–140KB、约 6k 条 `stop_time_update`，流式丢弃即可，不必整表进 RAM）。

#### 可行性结论（无代理）

| 方案 | 结论 |
| --- | --- |
| GitHub 托管**精简静态站牌** + 设备**直连 GTFS-RT 筛站** | **可行，且是无代理时的首选** |
| GitHub Actions 生成实时 ETA JSON 给设备轮询 | **不推荐**（时效与配额都不够） |
| 设备按需下载完整 GTFS ZIP 再本地算时刻表 | **不可行**（体积约 80MB，远超 flash） |
| 仅用静态时刻表、不要实时 | 能显示「计划到站」，**不是**现在香港版那种实时 ETA |

硬件要点：ESP32-S3 有 PSRAM、Wi-Fi 拉 130KB 可接受；需加 nanopb、信任 GlobalSign、四槽共享缓存。比香港「每站几百字节」更耗电，但**不必另跑代理**。

## 1. 结论（先读）

| 问题 | 结论 |
| --- | --- |
| 当前是否已有 GTFS / GTFS-RT？ | **没有**。实时层是香港运营商 REST JSON/XML 适配器。 |
| 能否“改用通用 GTFS-RT”直接替换现有香港栈？ | **不适合整仓替换**。显示契约可复用，但配置、目录、TLS、客户端语义都是香港专用。 |
| 能否以附加 Provider 适配 TTC？ | **技术上可行，成本高**。核心产品语义（站点到站倒计时）可用 TripUpdates 推导，但嵌入式约束显著。 |
| 推荐策略 | **无代理：GitHub（或固件内）托管精简站牌目录 + 设备直连 TripUpdates 并按 stop 过滤。** 若可接受小服务，站点 JSON 代理仍是固件改动最小的路径。保留香港栈。 |

**可行性等级：有条件可行（additive adapter），非整仓泛化。无代理时用「GitHub 静态目录 + 设备 GTFS-RT」即可实现同等到站功能。**

---

## 2. 项目逻辑框架

### 2.1 产品定位

TransitInk OS 是面向 **ESP32-S3 + 400×300 电子墨水屏**（Zectrix Note 4）的固件：

- 4 个独立可配置 widget 槽位
- 机上繁体中文配置门户
- 产品语义是 **站点/路线 ETA 倒计时**，不是车辆地图、也不是全系统告警墙

支持的实时类型（`WidgetType`）：

- `BusEta` — 九巴 / 龙运 / 城巴
- `GmbEta` — 绿色小巴（港岛 / 九龙 / 新界）
- `MtrEta` — 港铁重轨 / 轻铁
- `JourneyTime` — 运输署行程时间 XML

### 2.2 分层架构

```text
┌─────────────────────────────────────────────────────────────┐
│ main.cpp — 开机、Wi-Fi、唤醒/睡眠、调度循环、显示、配置门户 │
└───────────────────────────────┬─────────────────────────────┘
                                │
┌───────────────────────────────▼─────────────────────────────┐
│ WidgetScheduler — 按类型刷新间隔拉取到期槽位                 │
│   MTR 30s / Bus·GMB 60s / Journey 120s                     │
└───────────────────────────────┬─────────────────────────────┘
                                │
┌───────────────────────────────▼─────────────────────────────┐
│ WidgetProviderRouter — 按 WidgetType / RailMode 分发        │
└─┬─────────┬─────────┬──────────┬──────────────┬─────────────┘
  │         │         │          │              │
  ▼         ▼         ▼          ▼              ▼
BusProv  GmbProv  MtrProv  LightRailProv  JourneyTimeProv
  │         │         │          │              │
  ▼         ▼         ▼          ▼              ▼
Kmb/CTB  GmbClient MtrClient LrtClient   JourneyTimeClient
Client
  │         │         │          │              │
  └─────────┴─────────┴──────────┴──────────────┘
            TransitJsonParsers / JourneyTimeXmlParser
                                │
                                ▼
              normalize*Snapshot() → WidgetSnapshot
                                │
                                ▼
                         EInkDisplay（4 槽）
```

职责边界（见 `docs/PROJECT_STRUCTURE.md`）：

| 层 | 路径 | 职责 |
| --- | --- | --- |
| 应用组装 | `src/main.cpp` | 生命周期；不写运营商解析规则 |
| 领域核心 | `include/core/`, `src/core/` | 配置、快照、调度、校验；可主机测试 |
| Provider | `include/providers/`, `src/providers/` | 运营商响应 → `WidgetSnapshot` |
| 网络客户端 | `*Client.{h,cpp}` | HTTPS GET + TLS |
| 目录 | `data/catalog/`, `scripts/generate_transit_route_catalog.py` | 静态路线/站牌包；与 live ETA 解耦 |
| 硬件 | `include/hardware/`, `src/hardware/` | 板级 profile、墨水屏驱动 |

### 2.3 配置与身份模型

配置不是「可插拔 feed 注册表」，而是 **封闭枚举 + 运营商专用字段**：

```text
WidgetConfig
├── type: Disabled | BusEta | GmbEta | MtrEta | JourneyTime
├── bus:  operatorId(Kmb|LongWin|Citybus) + routeId/bound/serviceType/stopId
├── gmb:  region + routeCode/routeId/routeSeq/stopId/stopSeq
├── mtr:  mode(HeavyRail|LightRail) + lineOrRouteId/stationId/directionId
└── journeyTime: locationId/destinationId
```

归一化后的 live 记录同样按模式拆分（`BusEtaRecord` / `GmbEtaRecord` /
`RailArrivalRecord` / `JourneyTimeRecord`），最后收敛为显示用
`WidgetSnapshot`（标题、副标题、最多 2 个倒计时值、Fresh/Stale/Error）。

**不存在** `trip_id`、`vehicle_id`、GTFS shape、系统级 Service Alert 的一等模型。

### 2.4 双路径数据策略

```text
静态目录（低频）                         实时 ETA（高频）
─────────────────                       ────────────────
generate_transit_route_catalog.py       每槽位 HTTPS 轮询运营商 API
  → gzip packs + PROGMEM                  → 小体积 JSON/XML
ConfigPortal 离线选路线/站牌              → 解析 → Snapshot
可选 LittleFS 路由覆盖刷新
```

当前香港 ETA 请求是 **站点级、响应极小**（实测 KMB/城巴样本约百字节级）。
目录总量约 800KB gzip（九巴站牌包最大约 436KB），发布上限约 1.25MB
（`MAX_RELEASE_BYTES = 1_310_720`）。

### 2.5 硬编码耦合点（阻碍“通用 feed”）

1. **WidgetType / BusOperator / RailMode** 封闭集合，无 agency/feed ID 抽象  
2. **每运营商 URL 与 JSON schema** 写死在 Client / `TransitJsonParsers`  
3. **TLS 仅信任 Hongkong Post Root CA 3**（`TransitTlsTrust.h`）  
4. **门户文案、校验、字形** 面向繁体中文香港标识  
5. **目录生成脚本** 固定 DATA.GOV.HK / 运营商 URL  
6. **依赖** 仅 ArduinoJson + yxml；无 protobuf / nanopb / gtfs-realtime

---

## 3. TTC 数据现实（2026-07-23 实测）

### 3.1 官方方向

TTC 已要求集成方从旧 UmoIQ/NextBus XML 迁到 **BusTime GTFS-Realtime**。
Open Toronto 上部分数据集页面呈现 Retired，但 BusTime 端点仍在提供 protobuf：

| Feed | URL | Content-Type | 实测体积 |
| --- | --- | --- | --- |
| Trip Updates | `https://bustime.ttc.ca/gtfsrt/trips` | `application/x-google-protobuf` | **~130 KB** |
| Vehicle Positions | `https://bustime.ttc.ca/gtfsrt/vehicles` | 同上 | **~33 KB** |
| Service Alerts | `https://bustime.ttc.ca/gtfsrt/alerts` | 同上 | **~8 KB** |

同源别名亦见 `https://gtfsrt.ttc.ca/...`（返回同等量级数据）。
无需 API key。TLS 证书链为 **GlobalSign**（`*.ttc.ca`），与当前香港 CA 钉扎不兼容。

覆盖面：公开材料与第三方客户端一致指向 **地面交通（巴士 + 有轨电车）**；
地铁实时到站不在同一 surface GTFS-RT 模型里完整覆盖。

### 3.2 与当前产品语义的映射

| TransitInk 需要 | GTFS-RT 实体 | 映射方式 |
| --- | --- | --- |
| 某站某线下一班 ETA | `TripUpdate.stop_time_update[]` 按 `stop_id`（及可选 `route_id`）过滤 | 用 `arrival.time` / `departure.time` 生成倒计时 |
| 目的地/方向文案 | 静态 GTFS `trips.trip_headsign` / `routes.route_short_name` | **必须**有静态目录；RT  alone 不够 |
| 车辆位置地图 | `VehiclePosition` | 当前 UI **不需要** |
| 服务告警 | `Alert` | 当前 snapshot 无告警槽；可扩展副文案，非必须 |

因此：做 TTC 到站 widget，**主路径是 TripUpdates + 缩减静态 GTFS**，不是 VehiclePositions。

### 3.3 静态 GTFS 体量

公开资料中 TTC 完整 Schedule ZIP 常在 **数十 MB** 量级（含大量 `stop_times`）。
本仓库目录预算约 **1.25MB 发布上限**，无法原样嵌入。必须像现有香港流水线一样做
**字段缩减投影**（routes / stops / route-stop 关系 / headsign），并丢弃完整时刻表行，
除非另做按需下载到 LittleFS（约 3.4MB spiffs 分区，仍紧张）。

---

## 4. 可行性评估

### 4.1 架构契合度

| 维度 | 契合度 | 说明 |
| --- | --- | --- |
| `WidgetSnapshot` 显示契约 | 高 | 倒计时 UI 可直接复用 |
| Provider 适配器模式 | 高 | 可新增 `GtfsRtProvider` 挂到 Router |
| 配置模型 | 低 | 需新 `WidgetType` 或泛化 identity（agency、feed、gtfs stop/route） |
| 实时拉取模型 | 低 | 系统级 protobuf feed ≠ 站点级小 JSON |
| 静态目录流水线 | 中 | 概念可复用；输入格式从运营商 JSON 变为 GTFS CSV/ZIP |
| TLS / 区域信任 | 低 | 必须多 CA 或按区域切换 trust store |
| 门户 / 文案 / 字形 | 低–中 | 英语站名可显示，但产品与门户仍是香港繁中导向 |

### 4.2 嵌入式关键风险

1. **带宽与耗电**  
   每轮若为每个槽位各下完整 TripUpdates（~130KB），4 槽最坏约 0.5MB+/分钟级刷新，
   相对香港百字节级 ETA **高两个数量级**。必须 **每刷新周期只拉一次 feed，跨槽共享缓存**。

2. **RAM / 解析**  
   不宜 `String` 整包再解析。需要 **nanopb（或等价）流式解码**，边读边按
   `stop_id`/`route_id` 过滤，只保留每槽最多 2 条到站。ESP32-S3 有 PSRAM，但仍应避免
   把整棵 entity 树物化为 STL 大对象。

3. **Flash / 固件体积**  
   `app0`/`app1` 各约 6.25MB。nanopb + GTFS-RT `.proto` 生成代码可接受，但若同时保留
   全部香港客户端 + 双语门户 + 双目录，需要严格控制 PROGMEM 目录与字形集。

4. **身份对齐**  
   GTFS-RT 的 `trip_id`/`stop_id` 必须与配套 **Surface GTFS** 版本一致。静态目录过期会导致
   选得到站、对不上实时，或 headsign 错误。需要目录 revision 与 feed 对齐策略。

5. **数据源生命周期**  
   Open Toronto 页面标记 Retired，而 `bustime.ttc.ca` 仍在线。集成前应以 TTC/Open Data
   当前条款与端点稳定性为准，并在 `THIRD_PARTY_DATA.md` 记录 Open Government Licence –
   Toronto 署名要求。

6. **产品范围**  
   地面线可做；**地铁 ETA** 若也要一等支持，需另找数据源，不能假设同一 GTFS-RT surface
   feed 已覆盖。

### 4.3 “通用 GTFS-Realtime”是否值得做

**值得做成库内通用层的部分：**

- TripUpdate 流式解码 + 按 stop/route 投影为 `ArrivalRecord{eventEpoch, headsign, …}`
- 可选 Alert 摘要
- feed URL / 轮询间隔 / CA 套件的配置结构

**不值得假装已通用、却实际仍耦合的部分：**

- 把香港 KMB/城巴/GMB/MTR JSON 客户端“改写成 GTFS-RT”（香港官方实时并非 GTFS-RT）
- 用一份全球静态 GTFS 替换现有香港目录（体积与语言都不匹配）
- 取消 `WidgetSnapshot` 去直接渲染 protobuf 实体

正确表述是：**在现有 Provider→Snapshot 框架上增加一条 GTFS-RT 适配路径**，
用于 TTC（及未来同构机构），而不是把整仓“改为通用 GTFS-RT”。

---

## 5. 若实施的建议路径

优先做「同等到站功能」时：

### 路径 1（无常驻代理 · 推荐给你当前约束）— GitHub 目录 + 设备直连 GTFS-RT

1. Actions/脚本从 Surface GTFS 生成精简 `stops`/`routes` 包，进 Release 或 `data/catalog/`（与现有香港目录同模式）。  
2. 固件：nanopb 流式解码 `bustime.ttc.ca/gtfsrt/trips`；**每刷新周期只拉一次**，按最多 4 个已配置 `stop_id` 过滤。  
3. 门户按需加载站牌包；TLS 增加 GlobalSign（或按区域切换 trust）。  
4. 不把实时 ETA 写进 GitHub。

### 路径 2 — 站点 JSON 代理 + 固件薄客户端

适合想少改固件、愿意养 Worker/小服务时：代理过滤，设备只拿百字节 JSON。

### 明确不做（首版）

- VehiclePosition 地图  
- 完整 Service Alerts 时间线  
- 地铁完整 ETA  
- 用 GTFS-RT 替换香港运营商客户端  
- 依赖未公开的 `ttc.ca` 网页 JSON 或需申请的 Clever predictions key  
- 用 GitHub Actions/Pages 托管实时 ETA  
- 设备下载完整 ~80MB SurfaceGTFS.zip  

---

## 6. 工作量与风险定性（技术维度，非日历估时）

| 子系统 | 改动面 | 风险 |
| --- | --- | --- |
| nanopb + `.proto` 构建接入 | 构建系统、`lib_deps`、生成代码 | 中：工具链与 flash |
| 流式 TripUpdate 过滤 + 共享缓存 | 新 core + client | 高：RAM、超时、部分 entity |
| 配置 schema / 门户选择器 | `WidgetConfigCore`、门户 JS、迁移 | 中：schema v3、兼容 |
| 静态 GTFS 缩减目录 | 新 generator、体积预算 | 高：ID 对齐、目录膨胀 |
| 多 CA TLS | `TransitTlsTrust`、所有 Client | 中：证书轮换 |
| 区域/语言产品化 | 文案、字形、README/条款 | 中：范围蔓延 |

相对「再加一个香港运营商 JSON 客户端」，TTC GTFS-RT 适配大约是 **一个数量级更重**
的工程：协议、目录、TLS、拉取模型都变了。

---

## 7. 最终判断

1. **逻辑框架清晰且健康**：Client → Provider → Snapshot → Scheduler → Display 分层正确；
   静态目录与实时 ETA 解耦是正确设计。  
2. **当前框架是“多香港运营商适配器”，不是“通用实时公交协议栈”。**  
3. **对接 TTC 应采用通用 GTFS-RT TripUpdate 适配器作为新 Provider**，并配套缩减静态
   GTFS 与共享 feed 缓存；**不要**试图把现有香港实现改写成 GTFS-RT。  
4. **可行性：有条件可行。** 主要条件是接受系统级 feed 的带宽/解析成本、解决 TLS 与
   静态目录体积，以及把产品范围限定在 TTC 地面线路到站 ETA。  
5. Open Toronto 页面状态与 `bustime.ttc.ca` 在线状态不一致时，实施前应再确认官方
   端点与许可仍有效。

---

## 附录 A — 关键源码锚点

| 主题 | 位置 |
| --- | --- |
| Widget / 运营商枚举 | `include/core/WidgetConfigCore.h` |
| Snapshot / normalize | `include/core/WidgetCore.h`, `src/core/WidgetCore.cpp` |
| Provider 分发 | `include/providers/WidgetProviderRouter.h`, `src/providers/` |
| TLS 钉扎 | `include/TransitTlsTrust.h` |
| 目录体积预算 | `scripts/generate_transit_route_catalog.py` (`MAX_RELEASE_BYTES`) |
| 分区 | `partitions.csv`（app ~6.25MB ×2，spiffs ~3.4MB） |
| 第三方数据条款 | `THIRD_PARTY_DATA.md` |

## 附录 B — 实测命令摘要

```bash
curl -sI https://bustime.ttc.ca/gtfsrt/trips
# Content-Type: application/x-google-protobuf
curl -sL -o /tmp/trips.bin -w "%{size_download}\n" https://bustime.ttc.ca/gtfsrt/trips
# ~132000
```
