# HammerMiner OS Web Frontend

这是一个基于 Vue 3 + TypeScript + Vite 构建的矿机嵌入式 Web 管理界面（HammerMiner OS）。项目采用了轻量化架构设计，针对 ESP32 等嵌入式设备的网络栈进行了深度优化，支持实时数据监控、日志管理及系统配置等功能。

## 🛠️ 开发与调试 (Development)

### 环境准备 (Prerequisites)
* **Node.js**
: 建议版本 v18+
* **包管理器**
: npm 或 yarn/pnpm

### 1. 安装依赖 (Install Dependencies)
在项目根目录 (
`http_server/axe-os/`
) 下执行：
```bash
npm install
2. 启动本地调试 (Dev Server)
启动本地开发服务器，支持热重载 (HMR)：
Bash
Copy code
npm run dev
• 默认访问地址: http://localhost:5173
• 注意: 由于项目包含与后端硬件交互的 API，在没有硬件环境的情况下，您可能需要配置 Vite 代理或使用 Mock 数据来调试 API 请求。
3. 生产环境构建 (Build)
构建用于生产环境的静态文件：
Bash
Copy code
npm run build
• 构建产物将生成在 dist/ 目录中。
• 构建过程包含 TypeScript 类型检查 (vue-tsc) 和版本文件生成 (generate-version.cjs)。
• 针对嵌入式环境优化：默认启用了 terser 压缩（移除 console/debugger）及 CSS 注入（消除首屏阻塞）。
4. 本地预览构建产物 (Preview)
在本地预览打包后的效果：
Bash
Copy code
npm run preview
￼
📅 更新日志 (Changelog)

## 🚀 近期更新亮点 (Quick Highlights)
> [!TIP]
> **更加稳健且智能的监控与升级体验**：
> - 🧠 **动态固件识别**：废弃了静态编译宏，改为根据设备当前版本自动切换算法。版本 >= 2.0 自动启用加密校验，旧版则保持完美兼容。
> - 🛡️ **安全拦截增强**：针对加密固件（Web: 0x55, APP: 0xAA）提供精准识别与前置非法拦截。
> - 📐 **内容密度优化**：全页面边距优化至 **1.2rem**，视野更开阔，横向空间利用率更高。
> - 🛡️ **底层渲染重构**：彻底消除页面切换及语言变换时的白屏崩溃风险。

## [1.3.7] - 2026-03-26
### 🛠️ 稳定性与体验修复 (Stability & UX Fixes)
*   **深层表单校验重构 (Form Validation Refactor):**
    *   **消除静默拦截**: 彻底移除了设置页面 (Settings) 中对频率和电压字段底层的异步字符串校验器。改由组件原生的极值防护机制接管全场，完美解决合法数值依然报错“越界拦截”且无法保存的幽灵 Bug。
*   **深色模式渲染调优 (Dark Mode Render Optimization):**
    *   **UI 防黑洞**: 修复了深色主题下功能开关 (Switch) 处于关闭状态时因背景透明导致的极端不可见问题。重新注入了针对深灰关闭轨道的强行保护色 `#4b5563`。
    *   **风扇轨道修复**: 恢复了高级加厚版的蓝色仪表盘风扇进度条样式，确保全局 UI 风格的前后一致性。
    *   **表格美学重载**: 找回了横向空间利用率极高的边框嵌入式 `a-descriptions` 样式表单排版，与参考表格视觉紧凑感拉满。

### 🔌 硬件架构升级支持 (Hardware Infrastructure Update)
*   **Type-C 全新供电生态兼容 (Type-C Power Compatibility):**
    *   **统一度量衡**: 响应最新批次的 **BC01** 和 **BC02** 机型全面拥抱 Type-C 15V 标准供电输入。将全局 Dashboard 面板的“Input Voltage”实时监控仪表盘上限强制锁定为 **15V**。
    *   **无缝按比例兼容**: 无论接入硬件是 5V, 12V 亦或是 15V 机型，动态电压仪表盘均能按照 `15V` 基准进行横向视觉兼容渲染。针对 BC01/02 微调了最大功耗等显示量程。

