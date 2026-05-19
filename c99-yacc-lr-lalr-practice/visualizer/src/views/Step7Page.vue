<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 7 · LR(1) 规范族与状态转移图</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="状态总数" :value="data.summary.lr1_states ?? '-'" />
        <MetricCard title="转移边总数" :value="data.summary.lr1_transitions ?? '-'" />
        <MetricCard title="Step7 校验" :value="data.summary.lr1_step7_validation_passed ?? '-'" />
        <MetricCard title="Step6 校验" :value="data.summary.lr1_step6_validation_passed ?? '-'" />
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>状态项明细</h3>
          <select v-model="selectedStateId" class="input select">
            <option v-for="s in states" :key="s.state_id" :value="String(s.state_id)">
              I{{ s.state_id }} (items={{ s.item_count }})
            </option>
          </select>
        </header>
        <div class="code-list">
          <p v-for="item in selectedStateItems" :key="item" class="mono-line">{{ item }}</p>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>状态来源（I{{ selectedStateId }}）</h3>
        </header>
        <div class="code-list">
          <p v-for="line in selectedPredecessors" :key="line" class="mono-line">{{ line }}</p>
          <p v-if="selectedPredecessors.length === 0" class="mono-line">无来源边（通常是 I0）。</p>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>状态转移边</h3>
          <div class="toolbar">
            <button class="btn" type="button" :disabled="transitionPage <= 1" @click="transitionPage -= 1">
              上一页
            </button>
            <span class="mono-line">第 {{ transitionPage }} / {{ transitionTotalPages }} 页</span>
            <button
              class="btn"
              type="button"
              :disabled="transitionPage >= transitionTotalPages"
              @click="transitionPage += 1"
            >
              下一页
            </button>
            <span class="mono-line">每页 {{ transitionPageSize }} 条</span>
          </div>
        </header>
        <div class="table-wrap transition-table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>from</th>
                <th>symbol</th>
                <th>to</th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="(t, idx) in pagedTransitions"
                :key="`${idx}-${t.from_state}-${t.symbol_id}-${t.to_state}-${transitionPage}`"
              >
                <td>I{{ t.from_state }}</td>
                <td class="truncate-cell" :title="t.symbol_name">{{ t.symbol_name }}</td>
                <td>I{{ t.to_state }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>
    </template>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue";
import MetricCard from "../components/MetricCard.vue";
import { useYaccStore } from "../stores/yacc";
import { downloadJsonFile } from "../utils/data";

const transitionPageSize = 80;
const store = useYaccStore();
const data = computed(() => store.stepData[7]);
const selectedStateId = ref("0");
const transitionPage = ref(1);

const states = computed(() => data.value?.lr1_canonical?.states ?? []);
const transitions = computed(() => data.value?.lr1_canonical?.transitions ?? []);
const predecessors = computed(() => data.value?.lr1_canonical?.predecessors ?? []);

watch(
  states,
  (rows) => {
    if (rows.length === 0) {
      selectedStateId.value = "0";
      return;
    }
    const exists = rows.some((s) => String(s.state_id) === selectedStateId.value);
    if (!exists) {
      selectedStateId.value = String(rows[0].state_id);
    }
  },
  { immediate: true }
);

const selectedStateItems = computed(() => {
  const m = data.value?.lr1_canonical?.state_items ?? {};
  return m[selectedStateId.value] ?? [];
});

const transitionTotalPages = computed(() => {
  const total = transitions.value.length;
  if (total <= 0) {
    return 1;
  }
  return Math.ceil(total / transitionPageSize);
});

watch(
  transitions,
  () => {
    transitionPage.value = 1;
  },
  { immediate: true }
);

watch(transitionPage, (page) => {
  if (page < 1) {
    transitionPage.value = 1;
    return;
  }
  if (page > transitionTotalPages.value) {
    transitionPage.value = transitionTotalPages.value;
  }
});

const pagedTransitions = computed(() => {
  const begin = (transitionPage.value - 1) * transitionPageSize;
  return transitions.value.slice(begin, begin + transitionPageSize);
});

const selectedPredecessors = computed(() => {
  const sid = Number(selectedStateId.value);
  const row = predecessors.value.find((x) => x.state_id === sid);
  return row?.predecessors ?? [];
});

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step7_data.json", data.value);
}

onMounted(async () => {
  await store.ensureStep(7);
});
</script>
