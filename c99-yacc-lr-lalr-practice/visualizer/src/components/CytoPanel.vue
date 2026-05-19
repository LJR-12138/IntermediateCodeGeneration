<template>
  <section class="panel">
    <header class="panel-header">
      <h3>{{ title }}</h3>
      <div class="toolbar">
        <button class="btn" type="button" @click="zoomIn">放大</button>
        <button class="btn" type="button" @click="zoomOut">缩小</button>
        <button class="btn" type="button" @click="fit">适配</button>
        <button class="btn" type="button" @click="exportPng">导出 PNG</button>
      </div>
    </header>
    <div ref="containerRef" class="cyto-container"></div>
  </section>
</template>

<script setup lang="ts">
import cytoscape, { type ElementDefinition } from "cytoscape";
import { onBeforeUnmount, onMounted, ref, watch } from "vue";

const props = defineProps<{
  title: string;
  fileName: string;
  elements: ElementDefinition[];
  layoutOptions?: Record<string, unknown>;
}>();

const containerRef = ref<HTMLDivElement | null>(null);
let cy: cytoscape.Core | null = null;

function init() {
  if (!containerRef.value) {
    return;
  }
  cy?.destroy();
  cy = cytoscape({
    container: containerRef.value,
    elements: props.elements,
    style: [
      {
        selector: "node",
        style: {
          "background-color": "#a8b3c2",
          label: "data(label)",
          color: "#0f172a",
          shape: "round-rectangle",
          width: "label",
          height: "label",
          padding: "10px",
          "text-valign": "center",
          "text-halign": "center",
          "text-wrap": "wrap",
          "text-max-width": "160px",
          "font-size": "10px",
          "border-width": 1,
          "border-color": "#64748b"
        }
      },
      {
        selector: "edge",
        style: {
          width: 1.4,
          "line-color": "#94a3b8",
          "target-arrow-color": "#94a3b8",
          "target-arrow-shape": "triangle",
          "curve-style": "bezier",
          label: "data(label)",
          "font-size": "9px",
          color: "#334155"
        }
      }
    ],
    layout: {
      name: "cose",
      animate: false,
      fit: true,
      padding: 28,
      ...props.layoutOptions
    },
    wheelSensitivity: 0.2
  });
}

function fit() {
  cy?.fit(undefined, 28);
}

function zoomIn() {
  if (!cy) {
    return;
  }
  cy.zoom(cy.zoom() * 1.15);
}

function zoomOut() {
  if (!cy) {
    return;
  }
  cy.zoom(cy.zoom() / 1.15);
}

function exportPng() {
  if (!cy) {
    return;
  }
  const url = cy.png({ full: true, scale: 2, bg: "#ffffff" });
  const a = document.createElement("a");
  a.href = url;
  a.download = props.fileName;
  a.click();
}

watch(
  () => props.elements,
  () => {
    init();
  },
  { deep: true }
);

onMounted(init);
onBeforeUnmount(() => {
  cy?.destroy();
});
</script>
