import { test, expect } from '@playwright/test';
import fs from 'fs';
import os from 'os';
import path from 'path';

type Model = 'DC02' | 'BC04';

function modelFromProjectName(name: string): Model {
  return name.includes('BC04') ? 'BC04' : 'DC02';
}

function makeMinerStatus(
  model: Model,
  opts?: { valid?: boolean }
) {
  const valid = opts?.valid ?? true;
  const isBc04 = model === 'BC04';

  // SettingsEdit 表单校验单位：coreVoltage 使用 cV（= V * 100）
  const coreVoltage = isBc04 ? 490 : 127;
  const frequency = isBc04 ? 750 : 2300;

  const base: any = {
    DeviceModel: model,
    version: '1.3.6 20260325',
    frequency,
    coreVoltage,

    // maintainDataset needs these
    temp: 50,
    temp1: 45,
    power: isBc04 ? 121.67 : 50,

    // validData needs hashRate in root
    hashRate: valid ? 0.236 : undefined,

    voltage: 12208.75,
    current: 2237.5,
    coreVoltageActual: coreVoltage * 10, // Dashboard uses /1000
    coreVoltageActual1: coreVoltage * 10,
    maxPower: 25,
    nominalVoltage: 12000,
    vrTemp: 45,

    apEnabled: 0,
    wifiStatus: 'Connected!',
    wifiRSSI: -55,

    // SettingsEdit needs these
    autofanspeed: 1,
    fanspeed: 80,
    flipscreen: 1,
    invertfanpolarity: 1,
    invertscreen: 1,
    overheat_mode: 0,
    boot_mode: 0,
    ntpServerBackup: 'time.windows.com',
    ntpServer: 'pool.ntp.org',

    boardtemp1: 30,
    boardtemp2: 35,
    power_fault: '',

    // misc fields used across the UI
    freeHeap: 2000,
    hostname: 'Hammer',
    macAddr: '2C:54:91:88:C9:E3',
    ssid: 'default',
    wifiPass: 'password',
    isStaticIP: '0',
    staticIP: '192.168.1.17',
    subnetMask: '255.255.255.0',
    gateway: '192.168.1.1',
    dns: '1.1.1.1',

    sharesAccepted: 3,
    sharesRejected: 0,
    sharesRejectedReasons: [],
    uptimeSeconds: 120,
    asicCount: 1,
    smallCoreCount: 672,
    ASICModel: 'BM1366',
    stratumURL: 'public-pool.io',
    stratumPort: 21496,
    fallbackStratumURL: 'test.public-pool.io',
    fallbackStratumPort: 21497,
    isUsingFallbackStratum: false,
    stratumUser: 'bc1q99n3pu025yyu0jlywpmwzalyhm36tg5u37w20d.bitaxe-U1',
    fallbackStratumUser: 'bc1q99n3pu025yyu0jlywpmwzalyhm36tg5u37w20d.bitaxe-U1',

    fanrpm: 1000,
    fanrpm1: 2000,

    auth_enable: false,
    bestDiff: '2',
    bestSessionDiff: '1',
    hwRate: '5',
    hwNumber: '100',
    nonceNumber: '2000',
    idfVersion: 'v5.1.2',
    boardVersion: '204',
    currentTime: '2025-08-09 01:59:52',
    autotuneActive: false,
    chipData: undefined,
  };

  if (!valid) {
    delete base.hashRate;
  }

  return base;
}

function createBinWithFirstByte(firstByte: number, size = 32): string {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'axe-os-test-'));
  const filePath = path.join(dir, 'fixture.bin');
  const buf = Buffer.alloc(size, 0);
  buf[0] = firstByte;
  fs.writeFileSync(filePath, buf);
  return filePath;
}

