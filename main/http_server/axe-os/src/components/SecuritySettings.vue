<script setup lang="ts">
import { reactive, onMounted, ref } from "vue";
import { useI18n } from "vue-i18n";
import { useAppStore } from "@/store";
import { updateSystem, getMinerStatus } from "@/api";
import { showNotification, getUrl, validData } from "@/util/utils.ts";
import { useRouter } from "vue-router";
import { LogoutOutlined } from "@ant-design/icons-vue";

const { t } = useI18n();
const appStore = useAppStore();
const router = useRouter();

const formState = reactive({
  apiPassword: '',
  confirmPassword: ''
});

// Reflects the device's actual state, reported by /api/system/info.
const authEnabled = ref(false);
const loading = ref(false);
const error = ref('');

const handleLogout = () => {
    appStore.logout();
    router.push('/login');
};

const updateSecurity = async () => {
    error.value = '';

    if (formState.apiPassword !== formState.confirmPassword) {
        error.value = 'The two passwords do not match.';
        return;
    }

    if (formState.apiPassword !== '' && formState.apiPassword.length < 8) {
        error.value = 'Use at least 8 characters.';
        return;
    }

    if (formState.apiPassword === '' &&
        !window.confirm('Clearing the password disables authentication. ' +
                        'Every endpoint on this miner, including pool settings ' +
                        'and firmware update, becomes reachable by any host on ' +
                        'your network without credentials.\n\nContinue?')) {
        return;
    }

    loading.value = true;
    try {
        await updateSystem(getUrl(window.location.host), { apiPassword: formState.apiPassword });
        showNotification(t('com.msg_save_success'), 'success');
        // The change revoked this session's token along with the rest.
        appStore.logout();
        router.push('/login');
    } catch (e) {
        showNotification(t('com.msg_save_failed'), 'error');
    } finally {
        loading.value = false;
    }
};

const syncStatus = async () => {
    try {
        const res = await getMinerStatus(getUrl(window.location.host));
        const data = validData(res);
        if (data) {
            authEnabled.value = !!data.authEnabled;
        }
    } catch(e) {
        console.error(e);
    }
}

onMounted(() => {
    syncStatus();
});
</script>

<template>
  <div class="card security-card">
    <div class="security-header">
        <div class="security-title">Security Settings</div>
        <a-button type="primary" danger @click="handleLogout" class="logout-btn">
            <template #icon><LogoutOutlined /></template>
            Logout
        </a-button>
    </div>
    
    <a-alert
      v-if="!authEnabled"
      type="warning"
      show-icon
      class="auth-warning"
      message="This miner has no password"
      description="Every endpoint, including pool settings and firmware update, is reachable by any host on your network without credentials. Set a password below."
    />

    <a-form :model="formState" layout="vertical" @finish="updateSecurity">
      <a-form-item label="API password">
         <a-input-password v-model:value="formState.apiPassword"
                           placeholder="At least 8 characters; empty disables authentication" />
      </a-form-item>

      <a-form-item label="Confirm password">
         <a-input-password v-model:value="formState.confirmPassword"
                           placeholder="Repeat the password" />
      </a-form-item>

      <div v-if="error" class="error-message">{{ error }}</div>

      <a-button type="primary" html-type="submit" :loading="loading">
        Save Security Settings
      </a-button>

      <p class="auth-note">
        Saving signs you out on every device, since existing sessions were
        issued against the old password.
      </p>
    </a-form>

  </div>
</template>

<style scoped lang="scss">
.auth-warning {
    margin-bottom: 1.5rem;
}

.error-message {
    color: var(--ant-error-color, #ff4d4f);
    margin-bottom: 1rem;
}

.auth-note {
    margin-top: 1rem;
    font-size: 0.85rem;
    opacity: 0.75;
}

.security-card {
    padding: 2.2rem 1.2rem; 
    margin-top: 2rem;
}

.security-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 1.5rem;
}

.security-title {
    font-size: 1.5rem;
    font-weight: 500;
    color: var(--text-color);
    margin-bottom: 0;
}

.logout-btn {
    font-size: 1rem;
    display: flex;
    align-items: center;
}

:deep(.auth-switch.ant-switch-checked) {
    background-color: var(--ant-primary-color) !important; 
}
</style>
