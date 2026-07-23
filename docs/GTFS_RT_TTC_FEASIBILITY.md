# TransitInk OS 逻辑框架与 TTC GTFS-Realtime 可行性评估

本文档分析当前固件的领域分层与数据流，并评估改为（或增补）通用
[GTFS-Realtime](https://gtfs.org/realtime/) 以适配
[TTC GTFS-RT](https://open.toronto.ca/dataset/ttc-gtfs-realtime-gtfs-rt/)
的可行性。评估基于仓库现状与 2026-07-23 对公开端点的实测。

## 1. 结论（先读）

| 问题 | 结论 |
| --- | --- |
| 当前是否已有 GTFS / GTFS-RT？ | **没有**。实时层是香港运营商 REST JSON/XML 适配器。 |
| 能否“改用通用 GTFS-RT”直接替换现有香港栈？ | **不适合整仓替换**。显示契约可复用，但配置、目录、TLS、客户端语义都是香港专用。 |
| 能否以附加 Provider 适配 TTC？ | **技术上可行，成本高**。核心产品语义（站点到站倒计时）可用 TripUpdates 推导，但嵌入式约束显著。 |
| 推荐策略 | **保留香港栈**；若要做 TTC，以「区域配置 / 新 Widget 类型 + 通用 TripUpdate→Snapshot 适配器」增量引入，并强制共享 feed 缓存与静态 GTFS 缩减目录。 |

**可行性等级：有条件可行（additive adapter），非整仓泛化。**

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

按侵入性递增：

### 阶段 A — 验证原型（主机侧）

1. 用主机测试解码 `bustime.ttc.ca/gtfsrt/trips`，按给定 `stop_id` 抽出下一两班。  
2. 确认字段完备度（是否总有 `arrival.time`、`stop_id`、`route_id`、NEW trip 比例）。  
3. 不改固件行为，只加 `test_host` fixture 与解析模块。

### 阶段 B — 固件增量 Provider

1. 新增 `WidgetType::GtfsEta`（或 `BusEta` 下 `BusOperator::Gtfs` / 区域 profile）。  
2. `GtfsRtClient`：流式 HTTPS + nanopb；全局 `TripUpdateCache`（TTL ≈ 刷新间隔）。  
3. `normalizeGtfsSnapshot()` → 复用现有两行倒计时 UI。  
4. TLS：按区域选择 Hongkong Post vs GlobalSign（或系统信任包，需评估 flash）。  

### 阶段 C — TTC 目录与门户

1. 新生成脚本：从 TTC Surface GTFS 投影 `index` + `stops-ttc`（仅 surface）。  
2. 门户：英语标签路径或最小英/中切换；避免把 TTC 站名硬塞进现有繁中文案假设。  
3. 更新 `THIRD_PARTY_DATA.md` 署名与刷新策略。

### 明确不做（首版）

- VehiclePosition 地图  
- 完整 Service Alerts 时间线  
- 地铁完整 ETA  
- 用 GTFS-RT 替换香港运营商客户端  

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
