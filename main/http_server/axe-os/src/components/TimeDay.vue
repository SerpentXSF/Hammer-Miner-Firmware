<script setup lang="ts">
import {useAppStore} from "@/store";
import {computed, onMounted, onUnmounted, ref} from "vue";
import {useI18n} from "vue-i18n";

const {locale} = useI18n();
const timeDiffRef = ref<number>(0);
const appStore = useAppStore();

const currentTime = computed(() => {
  const time = new Date(appStore.currentTime.getTime() + timeDiffRef.value);
  return {
    day: time.toLocaleDateString(locale.value),
    time: time.toLocaleTimeString(locale.value, {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false
    })
  }
})

let intervalId: any = 0;

onUnmounted(() => {
  if (intervalId) {
    clearInterval(intervalId)
  }
});

onMounted(() => {
  intervalId = setInterval(() => {
    timeDiffRef.value = new Date().getTime() - appStore.localTime.getTime();
  }, 1000);
})

</script>

<template>
  <div class="sd-time-data-wrap" :class="[appStore.isDesktop?'':'mobile']">
    <div class="sd-time-data-sec">{{ currentTime.time }}</div>
    <div class="sd-time-data-day">{{ currentTime.day }}</div>
  </div>
</template>

<style scoped lang="scss">
.sd-time-data-wrap {
  width: fit-content;
  text-align: center;
}

.sd-time-data-sec {
  color: var(--text-color); // 修复：使用 CSS 变量
  font-size: 1.35rem;
  //font-weight: 500;
  line-height: 1;
  white-space: nowrap;

}

.sd-time-data-day {
  color: var(--text-color-secondary); // 修复：使用 CSS 变量
  font-size: 0.95rem;
  //font-weight: 400;
  line-height: 2.1rem;
  white-space: nowrap;
}

.mobile {
  .sd-time-data-sec {
    color: var(--text-color); // 修复：使用 CSS 变量
    font-size: 1.1rem;
    //font-weight: 500;
    line-height: 1;
    text-align: start;
    white-space: nowrap;

  }

  .sd-time-data-day {
    color: var(--text-color-secondary); // 修复：使用 CSS 变量
    font-size: 0.9rem;
    //font-weight: 400;
    line-height: 1.8rem;
    white-space: nowrap;
  }
}
</style>
