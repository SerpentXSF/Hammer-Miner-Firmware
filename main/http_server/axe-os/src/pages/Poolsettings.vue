<script setup lang="ts">
import {onMounted, reactive, ref, watch, computed} from "vue";
import {MinerStatusData} from "@/api/type.ts";
import {getMinerStatus, restartMiner, updateSystem} from "@/api";
import {showNotification, showNotificationLoading, validData} from "@/util/utils.ts";
import {useAppStore} from "@/store";
import {useI18n} from "vue-i18n";
import {FormInstance} from "ant-design-vue";
import { POOL_MODELS } from "@/util/pools.ts";

const {t, locale} = useI18n();

const pl = (code: string): string => t(`pool.${code}`);
const dpl = (code: string): string => t(`dashboard.pool.${code}`); // 用于获取 Primary/Backup 翻译

const appStore = useAppStore();

interface ActionStatus {
  saving: boolean;
  restarting: boolean;
}

interface FormState {
  stratumURL: string;
  fallbackStratumURL: string;
  stratumUser: string;
  stratumPassword: string;
  fallbackStratumUser: string;
  fallbackStratumPassword: string;
  stratumTLS: number;
  stratumExtranonceSubscribe: number;
  fallbackStratumTLS: number;
  fallbackStratumExtranonceSubscribe: number;
}

const formState = reactive<FormState>({
  stratumURL: '',
  fallbackStratumURL: '',
  stratumUser: '',
  stratumPassword: '',
  fallbackStratumUser: '',
  fallbackStratumPassword: null,
  stratumTLS: 0,
  stratumExtranonceSubscribe: 0,
  fallbackStratumTLS: 0,
  fallbackStratumExtranonceSubscribe: 0,
});

const cfgsFormRef = ref<FormInstance>();
const minerStatusRef = ref<MinerStatusData>();
const actionStatus = reactive<ActionStatus>({saving: false, restarting: false});
const activeTab = ref<string>('primary'); // 控制 Tab 切换

// --- 推荐矿池逻辑 ---

const recommendedPools = computed(() => {
    const model = appStore.statusRaw?.DeviceModel || '';
    let type = 'LTC';
    if (model.startsWith('BC') || model.includes('SHA256') || model.includes('BM13')) {
        type = 'BTC';
    }
    return POOL_MODELS.filter(p => p.type === type);
});

const onPoolSelect = (value: string) => {
    if (activeTab.value === 'primary') {
        formState.stratumURL = value;
    } else {
        formState.fallbackStratumURL = value;
    }
};

// -------------------

watch(
    () => locale.value,
    () => {
      setTimeout(() => {
        cfgsFormRef.value?.validate();
      }, 100);
    },
);

const syncMinerStatus = async () => {
  try {
    const resData = await getMinerStatus('');
    minerStatusRef.value = validData(resData);
  } catch (e) {
    console.log(e);
  }
}

const updateSys = async (values: any) => {
  actionStatus.saving = true;
  try {
    const stratumPasswordStr = (values.stratumPassword === '*****' ? undefined : values.stratumPassword);
    
    // 解析 URL 和 Port
    const parseAddress = (addr: string) => {
        let url = addr.trim();
        let port = 3333; 

        // 移除协议前缀
        if (url.startsWith('stratum+ssl://')) {
            url = url.substring(14);
        } else if (url.startsWith('stratum+tcp://')) {
            url = url.substring(14);
        }

        const parts = url.split(':');
        if (parts.length > 1) {
            const p = parseInt(parts[parts.length - 1]);
            if (!isNaN(p)) {
                port = p;
                // 重组 URL (去除端口部分)
                url = parts.slice(0, parts.length - 1).join(':');
            }
        }
        return { url, port };
    };

    const primary = parseAddress(formState.stratumURL);
    const fallback = parseAddress(formState.fallbackStratumURL);

    const formData = {
      stratumURL: primary.url,
      stratumPort: primary.port,
      stratumUser: values.stratumUser,
      stratumPassword: stratumPasswordStr,
      stratumTLS: values.stratumTLS,
      stratumExtranonceSubscribe: values.stratumExtranonceSubscribe,

      fallbackStratumURL: fallback.url,
      fallbackStratumPort: fallback.port,
      fallbackStratumUser: values.fallbackStratumUser,
      fallbackStratumPassword: values.fallbackStratumPassword,
      fallbackStratumTLS: values.fallbackStratumTLS,
      fallbackStratumExtranonceSubscribe: values.fallbackStratumExtranonceSubscribe,
    }

    await updateSystem('', formData);
    showNotification(t('com.msg_save_success'), 'success')
    appStore.setInfo({ needsRestart: true });
  } catch (err) {
    showNotification(t('com.msg_save_failed'), 'error')
    appStore.setInfo({ needsRestart: false });
  } finally {
    actionStatus.saving = false;
  }
}

