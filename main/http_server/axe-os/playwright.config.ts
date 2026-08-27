import { defineConfig, devices } from '@playwright/test';

export default defineConfig({
  testDir: './tests',
  fullyParallel: false, // 取消并行
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 2 : 0,
  workers: 1, // 强制单 worker
  reporter: [['html', { open: 'never' }]],
  use: {
    baseURL: 'http://localhost:5173',
    trace: 'on-first-retry',
    headless: false, // 显示浏览器
  },
  projects: [
    { name: 'DC02-Desktop', use: { ...devices['Desktop Chrome'], contextOptions: { extraHTTPHeaders: { 'x-mock-model': 'DC02' } } } },
    { name: 'DC02-Mobile', use: { ...devices['Pixel 5'], contextOptions: { extraHTTPHeaders: { 'x-mock-model': 'DC02' } } } },
    { name: 'BC04-Desktop', use: { ...devices['Desktop Chrome'], contextOptions: { extraHTTPHeaders: { 'x-mock-model': 'BC04' } } } },
    { name: 'BC04-Mobile', use: { ...devices['Pixel 5'], contextOptions: { extraHTTPHeaders: { 'x-mock-model': 'BC04' } } } },
  ],
});
