<script setup lang="ts">
import Lang from "@/components/Lang.vue";
import Restart from "@/components/Restart.vue";
import ThemeSwitcher from "@/components/ThemeSwitcher.vue"; // <-- 导入新组件
</script>

<template>
  <div class="tb-wrap">
    <ThemeSwitcher /> <!-- <-- 添加新组件 -->
    <lang></lang>
    <restart class="tb-restart-wrap"></restart>
  </div>
</template>

<style scoped lang="scss">
.tb-wrap {
  display: flex;
  gap: 0.5rem; /* (修改) 将 gap 从 1rem 减小到 0.5rem，让按钮更紧凑 */
  align-items: center; // 垂直居中对齐
}

.tb-restart-wrap {
}
</style>