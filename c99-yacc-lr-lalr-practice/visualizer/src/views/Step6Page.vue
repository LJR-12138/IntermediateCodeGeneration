<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 6 · LR(1) I0 闭包与 Goto</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="I0 kernel 项数" :value="data.summary.i0_kernel_items ?? '-'" />
        <MetricCard title="I0 closure 项数" :value="data.summary.i0_closure_items ?? '-'" />
        <MetricCard title="goto 边数" :value="data.summary.i0_goto_edges ?? '-'" />
        <MetricCard title="校验通过" :value="data.summary.lr1_step6_validation_passed ?? '-'" />
      </section>

      <CytoPanel
        title="I0 出边图（目标状态视图）"
        file-name="step6_i0_goto_graph.png"
        :elements="graphElements"
        :layout-options="relaxedLayoutOptions"
      />

      <section class="panel">
        <header class="panel-header">
          <h3>I0 项目列表</h3>
          <div class="toolbar">
            <button class="btn" @click="activeTab = 'kernel'" :class="{ active: activeTab === 'kernel' }">
              Kernel ({{ kernelCount }})
            </button>
            <button class="btn" @click="activeTab = 'closure'" :class="{ active: activeTab === 'closure' }">
              Closure ({{ closureCount }})
            </button>
          </div>
        </header>
        <div class="code-list">
          <p v-for="item in pagedActiveItems" :key="item" class="mono-line">{{ item }}</p>
        </div>
        <div class="panel-body toolbar">
          <button class="btn" type="button" @click="prevItemsPage" :disabled="itemsPage <= 1">上一页</button>
          <span>第 {{ itemsPage }} / {{ itemsTotalPages }} 页</span>
          <button class="btn" type="button" @click="nextItemsPage" :disabled="itemsPage >= itemsTotalPages">
            下一页
          </button>
          <span>每页 {{ itemsPageSize }} 条</span>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>I0 一跳 Goto 明细</h3>
          <select v-model="selectedSymbol" class="input select">
            <option v-for="edge in gotoEdges" :key="edge.symbol_name" :value="edge.symbol_name">
              I0 --{{ edge.symbol_name }}--> {{ edge.to_state >= 0 ? `I${edge.to_state}` : "?" }} ({{ edge.item_count }})
            </option>
          </select>
        </header>
        <div class="code-list">
          <p v-for="item in selectedGotoItems" :key="item" class="mono-line">{{ item }}</p>
        </div>
      </section>
    </template>
  </section>
</template>

<script setup lang="ts">
import type { ElementDefinition } from "cytoscape";
import { computed, onMounted, ref, watch } from "vue";
import CytoPanel from "../components/CytoPanel.vue";
import MetricCard from "../components/MetricCard.vue";
import { useYaccStore } from "../stores/yacc";
import { downloadJsonFile } from "../utils/data";

const store = useYaccStore();
const data = computed(() => store.stepData[6]);
const activeTab = ref<"kernel" | "closure">("closure");
const selectedSymbol = ref("");
const itemsPage = ref(1);
const itemsPageSize = 120;

const gotoEdges = computed(() => data.value?.lr1_i0?.goto_edges ?? []);
const relaxedLayoutOptions = {
  idealEdgeLength: 240,
  nodeRepulsion: 120000,
  edgeElasticity: 60,
  gravity: 0.4,
  padding: 56
};

watch(
  gotoEdges,
  (edges) => {
    if (!selectedSymbol.value && edges.length > 0) {
      selectedSymbol.value = edges[0].symbol_name;
    }
  },
  { immediate: true }
);

const graphElements = computed<ElementDefinition[]>(() => {
  const elements: ElementDefinition[] = [{ data: { id: "I0", label: "I0" } }];
  const createdNodes = new Set<string>(["I0"]);
  for (const edge of gotoEdges.value) {
    const toState = edge.to_state >= 0 ? edge.to_state : undefined;
    const nodeId = toState !== undefined ? `I${toState}` : `G-${edge.symbol_name}`;
    if (!createdNodes.has(nodeId)) {
      elements.push({
        data: {
          id: nodeId,
          label:
            toState !== undefined
              ? `I${toState}\nitems=${edge.item_count}`
              : `${edge.symbol_name}\nitems=${edge.item_count}`
        }
      });
      createdNodes.add(nodeId);
    }
    elements.push({
      data: {
        id: `I0->${nodeId}-${edge.symbol_name}`,
        source: "I0",
        target: nodeId,
        label: edge.symbol_name
      }
    });
  }
  return elements;
});

const activeItems = computed(() => {
  const lr1 = data.value?.lr1_i0;
  if (!lr1) {
    return [];
  }
  return activeTab.value === "kernel" ? lr1.kernel_items : lr1.closure_items;
});

const kernelCount = computed(() => data.value?.lr1_i0?.kernel_items?.length ?? 0);
const closureCount = computed(() => data.value?.lr1_i0?.closure_items?.length ?? 0);

const itemsTotalPages = computed(() => {
  const total = activeItems.value.length;
  if (total <= 0) {
    return 1;
  }
  return Math.ceil(total / itemsPageSize);
});

const pagedActiveItems = computed(() => {
  const begin = (itemsPage.value - 1) * itemsPageSize;
  return activeItems.value.slice(begin, begin + itemsPageSize);
});

watch(
  activeTab,
  () => {
    itemsPage.value = 1;
  },
  { immediate: true }
);

watch(
  itemsTotalPages,
  () => {
    if (itemsPage.value > itemsTotalPages.value) {
      itemsPage.value = itemsTotalPages.value;
    }
    if (itemsPage.value < 1) {
      itemsPage.value = 1;
    }
  },
  { immediate: true }
);

const selectedGotoItems = computed(() => {
  const lr1 = data.value?.lr1_i0;
  if (!lr1) {
    return [];
  }
  return lr1.goto_items[selectedSymbol.value] ?? [];
});

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step6_data.json", data.value);
}

function prevItemsPage() {
  itemsPage.value = Math.max(1, itemsPage.value - 1);
}

function nextItemsPage() {
  itemsPage.value = Math.min(itemsTotalPages.value, itemsPage.value + 1);
}

onMounted(async () => {
  await store.ensureStep(6);
});
</script>
