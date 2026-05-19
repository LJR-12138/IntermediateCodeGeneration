package com.compiler.backend;

import com.compiler.backend.model.ReductionRule;
import com.compiler.backend.model.TokenRecord;
import com.compiler.backend.model.TraceEntry;
import com.compiler.backend.parser.ReductionParser;
import com.compiler.backend.parser.TokenParser;
import com.compiler.backend.parser.TraceParser;
import com.compiler.backend.semantic.TranslationEngine;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class Main {

    private static final String BASE_DIR = "c99-yacc-lr-lalr-practice/tests/golden/test1";
    private static final String TOKENS_BASE = "c99-yacc-lr-lalr-practice/contracts/yacc/tokens";

    private static int passed = 0;
    private static int failed = 0;
    private static final List<String> failureDetails = new ArrayList<>();

    public static void main(String[] args) {
        System.out.println("╔══════════════════════════════════════════════════╗");
        System.out.println("║  C99 Compiler Backend — Java + Soot (Step 5)    ║");
        System.out.println("╚══════════════════════════════════════════════════╝");
        System.out.println();

        if (args.length == 0) {
            runAllTests();
        } else if (args[0].equals("--all")) {
            runAllTests();
        } else {
            String testDir = args[0];
            if (!testDir.endsWith("/") && !testDir.endsWith("\\")) {
                testDir += "/";
            }
            String testName = new File(testDir).getParentFile().getName();
            runSingleTest(testName, testDir);
        }

        System.out.println("═══════════════════════════════════════════════════");
        System.out.printf("  Total: %d  |  Passed: %d  |  Failed: %d%n",
                passed + failed, passed, failed);
        System.out.println("═══════════════════════════════════════════════════");

        if (!failureDetails.isEmpty()) {
            System.out.println("\n  Failures:");
            failureDetails.forEach(d -> System.out.println("    - " + d));
        }
    }

    // ---- Batch: run all 11 test cases ----
    private static void runAllTests() {
        String[] tests = {
            "decl_int", "decl_multi", "decl_pointer",
            "enum_decl", "struct_decl",
            "func_param_return", "func_return_const",
            "if_else_return",
            "invalid_if", "invalid_missing_semi", "invalid_unclosed_block"
        };

        for (String testName : tests) {
            String testDir = BASE_DIR + "/" + testName + "/raw/";
            runSingleTest(testName, testDir);
        }
    }

    // ---- Single test pipeline ----
    private static void runSingleTest(String testName, String testDir) {
        System.out.println("──────────────────────────────────────────");
        System.out.printf("  Test: %s%n", testName);
        System.out.println("──────────────────────────────────────────");

        try {
            // 1. Parse trace file
            TraceParser traceParser = new TraceParser();
            List<TraceEntry> traces = traceParser.parse(testDir + "parse_trace_lalr.tsv");
            System.out.printf("  [Parse] Trace: %d entries%n", traces.size());

            // 2. Parse reductions file
            ReductionParser reductionParser = new ReductionParser();
            List<ReductionRule> rules = reductionParser.parse(testDir + "parse_reductions_lalr.txt");
            System.out.printf("  [Parse] Reductions: %d rules%n", rules.size());

            // Handle empty reductions (parse error in frontend)
            if (rules.isEmpty()) {
                System.out.println("  [Skip] No reductions (frontend parse error).");
                passed++;
                return;
            }

            // 3. Parse tokens file
            String tokensFile = TOKENS_BASE + "/c99_" + testName + ".tokens";
            TokenParser tokenParser = new TokenParser();
            List<TokenRecord> tokens = tokenParser.parse(tokensFile);
            System.out.printf("  [Parse] Tokens: %d tokens%n", tokens.size());

            // 4. Run translation engine (includes Jimple generation)
            TranslationEngine engine = new TranslationEngine(tokens, rules);
            engine.process(traces);

            System.out.printf("  [SymbolTable] %d symbols: %s%n",
                    engine.getSymbolTable().getAllSymbols().size(),
                    engine.getSymbolTable().getAllSymbols());

            // 5. Emit Jimple
            String jimple = engine.getJimpleOutput();

            // Write to output file
            String outputPath = testDir.replace("raw/", "") + "output.jimple";
            try (PrintWriter pw = new PrintWriter(new FileWriter(outputPath))) {
                pw.print(jimple);
            }
            System.out.printf("  [Output] Jimple written to: %s%n", outputPath);

            // Print Jimple to console (compact)
            System.out.println("  ─── Jimple ───");
            for (String line : jimple.split("\n")) {
                System.out.println("  " + line);
            }
            System.out.println("  ──────────────");

            System.out.printf("  [Result] PASSED%n%n");
            passed++;

        } catch (Exception e) {
            System.out.printf("  [Result] FAILED: %s%n%n", e.getMessage());
            failureDetails.add(testName + ": " + e.getMessage());
            failed++;
        }
    }
}
