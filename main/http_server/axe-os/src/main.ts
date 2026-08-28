import { createApp } from 'vue'
// 移除全量引入
// import Antd from "ant-design-vue"; 
import "ant-design-vue/dist/reset.css";
import router from "./router";
import piniaStore from "./store";
import i18n from "@/i18n";

import App from "./pages/App.vue";
import "./styles/layout/layout.scss";
import { restoreTheme } from "@/util/theme";

/* apply the stored theme before first paint, not after */
restoreTheme();

const app = createApp(App);

// 移除全量注册，改用插件自动按需加载
// app.use(Antd);
app.use(router);
app.use(piniaStore);
app.use(i18n);
app.mount("#app");
