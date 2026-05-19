<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 5 · First 集计算</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="First 收敛" :value="data.summary.first_converged ?? '-'" />
        <MetricCard title="迭代次数" :value="data.summary.first_iterations ?? '-'" />
        <MetricCard title="nullable 数量" :value="data.summary.nullable_nonterminals_count ?? '-'" />
        <MetricCard title="校验通过" :value="data.summary.first_validation_passed ?? '-'" />
      </section>

      <EChartPanel title="First 集规模 Top 20" file-name="step5_first_top20.png" :option="chartOption" />

      <section class="panel">
        <header class="panel-header">
          <h3>First 集明细（非终结符）</h3>
          <input
            v-model.trim="keyword"
            class="input"
            type="text"
            placeholder="输入符号名过滤"
          />
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>symbol</th>
                <th>FIRST</th>
                <th>size</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in filteredRows" :key="row.symbol_id">
                <td>{{ row.symbol_name }}</td>
                <td>{{ row.first_set.join(", ") }}</td>
                <td>{{ row.first_set.length }}</td>
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
const data = computed(() => store.stepData[5]);

const firstRows = computed(() =>
  (data.value?.first_sets ?? []).filter((x) => x.kind === "Nonterminal")
);

const filteredRows = computed(() => {
  if (!keyword.value) {
    return firstRows.value;
  }
  const k = keyword.value.toLowerCase();
  return firstRows.value.filter((x) => x.symbol_name.toLowerCase().includes(k));
});

const chartOption = computed<EChartsOption>(() => {
  const top = [...firstRows.value]
    .sort((a, b) => b.first_set.length - a.first_set.length)
    .slice(0, 20);
  return {
    backgroundColor: "transparent",
    xAxis: {
      type: "category",
      data: top.map((x) => x.symbol_name),
      axisLabel: { rotate: 35, color: "#334155" }
    },
    yAxis: { type: "value", name: "FIRST 元素数", axisLabel: { color: "#334155" } },
    tooltip: { trigger: "axis" },
    series: [
      {
        type: "bar",
        data: top.map((x) => x.first_set.length),
        itemStyle: {
          color: "#0284c7",
          borderRadius: [8, 8, 0, 0]
        }
      }
    ]
  };
});

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step5_data.json", data.value);
}

onMounted(async () => {
  await store.ensureStep(5);
});
</script>
