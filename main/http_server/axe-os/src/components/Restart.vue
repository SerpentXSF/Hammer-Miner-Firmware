<script setup lang="ts">
import {createVNode, ref} from "vue";
import {restartMiner} from "@/api";
import {showNotification, showNotificationLoading} from "@/util/utils.ts";
import {Modal} from "ant-design-vue";
import {ExclamationCircleOutlined, PoweroffOutlined} from "@ant-design/icons-vue";
import {useI18n} from "vue-i18n";
import {useAppStore} from "@/store";

const {t} = useI18n();
const restarting = ref<boolean>(false);
const appStore = useAppStore();

const restart = async () => {
  if (restarting.value) {
    return;
  }
  restarting.value = true;
  try {
    appStore.setInfo({ needsRestart: false });
    await restartMiner('')
    showNotificationLoading(t('com.msg_restarting_system'), 30);
    setTimeout(() => {
      restarting.value = false;
      window.location.reload();
    }, 30000);
  } catch (e) {
    restarting.value = false;
    showNotification(t('com.msg_restart_failed'), 'error');
  } finally {

  }
}

const restartConfirm = () => {
  Modal.confirm({
    title: t('com.confirm_restart_title'),
    icon: createVNode(ExclamationCircleOutlined),
    content: t('com.confirm_restart_content'),
    okText: t('com.confirm'),
    cancelText: t('com.cancel'),
    async onOk() {
      await restart();
    }
  });
};
</script>

<template>
  <a-button :loading='restarting' @click.stop='restartConfirm' class='tb-restart-btn' type='primary'>
    <PoweroffOutlined class='tb-restart-icon'/>
    {{ appStore.needsRestart ? t('com.restart_pending') : t('com.restart') }}
  </a-button>
</template>

<style scoped lang="scss">
.tb-restart-icon {
  font-size: 1rem;
}

.tb-restart-content {
  position: relative;
  top: -1px;
}

.tb-restart-btn {
  height: 2.4rem;
  vertical-align: middle;
  line-height: 1rem;
  padding: 0 0.8rem;
}
</style>