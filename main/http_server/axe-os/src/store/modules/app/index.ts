import { defineStore } from "pinia";
import type { AppState } from "./types";
import { isDesktop } from "@/util/common";
import { DEVICE_MODELS_INFO, ModelConfig } from "@/util/const.ts"; // [修改] 引入类型
import { validData } from "@/util/utils.ts"; // [新增]
import { MinerStatusData } from "@/api/type.ts";
import { getMinerStatus, login } from "@/api";

// Max log lines
const MAX_LOG_LINES = 5000;

export const useAppStore = defineStore("app", {
    state: (): AppState => ({
        isAPMode: false,
        isDataLoaded: false,
        token: localStorage.getItem('auth_token') || '',
        staticMenuDesktopInactive: false,
        staticMenuMobileActive: false,
        isDebugMode: false,
        windowInnerWidth: window.innerWidth,
        currentTime: new Date('1970-01-01T00:00:00Z'),
        localTime: new Date(),
        deviceModel: "",

        statusRaw: null,
        dataLabel: JSON.parse(sessionStorage.getItem("dataLabel") || "[]"),
        hashrateData: JSON.parse(sessionStorage.getItem("hashrateData") || "[]"),
        temperatureData: JSON.parse(sessionStorage.getItem("temperatureData") || "[]"),
        temperatureData2: JSON.parse(sessionStorage.getItem("temperatureData2") || "[]"),
        powerData: JSON.parse(sessionStorage.getItem("powerData") || "[]"),
        chartData: JSON.parse(sessionStorage.getItem("chartData") || "[]"),
        chartDataVersion: 0,
        domainsOrigin: {
            temp: [],
            hash: []
        },
        domainsDst: {
            temp: [0, 0],
            hash: [0, 0]
        },
        chartResetting: false,

        // WebSocket Status
        ws: null,
        wsConnecting: false,
        wsConnected: false,
        logContent: "",
        needsRestart: false,
        consecutiveFailures: 0,

        isPollingPaused: false,
    }),

    getters: {
        isDesktop(state: AppState) {
            return state.windowInnerWidth > 991;
        },
        // [新增] 核心 Getter: 获取当前机型配置
        currentModelConfig(state: AppState): ModelConfig {
            return DEVICE_MODELS_INFO[state.deviceModel] || DEVICE_MODELS_INFO['default'];
        },
        // [修改] 以下 Getter 全部复用 currentModelConfig
        hasSecondHashBord(): boolean {
            return this.currentModelConfig.hashboard_count > 1;
        },
        hasSecondFan(): boolean {
            return this.currentModelConfig.fan_count > 1;
        },
        hasFlipScreen(): boolean {
            return this.currentModelConfig.flip_screen;
        },
        hasBacklight(): boolean {
            return this.currentModelConfig.backlight;
        },
        hasInvertFanDutyCycle(): boolean {
            return this.currentModelConfig.invert_fan_duty_cycle;
        },
        // [新增] 是否支持有线网络
        hasEthernet(): boolean {
            return this.currentModelConfig.has_ethernet;
        },
        minerStatus(state: AppState): MinerStatusData | null {
            return state.statusRaw;
        },
        isAuthenticated(state: AppState): boolean {
            return !!state.token;
        }
    },
    actions: {
        setInfo(partial: Partial<AppState>) {
            this.$patch(partial);
        },
        onMenuToggle() {
            if (isDesktop()) {
                this.staticMenuDesktopInactive = !this.staticMenuDesktopInactive;
            } else {
                this.staticMenuMobileActive = !this.staticMenuMobileActive;
            }
        },
        onMenuMobileClose() {
            if (!isDesktop()) {
                this.staticMenuMobileActive = !this.staticMenuMobileActive;
            }
        },
        onMaskClick() {
            this.onMenuMobileClose();
        },
        onTopbarMenuClick() {
            this.onMenuMobileClose();
        },
        resetChartData() {
            this.domainsOrigin.temp = [];
            this.domainsOrigin.hash = [];
            this.domainsDst.temp = [0, 0];
            this.domainsDst.hash = [0, 0];
            this.dataLabel = [];
            this.hashrateData = [];
            this.temperatureData = [];
            this.temperatureData2 = []; // [新增]
            this.powerData = [];
            this.chartData = [];

            sessionStorage.removeItem("dataLabel");
            sessionStorage.removeItem("hashrateData");
            sessionStorage.removeItem("temperatureData");
            sessionStorage.removeItem("temperatureData2");
            sessionStorage.removeItem("powerData");
            sessionStorage.removeItem("chartData");
        },
        setToken(token: string) { // [新增] setToken action
            this.token = token;
            localStorage.setItem('auth_token', token);
        },
        logout() { // [新增] logout action
            this.token = '';
            localStorage.removeItem('auth_token');
        },
        async login(username: string, password: string) { // [新增] login action
            try {
                const res = await login(username, password);
                if (res && res.token) {
                    this.setToken(res.token);
                    return true;
                }
                return false;
            } catch (e) {
                console.error("Login failed", e);
                return false;
            }
        },
        async updateState() {
            try {
                const res = await getMinerStatus('');
                const data = validData(res);
                if (data) {
                    this.setInfo({
                        statusRaw: data,
                        deviceModel: data.DeviceModel,
                        isDataLoaded: true
                    });
                    return true;
                }
            } catch (e) {
                console.error("Failed to update state", e);
                // Check if axios interceptor cleared the token (401)
                if (!localStorage.getItem('auth_token') && this.token) {
                    this.setToken('');
                }
            }
            return false;
        },
        maintainDataset(statusRaw?: MinerStatusData | null) {
            const time = new Date().getTime();
            const lastTime = this.dataLabel.length > 0 ? this.dataLabel[this.dataLabel.length - 1] : 0;
            const threshold = 100000;

            // 1. 休眠检测
            if (lastTime > 0 && (time - lastTime) > threshold) {
                this._internalPushData(lastTime + 10000, 0, 0, 0, 0, true);
                this._internalPushData(time - 1000, 0, 0, 0, 0, true);
                this.consecutiveFailures = 0;
            }

            // 2. 采样逻辑
            const isPowerFault = !!statusRaw?.power_fault;
            const isDataMissing = statusRaw === null || !statusRaw;

            if (isDataMissing || isPowerFault) {
                this.consecutiveFailures++;

                // [离线确认逻辑] 恢复为 10 次
                const OFFLINE_THRESHOLD = 10;

                if (this.consecutiveFailures === OFFLINE_THRESHOLD) {
                    // [回填逻辑] 查找上一个点到现在的空隙，补上离线点
                    // 由于之前 "不操作"，所以 dataLabel 等数组没动，这里直接推入即可
                    // 补齐从 lastTime+10s 到当前时间前的点
                    if (lastTime > 0) {
                        for (let t = lastTime + 10000; t < time - 5000; t += 10000) {
                            this._internalPushData(t, 0, 0, 0, 0, true);
                        }
                    }
                    this._internalPushData(time, 0, 0, 0, 0, true);
                } else if (this.consecutiveFailures > OFFLINE_THRESHOLD || isPowerFault) {
                    // 已确认离线或电源故障，直接推入
                    this._internalPushData(time, 0, 0, 0, 0, true);
                }
                // 如果小于阈值且非 power_fault，则 "不操作" (静默等待)
            } else {
                this.consecutiveFailures = 0;
                const hashrate = statusRaw.hashRate * 1000000000;
                const temp1 = statusRaw.temp;
                const temp2 = statusRaw.temp1 || 0;
                const power = statusRaw.power;
                this._internalPushData(time, hashrate, temp1, temp2, power, false);
            }
        },

        _internalPushData(time: number, hashrate: number, temp1: number, temp2: number, power: number, isOffline: boolean) {
            this.setDomains(hashrate, Math.max(temp1, temp2));
            this.dataLabel.push(time);
            this.hashrateData.push(hashrate);
            this.temperatureData.push(temp1);

            if (!this.temperatureData2) this.temperatureData2 = [];
            this.temperatureData2.push(temp2);

            if (!this.powerData) this.powerData = [];
            this.powerData.push(power);

            this.chartData.push({
                time,
                hashrate,
                temperature: temp1,
                offline: isOffline as any // 扩展类型定义或在这里强制转换
            } as any);

            this.chartDataVersion = this.chartDataVersion + 1;

            if (this.hashrateData.length >= 7200) {
                this.dataLabel.shift();
                this.hashrateData.shift();
                this.temperatureData.shift();
                this.temperatureData2.shift();
                this.powerData.shift();
                this.chartData.shift();
            }

            sessionStorage.setItem("dataLabel", JSON.stringify(this.dataLabel));
            sessionStorage.setItem("hashrateData", JSON.stringify(this.hashrateData));
            sessionStorage.setItem("temperatureData", JSON.stringify(this.temperatureData));
            sessionStorage.setItem("temperatureData2", JSON.stringify(this.temperatureData2));
            sessionStorage.setItem("powerData", JSON.stringify(this.powerData));
            sessionStorage.setItem("chartData", JSON.stringify(this.chartData));
        },
        setDomains(hashrate: number, temperature: number) {
            // 保持原有逻辑，用于计算简单的 min/max 范围，uPlot 会自动接管，所以这里逻辑影响不大
            if (this.domainsOrigin.hash.length == 0) {
                this.domainsOrigin.hash = [hashrate, hashrate];
            }

            if (this.domainsOrigin.temp.length == 0) {
                this.domainsOrigin.temp = [temperature, temperature];
            }

            if (hashrate > this.domainsOrigin.hash[1]) {
                this.domainsOrigin.hash[1] = hashrate;
            } else if (hashrate < this.domainsOrigin.hash[0]) {
                this.domainsOrigin.hash[0] = hashrate;
            }

            if (temperature > this.domainsOrigin.temp[1]) {
                this.domainsOrigin.temp[1] = temperature;
            } else if (temperature < this.domainsOrigin.temp[0]) {
                this.domainsOrigin.temp[0] = temperature;
            }

            // 下面的 domainsDst 计算对 uPlot 静态量程模式影响较小，保留即可
            const distanceHash = this.domainsOrigin.hash[1] - this.domainsOrigin.hash[0];
            const offsetHash = parseFloat(distanceHash * 0.1 + '').toFixed(0);

            const minHash = this.domainsOrigin.hash[0] - parseInt(offsetHash);
            const maxHash = this.domainsOrigin.hash[1] + parseInt(offsetHash);

            this.domainsDst.hash[0] = minHash < 0 ? 0 : minHash;
            this.domainsDst.hash[1] = maxHash;

            const offsetTemp = 5;

            const minTemp = this.domainsOrigin.temp[0] - offsetTemp;
            const maxTemp = this.domainsOrigin.temp[1] + offsetTemp;

            this.domainsDst.temp[0] = minTemp < 0 ? 0 : minTemp;
            this.domainsDst.temp[1] = maxTemp;
        },
        // WebSocket Connect (保持不变)
        connectWebSocket(translations: { [key: string]: string }) {
            // ... (保持原代码不变)
            if (this.ws || this.wsConnecting) return;
            this.wsConnecting = true;
            this.logContent = translations.connecting + "\n";
            const wsProtocol = window.location.protocol === "https:" ? "wss:" : "ws:";
            const wsUrl = `${wsProtocol}//${window.location.host}/api/ws`;
            try {
                const socket = new WebSocket(wsUrl);
                socket.onopen = () => {
                    this.ws = socket;
                    this.wsConnected = true;
                    this.wsConnecting = false;
                    this.logContent = translations.connected + "\n";
                };
                socket.onmessage = (event) => {
                    this.logContent += event.data;
                    const lines = this.logContent.split('\n');
                    if (lines.length > MAX_LOG_LINES) {
                        this.logContent = lines.slice(lines.length - MAX_LOG_LINES).join('\n');
                    }
                };
                socket.onclose = () => {
                    this.logContent += "\n" + translations.disconnected + "\n";
                    this.ws = null;
                    this.wsConnected = false;
                    this.wsConnecting = false;
                };
                socket.onerror = () => {
                    this.logContent += "\n" + translations.error + "\n";
                    this.ws = null;
                    this.wsConnected = false;
                    this.wsConnecting = false;
                };
            } catch (error) {
                this.wsConnecting = false;
            }
        },
        disconnectWebSocket() {
            if (this.ws) {
                this.ws.close();
            }
        }
    },
});