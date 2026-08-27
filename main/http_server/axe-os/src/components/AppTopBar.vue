<script setup lang="ts">
import { useAppStore } from "@/store";
import ToolBar from "@/components/TopToolBar.vue";
import { onMounted, onUnmounted, reactive, ref, watch, nextTick } from 'vue';
import { useRouter, useRoute } from 'vue-router';
import { useI18n } from 'vue-i18n';
import {
  HomeOutlined,
  MenuOutlined,
  SettingOutlined,
  UnorderedListOutlined,
  WifiOutlined,
} from '@ant-design/icons-vue';
import { getCookie, setCookie, showNotification, transformFreq } from "@/util/utils";
import { ENABLE_STATE } from "@/util/const";

const appStore = useAppStore();
const router = useRouter();
const route = useRoute();
const { t, locale } = useI18n();

// 动态布局状态
const isShowShortLabel = ref(false); // 是否显示缩写
const isWrapped = ref(false);        // 是否换行

const sml = (code: string): string => {
  return t(`sidebar.menu.${code}`);
}


// [新增] 路由导航函数
const navTo = (path: string) => {
  router.push(path);
};

const state = reactive({
  selectedKeys: [''],
});

// 计算文本宽度的辅助函数
const getTextWidth = (text: string, font: string = '14px -apple-system, BlinkMacSystemFont, "Segoe UI"') => {
    const canvas = document.createElement('canvas');
    const context = canvas.getContext('2d');
    if (context) {
        context.font = font;
        return context.measureText(text).width;
    }
    return 0;
};

// 计算菜单总宽度
const calculateMenuTotalWidth = (useShort: boolean) => {
    const keys = ['dashboard', 'network', 'pool_settings', 'settings', 'logs'];
    let totalWidth = 0;
    // 基础 padding (12px * 2) + margin (4px * 2) + Icon (approx 16px + gap) = approx 60px per item base
    // 实际 Ant Design Menu Item padding 是 16px，icon gap 10px
    const baseItemWidth = 12 * 2 + 8 + 16 + 10; // 估算值：左右padding + margin + icon + icon_gap
    
    keys.forEach(key => {
        const text = useShort ? sml(`${key}_short`) : sml(key);
        // 短标题字体稍小 (12px?)，全称默认 (14px)
        const font = useShort ? '13px Inter, sans-serif' : '14px Inter, sans-serif'; 
        totalWidth += getTextWidth(text, font) + baseItemWidth;
    });
    return totalWidth;
};

// 核心布局计算逻辑
const checkLayout = () => {
    const windowWidth = window.innerWidth;
    
    // 估算两侧占用宽度 (Logo区域 + 右侧Actions区域 + 布局Gap)
    // Logo ~180px, Actions ~150px, Gap ~2rem (32px)
    // 使用保守估计值
    const sideOccupied = 200 + 180 + 40; 
    const inlineAvailableWidth = windowWidth - sideOccupied;

    const fullMenuWidth = calculateMenuTotalWidth(false);
    const shortMenuWidth = calculateMenuTotalWidth(true);

    // 1. 尝试单行全称
    if (inlineAvailableWidth > fullMenuWidth) {
        isWrapped.value = false;
        isShowShortLabel.value = false;
    } 
    // 2. 尝试单行缩写
    else if (inlineAvailableWidth > shortMenuWidth) {
        isWrapped.value = false;
        isShowShortLabel.value = true;
    }
    // 3. 必须换行
    else {
        isWrapped.value = true;
        // 换行后，可用宽度为整个屏幕宽度 (减去一些 padding)
        const wrappedAvailableWidth = windowWidth - 40; 
        
        // 3.1 换行后尝试全称
        if (wrappedAvailableWidth > fullMenuWidth) {
             isShowShortLabel.value = false;
        } 
        // 3.2 换行后只能缩写 (极窄屏幕)
        else {
             isShowShortLabel.value = true;
        }
    }
};

let clickLastest = 0;
let clickCount = 0;

const onLogoClick = (event: MouseEvent) => {
  if (event.ctrlKey) {
    event.preventDefault();
    
    const timestamp = new Date().getTime();
    if (timestamp - clickLastest > 500) {
      clickCount = 1;
    } else {
      ++clickCount;
    }
    
    if (clickCount >= 6) {
      if (getCookie("debug_enable") == ENABLE_STATE.enable) {
        setCookie("debug_enable", "");
        appStore.setInfo({isDebugMode: false});
        showNotification("Debug mode disabled", "success");
      } else {
        setCookie("debug_enable", ENABLE_STATE.enable + '');
        appStore.setInfo({isDebugMode: true});
        showNotification("Debug mode enabled", "success");
      }
      clickCount = 0; 
    }
    clickLastest = timestamp;
  } else {
    router.push('/');
  }
};

watch(
  () => route.path,
  (newPath) => {
    if (newPath === '/' || newPath === '/dashboard') {
      state.selectedKeys = ['dashboard'];
    } else {
      state.selectedKeys = [newPath.replace('/', '')];
    }
  },
  { immediate: true }
);

