<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 10 · LR(1) 到 LALR(1) 合并与对比</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="LR1 状态数" :value="data.summary.lr1_states ?? '-'" />
        <MetricCard title="LALR 状态数" :value="data.summary.lalr_states ?? '-'" />
        <MetricCard title="压缩状态数" :value="data.summary.state_merged ?? '-'" />
        <MetricCard title="LR1 冲突数" :value="data.summary.lr1_conflicts ?? '-'" />
        <MetricCard title="LALR 冲突数" :value="data.summary.lalr_conflicts ?? '-'" />
        <MetricCard title="Step10 校验" :value="data.summary.lr1_step10_validation_passed ?? '-'" />
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>状态合并映射（LR1 -> LALR）</h3>
          <div class="toolbar">
            <button class="btn" type="button" :disabled="mapPage <= 1" @click="mapPage -= 1">上一页</button>
            <span class="mono-line">第 {{ mapPage }} / {{ mapTotalPages }} 页</span>
            <button class="btn" type="button" :disabled="mapPage >= mapTotalPages" @click="mapPage += 1">
              下一页
            </button>
          </div>
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>LR1 状态</th>
                <th>LALR 状态</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in pagedStateMap" :key="`map-${row.lr1_state}`">
                <td>I{{ row.lr1_state }}</td>
                <td>I{{ row.lalr_state }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>合并组明细</h3>
          <div class="toolbar">
            <button class="btn" type="button" :disabled="groupPage <= 1" @click="groupPage -= 1">上一页</button>
            <span class="mono-line">第 {{ groupPage }} / {{ groupTotalPages }} 页</span>
            <button class="btn" type="button" :disabled="groupPage >= groupTotalPages" @click="groupPage += 1">
              下一页
            </button>
          </div>
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>LALR 状态</th>
                <th>来源 LR1 状态</th>
                <th>项数</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in pagedMergeGroups" :key="`grp-${row.lalr_state}`">
                <td>I{{ row.lalr_state }}</td>
                <td>{{ row.source_lr1_states.map((x) => `I${x}`).join(", ") }}</td>
                <td>{{ row.item_count }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>LALR 冲突明细</h3>
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

const store = useYaccStore();
const data = computed(() => store.stepData[10]);

const mapPageSize = 100;
const groupPageSize = 60;
const mapPage = ref(1);
const groupPage = ref(1);

const stateMap = computed(() => data.value?.lalr?.state_map ?? []);
const mergeGroups = computed(() => data.value?.lalr?.merge_groups ?? []);
const conflicts = computed(() => data.value?.lalr?.conflicts ?? []);

const mapTotalPages = computed(() => Math.max(1, Math.ceil(stateMap.value.length / mapPageSize)));
const groupTotalPages = computed(() => Math.max(1, Math.ceil(mergeGroups.value.length / groupPageSize)));

watch([stateMap], () => {
  mapPage.value = 1;
});
watch([mergeGroups], () => {
  groupPage.value = 1;
});

const pagedStateMap = computed(() => {
  const begin = (mapPage.value - 1) * mapPageSize;
  return stateMap.value.slice(begin, begin + mapPageSize);
});

const pagedMergeGroups = computed(() => {
  const begin = (groupPage.value - 1) * groupPageSize;
  return mergeGroups.value.slice(begin, begin + groupPageSize);
});

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step10_data.json", data.value);
}

onMounted(async () => {
  await store.ensureStep(10);
});
</script>

