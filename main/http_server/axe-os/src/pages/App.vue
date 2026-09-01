<script setup lang="ts">
// import AppSidebar from "../components/AppSidebar.vue";
import AppTopBar from "../components/AppTopBar.vue";
import {computed, onMounted, onUnmounted, ref} from "vue";
import {useAppStore} from "@/store";
import {checkDebugMode, showNotification, validData} from "@/util/utils.ts";
import {getMinerStatus, getTheme} from "@/api"; 
import {ENABLE_STATE, TIMER_WORKER, WIFI_STATUS} from "@/util/const.ts";
import {useRouter, useRoute} from "vue-router";
import {useI18n} from "vue-i18n";
import {MinerStatusData} from "@/api/type.ts";

const appStore = useAppStore();
const {t} = useI18n();
const router = useRouter();
const route = useRoute();

const containerClass = computed(() => {
  return {
    'layout-theme-dark': true,
  }
})

const showPage = ref<boolean>(false);

/*
 * Only complain once per outage.
 *
 * The poll below runs every ten seconds and used to raise a notification on
 * every failure. Sitting at the login screen therefore produced a new "data
 * synchronization failed" toast every ten seconds, stacking down the page
 * indefinitely -- and the same thing happened if the miner was simply
 * unplugged. The first failure is news; the sixtieth is not.
 */
let syncFailureReported = false;

const syncMinerStatus = async () => {
  try {
    const resData = await getMinerStatus('');
    syncFailureReported = false;   // recovered: the next failure is news again
    return validData(resData);
  } catch (e: any) {
    // A 401 is not a failed service, it is a session that needs signing in.
    // The API layer already routes that; saying the miner is broken as well is
    // both wrong and alarming.
    const status = e?.response?.status;
    if (status !== 401 && !syncFailureReported) {
      syncFailureReported = true;
      showNotification(t('com.msg_sync_error'), 'error');
    }
    console.log(e);
  }
  return null;
}

// 1. 默认黑色主题
const defaultDarkTheme = {
  colorScheme: 'dark',
  theme: 'dark-black', 
  accentColors: {
    '--surface-ground': '#050910',
    '--surface-overlay': '#05070d',
    '--surface-card': '#050b16',
    '--surface-border': '#1f2937',
    '--text-color': '#f5f7fb',
    '--text-color-secondary': '#9ca3af',
    '--ant-primary-color': '#19e1a5',
    '--ant-primary-color-hover': '#2fedb6',
    '--ant-primary-color-active': '#14b889',
    '--ant-primary-color-text': '#020617',
    '--card-border-radius': '14px',
    '--button-border-radius': '999px',
    '--input-border-radius': '8px',
    '--primary-color': '#19e1a5',
    '--primary-color-text': '#020617',
    '--highlight-bg': '#19e1a5',
    '--highlight-text-color': '#020617',
    '--focus-ring': '0 0 0 0.2rem rgba(25, 225, 165, 0.2)',
    '--ant-menu-item-selected-bg': 'linear-gradient(90deg, #111827, #0f172a)',
  }
};

// 默认浅色主题
const defaultLightTheme = {
    colorScheme: 'light',
    theme: 'light-blue',
    accentColors: {
      '--surface-ground': '#F2F2F2',
      '--surface-overlay': '#FFFFFF',
      '--surface-card': '#FFFFFF',
      '--surface-border': '#E9E9E9',
      '--text-color': '#000000',
      '--text-color-secondary': '#666666',
      '--ant-primary-color': '#318EFF',
      '--ant-primary-color-hover': '#58a5ff',
      '--ant-primary-color-active': '#2272d9',
      '--ant-primary-color-text': '#ffffff',
      '--card-border-radius': '14px',
      '--button-border-radius': '999px',
      '--input-border-radius': '8px',
      '--primary-color': '#318EFF',
      '--primary-color-text': '#ffffff',
      '--highlight-bg': '#318EFF',
      '--highlight-text-color': '#ffffff',
      '--focus-ring': '0 0 0 0.2rem rgba(49, 142, 255, 0.2)'
    }
};