// [算力显示逻辑]
const isOffline = computed(() => {
    return appStore.statusRaw == null || appStore.consecutiveFailures >= 10;
});

const currentHashrate = computed(() => {
    if (isOffline.value) return { data: 'OFF', suffix: '' };
    const val = (appStore.statusRaw?.hashRate || 0) * 1000000000;
    const raw = transformFreq(val);
    return { data: raw.data, suffix: raw.suffix }; // 完整单位，如 GH/s
});

// 页面语言切换时重新计算

// 语言切换时重新计算
watch(locale, () => {
    nextTick(() => {
        checkLayout();
    });
});

onMounted(() => {
  checkLayout();
  window.addEventListener('resize', checkLayout);
});

onUnmounted(() => {
    window.removeEventListener('resize', checkLayout);
});
</script>

<template>
  <div class="layout-topbar">
    <a class="layout-topbar-logo" @click="onLogoClick">
      <div class="logo-container">
        <div class="logo"></div>
        <div class="logo-hash-badge" :class="{ 'is-offline': isOffline }">
          <span class="val">{{ currentHashrate.data }}</span>
          <span class="unit">{{ currentHashrate.suffix }}</span>
        </div>
      </div>
    </a>

    <div class="topbar-menu-wrapper" :class="{ 'is-wrapped': isWrapped }">
      <a-menu
          :key="locale"
          v-model:selectedKeys='state.selectedKeys'
          mode='horizontal'
          class="layout-topbar-menu"
          :disabledOverflow="true"
          @click="appStore.onTopbarMenuClick"
      >
        <a-menu-item key="dashboard" @click="navTo('/dashboard')">
          <template #icon><HomeOutlined /></template>
          <span :class="['menu-label-full', isShowShortLabel ? 'hidden' : '']">{{ sml('dashboard') }}</span>
          <span :class="['menu-label-short', !isShowShortLabel ? 'hidden' : '']">{{ sml('dashboard_short') }}</span>
        </a-menu-item>
        <a-menu-item key="network" @click="navTo('/network')">
          <template #icon><WifiOutlined /></template>
          <span :class="['menu-label-full', isShowShortLabel ? 'hidden' : '']">{{ sml('network') }}</span>
          <span :class="['menu-label-short', !isShowShortLabel ? 'hidden' : '']">{{ sml('network_short') }}</span>
        </a-menu-item>
        <a-menu-item key="pool_settings" @click="navTo('/pool_settings')">
          <template #icon><UnorderedListOutlined /></template>
          <span :class="['menu-label-full', isShowShortLabel ? 'hidden' : '']">{{ sml('pool_settings') }}</span>
          <span :class="['menu-label-short', !isShowShortLabel ? 'hidden' : '']">{{ sml('pool_settings_short') }}</span>
        </a-menu-item>
        <a-menu-item key="settings" @click="navTo('/settings')">
          <template #icon><SettingOutlined /></template>
          <span :class="['menu-label-full', isShowShortLabel ? 'hidden' : '']">{{ sml('settings') }}</span>
          <span :class="['menu-label-short', !isShowShortLabel ? 'hidden' : '']">{{ sml('settings_short') }}</span>
        </a-menu-item>
        <a-menu-item key="logs" @click="navTo('/logs')">
          <template #icon><MenuOutlined /></template>
          <span :class="['menu-label-full', isShowShortLabel ? 'hidden' : '']">{{ sml('logs') }}</span>
          <span :class="['menu-label-short', !isShowShortLabel ? 'hidden' : '']">{{ sml('logs_short') }}</span>
        </a-menu-item>
      </a-menu>
    </div>

    <div class="layout-topbar-actions">
      <tool-bar></tool-bar>
    </div>
  </div>
</template>

<style scoped lang="scss">
@import "../styles/layout/_variables";

.layout-topbar {
    position: fixed;
    height: 5.5rem;
    z-index: 997;
    left: 0;
    top: 0;
    width: 100%;
    padding: 0 1.5rem;
    background-color: var(--surface-overlay);
    border-bottom: 1px solid var(--surface-border);
    transition: left $transitionDuration, background-color 0.3s;
    
    display: grid;
    grid-template-columns: 1fr auto 1fr;
    align-items: center;
    gap: 1rem;
}

.layout-topbar-logo {
    grid-column: 1;
    justify-self: start;
    display: flex;
    align-items: center;
    flex-shrink: 0;
    text-decoration: none;
    cursor: pointer;
    outline: none;
    -webkit-tap-highlight-color: transparent;
    
    &:focus {
      outline: none;
    }
}

.logo-container {
  position: relative;
  display: inline-block;
  line-height: 0;
}

.logo {
  height: 36px;
  width: 153px;
  background: url("/public/icon/logo.svg") no-repeat top left / 100% 100%;
}

