/*
 * Theme table.
 *
 * Kept out of ThemeSwitcher.vue so main.ts can restore the stored choice
 * before the app paints, rather than flashing the default first.
 *
 * The firmware exposes no /api/theme handler, so the selection is held in
 * localStorage.
 */

export interface MinerTheme {
  theme: string;
  colorScheme: "light" | "dark";
  labelKey: string;
  /* dot shown beside the name in the menu */
  swatch: string;
  accentColors: Record<string, string>;
}

const STORAGE_KEY = "serpentx.theme";

/* rgb triple for the focus ring and chart wash, which need alpha */
const rgb = (hex: string): string => {
  const h = hex.replace("#", "");
  return [0, 2, 4].map((i) => parseInt(h.slice(i, i + 2), 16)).join(", ");
};

/*
 * Every dark theme shares one surface palette and differs only in accent, so
 * a new colour is one line rather than a copied block that drifts.
 */
const darkTheme = (
  name: string,
  labelKey: string,
  accent: string,
  hover: string,
  active: string
): MinerTheme => ({
  theme: name,
  colorScheme: "dark",
  labelKey,
  swatch: accent,
  accentColors: {
    "--surface-ground": "#050910",
    "--surface-overlay": "#05070d",
    "--surface-card": "#050b16",
    "--surface-border": "#1f2937",
    "--text-color": "#f5f7fb",
    "--text-color-secondary": "#9ca3af",

    "--ant-primary-color": accent,
    "--ant-primary-color-hover": hover,
    "--ant-primary-color-active": active,
    "--ant-primary-color-text": "#020617",

    "--card-border-radius": "14px",
    "--button-border-radius": "999px",
    "--input-border-radius": "8px",

    "--primary-color": accent,
    "--primary-color-text": "#020617",
    "--highlight-bg": accent,
    "--highlight-text-color": "#020617",
    "--focus-ring": `0 0 0 0.2rem rgba(${rgb(accent)}, 0.2)`,

    "--ant-menu-item-selected-bg": "linear-gradient(90deg, #111827, #0f172a)",

    "--pill-bg": "radial-gradient(circle at top left, #1d2633, #050910)",
    "--pill-border": "#1f2937",
    "--pill-text": "#f5f7fb",
    "--pill-shadow": "none",

    "--chart-bg": `radial-gradient(circle at top, rgba(${rgb(accent)}, 0.2), transparent 55%), linear-gradient(180deg, #050910, #050910)`,
  },
});

const lightTheme: MinerTheme = {
  theme: "light-blue",
  colorScheme: "light",
  labelKey: "light_theme_default",
  swatch: "#318EFF",
  accentColors: {
    "--surface-ground": "#F2F2F2",
    "--surface-overlay": "#FFFFFF",
    "--surface-card": "#FFFFFF",
    "--surface-border": "#E9E9E9",
    "--text-color": "#000000",
    "--text-color-secondary": "#666666",

    "--ant-primary-color": "#318EFF",
    "--ant-primary-color-hover": "#58a5ff",
    "--ant-primary-color-active": "#2272d9",
    "--ant-primary-color-text": "#ffffff",

    "--card-border-radius": "14px",
    "--button-border-radius": "999px",
    "--input-border-radius": "8px",

    "--primary-color": "#318EFF",
    "--primary-color-text": "#ffffff",
    "--highlight-bg": "#318EFF",
    "--highlight-text-color": "#ffffff",
    "--focus-ring": "0 0 0 0.2rem rgba(49, 142, 255, 0.2)",

    "--pill-bg": "#ffffff",
    "--pill-border": "#e5e7eb",
    "--pill-text": "#1f2937",
    "--pill-shadow":
      "0 1px 3px 0 rgba(0, 0, 0, 0.1), 0 1px 2px -1px rgba(0, 0, 0, 0.1)",

    "--chart-bg": "#ffffff",
  },
};

export const THEMES: MinerTheme[] = [
  darkTheme("dark-green", "theme_green", "#19e1a5", "#2fedb6", "#14b889"),
  darkTheme("dark-blue", "theme_blue", "#3b9dff", "#61b2ff", "#2179d6"),
  darkTheme("dark-red", "theme_red", "#ff4d5e", "#ff7280", "#d63a49"),
  darkTheme("dark-yellow", "theme_yellow", "#ffb81c", "#ffc94f", "#d69512"),
  lightTheme,
];

export const applyTheme = (theme: MinerTheme): void => {
  const root = document.documentElement;
  root.setAttribute(
    "style",
    `color-scheme: ${theme.colorScheme}; --theme: ${theme.theme};`
  );
  for (const [key, value] of Object.entries(theme.accentColors)) {
    root.style.setProperty(key, value);
  }
};

export const persistTheme = (theme: MinerTheme): void => {
  try {
    localStorage.setItem(STORAGE_KEY, theme.theme);
  } catch {
    /* private browsing, or storage disabled -- the theme still applied */
  }
};

/*
 * Restore the stored choice, falling back to the first theme. Any failure
 * here must not stop the app booting, so it is deliberately total.
 */
export const restoreTheme = (): void => {
  let stored: string | null = null;
  try {
    stored = localStorage.getItem(STORAGE_KEY);
  } catch {
    stored = null;
  }
  const theme = THEMES.find((t) => t.theme === stored) ?? THEMES[0];
  applyTheme(theme);
};
