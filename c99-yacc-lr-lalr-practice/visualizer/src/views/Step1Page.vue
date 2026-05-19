<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 1 · 输入文件与词法符号基线</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="源文件存在" :value="sourceExistsText" />
        <MetricCard title="总行数" :value="overview.total_lines ?? 0" />
        <MetricCard title="非空行数" :value="overview.nonempty_lines ?? 0" />
        <MetricCard title="符号总数" :value="data.summary.symbols ?? '-'" />
        <MetricCard title="终结符" :value="data.summary.terminals ?? '-'" />
        <MetricCard title="非终结符" :value="data.summary.nonterminals ?? '-'" />
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>源语法文件预览</h3>
        </header>
        <div class="code-list">
          <p v-for="line in overview.preview_lines ?? []" :key="line.line" class="mono-line">
            {{ line.line }} | {{ line.text }}
          </p>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>符号表（分页）</h3>
          <input v-model.trim="keyword" class="input" type="text" placeholder="按 name/kind 检索" />
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>id</th>
                <th>name</th>
                <th>kind</th>
                <th>is_literal_char</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in pagedSymbols" :key="row.id">
                <td>{{ row.id }}</td>
                <td>{{ row.name }}</td>
                <td>{{ row.kind }}</td>
                <td>{{ row.is_literal_char ? "1" : "0" }}</td>
              </tr>
            </tbody>
          </table>
        </div>
        <div class="panel-body toolbar">
          <button class="btn" type="button" @click="prevPage" :disabled="page <= 1">上一页</button>
          <span>第 {{ page }} / {{ totalPages }} 页</span>
          <button class="btn" type="button" @click="nextPage" :disabled="page >= totalPages">下一页</button>
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

const PAGE_SIZE = 25;
const store = useYaccStore();
const keyword = ref("");
const page = ref(1);
const data = computed(() => store.stepData[1]);
const overview = computed(() => data.value?.step1_overview ?? {
  source_exists: false,
  total_lines: 0,
  nonempty_lines: 0,
  preview_lines: []
});

const sourceExistsText = computed(() => (overview.value.source_exists ? "yes" : "no"));

const filteredSymbols = computed(() => {
  const list = data.value?.symbols ?? [];
  if (!keyword.value) {
    return list;
  }
  const k = keyword.value.toLowerCase();
  return list.filter((x) => x.name.toLowerCase().includes(k) || x.kind.toLowerCase().includes(k));
});

const totalPages = computed(() => Math.max(1, Math.ceil(filteredSymbols.value.length / PAGE_SIZE)));
const pagedSymbols = computed(() => {
  const start = (page.value - 1) * PAGE_SIZE;
  return filteredSymbols.value.slice(start, start + PAGE_SIZE);
});

watch([keyword, totalPages], () => {
  if (page.value > totalPages.value) {
    page.value = totalPages.value;
  }
  if (page.value < 1) {
    page.value = 1;
  }
});

function prevPage() {
  page.value = Math.max(1, page.value - 1);
}

function nextPage() {
  page.value = Math.min(totalPages.value, page.value + 1);
}

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step1_data.json", data.value);
}

onMounted(async () => {
  await store.ensureStep(1);
});
</script>
