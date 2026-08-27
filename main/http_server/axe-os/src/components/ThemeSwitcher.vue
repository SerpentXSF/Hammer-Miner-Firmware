<script setup lang="ts">
import { SkinOutlined } from "@ant-design/icons-vue";
import { useI18n } from "vue-i18n";
import { setTheme } from "@/api";
import { showNotification } from "@/util/utils.ts";

const { t } = useI18n();
const tl = (code: string): string => t(`tool.${code}`);

/* * 步骤 2.1: 重新定义所有主题变量
 * 所有主题现在都包含形状（--card-border-radius）和颜色。
*/

// 1. 浅色主题 (Light/Blue) - 更新
const lightTheme = {
  colorScheme: 'light',
  theme: 'light-blue',
  accentColors: {
    // 基础
    '--surface-ground': '#F2F2F2',
    '--surface-overlay': '#FFFFFF',
    '--surface-card': '#FFFFFF',
    '--surface-border': '#E9E9E9',
    '--text-color': '#000000',
    '--text-color-secondary': '#666666',
    
    // Antd 适配 (保留 AntD 默认蓝色)
    '--ant-primary-color': '#318EFF',
    '--ant-primary-color-hover': '#58a5ff',
    '--ant-primary-color-active': '#2272d9',
    '--ant-primary-color-text': '#ffffff',

    // 形状 (来自 HammerUI)
    '--card-border-radius': '14px',
    '--button-border-radius': '999px',
    '--input-border-radius': '8px',

    // 自定义组件
    '--primary-color': '#318EFF',
    '--primary-color-text': '#ffffff',
    '--highlight-bg': '#318EFF',
    '--highlight-text-color': '#ffffff',
    '--focus-ring': '0 0 0 0.2rem rgba(49, 142, 255, 0.2)',
    
    // (新增) 亮色主题特有的 Pills (算力卡片) 颜色覆盖
    '--pill-bg': '#ffffff',             // 白色背景
    '--pill-border': '#e5e7eb',         // 浅灰色边框
    '--pill-text': '#1f2937',           // 深灰色文字
    '--pill-shadow': '0 1px 3px 0 rgba(0, 0, 0, 0.1), 0 1px 2px -1px rgba(0, 0, 0, 0.1)', // 阴影

    // (新增) 图表背景色覆盖
    '--chart-bg': '#ffffff',
  }
};

// 2. 黑色主题 - 保持不变
const darkTheme = {
  colorScheme: 'dark',
  theme: 'dark-black', 
  accentColors: {
    // 基础 (来自 HammerUI Mock)
    '--surface-ground': '#050910',
    '--surface-overlay': '#05070d',
    '--surface-card': '#050b16',
    '--surface-border': '#1f2937', 
    '--text-color': '#f5f7fb',
    '--text-color-secondary': '#9ca3af',

    // Antd 适配 (来自 HammerUI)
    '--ant-primary-color': '#19e1a5',
    '--ant-primary-color-hover': '#2fedb6',
    '--ant-primary-color-active': '#14b889',
    '--ant-primary-color-text': '#020617', 

    // 形状 (来自 HammerUI)
    '--card-border-radius': '14px',
    '--button-border-radius': '999px',
    '--input-border-radius': '8px',

    // 自定义组件
    '--primary-color': '#19e1a5',
    '--primary-color-text': '#020617',
    '--highlight-bg': '#19e1a5',
    '--highlight-text-color': '#020617',
    '--focus-ring': '0 0 0 0.2rem rgba(25, 225, 165, 0.2)',
    
    '--ant-menu-item-selected-bg': 'linear-gradient(90deg, #111827, #0f172a)',

    // (恢复默认深色值，以防切换回深色时残留)
    '--pill-bg': 'radial-gradient(circle at top left, #1d2633, #050910)',
    '--pill-border': '#1f2937',
    '--pill-text': '#f5f7fb',
    '--pill-shadow': 'none',
    
    '--chart-bg': 'radial-gradient(circle at top, rgba(25, 225, 165, 0.2), transparent 55%), linear-gradient(180deg, #050910, #050910)'
  }
};

// 3. (已移除) 绿色主题

/**
 * 将主题变量应用到 CSS
 */
const applyTheme = (theme: { theme: string, colorScheme: string, accentColors: Record<string, string> }) => {
// ... (此函数不变) ...
  const root = document.documentElement;
  
  // 附加一个属性用于 CSS 选择器
  root.setAttribute('style', `color-scheme: ${theme.colorScheme}; --theme: ${theme.theme};`);
  
  for (const [key, value] of Object.entries(theme.accentColors)) {
    root.style.setProperty(key, value);
  }
};


/**
 * 切换主题，并调用 API 保存
 */
const switchTheme = async (theme: any) => {
  try {
    applyTheme(theme); // 1. 立即应用
    await setTheme(theme); // 2. 保存到后端 NVS
  } catch (e) {
    console.error("Failed to set theme:", e);
    showNotification("Failed to save theme", "error");
  }
};

</script>

<template>
  <a-dropdown placement="bottomLeft" class="tb-theme-btn-wrap" :trigger="['click']">
    <a-button type="text" class="tb-theme-btn">
      <SkinOutlined /> <span class="tb-theme-label">{{ tl('theme') }}</span>
    </a-button>
    <template #overlay>
      <a-menu>
        <a-menu-item @click="switchTheme(lightTheme)">
          {{ tl('light_theme_default') }}
        </a-menu-item>
        <a-menu-item @click="switchTheme(darkTheme)">
          {{ tl('dark_theme_black') }}
        </a-menu-item>
      </a-menu>
    </template>
  </a-dropdown>
</template>

<style scoped lang="scss">
.tb-theme-btn-wrap {
  /* (移除旧颜色) */
  color: var(--text-color); /* 步骤 2.1: 使用 CSS 变量 */
}

.tb-theme-btn {
  display: flex;
  align-items: center;
  font-size: 1.4rem;
  color: inherit; /* 继承父元素的颜色 */
  
  /* (新增) 移动端调整：减小内边距 */
  @media (max-width: 768px) {
      padding: 4px;
  }
}

.tb-theme-label {
  font-size: 0.9rem;
  
  /* (新增) 移动端隐藏文字，腾出空间 */
  @media (max-width: 768px) {
    display: none;
  }
}
</style>