## [1.3.6] - 2026-03-25
### 💻 机型参数扩展 (Hardware Model Expansion)
*   **新增机型配置支持 (New Model Configurations):**
    *   **BC08**: 新增双倍芯片机型支持。核心算力和功率翻倍（上限 14 T / 240 W），核心参考电压范围对齐 BC04 标准。
    *   **BC01 & BC02**: 补充 1 芯片与 2 芯片版本机型配置。系统电压参数智能调整为 BC04 的四分之一以匹配低压阵列架构，算力与功耗参考范围根据芯片物理数量进行精确等比折算。

### 🎨 全局视图整合 (UI Layout Integration)
*   **卡片风格标准化 (Unified Card Layout):**
    *   将网络 (Network)、矿池 (Pool) 及系统日志 (Logs) 页面原先各自独立的文本大标题剥离并重构入容器，无缝接入全局统一的细边框体系标准卡片头 (`<a-card>`)。
    *   修复了特定暗色模式下内置开关组件（a-switch）对比度不明显的缺陷，强化表单操作的准确度。

## [1.3.5] - 2026-03-10
### ⚙️ 升级逻辑智能化 (Smart Upgrade Logic)
*   **版本感知识别 (Version-Aware Detection):**
    *   **动态算法切换**: 删除了硬编码的 `VITE_FIRMWARE_ENCRYPTED` 编译开关。系统现在会自动解析当前的固件版本字符串（支持如 `1.0.0 20260305` 等复杂格式）。
    *   **平滑过渡策略**: 当检测到固件版本 >= 2.0 时，自动启用加密固件头识别（0x55/0xAA）；对于 < 2.0 版本，自动回退至兼容模式。确保了新旧版本设备在同一套前端代码下的无缝升级。

### 📡 远程直推升级 (Remote OTA Update)
*   **Github 版本多宿主检测 (Dual Repository Detection):**
    *   **Web 与 App 解耦**: 自动分别获取 `www` 公用仓库和设备专属机型 (如 `DC02`) 仓库的最新 Github Release 版本信息。
*   **一键远程无缝直刷 (One-Click Flash):**
    *   **极致体验**: 摒弃了由用户手动下载 `.zip` 再网页上传的旧途径。用户在设置页面能够一键流式抓取 Release 附件并内部直推向矿机，全过程无需脱离当前浏览器界面。
    *   **机型安全阀门 (Hardware Toggle)**: 该功能深度绑定机型 `ModelConfig` 中的 `support_ota_download` 配置，默认静默关闭。仅被官方授权远程升级的机型才可见并使用此超前部署功能。

## [1.3.4] - 2026-03-09
### 🛡️ 固件升级与安全增强 (Firmware Security & Upgrade)
*   **加密固件自动识别 (Automatic Identification):**
    *   **首字节精准导流**: 实现了基于固件首字节的识别逻辑。加密 Web 固件（`0x55`）和 APP 固件（`0xAA`）将自动匹配至对应的上传接口。
    *   **非法固件前置拦截**: 增加了上传前的头部校验。若检测到文件不符合加密规范或调试标准，系统将直接弹出错误提示并中止上传，有效防止误传导致的系统故障。
*   **环境感知构建 (Environment-Aware Build):**
    *   **动态策略开关**: 引入了 `VITE_FIRMWARE_ENCRYPTED` 编译宏。在生产环境强制执行强校验逻辑，而在开发环境保留对旧版本调试固件（`0xE9`）的兼容性，确保开发调试的灵活性。

## [1.3.3] - 2026-03-09
### 📐 布局与功能体验优化 (Layout & UX Refinement)
*   **页边距深度调优 (Padding Optimization):**
    *   **极致容量**: 将矿池设置、系统设置、安全设置及日志页面的水平内边距统一从 `3rem / 5rem` 缩减至 `1.2rem`。解决了在宽屏下内容过于集中、在窄屏下两侧留白过多的平衡问题。
*   **功能项恢复 (Feature Restoration):**
    *   **性能模式**: 重新启用了设置页面中的 `Over Frequency` (超频) 工作模式选项。