const applyTheme = (theme: { theme: string, colorScheme: string, accentColors: Record<string, string> }) => {
  const root = document.documentElement;
  
  root.setAttribute('style', `color-scheme: ${theme.colorScheme}; --theme: ${theme.theme};`);
  
  const baseColors = theme.colorScheme === 'light' 
    ? { ...defaultLightTheme.accentColors }
    : { ...defaultDarkTheme.accentColors };
    
  const finalColors = { ...baseColors, ...(theme.accentColors || {}) };

  for (const [key, value] of Object.entries(finalColors)) {
    if (value && typeof value === 'string') {
      root.style.setProperty(key, value);
    }
  }
};

const loadTheme = async () => {
  try {
    applyTheme(defaultDarkTheme);
    const savedTheme = await getTheme();
    if (savedTheme && savedTheme.theme && savedTheme.theme !== 'dark-black') {
       applyTheme(savedTheme);
    }
  } catch (e) {
    console.warn("Theme load failed, using default.", e);
  }
};



const syncChartData = async () => {
  // 核心修改：如果 polling 被暂停，则不获取数据
  if (appStore.isPollingPaused) return;

  // Nothing to poll for while signed out. Every request would be refused, and
  // the login screen has nothing to show from it either way.
  if (!appStore.isAuthenticated) return;

  const statusRaw = await syncMinerStatus();
  // 无论成功失败，都必须调用 maintainDataset，以便触发连续失败判定
  appStore.setInfo({statusRaw});
  appStore.maintainDataset(statusRaw);
}

const initSysData = (statusRaw: MinerStatusData | null) => { 
  // initChartData(); // 移除，由 Store 初始化加载
  if (statusRaw) {
    appStore.maintainDataset(statusRaw);
    const time = statusRaw.currentTime.split(' ');
    appStore.setInfo({
      currentTime: new Date(`${time[0]}T${time[1]}Z`),
      localTime: new Date(),
      deviceModel: statusRaw.DeviceModel
    });

    // 核心修改：如果处于 AP 模式 (未配置网络)，暂停数据刷新并跳转到网络配置页
    if (statusRaw.apEnabled == ENABLE_STATE.enable) {
      // 暂停刷新
      appStore.setInfo({ isPollingPaused: true });
      // 跳转网络页
      if (statusRaw.wifiStatus != WIFI_STATUS.Connecting) {
        router.push({path: `/network`});
      }
    } else {
      // 否则确保开启刷新
      appStore.setInfo({ isPollingPaused: false });
    }
  }
  
  appStore.setInfo({statusRaw: statusRaw});
}

let worker;
const createChartDataTimerWorker = (intervalTime: number) => {
  worker = new Worker(new URL(TIMER_WORKER, import.meta.url));
  worker.onmessage = async () => {
    await syncChartData();
  };
  worker.postMessage({interval: intervalTime});
}

const initSysStatus = async () => {
  const statusRaw = await syncMinerStatus();
  initSysData(statusRaw);
  createChartDataTimerWorker(10000); // 即使全黑开机也启动定时器
  showPage.value = true;
}

onUnmounted(() => {
  if (worker) {
    worker.terminate();
  }
});

onMounted(async () => {
  await loadTheme(); 

  if (checkDebugMode()) {
    appStore.setInfo({isDebugMode: true});
  }
  
  await initSysStatus();

  window.onresize = () => {
    return (() => {
      appStore.setInfo({windowInnerWidth: window.innerWidth});
    })();
  };
})
</script>

