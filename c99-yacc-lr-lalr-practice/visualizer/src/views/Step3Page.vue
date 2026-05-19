<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 3 · 输入解析结果</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="符号总数" :value="data.summary.symbols ?? '-'" />
        <MetricCard title="终结符" :value="data.summary.terminals ?? '-'" />
        <MetricCard title="非终结符" :value="data.summary.nonterminals ?? '-'" />
        <MetricCard title="产生式总数" :value="data.summary.productions_with_augmented ?? '-'" />
      </section>

      <EChartPanel
        title="符号种类分布"
        file-name="step3_symbol_kind.png"
        :option="symbolChartOption"
      />

      <section class="panel">
        <header class="panel-header">
          <h3>产生式列表（可检索）</h3>
          <input v-model.trim="keyword" class="input" type="text" placeholder="输入 lhs/rhs 关键词" />
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>#</th>
                <th>LHS</th>
                <th>RHS</th>
                <th>line</th>
                <th>action</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="p in filteredProductions" :key="p.id">
                <td>{{ p.id }}</td>
                <td>{{ p.lhs }}</td>
                <td>{{ p.rhs.join(" ") || "epsilon" }}</td>
                <td>{{ p.line }}</td>
                <td>{{ p.action ? "yes" : "no" }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>
    </template>
  </section>
</template>

<script setup lang="ts">
import type { EChartsOption } from "echarts";
import { computed, onMounted, ref } from "vue";
import EChartPanel from "../components/EChartPanel.vue";
import MetricCard from "../components/MetricCard.vue";
import { useYaccStore } from "../stores/yacc";
import { downloadJsonFile } from "../utils/data";

const store = useYaccStore();
const keyword = ref("");
const data = computed(() => store.stepData[3]);

const filteredProductions = computed(() => {
  const list = data.value?.productions ?? [];
  if (!keyword.value) {
    return list;
  }
  const k = keyword.value.toLowerCase();
  return list.filter((p) => p.text.toLowerCase().includes(k));
});

const symbolChartOption = computed<EChartsOption>(() => {
  const symbols = data.value?.symbols ?? [];
  const buckets = new Map<string, number>();
  for (const s of symbols) {
    buckets.set(s.kind, (buckets.get(s.kind) ?? 0) + 1);
  }
  return {
    backgroundColor: "transparent",
    tooltip: { trigger: "item" },
    series: [
      {
        type: "pie",
        radius: ["30%", "66%"],
        itemStyle: { borderRadius: 8, borderColor: "#ffffff", borderWidth: 2 },
        label: { color: "#0f172a" },
        data: Array.from(buckets.entries()).map(([name, value]) => ({ name, value }))
      }
    ]
  };
});

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step3_data.json", data.value);
}

onMounted(async () => {
  await store.ensureStep(3);
});
</script>
