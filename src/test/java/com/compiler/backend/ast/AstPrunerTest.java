package com.compiler.backend.ast;

import static org.junit.jupiter.api.Assertions.*;

import java.util.HashMap;
import org.junit.jupiter.api.Test;

class AstPrunerTest {

    @Test
    void pruneAndPrint() throws Exception {
        String path = getClass().getResource("/sample_ast_lalr.json").getPath();
        AstJsonParser.Result parsed = AstJsonParser.parse(path);
        AstPruner pruner = new AstPruner(parsed.nodes);
        TranslationUnitNode ast = pruner.prune(parsed.rootId);

        assertNotNull(ast);
        assertEquals("TranslationUnit", ast.nodeType);
        assertFalse(ast.declarations.isEmpty(), "AST should have at least one declaration");

        // All declarations should be VarDeclNodes (sample has no functions)
        for (AstNode decl : ast.declarations) {
            assertTrue(decl instanceof VarDeclNode,
                "Expected VarDeclNode, got " + decl.getClass().getSimpleName() + ": " + decl.nodeType);
        }

        // Count: int a, int b, char *p, int x = 4 declarations
        assertEquals(4, ast.declarations.size(), "Expected 4 variable declarations");

        // Verify specific declarations
        VarDeclNode a = (VarDeclNode) ast.declarations.get(0);
        assertEquals("int", a.typeName);
        assertEquals("a", a.varName);
        assertEquals(0, a.pointerDepth);

        VarDeclNode b = (VarDeclNode) ast.declarations.get(1);
        assertEquals("int", b.typeName);
        assertEquals("b", b.varName);

        VarDeclNode p = (VarDeclNode) ast.declarations.get(2);
        assertEquals("char", p.typeName);
        assertEquals("p", p.varName);
        assertEquals(1, p.pointerDepth);  // char *p

        VarDeclNode x = (VarDeclNode) ast.declarations.get(3);
        assertEquals("int", x.typeName);
        assertEquals("x", x.varName);

        // ── Print tree ────────────────────────────────────────────
        System.out.println();
        System.out.println("=== Pruned AST (indented) ===");
        AstPrinter.printTree(ast);

        // ── Graphviz DOT ──────────────────────────────────────────
        System.out.println();
        System.out.println("=== Graphviz DOT ===");
        String dot = AstPrinter.toDot(ast);
        System.out.println(dot);
    }

    @Test
    void verifyCstNodeCounts() throws Exception {
        String path = getClass().getResource("/sample_ast_lalr.json").getPath();
        AstJsonParser.Result parsed = AstJsonParser.parse(path);
        assertEquals(44, parsed.nodes.size());

        long leaves = parsed.nodes.values().stream().filter(JsonNode::isLeaf).count();
        assertEquals(12, leaves);  // 3 INT, 1 CHAR, 4 IDENTIFIER, 1 ',' 2 ';', 1 '*'
    }
}
