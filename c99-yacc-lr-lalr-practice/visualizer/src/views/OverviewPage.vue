<template>
  <section class="page">
    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else>
      <section class="metrics-grid" v-if="store.manifest">
        <MetricCard title="案例" :value="store.manifest.case_id" hint="来自 URL 参数 case" />
        <MetricCard title="来源语法文件" :value="store.manifest.source || '-'" />
        <MetricCard title="步骤数" :value="Object.keys(store.manifest.steps).length" />
        <MetricCard title="生成时间" :value="fmtTime(store.manifest.generated_at)" />
      </section>

      <section class="panel" v-if="store.manifest">
        <header class="panel-header">
          <h3>步骤清单</h3>
        </header>
        <table class="table">
          <thead>
            <tr>
              <th>Step</th>
              <th>标题</th>
              <th>数据文件</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(s, key) in store.manifest.steps" :key="key">
              <td>{{ s.step }}</td>
              <td>{{ s.title }}</td>
              <td>{{ s.files.data }}</td>
            </tr>
          </tbody>
        </table>
      </section>
    </template>
  </section>
</template>

<script setup lang="ts">
import { onMounted } from "vue";
import MetricCard from "../components/MetricCard.vue";
import { useYaccStore } from "../stores/yacc";

const store = useYaccStore();

function fmtTime(iso: string): string {
  if (!iso) {
    return "-";
  }
  return new Date(iso).toLocaleString("zh-CN");
}

onMounted(async () => {
  await store.ensureManifest();
});
</script>
