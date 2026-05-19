import { defineStore } from "pinia";
import type { Manifest, StepData } from "../types";
import { loadManifest, loadStepData } from "../utils/data";

interface YaccState {
  loading: boolean;
  error: string;
  manifest: Manifest | null;
  stepData: Record<number, StepData>;
}

export const useYaccStore = defineStore("yacc", {
  state: (): YaccState => ({
    loading: false,
    error: "",
    manifest: null,
    stepData: {}
  }),
  actions: {
    async ensureManifest(): Promise<void> {
      if (this.manifest) {
        return;
      }
      this.loading = true;
      this.error = "";
      try {
        this.manifest = await loadManifest();
      } catch (err) {
        this.error = err instanceof Error ? err.message : String(err);
      } finally {
        this.loading = false;
      }
    },
    async ensureStep(step: number): Promise<void> {
      await this.ensureManifest();
      if (!this.manifest || this.stepData[step]) {
        return;
      }
      this.loading = true;
      this.error = "";
      try {
        this.stepData[step] = await loadStepData(this.manifest, step);
      } catch (err) {
        this.error = err instanceof Error ? err.message : String(err);
      } finally {
        this.loading = false;
      }
    }
  }
});
