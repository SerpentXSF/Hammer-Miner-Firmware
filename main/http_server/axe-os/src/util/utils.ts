import type { CSSProperties } from "vue";
import { message, notification } from "ant-design-vue";
import i18n from "@/i18n";
import cookies from "js-cookie";
import { ENABLE_STATE } from "@/util/const.ts";
import { getMinerStatus } from "@/api";
import { h } from 'vue';
import { LoadingOutlined } from "@ant-design/icons-vue";

const t = i18n.global.t;

const validatorIP = (value: string) => {
    const reg =
        /^(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$/;
    return !(value && !reg.test(value));
};

let MESSAGE_STYLE: CSSProperties = {
    position: "absolute",
    // top: "75px",
    left: "0",
    right: "0",
    margin: "auto",
};
const showLoading = (msg: string, duration: number, topPos?: string) => {
    const style = { ...MESSAGE_STYLE };
    if (topPos) {
        style.top = topPos;
    }
    return message.loading({
        content: () => msg,
        duration: duration,
        style: style,
    });
};

const showSuccess = (msg: string, topPos?: string) => {
    const style = { ...MESSAGE_STYLE };
    if (topPos) {
        style.top = topPos;
    }
    message.success({
        content: () => msg,
        style: style,
    });
};

const showError = (msg: string, topPos?: string) => {
    const style = { ...MESSAGE_STYLE };
    if (topPos) {
        style.top = topPos;
    }
    message.error({
        content: () => msg,
        style: style,
    });
};

const showErrors = (err1Msg: string, err2Msg: string) => {
    showError(err1Msg);
    setTimeout(() => {
        showError(err2Msg);
    }, 1000);
};

const showSystemError = () => {
    showError(`${t("err.errxxx2")}, ${t("err.errhint0")}!`);
};


const showNotification = (msg: string, type: string) => {
    notification[type]({ description: msg });
};


const showNotificationLoading = (msg: string, duration: number) => {
    notification.info({ message: "", description: msg, duration: duration, icon: () => h(LoadingOutlined) });
}

const isNumber = (value: string) => {
    return /^[+-]?\d+(\.\d+)?$/.test(value);
};

const transformFreq = (value: number): any => {
    if (value == null || value < 0) {
        return '0';
    }

    const suffixes = [' H/s', ' KH/s', ' MH/s', ' GH/s', ' TH/s', ' PH/s', ' EH/s'];

    let power = Math.floor(Math.log10(value) / 3);
    if (power < 0) {
        power = 0;
    }
    const scaledValue = value / Math.pow(1000, power);
    const suffix = suffixes[power];

    const getRes = (data: string, suffix: string) => {
        return { data, suffix, value: data + suffix };
    }

    if (scaledValue < 10) {
        return getRes(scaledValue.toFixed(2), suffix);
    } else if (scaledValue < 100) {
        return getRes(scaledValue.toFixed(1), suffix);
    }
    return getRes(scaledValue.toFixed(0), suffix);
}

const dateAgo = (value: any, args?: any): any => {
    if (value) {
        value = new Date().getTime() - value * 1000;
        let seconds = Math.floor((+new Date() - +new Date(value)) / 1000);
        if (seconds < 29) // less than 30 seconds ago will show as 'Just now'
            return 'Just now';
        const intervals: { [key: string]: number } = {
            'yr': 31536000,
            'mo': 2592000,
            'wk': 604800,
            'day': 86400,
            'hr': 3600,
            'min': 60,
            'sec': 1
        };
        let result = '';
        let shownIntervals = 0;
        for (const i in intervals) {
            if (args?.intervals && shownIntervals >= args.intervals) break;
            const counter = Math.floor(seconds / intervals[i]);
            if (counter > 0) {
                if (counter === 1) {
                    if (result) result += ', '
                    result += counter + ' ' + i + ''; // singular (1 day ago)
                    seconds -= intervals[i]
                } else {
                    if (result) result += ', '
                    result += counter + ' ' + i + 's'; // plural (2 days ago)
                    seconds -= intervals[i] * counter
                }
                shownIntervals++;
            }
        }
        return result;
    }
    return value;
}

const isEnable = (enable: number) => {
    return enable == ENABLE_STATE.enable;
}

const getUrl = (ip: string, path?: string) => {
    return `http://${ip}${path ?? ''}`;
}

const validData = (resData: any): any => {
    // [测试专用] 模拟离线逻辑
    const SIMULATE_OFFLINE = localStorage.getItem('simulate_offline') === 'true';
    if (SIMULATE_OFFLINE) {
        // 模拟每隔一段时间发生一次长达 120s 的离线
        const now = new Date().getTime();
        const cycle = 300000; // 5分钟一个周期
        const offlineDuration = 120000; // 离线2分钟
        if (now % cycle < offlineDuration) {
            return null;
        }
    }

    const res = resData && resData instanceof Object && 'hashRate' in resData ? resData : null;
    if (!res) {
        showNotification(t('com.msg_data_format_error'), 'error');
    }
    return res;
}

const getCurrentTime = async () => {
    try {
        const resData = await getMinerStatus('');
        if (validData(resData)) {
            const time = resData.currentTime.split(' ');
            return new Date(`${time[0]}T${time[1]}Z`);
        }
    } catch (e) {
        console.log(e);
    }
    return null;
}

const checkDebugMode = () => {
    return getCookie("debug_enable") == ENABLE_STATE.enable;
}

const setCookie = (key: string, value: string) => {
    cookies.set(key, value);
};

const getCookie = (key: string) => {
    return cookies.get(key) || "";
};

export {
    validatorIP,
    showSuccess,
    showError,
    showSystemError,
    showErrors,
    showLoading,
    isNumber,
    transformFreq,
    dateAgo,
    showNotification,
    isEnable,
    getUrl,
    validData,
    getCurrentTime,
    checkDebugMode,
    setCookie,
    getCookie,
    showNotificationLoading
};
