<template>
  <section class="panel">
    <header class="panel-header">
      <h3>{{ title }}</h3>
      <button class="btn" type="button" @click="exportPng">导出 PNG</button>
    </header>
    <div ref="containerRef" class="chart-container"></div>
  </section>
</template>

<script setup lang="ts">
import * as echarts from "echarts";
import { onBeforeUnmount, onMounted, ref, watch } from "vue";

const props = defineProps<{
  title: string;
  option: echarts.EChartsOption;
  fileName: string;
}>();

const containerRef = ref<HTMLDivElement | null>(null);
let chart: echarts.ECharts | null = null;

function render() {
  if (!containerRef.value) {
    return;
  }
  if (!chart) {
    chart = echarts.init(containerRef.value);
  }
  chart.setOption(props.option, true);
}

function exportPng() {
  if (!chart) {
    return;
  }
  const url = chart.getDataURL({ type: "png", pixelRatio: 2, backgroundColor: "#ffffff" });
  const a = document.createElement("a");
  a.href = url;
  a.download = props.fileName;
  a.click();
}

onMounted(() => {
  render();
  window.addEventListener("resize", render);
});

watch(
  () => props.option,
  () => {
    render();
  },
  { deep: true }
);

onBeforeUnmount(() => {
  window.removeEventListener("resize", render);
  chart?.dispose();
});
</script>
