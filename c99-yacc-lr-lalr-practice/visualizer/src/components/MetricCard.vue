<template>
  <article class="metric-card">
    <p class="metric-title">{{ title }}</p>
    <p class="metric-value">{{ value }}</p>
    <p v-if="hint" class="metric-hint">{{ hint }}</p>
  </article>
</template>

<script setup lang="ts">
defineProps<{
  title: string;
  value: string | number;
  hint?: string;
}>();
</script>
