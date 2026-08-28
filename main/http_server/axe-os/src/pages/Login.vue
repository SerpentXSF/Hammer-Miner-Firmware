<template>
  <div class="login-container">
    <div class="login-card">
      <div class="logo-container">
        <img src="/icon/logo.svg" alt="SerpentX" class="logo" />
        <div class="tagline">Stay Open</div>
      </div>
      
      <div class="form-container">
        <a-input-password
          v-model:value="password"
          placeholder="Password"
          size="large"
          @keyup.enter="handleLogin"
        >
          <template #prefix>
            <LockOutlined />
          </template>
        </a-input-password>
        
        <div v-if="error" class="error-message">
          {{ error }}
        </div>

        <a-button 
          type="primary" 
          block 
          size="large" 
          :loading="loading"
          @click="handleLogin"
          class="login-button"
        >
          Login
        </a-button>
      </div>
      
      <div class="footer-container">
         <div class="footer-items">
            <ThemeSwitcher />
            <Lang />
            <Restart />
         </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue';
import { useRouter, useRoute } from 'vue-router';
import { useAppStore } from '@/store/modules/app';
import { LockOutlined } from '@ant-design/icons-vue';
import ThemeSwitcher from "@/components/ThemeSwitcher.vue";
import Lang from "@/components/Lang.vue";
import Restart from "@/components/Restart.vue";

const password = ref('');
const loading = ref(false);
const error = ref('');
const router = useRouter();
const route = useRoute();
const appStore = useAppStore();

const handleLogin = async () => {
  if (!password.value) {
    error.value = 'Please enter the device password';
    return;
  }
  
  loading.value = true;
  error.value = '';
  
  try {
    const success = await appStore.login(password.value);
    if (success) {
      const redirect = route.query.redirect as string || '/dashboard';
      router.push(redirect);
    } else {
      error.value = 'Invalid Credentials';
    }
  } catch (e) {
    error.value = 'Login failed';
  } finally {
    loading.value = false;
  }
};
</script>

<style scoped lang="scss">
.login-container {
  display: flex;
  justify-content: center;
  align-items: center;
  min-height: 100vh;
  background-color: var(--surface-ground);
  color: var(--text-color);
}

.login-card {
  width: 100%;
  max-width: 400px;
  padding: 2rem;
  background-color: var(--surface-card);
  border: 1px solid var(--surface-border);
  border-radius: var(--card-border-radius, 12px);
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06);
  display: flex;
  flex-direction: column;
  gap: 2rem;
}

.logo-container {
  text-align: center;
  
  .logo {
    width: auto;
    height: 48px; 
    margin-bottom: 0.5rem;
  }

  .tagline {
    font-size: 0.8rem;
    letter-spacing: 0.18em;
    text-transform: uppercase;
    opacity: 0.65;
    margin-bottom: 0.75rem;
  }
}

.form-container {
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.error-message {
  color: #ef4444;
  font-size: 0.875rem;
  text-align: center;
}

.footer-container {
  display: flex;
  justify-content: center;
  margin-top: 1rem;
  border-top: 1px solid var(--surface-border);
  padding-top: 1.5rem;
}

.footer-items {
  display: flex;
  gap: 1.5rem;
  align-items: center;
}
</style>
