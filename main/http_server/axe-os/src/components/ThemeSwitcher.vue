<script setup lang="ts">
import { SkinOutlined } from "@ant-design/icons-vue";
import { useI18n } from "vue-i18n";
import { setTheme } from "@/api";
import { applyTheme, persistTheme, THEMES, type MinerTheme } from "@/util/theme";

const { t } = useI18n();
const tl = (code: string): string => t(`tool.${code}`);

/*
 * Theme definitions live in util/theme.ts so the same table can be used at
 * start-up to restore the last choice before the app paints. This component
 * only presents them.
 */

const switchTheme = async (theme: MinerTheme) => {
  applyTheme(theme);
  persistTheme(theme);

  /*
   * The firmware has no /api/theme handler, so this call always fails and
   * used to raise "Failed to save theme" on every switch. The choice is kept
   * in localStorage instead; the request stays as a no-op for firmware that
   * does implement it, and its failure is not the user's problem.
   */
  try {
    await setTheme(theme);
  } catch {
    /* stored locally; nothing to report */
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
        <a-menu-item v-for="theme in THEMES" :key="theme.theme" @click="switchTheme(theme)">
          <span class="tb-swatch" :style="{ background: theme.swatch }"></span>
          {{ tl(theme.labelKey) }}
        </a-menu-item>
      </a-menu>
    </template>
  </a-dropdown>
</template>

<style scoped lang="scss">
.tb-theme-btn-wrap {
  color: var(--text-color);
}

.tb-theme-btn {
  display: flex;
  align-items: center;
  font-size: 1.4rem;
  color: inherit;

  @media (max-width: 768px) {
      padding: 4px;
  }
}

.tb-theme-label {
  font-size: 0.9rem;

  @media (max-width: 768px) {
    display: none;
  }
}

.tb-swatch {
  display: inline-block;
  width: 0.75rem;
  height: 0.75rem;
  border-radius: 50%;
  margin-right: 0.55rem;
  vertical-align: middle;
  border: 1px solid rgba(255, 255, 255, 0.25);
}
</style>
