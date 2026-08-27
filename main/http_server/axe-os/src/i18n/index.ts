import { createI18n } from "vue-i18n";
import en from "./en";
import zh from "./zh";

const messages = {
  en,
  zh,
};

const i18n = createI18n({
  legacy: false,
  messages,
  locale: localStorage.getItem("lang") || "en",
  globalInjection: true,
});

export const LANG_LIST = {
    en : 'English',
    zh : '中文'
};

export const langChange = (lang: any) => {
  localStorage.setItem("lang", lang);
  i18n.global.locale.value = lang;
};

export default i18n;

