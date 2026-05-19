<template>
  <div class="app-shell">
    <AppSidebar />
    <main class="content">
      <header class="topbar">
        <div>
          <p class="topbar-title">YACC 全步骤可视化</p>
          <p class="topbar-sub">Case: {{ caseId }} · 协议版本: v1</p>
        </div>
      </header>
      <RouterView />
    </main>
  </div>
</template>

<script setup lang="ts">
import AppSidebar from "./components/AppSidebar.vue";
import { getCaseId } from "./utils/data";

const caseId = getCaseId();
</script>
