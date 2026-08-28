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

    /*
     * Whether this miner wants a login is answered by the miner, not by a
     * per-model table in the UI.
     *
     * This used to read `appStore.minerStatus?.auth_enable`, gated behind
     * `support_login`. The firmware reports `authEnabled`, and support_login is
     * false for every model here -- so the flag was always undefined, the guard
     * always fell through, and a miner with a password set showed an empty
     * dashboard with no way to reach the login page. Both halves are fixed:
     * the field name matches what the firmware sends, and the static gate is
     * gone.
     */

    // 1. Ensure system status is loaded
    if (!appStore.isDataLoaded) {
        try {
            await appStore.updateState();
        } catch (e) {
            console.error("Failed to load system status", e);
        }
    }

    const status: any = appStore.minerStatus;
    // authRequired is set when the status call came back 401, which is the only
    // signal available before we hold a token.
    const isAuthEnabled = status ? !!status.authEnabled : !!appStore.authRequired;
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
