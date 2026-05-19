import type { Manifest, StepData } from "../types";

const DEFAULT_CASE = "c99";

function resolveCaseId(): string {
  const query = new URLSearchParams(window.location.search);
  return query.get("case") ?? DEFAULT_CASE;
}

export function getCaseId(): string {
  return resolveCaseId();
}

export async function loadManifest(): Promise<Manifest> {
  const caseId = resolveCaseId();
  const res = await fetch(`/data/v1/${caseId}/manifest.json`);
  if (!res.ok) {
    throw new Error(`加载 manifest 失败: ${res.status}`);
  }
  return (await res.json()) as Manifest;
}

export async function loadStepData(manifest: Manifest, step: number): Promise<StepData> {
  const stepInfo = manifest.steps[String(step)];
  if (!stepInfo) {
    throw new Error(`manifest 未包含 step${step}`);
  }
  const caseId = manifest.case_id;
  const dataPath = stepInfo.files.data;
  const res = await fetch(`/data/v1/${caseId}/${dataPath}`);
  if (!res.ok) {
    throw new Error(`加载 step${step} 数据失败: ${res.status}`);
  }
  return (await res.json()) as StepData;
}

export function downloadTextFile(filename: string, content: string): void {
  const blob = new Blob([content], { type: "text/plain;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}

export function downloadJsonFile(filename: string, payload: unknown): void {
  downloadTextFile(filename, JSON.stringify(payload, null, 2));
}
