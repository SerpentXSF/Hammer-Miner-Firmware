<template>
  <div class="chip-heatmap-container card">
    <div class="card-header">
      <div class="card-title">Chip Monitoring & Frequency</div>
      <div class="heatmap-controls">
        <span class="control-label">Color map by:</span>
        <a-select v-model:value="colorMap" size="small" style="width: 120px">
          <a-select-option value="temp">Temperature</a-select-option>
          <a-select-option value="hashrate">Hashrate</a-select-option>
          <a-select-option value="hw">HW Errors</a-select-option>
        </a-select>
        <button class="btn-reset" @click="applyAllFrequencies" :disabled="isApplying" style="margin-left: 10px;">
          Apply Frequencies
        </button>
      </div>
    </div>

    <div class="chip-grid" v-if="chips && chips.length > 0">
      <div 
        v-for="(chip, index) in chips" 
        :key="index"
        class="chip-card"
        :style="{ '--chip-color': getChipColor(chip) }"
        @click="openSettings(index, chip)"
      >
        <div class="chip-header">
          <span class="chip-id">#{{ index }}</span>
          <span class="chip-freq">{{ chipFrequencies[index] || chip.frequency }} MHz</span>
        </div>
        <div class="chip-body">
          <div class="chip-stat">
            <span class="stat-icon">🌡️</span>
            <span class="stat-value">{{ chip.temp }}°C</span>
          </div>
          <div class="chip-stat">
            <span class="stat-icon">⚡</span>
            <span class="stat-value">{{ chip.hashrate.toFixed(2) }} TH/s</span>
          </div>
          <div class="chip-stat">
            <span class="stat-icon">❌</span>
            <span class="stat-value">{{ chip.hw_errors }}</span>
          </div>
        </div>
      </div>
    </div>
    <div v-else class="no-data">
      No chip data available.
    </div>

    <!-- Frequency Setting Modal -->
    <a-modal 
      v-model:open="isModalVisible" 
      title="Set Chip Frequency" 
      @ok="handleOk"
      :confirmLoading="isApplying"
      okText="Set"
      cancelText="Cancel"
    >
      <div v-if="selectedChip !== null" style="display: flex; flex-direction: column; gap: 16px;">
        <p>You are setting the frequency for <strong>Chip #{{ selectedChipIndex }}</strong>.</p>
        <div style="display: flex; align-items: center; gap: 12px;">
          <span>Frequency (MHz):</span>
          <a-slider 
            v-model:value="tempFrequency" 
            :min="100" 
            :max="1000" 
            :step="5" 
            style="flex: 1" 
          />
          <a-input-number v-model:value="tempFrequency" :min="100" :max="1000" />
        </div>
      </div>
    </a-modal>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, watch } from 'vue';
import { useAppStore } from '@/store';
import { updateSystem } from '@/api';
import { message } from 'ant-design-vue';

const appStore = useAppStore();

const colorMap = ref<'temp' | 'hashrate' | 'hw'>('temp');
const chipFrequencies = ref<number[]>([]);

const chips = computed(() => appStore.statusRaw?.chipData || []);

watch(chips, (newChips) => {
  if (newChips && newChips.length > 0 && chipFrequencies.value.length !== newChips.length) {
    // Initialize frequencies if not already set
    chipFrequencies.value = newChips.map(c => c.frequency || 400);
  }
}, { immediate: true, deep: true });

// Colors
function getChipColor(chip: any) {
  if (colorMap.value === 'temp') {
    // scale roughly 30 to 90
    const temp = chip.temp;
    if (temp < 50) return '#10b981'; // green
    if (temp < 70) return '#facc15'; // yellow
    if (temp < 85) return '#f97316'; // orange
    return '#ef4444'; // red
  } else if (colorMap.value === 'hashrate') {
    // Assume ref hashrate around 0.1 ~ 0.2
    const hash = chip.hashrate;
    if (hash < 0.05) return '#ef4444'; 
    if (hash < 0.1) return '#f97316';
    return '#10b981';
  } else if (colorMap.value === 'hw') {
    const hw = chip.hw_errors;
    if (hw === 0) return '#10b981';
    if (hw < 10) return '#facc15';
    if (hw < 100) return '#f97316';
    return '#ef4444';
  }
  return '#22d3ee';
}

const isModalVisible = ref(false);
const selectedChipIndex = ref<number | null>(null);
const selectedChip = ref<any>(null);
const tempFrequency = ref<number>(400);
const isApplying = ref(false);

function openSettings(index: number, chip: any) {
  selectedChipIndex.value = index;
  selectedChip.value = chip;
  tempFrequency.value = chipFrequencies.value[index] || chip.frequency || 400;
  isModalVisible.value = true;
}

function handleOk() {
  if (selectedChipIndex.value !== null) {
    chipFrequencies.value[selectedChipIndex.value] = tempFrequency.value;
  }
  isModalVisible.value = false;
}

async function applyAllFrequencies() {
  isApplying.value = true;
  try {
    await updateSystem('', { chipFrequencies: chipFrequencies.value });
    message.success('Chip frequencies updated successfully');
  } catch (err: any) {
    message.error('Failed to update chip frequencies');
    console.error(err);
  } finally {
    isApplying.value = false;
  }
}

</script>

<style scoped lang="scss">
.chip-heatmap-container {
  display: flex;
  flex-direction: column;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
}

.heatmap-controls {
  display: flex;
  align-items: center;
  gap: 8px;
  
  .control-label {
    font-size: 12px;
    color: var(--text-color-secondary);
  }
}

.chip-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
  gap: 12px;
}

.chip-card {
  background: var(--surface-card);
  border: 1px solid var(--surface-border);
  border-radius: 8px;
  padding: 10px;
  cursor: pointer;
  transition: all 0.2s;
  position: relative;
  overflow: hidden;

  &::before {
    content: '';
    position: absolute;
    top: 0; left: 0; right: 0;
    height: 4px;
    background-color: var(--chip-color, #22d3ee);
    transition: background-color 0.3s;
  }

  &:hover {
    border-color: var(--chip-color, var(--ant-primary-color));
    transform: translateY(-2px);
    box-shadow: 0 4px 12px rgba(0,0,0,0.1);
  }
}

.chip-header {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  margin-bottom: 8px;
  margin-top: 2px;
}

.chip-id {
  font-weight: bold;
  font-size: 14px;
  color: var(--text-color);
}

.chip-freq {
  font-size: 11px;
  color: var(--chip-color, var(--text-color-secondary));
  font-weight: 500;
  background: rgba(0,0,0,0.1);
  padding: 2px 6px;
  border-radius: 4px;
}

.chip-body {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.chip-stat {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 12px;
  color: var(--text-color-secondary);

  .stat-icon {
    font-size: 11px;
    opacity: 0.8;
  }
  
  .stat-value {
    color: var(--text-color);
    font-family: "SF Mono", ui-monospace, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
  }
}

.no-data {
  text-align: center;
  padding: 30px;
  color: var(--text-color-secondary);
  font-size: 13px;
}

.btn-reset {
  border: 1px solid var(--surface-border);
  background: var(--surface-ground);
  color: var(--text-color-secondary);
  border-radius: 4px;
  padding: 4px 12px;
  font-size: 11px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s;
  text-transform: uppercase;
  letter-spacing: 0.5px;

  &:hover:not(:disabled) {
    border-color: var(--ant-primary-color);
    color: var(--ant-primary-color);
    background: rgba(25, 225, 165, 0.05);
  }

  &:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }
}
</style>
