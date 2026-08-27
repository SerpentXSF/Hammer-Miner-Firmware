import axios from "axios";
import type {
    AxiosInstance,
    AxiosRequestConfig,
    AxiosResponse,
    AxiosError,
    InternalAxiosRequestConfig,
} from "axios";
import { showMessage } from "./status";
import type { IResponse } from "./type";
import i18n from "@/i18n";
// import {statusData} from "@/api";

const t = i18n.global.t;

const lang = (code: string) => {
    return t(`err.${code}`);
};

const service: AxiosInstance = axios.create({
    timeout: 15000,
    headers: { "Content-Type": "application/json" },
});

service.interceptors.request.use(
    (config: InternalAxiosRequestConfig) => {
        const token = localStorage.getItem('auth_token');
        if (token) {
            config.headers.Authorization = `Bearer ${token}`;
        }
        return config;
    },
    (error: AxiosError) => {
        return Promise.reject(error);
    },
);

// axios实例拦截响应
service.interceptors.response.use(
    (response: AxiosResponse) => {
        if (response.status === 200) {
            return response;
        }
        console.log(showMessage(response.status));
        return response;
    },
    // 请求失败
    (error: any) => {
        // 不是2xx的都在这
        const { response } = error;
        if (response) {
            if (response.status === 401) {
                localStorage.removeItem('auth_token');
                if (!window.location.hash.includes('#/login')) {
                    window.location.href = '#/login';
                }
            }
            // 请求已发出，但是不在2xx的范围
            return Promise.reject(showMessage(response.status));
        } else {
            return Promise.reject(lang("errxxx1"));
        }
    },
);

const request = <T = any>(config: AxiosRequestConfig): Promise<T> => {
    const conf = config;
    // if(!conf.baseURL) {
    //     conf.baseURL = 'http://192.168.11.3/'
    // }
    return new Promise((resolve, reject) => {
        service
            .request<any, AxiosResponse<IResponse>>(conf)
            .then((res: AxiosResponse<IResponse>) => {
                let dataObj: any = res.data;
                resolve(dataObj as T);
            })
            .catch((err: any) => {
                reject(err);
            });
    });
};

export function get<T = any>(config: AxiosRequestConfig): Promise<T> {
    return request({ ...config, method: "GET" });
}

export function post<T = any>(config: AxiosRequestConfig): Promise<T> {
    return request({ ...config, method: "POST" });
}

export function patch<T = any>(config: AxiosRequestConfig): Promise<T> {
    return request({ ...config, method: "PATCH" });
}

export default request;

export type { AxiosInstance, AxiosResponse };
