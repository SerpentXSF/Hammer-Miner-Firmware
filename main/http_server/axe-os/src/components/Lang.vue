<script setup lang="ts">
import {GlobalOutlined} from "@ant-design/icons-vue";

import {LANG_LIST, langChange} from "@/i18n";
import {useI18n} from "vue-i18n";

const {locale} = useI18n();

</script>

<template>
  <a-dropdown placement='bottomLeft' class='tb-lang-btn-wrap' :trigger="['click']">
    <a-button type="text" class='tb-lang-btn'>
      <GlobalOutlined/> <span class="tb-lang-label">{{LANG_LIST[locale]}}</span>
    </a-button>
    <template #overlay>
      <a-menu>
        <a-menu-item v-for='lang in Object.entries(LANG_LIST)' @click='langChange(lang[0])'>
          {{ lang[1] }}
        </a-menu-item
        >
      </a-menu>
    </template>
  </a-dropdown>
</template>

<style scoped lang="scss">
.tb-lang-btn-wrap {
  color: var(--text-color); // 修复：使用 CSS 变量
}

.tb-lang-btn {
  display: flex;
  align-items: center;
  font-size: 1.4rem;
  color: inherit; // 继承父级颜色
  
  /* (新增) 移动端调整：减小内边距 */
  @media (max-width: 768px) {
      padding: 4px;
  }
}

.tb-lang-label {
  font-size: 0.9rem;

  /* (新增) 移动端隐藏文字，腾出空间 */
  @media (max-width: 768px) {
    display: none;
  }
}
</style>