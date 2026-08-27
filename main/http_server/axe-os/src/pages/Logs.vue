<script setup lang="ts">
import {onMounted, onUnmounted, ref, watch} from "vue";
import {getMinerStatus} from "@/api";
import {MinerStatusData} from "@/api/type.ts";
import {dateAgo, validData} from "../util/utils.ts";
import {useAppStore} from "@/store";

const appStore = useAppStore();

import {useI18n} from "vue-i18n";
import KernelLog from "@/components/KernelLog.vue";

const {t} = useI18n();

const ll = (code: string): string => {
  return t(`logs.${code}`);
}
const statusRaw = ref<MinerStatusData>();

let refreshIntervalRef: any = 0;

const syncMinerStatus = async () => {
  try {
    const resData = await getMinerStatus('');
    statusRaw.value = validData(resData);
  } catch (e) {
    console.log(e);
  }
}

onUnmounted(() => {
  clearInterval(refreshIntervalRef);
});

// 修复：移除 emit
// const emit = defineEmits(["status"]);
watch(
    () => statusRaw.value,
    (newValue, oldValue) => {
      if (newValue && !oldValue) {
        // 修复：移除 emit
        // emit('status', {ready: true});
      }
    },
);

onMounted(async () => {
  // 修复：移除 emit
  // emit('status', {ready: false});
  await syncMinerStatus();
  refreshIntervalRef = setInterval(async () => {
    await syncMinerStatus();
  }, 15000);

});
</script>

<template>
  <a-card :title="ll('overview')" class="card logs-card logs-overview" style="border: 1px solid var(--surface-border); box-shadow: none;">
    <div class="logs-overview-inner">
      <a-row>
        <a-col :xs="24" :md="24" :lg="9">
          <table class="logs-overview-table">
            <tr>
              <td class="logs-overview-label">{{ ll('model') }}:</td>
              <td class="logs-overview-data">{{ statusRaw?.DeviceModel }}</td>
            </tr>
            <tr>
              <td class="logs-overview-label">{{ ll('uptime') }}:</td>
              <td class="logs-overview-data">{{ statusRaw ? dateAgo(statusRaw.uptimeSeconds) : '' }}</td>
            </tr>
            <tr>
              <td class="logs-overview-label">{{ ll('wifi_status') }}:</td>
              <td class="logs-overview-data">{{ statusRaw?.wifiStatus }}</td>
            </tr>
            <tr v-if="appStore.isDebugMode">
              <td class="logs-overview-label">{{ ll('esp_idf_version') }}:</td>
              <td class="logs-overview-data">{{ statusRaw?.idfVersion }}</td>
            </tr>
          </table>
        </a-col>
        <a-col :xs="24" :md="24" :lg="12">
          <table class="logs-overview-table">
            <tr>
              <td class="logs-overview-label">{{ ll('mac_addr') }}:</td>
              <td class="logs-overview-data">{{ statusRaw?.macAddr }}</td>
            </tr>
            <tr>
              <td class="logs-overview-label">{{ ll('version') }}:</td>
              <td class="logs-overview-data">{{ statusRaw?.version}}</td>
            </tr>
            <tr>
              <td class="logs-overview-label">{{ ll('bord_version') }}:</td>
              <td class="logs-overview-data">{{ statusRaw?.boardVersion }}</td>
            </tr>
            <tr v-if="appStore.isDebugMode">
              <td class="logs-overview-label">{{ ll('free_heap_mem') }}:</td>
              <td class="logs-overview-data">{{ statusRaw?.freeHeap }}</td>
            </tr>
          </table>
        </a-col>
      </a-row>
    </div>

  </a-card>

  <a-card :title="ll('kernel_log')" class="card logs-card" style="border: 1px solid var(--surface-border); box-shadow: none;">
    <kernel-log></kernel-log>
  </a-card>
</template>

<style scoped lang="scss">

.logs-card {
  margin-top: 2rem;
  margin-bottom: 2rem;
}

.logs-overview {
  user-select: none;
}

.logs-overview-title {
  font-size: 1.4rem;
  font-weight: 500;
  color: var(--text-color); // 修复：使用 CSS 变量
  margin-bottom: 1.8rem;
}

.logs-overview-inner {
  padding: 1.4rem 1.2rem;
  border-radius: 0.3rem;
  border: 1px solid var(--surface-border); // 修复：使用 CSS 变量
  background-color: var(--surface-ground); // 修复：使用 CSS 变量
}

.logs-overview-label {
  width: 8rem;
  color: var(--text-color-secondary); // 修复：使用 CSS 变量
}

.logs-overview-data {
  font-size: 1rem;
  font-weight: 500;
  color: var(--text-color); // 修复：使用 CSS 变量
}

.logs-overview-table td {
  height: 50px;
}

.logs-showlogs-label {
  font-size: 1.4rem;
  font-weight: 500;
}

.logs-action-btn {
  padding: 0.3rem 1.4rem;
}


</style>