## [1.3.2] - 2026-03-09
### 🛠️ 系统稳定性与适配 (Stability & Compatibility)
*   **菜单渲染底层重构 (Menu Rendering Refactor):**
    *   **零崩溃导航**: 弃用了容易导致 VNode 复用冲突的 `items` 属性方式，改用标准的模板 Slot 声明。彻底解决了频繁切换菜单或语言时可能触发的 Vue 渲染错误（白屏）。
*   **机型敏感布局管理 (Model-Aware Layout):**
    *   **按需显示参考配置**: 新增 `showRefConfig` 配置项。针对 **BC04** 等特定机型，自动隐藏“参考配置”表格及其外层容器，避免展示无效信息。
    *   **布局自动紧凑**: 隐藏面板时会自动移除背景装饰框，确保表单在不同机型下都能获得最佳的视觉重心。

## [1.3.1] - 2026-03-09
### 🎨 视觉集成优化 (UI Integration Enhancements)
*   **Logo 仪表盘集成 (Logo Dashboard Integration):**
    *   **极致紧凑设计**: 算力数值精准嵌入到左上角 Hammer Logo 的金属圆环内，全页面统一显示，极致利用空间。
    *   **实时状态联动**: 在线时显示亮绿数值 + 单位 (GH/s / TH/s)；离线时内圆自动切换为鲜红色并显示 "OFF"。
    *   **高对比度**: 深色背景 (#0f172a) 确保数值在任何主题下清晰易读。
*   **交互精简化 (Interaction Simplification):**
    *   **重启按钮还原**: 恢复了 Restart 按钮的原始电源图标设计，界面更简洁直觉。

## [1.3.0] - 2026-03-08
### 🌟 新特性与体验升级 (New Features & User Experience)
*   **智能离线监测系统 (Intelligent Offline Detection):**
    *   **掉线精准识别**: 优化了数据断点连接逻辑。当探测到数据中断时，算力曲线不再产生虚假的“对角连线”，而是准确地在中断处切断并归零。
    *   **离线确认与回填 (Backfill)**: 引入 10 次采集失败（约 100 秒）的确认机制。一旦确认离线，系统将追溯并把从最后一个有效点到当前的所有空白区域标记为离线状态。
    *   **可视化标记**: 在图表上以半透明红色块清晰展示离线时段，并配有单一居中的 "OFFLINE" 提示标签，极大增强了异常状态的感知度。
*   **硬件安全输入限制 (Hardware Input Guard)**:
    - **动态上限**: 设置页面的频率和电压输入现在根据当前机型配置自动设置 `max` 上限。
    - **单位一致性**: 自动处理机型配置 (V) 与 UI (cV) 之间的单位转换。
    - **即时校验**: 增加响应式校验规则，超出范围时输入框自动归位并提示合法范围。
*   **状态持久化增强 (Persistence Logic):**
    *   **刷新不丢状态**: 彻底重构了 `sessionStorage` 恢复机制。即使在离线期间或数据异常时刷新浏览器，算力曲线的历史记录、离线标记以及之前的统计读数也能完整恢复。
*   **移动端图例适配 (Mobile UX):**
    *   **响应式图例菜单**: 在移动端窄屏下，算力图表的横向筛选标签会自动切换为精美的下拉菜单模式，解决了多曲线环境下图例堆叠和显示不全的问题。

### 🛠️ 性能与稳定性修复 (Performance & Stability Fixes)
*   **轮询逻辑优化**: 修复了以往在网络完全断开时前端轮询逻辑会意外“挂起”的缺陷。现在即使处于无网络状态，离线判定状态机依然能保持运转。
*   **初始化流程修正**: 删除了 `App.vue` 中冗余且冲突的初始化代码，解决了首屏加载时偶尔出现的旧数据覆盖新数据的同步冲突。
*   **模拟环境扩展 (Mock Extension)**: 模拟 API 现已覆盖系统配置、主题设置及重启等管理类接口，提供更完整的离线开发闭环体验。

### 🔧 硬件参数微调
*   **BC04 模型优化**: 针对最新硬件批次微调了 BC04 的频率范围 (`820MHz`) 和电压基准 (`4.9V`)，确保监控读数与硬件实际表现一致。

## [1.2.1] - 2026-03-05
### 🌟 新特性与体验升级 (New Features & User Experience)
*   **ASIC 芯片层级监控 (Chip Monitoring):**
    *   新增特定机型 (如 BC04) 的独立芯片频率、温度和算力状态显示模块 (`ChipHeatmap`)。
    *   通过机型配置文件严格控制此高阶监控功能的呈现，同时前端实现了监控卡片自适应填充 (auto-fit)，无论窗口宽度如何变化都能保持完美的网格排布。
*   **模拟测试环境完善 (Mock Environment Improvement):**
    *   为离线测试和前端预览量身定做了动态伪造数据接口，算力、功耗、温度、运行时长等均能够根据不同机型的定义限制产生逼真的边界内跳动，以最高度还原网络轮询时的真实表现。
*   **性能监控图表优化 (Performance Chart):**
    *   **交互式数据点悬浮窗 (Hover Tooltip):** 配置底层的 `uPlot` 图表引擎，新增基于游标悬停事件的高亮数据点与自定义 Tooltip 悬浮数据框。解决了原版移动端不易精确辨识时刻读数的问题，并在闲时默认隐藏全部冗余节点以减少线框拥堵感。

### 🐛 问题修复 (Bug Fixes)
*   **登录会话重定向:** 修复了当机型不支持登录配置(`support_login: false`)且发生局部 API 超时或读取异常时，前端被意外强制重定向到 `/login` 的逻辑缺陷。
*   **设置页面显示优化:** 净化了 `SettingsEdit.vue` 面板，移除了不必要且可能误导新手的电压辅助解释文本。

## [1.2.0] - 2026-02-20
### 🔐 安全与认证 (Security & Authentication)
*   **Web 访问控制 (Access Control):**
    *   **登录保护:** 新增了基于 Token 的登录页面 (`/login`)。现在可以为 Web 管理界面设置访问密码，防止未授权访问。
    *   **全局路由守卫:** 实现了严格的前端路由拦截。未登录用户访问任何页面（Dashboard, Network, Settings）都会被强制重定向至登录页。
    *   **会话管理:** 支持用户手动注销 (Logout)，并在侦测到 Token 失效（如后端重启或会话过期）时自动跳转回登录页。

### 🎨 界面与交互 (UI/UX)
*   **登录页设计:**
    *   全新设计的极简登录界面，移除顶部导航栏，专注于身份验证。
    *   支持深色/浅色主题切换。
    *   右上角集成 **Restart** 按钮，方便在无法登录时重启设备。
*   **设置页优化:**
    *   **安全设置分离:** 新增独立的 **Security Settings** 区域，位于固件升级模块下方。
    *   **交互优化:** 
        *   "Enable Authentication" 开关采用品牌绿色 (`Verify Green`)，状态更直观。
        *   Logout 按钮移至安全设置区域右上角，操作更顺手。


## [1.1.4] - 2026-02-19
    *   **动态菜单布局 (Dynamic Menu Layout):**
        *   引入 JS 智能计算逻辑，根据屏幕剩余空间自动切换菜单显示模式（全称/缩写/换行）。
        *   彻底解决了在平板、窄屏桌面等不同尺寸下的菜单换行与布局错乱问题。
    *   **移动端优化:**
        *   优化了移动端菜单的滚动体验与居中对齐。

## '1.1.3 20260212'
    新增DC06，BC04
    取消SHA256验证
    增加能效显示
    新增矿池快速选择功能 (Solo模式)
    支持矿池Logo显示及配置文件管理
    合并Stratum地址端口输入

## [1.1.1] - 2026-01-13

### 🎨 交互与视觉升级 (UI/UX Improvements)
* **标签页式布局 (Tabbed Interface):**
    * **网络设置:** 将原本冗长的单一表单拆分为 **"Wi-Fi 配置"** 和 **"有线网络 (Ethernet)"** 两个标签页，并根据机型动态显示。
    * **矿池设置:** 将主矿池 (Primary) 和备用矿池 (Fallback) 分离为独立标签页，大幅缩短页面高度，提升移动端体验。
    * **视觉优化:** 修复了暗黑模式下 Tab 选中态不明显的问题，统一使用品牌绿色高亮选中项。
* **术语标准化:** 将矿池相关的 "Backup" 统一更为专业的 "Fallback" (故障转移)。

### 🏗️ 网络架构重构 (Network Architecture)
* **双网独立配置:**
    * 前端逻辑升级，支持为 **Wi-Fi** (`wlan0`) 和 **Ethernet** (`eth0`) 分别配置独立的静态 IP 或 DHCP 策略。
    * 增加了 `wifi_conf_*` 和 `eth_conf_*` 两组独立字段，解决了以往单组 IP 配置在多接口环境下冲突的问题。

### 📂 文件变更清单 (File Changes)
* **src/pages/Network.vue**: [Refactor] 引入 Tabs 布局，分离有线/无线 IP 设置。
* **src/pages/Poolsettings.vue**: [Refactor] 引入 Tabs 布局，分离主备矿池设置。
* **src/i18n/**: [Locales] 新增 Tab 相关翻译，修正 Backup -> Fallback。
* **src/api/type.ts**: [Type] 扩展 NetworkInfo 接口定义。
3. 后端数据对接清单 (Backend Integration Guide)
为了支持新的网络设置页面，后端 API (通常是 get_network_info 和 set_network_conf) 需要增加以下字段的处理。

A. 下行数据 (Get Network Info)
后端返回的 JSON 数据结构建议增加以下字段。如果后端未返回这些新字段，前端已做兼容处理（自动回退读取旧的 bb_conf_* 字段填充到 Wi-Fi 设置中）。
---
字段名 (Key),类型,说明,备注
wifi_conf_nettype,string,Wi-Fi 协议,"""DHCP"" 或 ""Static"""
wifi_conf_ipaddress,string,Wi-Fi IP地址,"e.g. ""192.168.1.100"""
wifi_conf_netmask,string,Wi-Fi 子网掩码,"e.g. ""255.255.255.0"""
wifi_conf_gateway,string,Wi-Fi 网关,"e.g. ""192.168.1.1"""
wifi_conf_dnsservers,string,Wi-Fi DNS,"e.g. ""8.8.8.8"""
eth_conf_nettype,string,有线 协议,"""DHCP"" 或 ""Static"""
eth_conf_ipaddress,string,有线 IP地址,
eth_conf_netmask,string,有线 子网掩码,
eth_conf_gateway,string,有线 网关,
eth_conf_dnsservers,string,有线 DNS,
## [1.1.0] - 2026-01-09
B. 上行数据 (Set Network Conf)
前端提交表单时，会同时发送三组数据，后端应按需解析：

wifi_conf_*: 明确用于配置 Wi-Fi 接口。

eth_conf_*: 明确用于配置有线接口。

bb_conf_* (旧字段): 前端仍会发送这组数据（以此兼容旧版后端），其值默认与 Wi-Fi 配置保持同步。

建议后端逻辑：

优先检查是否存在 eth_conf_nettype / wifi_conf_nettype 等新字段。

如果存在，则分别配置对应的物理接口 (eth0 / wlan0)。

如果不存在（旧版前端），则读取 bb_conf_* 并配置到主接口。

### 🌟 新特性 (New Features)
* **多机型适配 (Multi-Model Support):**
    * 新增 **DC02, DC04, DC08** 全系列机型支持。
    * 系统现在根据机型自动加载对应的功率上限、算力范围、报警阈值及参考电压/频率表。
* **双算力板支持 (Dual Hashboard):**
    * **Dashboard:** 新增第二块算力板温度曲线 (**Temp 2**) 及实时数值显示（粉色曲线区分）。
    * **Fans:** 支持双风扇 (**Fan 1 / Fan 2**) 独立转速显示。
* **动态设置参考 (Dynamic Settings):**
    * 设置页面中的 "Reference Configuration" 表格不再硬编码，而是根据当前机型（标准电压/高压模式）自动渲染对应的参考数据。

### 🎨 仪表盘优化 (Dashboard Improvements)
* **动态图表量程 (Adaptive Charts):**
    * 算力与功率图表的 Y 轴刻度现在根据机型配置自动缩放（例如 DC08 最大显示 200W，DC02 显示 50W）。
    * 优化了网格线步进算法 (`incrs`)，解决了高功率/高算力机型下刻度显示过密或消失的问题。
* **智能仪表指示 (Smart Meters):**
    * **REF 标记:** 电压与频率进度条上新增动态 **REF** 游标，直观展示当前值与官方参考值的偏差。
    * **动态告警:** 温度仪表盘的告警阈值（如 70°C 或 75°C）现在完全由机型配置决定。

### 🛠️ 代码重构 (Refactoring)
* **配置中心化:** 将分散的电气参数统一移至 `const.ts` 的 `DEVICE_MODELS_INFO` 结构中管理，便于后续新增机型。
* **状态管理升级:** Pinia Store 新增 `temperatureData2` 状态管理，实现了双路温度历史数据的采集、存储与回放。

### 📂 文件变更清单 (File Changes)
* **src/util/const.ts**: [Config] 定义 `ModelConfig` 接口，录入 DC02/04/08 详细参数表。
* **src/store/modules/app/**: [Store] 增加 `currentModelConfig` getter 及双温度数据流处理。
* **src/pages/Dashboard.vue**: [UI] 重构图表渲染逻辑，支持动态刻度与 REF 标记。
* **src/components/SettingsEdit.vue**: [UI] 参考表格改为计算属性，实时响应机型变化。

---
[1.0.10] - 2025-12-02
📝 日志系统重构 (Log System Refactor)
• 全量启动日志捕获:
• 机制: 在 main.c 系统入口处优先初始化日志系统，配合后端新增的 64KB 内存环形缓冲区 (Ring Buffer)，实现了日志的持久化缓存。
• 效果: 彻底解决了 WebSocket 连接前（如系统启动、WiFi 连接阶段）日志丢失的问题。用户现在可以看到设备上电后的完整历史记录。
• 交互流程简化 (Auto Connect):
• 优化: 移除了手动的“连接/断开” WebSocket 按钮。
• 行为: 页面加载时自动建立实时连接，无需人工干预，提供“开箱即看”的流畅体验。
• 日志下载支持:
• 新增: 工具栏新增 下载 (Download) 按钮。
• 细节: 后端实现流式传输接口，支持一键导出当前内存中的所有日志为 system_log.txt文件。针对浏览器安全策略进行了优化（使用 Internal RAM 中转及 .txt 后缀），有效规避了下载被拦截的问题。
• 视觉体验升级:
• 主题适配: 日志终端样式升级为 CSS 变量 (var(--surface-ground))，完美适配系统的明亮/暗黑模式切换。
• 解析增强: 更新了日志行解析逻辑，支持解析后端新增的带时间戳 ([YYYY-MM-DD HH:MM:SS]) 的日志格式，确保高亮渲染准确无误。
📂 文件变更清单 (File Changes)
• API: 新增 downloadLog 接口定义。
• UI: KernelLog.vue 移除手动连接，新增自动连接与下载逻辑。
• Core: 配合后端 http_server.c 的环形缓冲区实现。
￼
[1.0.9] - 2025-11-25
🎨 仪表盘与图表升级 (Dashboard & Chart)
• 曲线卡片重构:
• 新增功率曲线 (Power Draw): 实现了算力、温度、功率三条曲线同屏显示，提供更全面的设备运行状态监控。
• 图例交互增强: 将图例移至卡片头部，并升级为 Checkbox 复选框 样式。用户现在可以自由勾选/取消，灵活控制各条曲线的显示与隐藏。
• 视觉优化: 实现了三轴刻度的像素级对齐，保持图表整洁美观。
⚡ 交互体验优化 (UX Improvements)
• 智能隐藏策略:
• 优化: 改进了曲线隐藏的底层逻辑。
• 效果: 当用户取消勾选某条曲线（如算力）时，系统仅隐藏线条本身，保留背景网格和坐标轴参考系，防止图表布局跳变，提供更稳定的视觉体验。
🐛 问题修复 (Fixes)
• 数据持久化修复: 修复了刷新页面后功率数据丢失或错位的问题，确保了历史数据的完整性和时序准确性。
￼
[1.0.8] - 2025-11-23
🚀 核心架构与性能重构 (Core Architecture & Performance)
• 网络层：强制串行加载 (Serial Loading Strategy):
• 机制: 在 Vite 配置中禁用 modulePreload，强制浏览器按 HTML -> Index JS -> Vendor JS 的顺序串行下载。
• 解决痛点: 彻底解决了多文件并发下载导致的 ESP32 TCP 拥塞窗口竞争问题。实测在单线程网络栈设备上，串行下载大文件的吞吐率远高于并发下载小文件。
• 构建策略：单 Vendor 合包:
• 策略: 摒弃了细粒度的拆包策略，将 Vue、Pinia、Router、Ant Design 等核心依赖合并为单一的 vendor.js 文件。
• 目的: 配合串行加载机制，最大化利用单连接带宽，减少 HTTP 握手开销。
• 图表引擎轻量化:
• 变更: 将体积巨大的 @antv/g2 替换为微型库 uPlot。
• 收益: 显著减小了打包体积，并大幅提升了低功耗设备上的图表渲染帧率。
• 极限代码压缩:
• 工具: 引入 Terser 压缩器。
• 效果: 生产环境构建时自动移除 console 日志、debugger 断点及所有注释，进一步压榨文件体积。
⚡ 加载体验优化 (UX Optimization)
• 消除首屏黑屏 (Instant Loading):
• 技术: 引入 vite-plugin-css-injected-by-js 插件。
• 效果: 将 CSS 样式直接注入 JS 包，移除了 HTML 头部阻塞渲染的 <link> 标签。index.html (仅 ~1KB) 下载完毕后，Loading 动画能瞬间显示（<200ms），彻底消除了弱网环境下长达 8-12 秒的黑屏/白屏现象。
• 升级后自动刷新:
• 优化: 修改 Settings.vue 中的 Web 升级逻辑。
• 行为: 升级成功后，自动清空路由 Hash 并执行 window.location.reload() 强制刷新，防止浏览器缓存旧版 index.html 导致页面异常。
✨ 新增功能与资源优化 (Features & Assets)
• 资源 SVG 化: Logo 和 Favicon 替换为 SVG 格式。
• 按需动态加载: SHA256 (升级页面) 和 uPlot (仪表盘) 仅在需要时加载。
• 智能流控: 配网或升级模式下自动暂停后台数据轮询 (isPollingPaused)。
🐛 问题修复 (Fixes)
• 后端兼容: 修复配网模式 (AP Mode) 识别问题。
• 导航交互: 修复 Logo 点击跳转后的菜单状态同步。
• 类型系统: 修复 TypeScript 自动导入类型错误。
￼
[1.0.7] - 2025-11-21
• 基础版本发布。


[1.3.9] - 2026-05-27
1、增加TLS以及Extranonce Subscribe 开关
2、增加开关屏幕

[1.3.9] - 2026-05-29
1、Pool list里去掉ViaBTC
2、Pool list里增加AtlasPool.io

[1.3.9] - 2026-06-01
1、Pool list将AtlasPool.io位置放在第一个。

[1.3.9] - 2026-06-04
1、修改AtlasPool.io的图标。

[1.3.9] - 2026-06-05
1、增加github固件对比以及下载。
2、修复Mining-Dutch stratum host 默认名不通bug。

[1.3.9] - 2026-06-06
1、增加机型判断，只针对bc04起作用（屏幕开关，tls开关，extranonce subscribe开关 ota检查更新）

[1.3.9] - 2026-06-09
1、web端增加bc01-pro界面配置。

[1.3.9] - 2026-06-10
1、同步app日期。

[1.4.0] - 2026-06-15
1、自定义模式下，增加每个芯片设置频率。（在miner.h进行开关 界面 CONFIG_BC04_INDIVIDUAL_FREQ ）


[1.4.0] - 2026-06-24
1、增加以太网界面。
2、增加频率和电压单位。


