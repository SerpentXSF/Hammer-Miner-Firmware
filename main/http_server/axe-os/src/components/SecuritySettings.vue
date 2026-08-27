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
  auth_enable: false,
  web_password: '',
  web_username: ''
});

const loading = ref(false);

const handleLogout = () => {
    appStore.logout();
    router.push('/login');
};

const updateSecurity = async () => {
    loading.value = true;
    try {
        await updateSystem(getUrl(window.location.host), { ...formState });
        showNotification(t('com.msg_save_success'), 'success');
        appStore.setInfo({ needsRestart: true });
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
            formState.auth_enable = !!data.auth_enable;
            if (data.web_username) {
                 formState.web_username = data.web_username;
            }
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
    
    <a-form :model="formState" layout="vertical" @finish="updateSecurity">
      <a-form-item label="Enable Authentication">
         <a-switch v-model:checked="formState.auth_enable" class="auth-switch" />
      </a-form-item>
      
      <div v-if="formState.auth_enable">
          <a-form-item label="Username">
             <a-input v-model:value="formState.web_username" placeholder="Leave empty to keep current" />
          </a-form-item>
          
          <a-form-item label="New Password">
             <a-input-password v-model:value="formState.web_password" placeholder="Leave empty to keep current" />
          </a-form-item>
      </div>

      <a-button type="primary" html-type="submit" :loading="loading">
        Save Security Settings
      </a-button>
    </a-form>
  </div>
</template>

<style scoped lang="scss">
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
