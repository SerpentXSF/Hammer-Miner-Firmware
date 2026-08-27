<script setup lang='ts'>
import {useRouter} from 'vue-router';
import {useI18n} from 'vue-i18n';
import {computed, h, onMounted, reactive} from 'vue';

import {HomeOutlined, MenuOutlined, SettingOutlined, UnorderedListOutlined, WifiOutlined,} from '@ant-design/icons-vue';
import TimeMachine from "@/components/TimeMachine.vue";
import {useAppStore} from "@/store";
import BottomToolBar from "@/components/BottomToolBar.vue";

const appStore = useAppStore();
const router = useRouter();
const {t} = useI18n();

const sml = (code: string): string => {
  return t(`sidebar.menu.${code}`);
}
const getMenus = () => {
  return [
    {
      key: 'dashboard',
      icon: () => h(HomeOutlined),
      label: sml('dashboard'),
      onclick: () => {
        router.push({path: '/dashboard'});
      },
    },
    // {
    //   key: 'swarm',
    //   icon: () => h(ShareAltOutlined),
    //   label: sml('swarm'),
    //   onclick: () => {
    //     router.push({path: '/swarm'});
    //   },
    // },
    {
      key: 'network',
      icon: () => h(WifiOutlined),
      label: sml('network'),
      onclick: () => {
        router.push({path: `/network`});
      },
    },
    {
      key: 'pool_settings',
      icon: () => h(UnorderedListOutlined),
      label: sml('pool_settings'),
      onclick: () => {
        router.push({path: `/pool_settings`});
      },
    },
    {
      key: 'settings',
      icon: () => h(SettingOutlined),
      label: sml('settings'),
      onclick: () => {
        router.push({path: `/settings`});
      },
    },
    {
      key: 'logs',
      icon: () => h(MenuOutlined),
      label: sml('logs'),
      onclick: () => {
        router.push({path: `/logs`});
      },
    },
  ];
};
const menus = computed(() => {
  return getMenus();
});

const state = reactive({
  collapsed: false,
  selectedKeys: [''],
  openKeys: [''],
  preOpenKeys: [''],
});


onMounted(() => {
  const path = window.location.hash.replace('#', '').replace('/', '');
  state.selectedKeys = [path];
});
</script>

<template>
  <div>
    <a-menu
        v-model:openKeys='state.openKeys'
        v-model:selectedKeys='state.selectedKeys'
        mode='inline'
        :inline-collapsed='state.collapsed'
        :items='menus'
        style='width: 100%;'
    ></a-menu> <!-- 修复：移除了硬编码的 theme='light' -->
    <time-machine class="sd-bottom-bar-wrap" v-if="appStore.isDesktop"></time-machine>
    <bottom-tool-bar class="sd-bottom-bar-wrap"  v-else></bottom-tool-bar>
  </div>
</template>

<style scoped lang='scss'>
:deep(.ant-menu-root) {
  border-inline-end: none !important;
}

:deep(.ant-menu-item, .ant-menu-submenu-title) {
  font-size: 14px;
  height: 48px !important;
  cursor: pointer;
}

:deep(.ant-menu-item-selected) {
  font-size: 14px;
  // background-color: rgba(26, 94, 255, 0.2) !important; // 修复：移除硬编码颜色
}

/* 当项在悬停状态下的背景色 */
:deep(.ant-menu-item-active:not(.ant-menu-item-selected)) {
  // background-color: rgba(26, 94, 255, 0.2) !important; // 修复：移除硬编码颜色
}

:deep(.ant-menu-submenu-title) {
  height: 48px !important;
}

:deep(.ant-menu-submenu-active > .ant-menu-submenu-title) {
  // background-color: rgba(26, 94, 255, 0.2) !important; // 修复：移除硬编码颜色
}

.layout-sidebar {
  padding-top: 45px !important;
  padding-left: 16px !important;
  padding-right: 16px !important;
  // background-color: #ffffff !important; // 修复：移除硬编码颜色 (由 App.vue 控制)
}

.sd-bottom-bar-wrap {
  position: absolute;
  bottom: 2.8rem;
  left: 1.4rem;
}


</style>