const restart = async () => {
  if(actionStatus.restarting) {
    return;
  }
  actionStatus.restarting = true;
  try {
    appStore.setInfo({ needsRestart: false });
    await restartMiner('');
    showNotificationLoading(t('com.msg_restarting_system'), 30);
    setTimeout(() => {
      actionStatus.restarting = false;
      window.location.reload();
    }, 30000);
  } catch (e) {
    actionStatus.restarting = false;
    showNotification(t('com.msg_restart_failed'), 'error');
  } finally {
  }
}

watch(
    () => minerStatusRef.value,
    (newValue, oldValue) => {
      if (newValue && !oldValue) {
        // Ready state check if needed
      }
    },
);

onMounted(async () => {
  await syncMinerStatus();
  if (minerStatusRef.value) {
    // 合并显示
    formState.stratumURL = `${minerStatusRef.value.stratumURL}:${minerStatusRef.value.stratumPort}`;
    formState.fallbackStratumURL = `${minerStatusRef.value.fallbackStratumURL}:${minerStatusRef.value.fallbackStratumPort}`;
    
    formState.stratumUser = minerStatusRef.value.stratumUser;
    formState.stratumPassword = '*****';
    formState.fallbackStratumUser = minerStatusRef.value.fallbackStratumUser;
    formState.fallbackStratumPassword = 'password';
    formState.stratumTLS = minerStatusRef.value.stratumTLS ?? 0;
    formState.stratumExtranonceSubscribe = minerStatusRef.value.stratumExtranonceSubscribe ?? 0;
    formState.fallbackStratumTLS = minerStatusRef.value.fallbackStratumTLS ?? 0;
    formState.fallbackStratumExtranonceSubscribe = minerStatusRef.value.fallbackStratumExtranonceSubscribe ?? 0;
  }
})
</script>

