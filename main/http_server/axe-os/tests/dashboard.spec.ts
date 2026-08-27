import { test, expect } from '@playwright/test';

type Model = 'DC02' | 'BC04';

function modelFromProjectName(name: string): Model {
  return name.includes('BC04') ? 'BC04' : 'DC02';
}

function makeMinerStatus(model: Model, opts?: { valid?: boolean; withChipData?: boolean }) {
  const valid = opts?.valid ?? true;
  const withChipData = opts?.withChipData ?? false;

  const isBc04 = model === 'BC04';
  const frequency = isBc04 ? 750 : 2300;
  const coreVoltage = isBc04 ? 490 : 127; // cV for SettingsEdit validation

  const base: any = {
    DeviceModel: model,
    version: '1.3.6 20260325',
    frequency,
    coreVoltage,
    // For maintainDataset
    temp: 50,
    temp1: 45,
    power: isBc04 ? 121.67 : 50,
    // For validData()
    hashRate: valid ? 0.236 : undefined,
    // For Dashboard computations (optional but helps avoid NaN noise)
    voltage: 12208.75,
    current: 2237.5,
    coreVoltageActual: coreVoltage * 10,
    coreVoltageActual1: coreVoltage * 10,
    maxPower: 25,
    nominalVoltage: 12000,
    vrTemp: 45,
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
    wifiStatus: 'Connected!',
    wifiRSSI: -55,
    apEnabled: 0,
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
    invertfanpolarity: 1,
    autofanspeed: 1,
    fanspeed: 80,
    fanrpm: 1000,
    fanrpm1: 2000,
    boardtemp1: 30,
    boardtemp2: 35,
    overheat_mode: 0,
    power_fault: '',
    boot_mode: 0,
    idfVersion: 'v5.1.2',
    boardVersion: '204',
    flipscreen: 1,
    ntpServerBackup: 'time.windows.com',
    ntpServer: 'pool.ntp.org',
    auth_enable: false,
    // For Dashboard label / logic
    bestDiff: '2',
    bestSessionDiff: '1',
    hwRate: '5',
    hwNumber: '100',
    nonceNumber: '2000',
    // required by MinerStatusData typing but optional for runtime
    chipData: withChipData
      ? [
          { temp: 35, hashrate: 0.1, hw_errors: 0, frequency: 400 },
          { temp: 40, hashrate: 0.12, hw_errors: 2, frequency: 400 },
        ]
      : undefined,
    // used by App.vue initSysData for time parsing
    currentTime: '2025-08-09 01:59:52',
    // keep OTA decision stable
    autotuneActive: false,
  };

  if (!valid) {
    delete base.hashRate;
  }

  return base;
}

test.describe('Dashboard page', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('about:blank');
    // Keep i18n stable for role-name assertions
    await page.addInitScript(() => {
      localStorage.setItem('lang', 'en');
    });
  });

  test('renders online UI for valid data', async ({ page }, testInfo) => {
    const model = modelFromProjectName(testInfo.project.name);
    await page.route('**/api/system/info*', async (route) => {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify(makeMinerStatus(model, { valid: true, withChipData: model === 'BC04' })),
      });
    });

    await page.goto('/#/');

    // Status strip should exist and topbar should not show offline.
    await expect(page.locator('.status-strip')).toBeVisible({ timeout: 10000 });
    await expect(page.locator('.logo-hash-badge')).not.toHaveClass(/is-offline/);
  });

  test('shows ChipHeatmap on BC04 when chipData exists', async ({ page }, testInfo) => {
    const model = modelFromProjectName(testInfo.project.name);
    await page.route('**/api/system/info*', async (route) => {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify(makeMinerStatus(model, { valid: true, withChipData: model === 'BC04' })),
      });
    });
    await page.goto('/#/');

    if (model !== 'BC04') {
      await expect(page.locator('text=Chip Monitoring & Frequency')).toHaveCount(0);
      return;
    }

    await expect(page.locator('text=Chip Monitoring & Frequency')).toBeVisible({ timeout: 10000 });
  });

  test('renders offline fallback when system info is invalid (no hashRate)', async ({ page }, testInfo) => {
    const model = modelFromProjectName(testInfo.project.name);
    await page.route('**/api/system/info*', async (route) => {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify(makeMinerStatus(model, { valid: false })),
      });
    });

    await page.goto('/#/');

    await expect(page.locator('.status-strip')).toBeVisible({ timeout: 10000 });
    await expect(page.locator('.logo-hash-badge')).toHaveClass(/is-offline/);
  });
});

