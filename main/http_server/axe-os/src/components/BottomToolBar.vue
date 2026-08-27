<script setup lang="ts">
import Lang from "@/components/Lang.vue";
import Restart from "@/components/Restart.vue";
import ThemeSwitcher from "@/components/ThemeSwitcher.vue"; // <-- 保留
import TimeMachine from "@/components/TimeMachine.vue"; // <-- 新增：从 Sidebar 迁移
</script>

<template>
  <div class="tb-wrap">
    <ThemeSwitcher />
    <lang></lang>
    <restart class="tb-restart-wrap"></restart>
    <time-machine class="tb-time-wrap"></time-machine> <!-- <-- 新增 -->
  </div>
</template>

<style scoped lang="scss">
.bt-wrap {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 1rem;
  align-items: center; // 垂直居中对齐  
}

.bt-top-wrap {
  display: flex;
  justify-content: start;
  align-items: center;
}

.bt-time-sync-wrap {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.bt-time-sync-hint {
  color: #000000;
  font-size: 12px;
}

.bt-restart-wrap {
  margin-top: 1.2rem;
  width: 10rem;
  height: 2.8rem;
  font-size: 1rem;

}
/* 新增：为 TimeMachine 组件添加样式，使其适应顶部栏 */
.tb-time-wrap {
  /* 覆盖 TimeMachine 自己的样式 */
  position: relative;
  bottom: 0;
  left: 0;
  width: auto;
  height: auto;
  background: var(--surface-card); /* 使用卡片背景色 */
  border: 1px solid var(--surface-border); /* 使用边框色 */
  border-radius: 8px;
  padding: 5px 10px;
  display: flex;
  align-items: center;
  gap: 10px;

  /* 覆盖 TimeSync (图标) 的样式 */
  :deep(.sd-time-sync-wrap) {
    flex-direction: row; /* 水平排列图标和文字 */
    gap: 5px;
  }

  /* 覆盖 TimeSync (图标) 的提示文字 */
  :deep(.sd-time-sync-hint) {
    color: var(--text-color-secondary);
    font-size: 0.8rem;
    line-height: 1;
  }
  /* 覆盖 TimeDay (日期) 的样式 */
  :deep(.sd-time-data-wrap) {
    text-align: left;
  }
  :deep(.sd-time-data-sec) {
    font-size: 1.0rem;
    line-height: 1.1;
  }
  :deep(.sd-time-data-day) {
    font-size: 0.8rem;
    line-height: 1.2;
  }
}  
</style>