test.describe('Settings page', () => {
  let webCalls = 0;
  let fwCalls = 0;

  test.beforeEach(async ({ page }, testInfo) => {
    // Force i18n to English so button labels are deterministic.
    await page.addInitScript(() => {
      localStorage.setItem('lang', 'en');
    });

    const model = modelFromProjectName(testInfo.project.name);

    webCalls = 0;
    fwCalls = 0;

    // System info mock (initial load + worker polling + SettingsEdit sync).
    await page.route('**/api/system/info*', async (route) => {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify(makeMinerStatus(model, { valid: true })),
      });
    });

    // Mock GitHub releases so Settings.vue remote OTA doesn't rely on external network.
    // We intentionally delay responses so the UI stays in "Checking..." state long enough
    // for click/locking tests to be meaningful.
    const webUrlPattern = 'https://api.github.com/repos/Hammer-Miner/www/releases/latest*';
    const fwUrlPattern = `https://api.github.com/repos/Hammer-Miner/${model}/releases/latest*`;

    await page.route(webUrlPattern, async (route) => {
      webCalls++;
      await new Promise((r) => setTimeout(r, 1200));
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          tag_name: 'v1.3.7',
          html_url: 'https://example.com/web-release',
          body: 'web notes',
          assets: [
            {
              name: 'www_1.3.7_20260401.bin',
              browser_download_url: 'https://example.com/web.bin',
            },
          ],
        }),
      });
    });

    await page.route(fwUrlPattern, async (route) => {
      fwCalls++;
      await new Promise((r) => setTimeout(r, 1200));
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          tag_name: 'v1.3.7',
          html_url: 'https://example.com/fw-release',
          body: 'fw notes',
          assets: [
            {
              name: `${model}_1.3.7_20260401.bin`,
              browser_download_url: 'https://example.com/fw.bin',
            },
          ],
        }),
      });
    });

    await page.goto('/#/settings');
    await page.waitForSelector('.set-title, text=Firmware Update, button:has-text("SAVE")', { timeout: 15000 }).catch(() => {});
  });

  test('DC02: no remote OTA check button', async ({ page }, testInfo) => {
    const model = modelFromProjectName(testInfo.project.name);
    if (model !== 'DC02') test.skip();

    const otaButton = page.getByRole('button', { name: 'Check for Updates' });
    await expect(otaButton).toHaveCount(0);
  });

  test('BC04: remote OTA check shows download buttons', async ({ page }, testInfo) => {
    const model = modelFromProjectName(testInfo.project.name);
    if (model !== 'BC04') test.skip();

    // Wait for GitHub checks to complete and buttons to appear.
    const downloadButtons = page.getByRole('button', { name: 'Download' });
    await expect(downloadButtons).toHaveCount(2, { timeout: 20000 });
  });

  test('BC04: high-frequency click during checking does not send extra GitHub requests', async ({ page }, testInfo) => {
    const model = modelFromProjectName(testInfo.project.name);
    if (model !== 'BC04') test.skip();

    const checkBtn = page.getByRole('button', { name: 'Check for Updates' });
    await expect(checkBtn).toBeVisible();

    // Wait until the auto-check has at least fired once for both endpoints.
    await expect.poll(() => webCalls, { timeout: 10000 }).toBeGreaterThanOrEqual(1);
    await expect.poll(() => fwCalls, { timeout: 10000 }).toBeGreaterThanOrEqual(1);

    const baseWeb = webCalls;
    const baseFw = fwCalls;

    // Click multiple times quickly. If UI locking works, counters should not increase.
    await Promise.allSettled([checkBtn.click(), checkBtn.click(), checkBtn.click()]);

    await page.waitForTimeout(300);
    expect(webCalls).toBe(baseWeb);
    expect(fwCalls).toBe(baseFw);
  });

  test('firmware upload: shows SHA256 mismatch error when device rejects OTA', async ({ page }) => {
    // Mock POST /api/system/OTA to fail with SHA256 mismatch.
    await page.route('**/api/system/OTA', async (route) => {
      const u = new URL(route.request().url());
      if (route.request().method() !== 'POST' || u.pathname !== '/api/system/OTA') return route.continue();
      await route.fulfill({
        status: 500,
        contentType: 'text/plain',
        body: 'SHA256 Mismatch or Unknown Format',
      });
    });

    const binPath = createBinWithFirstByte(0xe9);
    const fileInput = page.locator('input[type="file"]');
    await expect(fileInput).toBeVisible({ timeout: 15000 });

    await fileInput.setInputFiles(binPath);

    // Upload is asynchronous; wait for error alert.
    await expect(page.locator('.ant-alert-error')).toBeVisible({ timeout: 20000 });
    await expect(page.locator('.ant-alert-error')).toContainText('SHA256');
  });

  test('SettingsEdit: save failure shows notification error', async ({ page }, testInfo) => {
    // Force PATCH /api/system to fail (SettingsEdit uses updateSystem -> PATCH /api/system).
    await page.route('**/api/system', async (route) => {
      const u = new URL(route.request().url());
      const method = route.request().method();
      if (method === 'PATCH' && u.pathname === '/api/system') {
        await route.fulfill({ status: 500, body: 'Save failed' });
      } else {
        await route.continue();
      }
    });

    const saveBtn = page.getByRole('button', { name: 'SAVE' });
    await expect(saveBtn).toBeVisible();
    await saveBtn.click();

    await expect(page.locator('.ant-notification-notice-error')).toBeVisible({ timeout: 10000 });
  });

  test('SettingsEdit: save success enables restart (Pending Restart)', async ({ page }, testInfo) => {
    // Allow PATCH /api/system success.
    await page.route('**/api/system', async (route) => {
      const u = new URL(route.request().url());
      const method = route.request().method();
      if (method === 'PATCH' && u.pathname === '/api/system') {
        await route.fulfill({ status: 200, body: '{}' });
      } else {
        await route.continue();
      }
    });

    const saveBtn = page.getByRole('button', { name: 'SAVE' });
    await expect(saveBtn).toBeVisible();
    await saveBtn.click();

    const pendingRestartBtn = page.getByRole('button', { name: 'Pending Restart' });
    await expect(pendingRestartBtn).toBeVisible({ timeout: 15000 });
    await expect(pendingRestartBtn).toBeEnabled();
  });
});
