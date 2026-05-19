<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 8 · Action/Goto 分析表与冲突处理</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="Action 表项" :value="data.summary.action_entries ?? '-'" />
        <MetricCard title="Goto 表项" :value="data.summary.goto_entries ?? '-'" />
        <MetricCard title="冲突数" :value="data.summary.parse_table_conflicts ?? '-'" />
        <MetricCard title="Step8 校验" :value="data.summary.lr1_step8_validation_passed ?? '-'" />
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>视图模式</h3>
          <div class="toolbar">
            <button class="btn" :class="{ active: viewMode === 'split' }" type="button" @click="viewMode = 'split'">
              双表视图
            </button>
            <button class="btn" :class="{ active: viewMode === 'merged' }" type="button" @click="viewMode = 'merged'">
              合并视图
            </button>
          </div>
        </header>
      </section>

      <section v-if="viewMode === 'split'" class="panel">
        <header class="panel-header">
          <h3>Action 表（分页）</h3>
          <div class="toolbar">
            <input v-model.trim="actionStateFilter" class="input" type="text" placeholder="按 state_id 过滤" />
            <input v-model.trim="actionSymbolFilter" class="input" type="text" placeholder="按 terminal 过滤" />
            <button class="btn" type="button" :disabled="actionPage <= 1" @click="actionPage -= 1">上一页</button>
            <span class="mono-line">第 {{ actionPage }} / {{ actionTotalPages }} 页</span>
            <button class="btn" type="button" :disabled="actionPage >= actionTotalPages" @click="actionPage += 1">
              下一页
            </button>
            <span class="mono-line">每页 {{ pageSize }} 条</span>
          </div>
        </header>
        <div class="table-wrap transition-table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>state</th>
                <th>terminal</th>
                <th>action</th>
                <th>target</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in pagedActionRows" :key="`${row.state_id}-${row.terminal_id}-${row.action}-${row.target}`">
                <td>I{{ row.state_id }}</td>
                <td>{{ row.terminal_name }} ({{ row.terminal_id }})</td>
                <td>{{ row.action }}</td>
                <td>{{ row.target }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>

      <section v-if="viewMode === 'split'" class="panel">
        <header class="panel-header">
          <h3>Goto 表（分页）</h3>
          <div class="toolbar">
            <input v-model.trim="gotoStateFilter" class="input" type="text" placeholder="按 state_id 过滤" />
            <input v-model.trim="gotoSymbolFilter" class="input" type="text" placeholder="按 nonterminal 过滤" />
            <button class="btn" type="button" :disabled="gotoPage <= 1" @click="gotoPage -= 1">上一页</button>
            <span class="mono-line">第 {{ gotoPage }} / {{ gotoTotalPages }} 页</span>
            <button class="btn" type="button" :disabled="gotoPage >= gotoTotalPages" @click="gotoPage += 1">
              下一页
            </button>
            <span class="mono-line">每页 {{ pageSize }} 条</span>
          </div>
        </header>
        <div class="table-wrap transition-table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>state</th>
                <th>nonterminal</th>
                <th>to</th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="row in pagedGotoRows"
                :key="`${row.state_id}-${row.nonterminal_id}-${row.to_state}`"
              >
                <td>I{{ row.state_id }}</td>
                <td>{{ row.nonterminal_name }} ({{ row.nonterminal_id }})</td>
                <td>I{{ row.to_state }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>

      <section v-if="viewMode === 'merged'" class="panel">
        <header class="panel-header">
          <h3>Action/Goto 合并表（按状态）</h3>
          <div class="toolbar">
            <input v-model.trim="mergedStateFilter" class="input" type="text" placeholder="按 state_id 过滤" />
            <button class="btn" type="button" :disabled="mergedPage <= 1" @click="mergedPage -= 1">上一页</button>
            <span class="mono-line">第 {{ mergedPage }} / {{ mergedTotalPages }} 页</span>
            <button class="btn" type="button" :disabled="mergedPage >= mergedTotalPages" @click="mergedPage += 1">
              下一页
            </button>
            <span class="mono-line">每页 {{ mergedPageSize }} 个 state</span>
          </div>
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>state</th>
                <th>Action（terminal -> act）</th>
                <th>Goto（nonterminal -> state）</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in pagedMergedRows" :key="`merged-${row.state_id}`">
                <td>I{{ row.state_id }}</td>
                <td class="truncate-cell" :title="row.action_full">{{ row.action_preview }}</td>
                <td class="truncate-cell" :title="row.goto_full">{{ row.goto_preview }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>冲突明细</h3>
        </header>
        <div class="table-wrap transition-table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>state</th>
                <th>symbol</th>
                <th>type</th>
                <th>existing</th>
                <th>incoming</th>
                <th>related_items</th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="row in conflicts"
                :key="`${row.state_id}-${row.symbol_id}-${row.existing_action}-${row.incoming_action}`"
              >
                <td>I{{ row.state_id }}</td>
                <td>{{ row.symbol_name }} ({{ row.symbol_id }})</td>
                <td>{{ row.conflict_type }}</td>
                <td>{{ row.existing_action }}</td>
                <td>{{ row.incoming_action }}</td>
                <td class="truncate-cell" :title="row.related_items">{{ row.related_items }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>冲突消解决策日志</h3>
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>state</th>
                <th>symbol</th>
                <th>type</th>
                <th>resolved</th>
                <th>reason</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in resolutions" :key="`${row.state_id}-${row.symbol_id}-${row.resolved_action}`">
                <td>I{{ row.state_id }}</td>
                <td>{{ row.symbol_name }} ({{ row.symbol_id }})</td>
                <td>{{ row.conflict_type }}</td>
                <td>{{ row.resolved_action }}</td>
                <td>{{ row.reason }}</td>
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

const pageSize = 80;
const store = useYaccStore();
const data = computed(() => store.stepData[8]);

const actionStateFilter = ref("");
const actionSymbolFilter = ref("");
const gotoStateFilter = ref("");
const gotoSymbolFilter = ref("");
const mergedStateFilter = ref("");
const actionPage = ref(1);
const gotoPage = ref(1);
const mergedPage = ref(1);
const viewMode = ref<"split" | "merged">("split");

const actionRows = computed(() => data.value?.parse_table?.action_rows ?? []);
const gotoRows = computed(() => data.value?.parse_table?.goto_rows ?? []);
const conflicts = computed(() => data.value?.parse_table?.conflicts ?? []);
const resolutions = computed(() => data.value?.parse_table?.conflict_resolutions ?? []);
const mergedPageSize = 30;

const filteredActionRows = computed(() => {
  const stateKw = actionStateFilter.value.trim();
  const symbolKw = actionSymbolFilter.value.trim().toLowerCase();
  return actionRows.value.filter((row) => {
    const byState = stateKw ? String(row.state_id).includes(stateKw) : true;
    const bySymbol = symbolKw
      ? row.terminal_name.toLowerCase().includes(symbolKw) || String(row.terminal_id).includes(symbolKw)
      : true;
    return byState && bySymbol;
  });
});

const filteredGotoRows = computed(() => {
  const stateKw = gotoStateFilter.value.trim();
  const symbolKw = gotoSymbolFilter.value.trim().toLowerCase();
  return gotoRows.value.filter((row) => {
    const byState = stateKw ? String(row.state_id).includes(stateKw) : true;
    const bySymbol = symbolKw
      ? row.nonterminal_name.toLowerCase().includes(symbolKw) || String(row.nonterminal_id).includes(symbolKw)
      : true;
    return byState && bySymbol;
  });
});

const actionTotalPages = computed(() => Math.max(1, Math.ceil(filteredActionRows.value.length / pageSize)));
const gotoTotalPages = computed(() => Math.max(1, Math.ceil(filteredGotoRows.value.length / pageSize)));
const mergedRows = computed(() => {
  const actionByState = new Map<number, string[]>();
  const gotoByState = new Map<number, string[]>();
  const states = new Set<number>();

  for (const row of actionRows.value) {
    states.add(row.state_id);
    const arr = actionByState.get(row.state_id) ?? [];
    arr.push(`${row.terminal_name}:${row.action}`);
    actionByState.set(row.state_id, arr);
  }
  for (const row of gotoRows.value) {
    states.add(row.state_id);
    const arr = gotoByState.get(row.state_id) ?? [];
    arr.push(`${row.nonterminal_name}:I${row.to_state}`);
    gotoByState.set(row.state_id, arr);
  }

  return Array.from(states).sort((a, b) => a - b).map((stateId) => {
    const actionOfState = actionByState.get(stateId) ?? [];
    const gotoOfState = gotoByState.get(stateId) ?? [];
    return {
      state_id: stateId,
      action_full: actionOfState.join(" | "),
      goto_full: gotoOfState.join(" | "),
      action_preview: summarizeCell(actionOfState),
      goto_preview: summarizeCell(gotoOfState)
    };
  });
});

const filteredMergedRows = computed(() => {
  const kw = mergedStateFilter.value.trim();
  if (!kw) {
    return mergedRows.value;
  }
  return mergedRows.value.filter((x) => String(x.state_id).includes(kw));
});

const mergedTotalPages = computed(() => Math.max(1, Math.ceil(filteredMergedRows.value.length / mergedPageSize)));

watch([filteredActionRows], () => {
  actionPage.value = 1;
});

watch([filteredGotoRows], () => {
  gotoPage.value = 1;
});

watch([filteredMergedRows], () => {
  mergedPage.value = 1;
});

watch(actionPage, (p) => {
  if (p < 1) {
    actionPage.value = 1;
    return;
  }
  if (p > actionTotalPages.value) {
    actionPage.value = actionTotalPages.value;
  }
});

watch(gotoPage, (p) => {
  if (p < 1) {
    gotoPage.value = 1;
    return;
  }
  if (p > gotoTotalPages.value) {
    gotoPage.value = gotoTotalPages.value;
  }
});

watch(mergedPage, (p) => {
  if (p < 1) {
    mergedPage.value = 1;
    return;
  }
  if (p > mergedTotalPages.value) {
    mergedPage.value = mergedTotalPages.value;
  }
});

const pagedActionRows = computed(() => {
  const begin = (actionPage.value - 1) * pageSize;
  return filteredActionRows.value.slice(begin, begin + pageSize);
});

const pagedGotoRows = computed(() => {
  const begin = (gotoPage.value - 1) * pageSize;
  return filteredGotoRows.value.slice(begin, begin + pageSize);
});

const pagedMergedRows = computed(() => {
  const begin = (mergedPage.value - 1) * mergedPageSize;
  return filteredMergedRows.value.slice(begin, begin + mergedPageSize);
});

function summarizeCell(items: string[]): string {
  if (items.length === 0) {
    return "-";
  }
  const previewCount = 8;
  if (items.length <= previewCount) {
    return items.join(" | ");
  }
  return `${items.slice(0, previewCount).join(" | ")} | ... (共 ${items.length} 项)`;
}

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step8_data.json", data.value);
}

onMounted(async () => {
  await store.ensureStep(8);
});
</script>