<template>
  <div>
    <a-card :title="pl('title')" class="card ps-card" style="border: 1px solid var(--surface-border); box-shadow: none;">
      <div class="ps-form-wrap">
      <a-form ref="cfgsFormRef" :wrapper-col="{xs:24, sm: 12}" :model="formState" :hideRequiredMark="true"
              @finish="updateSys">
        
        <a-tabs v-model:activeKey="activeTab" type="card">
          
          <a-tab-pane key="primary" :tab="dpl('primary')">
            <div class="tab-content">
              <!-- 推荐矿池下拉选择 -->
               <a-form-item label="Quick Select">
                  <a-select class="pool-quick-select" placeholder="Select a recommended solo pool" @change="onPoolSelect" style="width: 100%" dropdownClassName="pool-quick-select-dropdown">
                      <a-select-option v-for="pool in recommendedPools" :key="pool.id" :value="pool.value">
                          <div class="pool-option">
                              <img :src="pool.logo" class="pool-logo" />
                              <div class="pool-info">
                                  <div class="pool-name">{{ pool.label }}</div>
                                  <div class="pool-addr">{{ pool.value }}</div>
                              </div>
                          </div>
                      </a-select-option>
                  </a-select>
               </a-form-item>

              <a-form-item :label="pl('stratum_host')" name="stratumURL" :rules="[{required: true, message: t('com.rule_required')}]">
                <a-input v-model:value="formState.stratumURL" placeholder="host:port"></a-input>
              </a-form-item>
              
              <a-form-item :label="pl('stratum_user')" name="stratumUser"
                           :rules="[{required: true, message: t('com.rule_required')}]">
                <a-input v-model:value="formState.stratumUser"></a-input>
              </a-form-item>
              <a-form-item :label="pl('stratum_password')" name="stratumPassword"
                           :rules="[{required: true, message: t('com.rule_required')}]">
                <a-input-password v-model:value="formState.stratumPassword"></a-input-password>
              </a-form-item>

              <a-form-item v-if="appStore.currentModelConfig?.support_tls" name="stratumTLS" :colon="false" style="margin-bottom: 0;">
              <div class="ps-switch-row">
                <span class="ps-switch-label">{{ pl('stratum_tls') }}</span>
                <a-switch :checked="formState.stratumTLS === 1" @change="(checked: boolean) => formState.stratumTLS = checked ? 1 : 0" />
              </div>
              </a-form-item>
              <a-form-item v-if="appStore.currentModelConfig?.support_xnsub" name="stratumExtranonceSubscribe" :colon="false" style="margin-bottom: 0;">
              <div class="ps-switch-row">
                <span class="ps-switch-label">{{ pl('stratum_xnsub') }}</span>
                <a-switch :checked="formState.stratumExtranonceSubscribe === 1" @change="(checked: boolean) => formState.stratumExtranonceSubscribe = checked ? 1 : 0" />
              </div>
              </a-form-item>
            </div>
          </a-tab-pane>

          <a-tab-pane key="fallback" :tab="dpl('fallback')">
            <div class="tab-content">
               <!-- 推荐矿池下拉选择 (Fallback) -->
               <a-form-item label="Quick Select">
                  <a-select class="pool-quick-select" placeholder="Select a recommended solo pool" @change="onPoolSelect" style="width: 100%" dropdownClassName="pool-quick-select-dropdown">
                      <a-select-option v-for="pool in recommendedPools" :key="pool.id" :value="pool.value">
                          <div class="pool-option">
                              <img :src="pool.logo" class="pool-logo" />
                              <div class="pool-info">
                                  <div class="pool-name">{{ pool.label }}</div>
                                  <div class="pool-addr">{{ pool.value }}</div>
                              </div>
                          </div>
                      </a-select-option>
                  </a-select>
               </a-form-item>

              <a-form-item :label="pl('fallback_stratum_host')" name="fallbackStratumURL">
                <a-input v-model:value="formState.fallbackStratumURL" placeholder="host:port"></a-input>
              </a-form-item>
              
              <a-form-item :label="pl('fallback_stratum_user')" name="fallbackStratumUser">
                <a-input v-model:value="formState.fallbackStratumUser"></a-input>
              </a-form-item>
              <a-form-item :label="pl('fallback_stratum_password')" name="fallbackStratumPassword">
                <a-input-password v-model:value="formState.fallbackStratumPassword"></a-input-password>
              </a-form-item>

              <a-form-item v-if="appStore.currentModelConfig?.support_tls" name="fallbackStratumTLS" :colon="false" style="margin-bottom: 0;">
              <div class="ps-switch-row">
                <span class="ps-switch-label">{{ pl('stratum_tls') }}</span>
                <a-switch :checked="formState.fallbackStratumTLS === 1" @change="(checked: boolean) => formState.fallbackStratumTLS = checked ? 1 : 0" />
              </div>
              </a-form-item>
              <a-form-item v-if="appStore.currentModelConfig?.support_xnsub" name="fallbackStratumExtranonceSubscribe" :colon="false" style="margin-bottom: 0;">
              <div class="ps-switch-row">
                <span class="ps-switch-label">{{ pl('stratum_xnsub') }}</span>
                <a-switch :checked="formState.fallbackStratumExtranonceSubscribe === 1" @change="(checked: boolean) => formState.fallbackStratumExtranonceSubscribe = checked ? 1 : 0" />
              </div>
              </a-form-item>
            </div>
          </a-tab-pane>

        </a-tabs>

        <div class="ps-action">
          <a-button :loading="actionStatus.saving" class="ps-action-btn" type="primary" html-type="submit">
            {{ t('com.save') }}
          </a-button>
          <a-button :loading="actionStatus.restarting" class="ps-action-btn" :disabled="!appStore.needsRestart" type="primary"
                    @click="restart">{{ appStore.needsRestart ? t('com.restart_pending') : t('com.restart') }}
          </a-button>
          <div class="ps-restart-hint">{{ t('com.restart_hint') }}</div>
        </div>
      </a-form>
      </div>
    </a-card>
  </div>