.logo-hash-badge {
  position: absolute;
  // Logo SVG viewBox=837x197, rendered at 153x36.
  // Inner circle: cx=147, cy=98.5, r=70 in SVG coords.
  // Scale factor = 36/197 = 0.1827
  // Rendered center: x=147*0.1827=26.9px, y=36/2=18px
  // Rendered radius: 70*0.1827=12.8px => diameter ~26px
  width: 26px;
  height: 26px;
  top: 50%;
  left: 26.9px;
  transform: translate(-50%, -50%);
  border-radius: 50%;
  background: rgba(10, 20, 35, 0.82);
  border: 1px solid rgba(255, 255, 255, 0.15);
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  line-height: 1;
  pointer-events: none; // 不阻断 Logo 点击
  backdrop-filter: blur(2px);
  transition: background 0.3s, border-color 0.3s;

  .val {
    font-family: 'Inter', 'Roboto', sans-serif;
    // 使用 scale() 绕过 QQ/iOS 等浏览器的最小字体限制
    // 实际字体 16px * 0.5 = 8px 的视觉效果
    font-size: 16px;
    transform: scale(0.5);
    transform-origin: center;
    font-weight: 900;
    color: #4ade80;
    letter-spacing: -0.3px;
    line-height: 1;
    margin-bottom: -2px; // 补偿 scale 后的间距压缩
  }

  .unit {
    // 实际字体 16px * 0.344 ≈ 5.5px 的视觉效果
    font-size: 16px;
    transform: scale(0.344);
    transform-origin: center;
    font-weight: 800;
    color: rgba(255, 255, 255, 0.85);
    letter-spacing: -0.2px;
    line-height: 1;
    margin-top: -4px; // 补偿 scale 压缩后的堆叠间距
  }

  &.is-offline {
    background: rgba(220, 38, 38, 0.9);
    border-color: rgba(255, 100, 100, 0.4);
    .val {
      // 离线时适当放大一点 scale 让 "OFF" 更清晰
      transform: scale(0.45);
      margin-bottom: 0;
      color: #ffffff;
    }
    .unit { display: none; }
  }
}

.topbar-menu-wrapper {
    grid-column: 2;
    justify-self: center;
    min-width: 0;
    height: 100%;
    display: flex;
    align-items: center;
    justify-content: center;
    
    /* Global scroll/overflow handling */
    overflow-x: auto;
    -webkit-overflow-scrolling: touch;
    &::-webkit-scrollbar {
        display: none;
    }
    scrollbar-width: none;
    max-width: 100%; 
    
    /* 换行模式样式 */
    &.is-wrapped {
        grid-column: 1 / span 3; /* 占满整行 */
        grid-row: 2;
        width: 100%;
        justify-content: center;
        border-top: 1px solid rgba(255, 255, 255, 0.05); /* 可选：增加分割线 */
    }
}

.layout-topbar-menu {
    /* 基础样式，具体布局由 wrapper 和全局样式控制 */
    background: transparent;
    flex-shrink: 0; /* 防止被挤压 */
    
    :deep(.ant-menu-horizontal) {
        background: transparent !important;
        border-bottom: none !important;
        line-height: 5.4rem;
        width: auto;

        .ant-menu-item {
            font-size: 13px;
            color: var(--text-color-secondary);
            border-bottom: 3px solid transparent !important;
            padding: 0 12px;
            margin: 0 4px;
            border-radius: 8px 8px 0 0;

            &.ant-menu-item-selected {
                color: var(--ant-primary-color) !important;
                background: linear-gradient(90deg, #111827, #0f172a);
                border-bottom-color: var(--ant-primary-color) !important;
            }

            &:not(.ant-menu-item-selected):hover {
                color: var(--text-color) !important;
                background: #111827;
                border-bottom-color: var(--surface-border) !important;
            }
        }
    }
}

.layout-topbar-actions {
    grid-column: 3;
    justify-self: end;
    display: flex;
    align-items: center;
    flex-shrink: 0;
}

@media (max-width: 768px) {
    .layout-topbar {
        padding: 0.5rem 1rem;
        // height: auto; // 高度由内容撑开，不用显式写
    }
    
    // 即使在移动端，默认也尝试单行（grid-column: 2）。只有当 JS 判断需要 wrap 时才切换到 row 2。
    // 这里只需调整一些基础间距
}
</style>

<!-- 新增非 scoped 样式块，确保样式能应用到动态生成的 VNode 上 -->
<style lang="scss">
.hidden {
    display: none !important;
}

.layout-topbar-menu.ant-menu-horizontal,
.layout-topbar-menu .ant-menu-horizontal {
   // 核心布局：单行、不换行、透明背景
   display: inline-flex !important;
   flex-direction: row !important;
   flex-wrap: nowrap !important;
   white-space: nowrap !important;
   background: transparent !important;
   
   // 宽度自适应内容
   width: auto !important; 
   min-width: auto !important;
   
   // 移除默认滚动/对齐（交给 wrapper）
   overflow: visible !important;
   justify-content: center !important; 
   
   border-bottom: none !important;
   padding: 0 !important;
   
   // 强制子项不换行且不收缩
   .ant-menu-item, .ant-menu-submenu {
      flex-shrink: 0 !important;
      display: inline-flex !important;
      align-items: center !important;
      white-space: nowrap !important;
      float: none !important;
      padding: 0 8px !important; 
      margin: 0 !important;
   }
   
   &::before, &::after {
       display: none !important;
   }
}
</style>