<template>
  <div class="layout-wrapper" :class="containerClass">
    <a-config-provider
        :theme="{
          token: {
            colorPrimary: 'var(--ant-primary-color)',
            colorInfo: 'var(--ant-primary-color)',
            colorBgBase: 'var(--surface-ground)',
            colorText: 'var(--text-color)',
            colorTextSecondary: 'var(--text-color-secondary)',
            colorBorder: 'var(--surface-border)',
            colorBorderSecondary: 'var(--surface-border)',
            colorBgContainer: 'var(--surface-overlay)',
            colorBgElevated: 'var(--surface-overlay)',
            colorTextHeading: 'var(--text-color)',
          },
          components: {
            Menu: {
              colorItemBg: 'var(--ant-menu-bg, var(--surface-overlay))',
              colorItemText: 'var(--ant-menu-item-color, var(--text-color-secondary))',
              colorItemTextSelected: 'var(--ant-menu-item-selected-color, var(--ant-primary-color))',
              colorItemBgSelected: 'var(--ant-menu-item-selected-bg)',
              colorItemTextHover: 'var(--ant-menu-item-selected-color, var(--ant-primary-color))',
              colorItemBgHover: 'var(--ant-menu-item-active-bg)',
              colorItemBgSelectedHorizontal: 'transparent',
              colorItemTextSelectedHorizontal: 'var(--ant-primary-color)',
            },
            Modal: {
              colorBgElevated: 'var(--ant-modal-content-bg, var(--surface-overlay))',
              colorTextHeading: 'var(--ant-modal-header-title-color, var(--text-color))',
              colorText: 'var(--ant-text-color, var(--text-color))',
              colorIcon: 'var(--ant-modal-close-color, var(--text-color-secondary))',
              colorIconHover: 'var(--ant-text-color, var(--text-color))',
            },
            Notification: {
              colorBgElevated: 'var(--surface-card)',
              colorText: 'var(--text-color)',
              colorTextHeading: 'var(--text-color)',
              colorIcon: 'var(--text-color-secondary)',
              colorIconHover: 'var(--text-color)', 
              colorClose: 'var(--text-color-secondary)',
            },
            Card: {
              colorBgContainer: 'var(--ant-card-bg, var(--surface-overlay))',
              colorTextHeading: 'var(--ant-card-head-color, var(--text-color))',
              colorBgContainerHead: 'var(--ant-card-head-background, var(--surface-overlay))',
              colorBorderSecondary: 'var(--surface-border)',
            },
            Input: {
              colorBgContainer: 'var(--ant-input-bg, #ffffff)',
              colorText: 'var(--ant-input-color, var(--text-color))',
              colorTextPlaceholder: 'var(--ant-input-placeholder-color, var(--text-color-secondary))'
            },
            InputNumber: {
              colorBgContainer: 'var(--ant-input-bg, #ffffff)',
              colorText: 'var(--ant-input-color, var(--text-color))',
              colorBgContainerDisabled: 'var(--ant-input-bg, #f5f5f5)',
              colorTextDisabled: 'var(--ant-text-color-secondary, #00000040)',
              colorBgElevated: 'var(--ant-input-number-handler-bg, #ffffff)',
              colorBorder: 'var(--surface-border)',
              colorTextPlaceholder: 'var(--ant-input-placeholder-color, var(--text-color-secondary))'
            },
            Select: {
              colorBgContainer: 'var(--ant-input-bg, #ffffff)',
              colorBgElevated: 'var(--ant-popover-background, var(--surface-overlay))',
            },
            Button: {
              colorBgContainer: 'var(--ant-btn-default-bg, var(--surface-card))', 
              colorBorder: 'var(--ant-btn-default-border, var(--surface-border))',
              colorText: 'var(--ant-btn-default-color, var(--text-color))',
              colorPrimary: 'var(--ant-primary-color)',
              colorPrimaryHover: 'var(--ant-primary-color-hover)',
              colorPrimaryActive: 'var(--ant-primary-color-active)',
              colorTextLightSolid: 'var(--ant-primary-color-text)',
            },
            Progress: {
                colorProgress: 'var(--ant-primary-color)',
                colorRemaining: 'var(--ant-progress-remaining-color, var(--surface-border))',
            },
            Spin: {
              colorText: 'var(--ant-primary-color)',
            },
            Slider: {
                colorBgTrack: 'var(--surface-border)',
                colorBgRail: 'var(--surface-border)',
            }
          }
        } as any"> <!-- 强制类型断言为 any，忽略严格的主题 Token 检查 -->
      <template #default>
        <app-top-bar v-if="!route.meta.hideLayout"></app-top-bar>
        <div class="layout-main-container" :class="{ 'no-layout': route.meta.hideLayout }">
          <a-spin :spinning="!showPage" tip="loading">
            <div class="layout-main">
              <router-view v-slot="{ Component }">
                <transition>
                  <component :is="Component" />
                </transition>
              </router-view>
            </div>
          </a-spin>
        </div>
      </template>
    </a-config-provider>
  </div>
</template>

<style scoped lang="scss">
@import "../styles/layout/layout";

.layout-wrapper {
  background-color: var(--surface-ground);
  transition: background-color 0.3s;
}

.layout-main-container {
    padding: 7rem 2rem 2rem 2rem;
    min-height: 100vh; 

    @media (max-width: 991px) {
        padding: 9rem 1rem 1rem 1rem;
    }
    
    &.no-layout {
        padding: 0;
    }
}
</style>