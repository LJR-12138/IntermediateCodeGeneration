package com.compiler.backend;

import com.compiler.backend.ast.AstJsonParser;
import com.compiler.backend.ast.AstPrinter;
import com.compiler.backend.ast.AstPruner;
import com.compiler.backend.ast.TranslationUnitNode;
import com.compiler.backend.config.LanguageConfig;
import com.compiler.backend.generator.JimpleGenerator;
import com.compiler.backend.semantic.SymbolTable;

import soot.Scene;
import soot.SootClass;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.util.HashMap;

public class Main {

    public static void main(String[] args) {
        // ── Parse CLI arguments ─────────────────────────────────────
        String jsonPath = null;
        String outputPath = null;
        String semConfigPath = null;

        for (int i = 0; i < args.length; i++) {
            switch (args[i]) {
                case "--sem-config" -> {
                    if (i + 1 < args.length) semConfigPath = args[++i];
                }
                default -> {
                    if (jsonPath == null) jsonPath = args[i];
                    else if (outputPath == null) outputPath = args[i];
                }
            }
        }

        if (jsonPath == null) {
            System.out.println("Usage: java -cp <classpath> com.compiler.backend.Main <ast_lalr.json> [output.jimple] [--sem-config path/to/lang.sem.json]");
            System.exit(1);
        }

        // ── Load language config (C99 default if no file provided) ──
        LanguageConfig config;
        try {
            if (semConfigPath != null) {
                System.out.println("[Config] Loading semantic config from: " + semConfigPath);
                config = LanguageConfig.parse(semConfigPath);
            } else {
                config = LanguageConfig.c99Default();
            }
            System.out.println("[Config] Language: " + config.getLanguage());
        } catch (Exception e) {
            System.err.println("Error loading semantic config: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
            return;
        }

        if (outputPath == null) {
            File inputFile = new File(jsonPath);
            outputPath = new File(inputFile.getParentFile(), "output.jimple").getPath();
        }

        try {
            // Step A: Parse AST JSON
            System.out.println("[Step A] Parsing: " + jsonPath);
            AstJsonParser.Result result = AstJsonParser.parse(jsonPath);
            System.out.println("  root=" + result.rootId + ", nodes=" + result.nodes.size());

            // Step B: Prune CST → clean OOP AST
            System.out.println("[Step B] Pruning CST...");
            AstPruner pruner = new AstPruner(result.nodes, config);
            TranslationUnitNode astRoot = pruner.prune(result.rootId);
            System.out.println("  declarations=" + astRoot.declarations.size());

            // Step C: Print AST
            System.out.println("[Step C] AST structure:");
            AstPrinter.printTree(astRoot);
            System.out.println();

            // Step D: Create SymbolTable
            System.out.println("[Step D] Initializing SymbolTable...");
            SymbolTable symbolTable = new SymbolTable(config);

            // Step E: Generate Jimple
            System.out.println("[Step E] Generating Jimple...");
            JimpleGenerator generator = new JimpleGenerator(symbolTable, config);
            generator.generate(astRoot);

            // Output Jimple
            System.out.println("[Output] Writing Jimple to: " + outputPath);
            SootClass sClass = Scene.v().getSootClass(config.getOutputClassName());
            try (PrintWriter writer = new PrintWriter(
                    new OutputStreamWriter(new FileOutputStream(outputPath)))) {
                soot.Printer.v().printTo(sClass, writer);
                System.out.println("Jimple generated successfully at: " + outputPath);
            }

        } catch (Exception e) {
            System.err.println("Error: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }
}
