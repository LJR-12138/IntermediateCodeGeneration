# YACC 可视化 Sidecar

## 1. 设计定位

该目录是独立前端子系统，不依赖 `/src` 内部代码。  
数据来源仅为 `visualizer/public/data/v1/<case>/...` 的 JSON 协议文件。

## 2. 数据准备

在仓库根目录执行：

```bash
python3 scripts/yacc_visualizer_prepare.py --case c99
```

默认会把 step3~step6 数据转换到：

```text
visualizer/public/data/v1/c99/
```

## 3. 本地运行

```bash
cd visualizer
npm install
npm run dev
```

浏览器打开：

```text
http://localhost:5174/?case=c99
```

## 4. 页面能力

1. 总览页：查看 manifest 与步骤清单。
2. Step3：符号分布图 + 产生式检索表。
3. Step4：LHS->产生式索引图（支持拖拽/缩放/导出 PNG）。
4. Step5：First 集规模统计图 + 明细表。
5. Step6：I0 出边图 + kernel/closure/goto 明细切换。

## 5. 导出能力

页面支持：

1. 导出当前 step 的 JSON 数据。
2. 导出图形 PNG（ECharts / Cytoscape）。
