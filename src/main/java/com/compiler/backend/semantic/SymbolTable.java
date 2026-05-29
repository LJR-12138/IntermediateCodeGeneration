package com.compiler.backend.semantic;

import com.compiler.backend.config.LanguageConfig;

import java.util.ArrayDeque;
import java.util.Deque;
import java.util.HashMap;
import java.util.Map;
import soot.Local;

/**
 * Scope-sensitive symbol table with stack-based nesting.
 *
 * Each scope is a Map from C variable name → Soot Local.
 * {@code lookup()} searches from innermost to outermost scope.
 * {@code put()} inserts into the current (topmost) scope, rejecting redefinitions.
 */
public class SymbolTable {

    private final LanguageConfig config;

    private final Deque<Map<String, Local>> scopeStack = new ArrayDeque<>();

    public SymbolTable(LanguageConfig config) {
        this.config = config;
        enterScope(); // global scope
    }

    /** Backward-compatible constructor using C99 defaults. */
    public SymbolTable() {
        this(LanguageConfig.c99Default());
    }

    // ─── Scope management ──────────────────────────────────────────

    /** Push a new nested scope (e.g. on entering a compound statement). */
    public void enterScope() {
        scopeStack.push(new HashMap<>());
    }

    /** Pop the current scope (e.g. on leaving a compound statement). */
    public void exitScope() {
        if (scopeStack.size() > 1) {
            scopeStack.pop();
        }
    }

    public int currentDepth() {
        return scopeStack.size();
    }

    // ─── Registration ──────────────────────────────────────────────

    /**
     * Bind varName → local in the current (innermost) scope.
     * @throws SemanticException if varName is already declared in the current scope.
     */
    public void put(String varName, Local local) {
        Map<String, Local> current = scopeStack.peek();
        if (current.containsKey(varName)) {
            throw new SemanticException(
                "redefinition of '" + varName + "' in the same scope");
        }
        current.put(varName, local);
    }

    // ─── Lookup ────────────────────────────────────────────────────

    /**
     * Search for varName from innermost to outermost scope.
     * @return the corresponding Soot Local
     * @throws SemanticException if the variable is not declared in any active scope.
     */
    public Local lookup(String varName) {
        for (Map<String, Local> scope : scopeStack) {
            Local l = scope.get(varName);
            if (l != null) return l;
        }
        throw new SemanticException("undeclared variable '" + varName + "'");
    }

    /**
     * Returns true if varName is visible in any active scope.
     */
    public boolean contains(String varName) {
        for (Map<String, Local> scope : scopeStack) {
            if (scope.containsKey(varName)) return true;
        }
        return false;
    }

    /**
     * Returns true if name is a known C builtin / library function
     * (e.g. printf, scanf) that should not trigger an undeclared-variable error.
     */
    public boolean isBuiltin(String name) {
        return config.isBuiltin(name);
    }

    // ─── Introspection ─────────────────────────────────────────────

    /**
     * Returns all variable names visible across all scopes.
     * @deprecated Will be removed in Stage 4.
     */
    @Deprecated
    public java.util.List<String> getAllSymbols() {
        java.util.List<String> all = new java.util.ArrayList<>();
        for (Map<String, Local> scope : scopeStack) {
            all.addAll(scope.keySet());
        }
        return all;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder("SymbolTable {\n");
        int depth = 0;
        // Iterate from outermost (bottom) to innermost (top)
        var it = scopeStack.descendingIterator();
        while (it.hasNext()) {
            Map<String, Local> scope = it.next();
            sb.append("  scope ").append(depth).append(": ");
            sb.append(scope.keySet());
            sb.append("\n");
            depth++;
        }
        sb.append("}");
        return sb.toString();
    }
}
