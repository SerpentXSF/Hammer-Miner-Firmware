<script setup lang="ts">
import {useAppStore} from "@/store";
import {ref} from "vue";
import {syncUTCTime} from "@/api";
import {showNotification, getCurrentTime} from "@/util/utils.ts";
import {useI18n} from "vue-i18n";
import {RET_STATUS} from "@/util/const.ts";

const {t} = useI18n();

const appStore = useAppStore();
const syncing = ref<boolean>(false);

const syncTimeInner = async () => {
  if (syncing.value) {
    return;
  }
  syncing.value = true;

  try {
    const resData = await syncUTCTime();
    if (resData == RET_STATUS.OK) {
      setTimeout(async () => {
        const currentTime = await getCurrentTime();
        syncing.value = false;
        if (currentTime) {
          appStore.setInfo({currentTime: currentTime, localTime: new Date()});
          showNotification(t('tool.msg_sync_time_success'), 'success');
        } else {
          showNotification(t('tool.msg_sync_time_failed'), 'error');
        }
      }, 15000);
    } else {
      showNotification(t('tool.msg_sync_time_failed'), 'error');
      syncing.value = false;
    }
  } catch (e) {
    showNotification(t('tool.msg_sync_time_failed'), 'error');
    syncing.value = false;
    console.log(e);
  }
}

defineExpose({
  syncTimeInner,
});

</script>

<template>
  <div class="sd-time-clock-wrap" :class="[!appStore.isDesktop? 'mobile': '']" @click.stop="syncTimeInner">
    <div class="sd-time-clock-refresh" :class="syncing? 'sd-time-clock-syncing':''"></div>
    <div class="sd-time-clock-panel">
      <div class="sd-pointer-1"></div>
      <div class="sd-pointer-2"></div>
    </div>
  </div>
</template>

<style scoped lang="scss">
.sd-time-clock-wrap {
  position: relative;
  display: flex;
  justify-content: center;
  align-items: center;
  width: 24px;
  height: 20.5px;
  padding: 0;

  &:hover {
    opacity: 0.8;
    cursor: pointer;
  }
}

.sd-time-clock-refresh {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: url("/public/icon/refresh.svg") no-repeat top left / 100% 100%;
  z-index: 1;
}

.sd-time-clock-syncing {
  animation: sd-spin 2s linear infinite;
}

.sd-time-clock-panel {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  display: flex;
  justify-content: center;
  align-items: center;
  position: relative;
  width: 13.71px;
  height: 13.71px;
  border-radius: 50%;
  background: url("/public/icon/panel-bg.svg") no-repeat top left / 100% 100%;
  z-index: 2;
}

.sd-pointer-1 {
  position: absolute;
  width: 1.92px;
  height: 6.5px;
  background-color: #1B5FFF;
  border-radius: 1px;
  left: 5.5px;
  top: 1.5px;

}

.sd-pointer-2 {
  position: absolute;
  width: 1.92px;
  height: 5px;
  background-color: #1B5FFF;
  border-radius: 2px;
  transform-origin: left bottom;
  transform: rotateZ(135deg);
  left: 6.5px;
  top: 1.2px;
}

@keyframes sd-spin {
  from {
    transform: rotate(0deg);
  }
  to {
    transform: rotate(360deg);
  }
}

.mobile {
  .sd-time-clock-panel {
    background-image: url("/public/icon/panel-bg-m.svg");
  }

  .sd-time-clock-refresh {
    background-image: url("/public/icon/refresh-m.svg");
  }

  .sd-pointer-1, .sd-pointer-2 {
    background-color: #333333;
  }
}


</style>
