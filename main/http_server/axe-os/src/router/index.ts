import { createRouter, createWebHashHistory, RouteRecordRaw } from 'vue-router'
import { useAppStore } from '@/store/modules/app';

const routes: Array<RouteRecordRaw> = [
    {
        path: "/dashboard",
        component: () => import("@/pages/Dashboard.vue"),
    },
    {
        path: "/network",
        component: () => import("@/pages/Network.vue"),
    },
    {
        path: "/pool_settings",
        component: () => import("@/pages/Poolsettings.vue"),
    },
    {
        path: "/settings",
        component: () => import("@/pages/Settings.vue"),
    },
    {
        path: "/logs",
        component: () => import("@/pages/Logs.vue"),
    },
    {
        path: '/',
        component: () => import("@/pages/Dashboard.vue"),
    },
    {
        path: '/login',
        component: () => import("@/pages/Login.vue"),
        meta: { hideLayout: true }
    }
]

const router = createRouter({
    history: createWebHashHistory(import.meta.env.BASE_URL),
    routes
})

router.beforeEach(async (to, _, next) => {
    const appStore = useAppStore();

    // 1. Ensure system status is loaded
    if (!appStore.isDataLoaded) {
        try {
            const success = await appStore.updateState();
            if (!success) {
                // If we failed to get status, check if the current model config even supports login.
                // By default `support_login` is false. We only redirect to login if it's explicitly supported
                // or if there's evidence we need auth (e.g. 401 cleared the token, though hard to detect strictly here).
                // Let's rely on the config. If it doesn't support login, DO NOT redirect.
                if (appStore.currentModelConfig?.support_login && to.path !== '/login') {
                    next('/login');
                    return;
                }
            }
        } catch (e) {
            console.error("Failed to load system status", e);
            if (appStore.currentModelConfig?.support_login && to.path !== '/login') {
                next('/login');
                return;
            }
        }
    }

    // 2. Auth Check: If device doesn't support login, bypass auth
    const isAuthEnabled = appStore.currentModelConfig?.support_login ? appStore.minerStatus?.auth_enable : false;
    const isAuthenticated = appStore.isAuthenticated;

    if (to.path === '/login') {
        if (isAuthEnabled && isAuthenticated) {
            next('/dashboard'); // Already logged in
        } else {
            next(); // Allow access to login
        }
        return;
    }

    if (isAuthEnabled && !isAuthenticated) {
        next(`/login?redirect=${to.fullPath}`);
        return;
    }

    next();
});

export default router
