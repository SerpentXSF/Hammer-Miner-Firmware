<script setup lang="ts">

import TimeSync from "@/components/TimeSync.vue";
import TimeDay from "@/components/TimeDay.vue";
import {useI18n} from "vue-i18n";
import {ref} from "vue";

const {t} = useI18n();

const timeSyncRef = ref(null);

const syncTimeInner = ()=> {
  timeSyncRef.value.syncTimeInner();
}
</script>

<template>
  <div class="sd-time-wrap" @click="syncTimeInner">
    <div class="sd-time-sync-wrap">
      <time-sync ref="timeSyncRef"></time-sync>
      <div class="sd-time-sync-hint">{{t('tool.sync_time')}}</div>
    </div>
    <time-day></time-day>
  </div>
</template>

<style scoped lang="scss">

.sd-time-wrap {
  width: 14rem;
  height: 5.5rem;
  background: linear-gradient(90deg, rgba(195, 212, 255, 0.65) 0%, rgba(246, 249, 255, 0.65) 100%);
  border-left: 4px solid #1A5EFFD1;
  padding: 5px 30px 0 20px;
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.sd-time-sync-wrap {
  width: fit-content;
  display: flex;
  flex-direction: column;
  align-items: center;
  cursor: pointer;
}

.sd-time-sync-hint {
  color: #1B5FFF;
  font-size: 0.9rem;
  text-align: center;
  line-height: 2rem;
  white-space: nowrap;
}

</style>