</template>

<style scoped lang="scss">
.ps-card {
  margin-top: 2rem;
  margin-bottom: 2rem;
}

.ps-title {
  font-size: 1.5rem;
  font-weight: 500;
  margin-bottom: 0.5rem;
}

.ps-form-wrap {
  margin-top: 1rem;
}

.tab-content {
  padding-top: 20px;
}

:deep(.ant-form-item .ant-form-item-label >label) {
  color: var(--text-color);
  font-size: 1rem;
  font-weight: 500;
  text-align: left;
  width: 15rem;
}

:deep(.ant-input) {
  height: 35px;
}

:deep(.ant-input-number) {
  width: 100%;
  height: 35px;
  line-height: 35px;
}

:deep(.ant-input-affix-wrapper.ant-input-password ) {
  padding-top: 0 !important;
  padding-bottom: 0 !important;;
}

.ps-action {
  margin-top: 2rem;
  border-top: 1px solid var(--surface-border);
  padding-top: 1.5rem;
  margin-bottom: 1rem;
}

.ps-action-btn {
  margin-right: 10px;
  margin-bottom: 10px;
}

.ps-restart-hint {
  font-size: 0.9rem;
}

/* Tabs 样式修复：高亮选中项，适配暗黑模式 */
:deep(.ant-tabs-card > .ant-tabs-nav .ant-tabs-tab) {
  background-color: transparent;
  border-color: var(--surface-border);
  color: var(--text-color); 
  transition: all 0.3s;
}

:deep(.ant-tabs-card > .ant-tabs-nav .ant-tabs-tab:hover) {
  color: var(--ant-primary-color);
}

:deep(.ant-tabs-card > .ant-tabs-nav .ant-tabs-tab-active) {
  background-color: var(--ant-primary-color) !important;
  border-color: var(--ant-primary-color) !important;
  color: #fff !important; 
  font-weight: 500;
}

.pool-option {
    display: flex;
    align-items: center;
    padding: 4px 0;
}

.pool-logo {
    width: 24px;
    height: 24px;
    margin-right: 10px;
    border-radius: 4px; /* Optional rounded corners */
}

.pool-info {
    display: flex;
    flex-direction: column;
    justify-content: center;
    line-height: 1.2;
}

.pool-name {
    font-weight: 500;
    font-size: 14px;
    color: var(--text-color);
}

.pool-addr {
    font-size: 11px;
    color: var(--text-color-secondary);
}

:deep(.pool-quick-select .ant-select-selector) {
  height: auto !important;
  min-height: 50px;
  padding-top: 4px;
  padding-bottom: 4px;
  display: flex !important;
  align-items: center;
}

:deep(.pool-quick-select .ant-select-selection-item) {
  display: flex !important;
  align-items: center;
  line-height: normal !important;
}

.ps-switch-row {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 15px;
}

.ps-switch-label {
  font-size: 1rem;
  font-weight: 500;
  color: var(--text-color);
  min-width: 15rem;
}

.ps-switch-row :deep(.ant-switch-checked) {
  background-color: #34d0b6 !important;
  border-color: #34d0b6 !important;
}


</style>

<style lang="scss">
.pool-quick-select-dropdown {
  .ant-select-item {
    min-height: 50px !important;
    padding: 8px 12px !important;
    align-items: center;
    display: flex;
  }
  
  .ant-select-item-option-content {
    white-space: normal !important;
    height: auto !important;
    display: flex;
    align-items: center;
    width: 100%;
  }
}
</style>