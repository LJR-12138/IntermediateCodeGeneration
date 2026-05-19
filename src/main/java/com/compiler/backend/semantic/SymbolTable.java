package com.compiler.backend.semantic;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class SymbolTable {

    public static class Symbol {
        private String name;
        private String type;   // C type: "int", "float", "char", etc.
        private SymbolKind kind;

        public enum SymbolKind { VARIABLE, FUNCTION, PARAMETER }

        public Symbol(String name, String type, SymbolKind kind) {
            this.name = name;
            this.type = type;
            this.kind = kind;
        }

        public String getName() { return name; }
        public String getType() { return type; }
        public SymbolKind getKind() { return kind; }

        @Override
        public String toString() {
            return type + " " + name + " (" + kind + ")";
        }
    }

    private final List<Map<String, Symbol>> scopes = new ArrayList<>();

    public SymbolTable() {
        enterScope(); // global scope
    }

    public void enterScope() {
        scopes.add(new HashMap<>());
    }

    public void exitScope() {
        if (scopes.size() > 1) {
            scopes.remove(scopes.size() - 1);
        }
    }

    public void insert(String name, String type, Symbol.SymbolKind kind) {
        currentScope().put(name, new Symbol(name, type, kind));
    }

    public Symbol lookup(String name) {
        for (int i = scopes.size() - 1; i >= 0; i--) {
            Symbol sym = scopes.get(i).get(name);
            if (sym != null) return sym;
        }
        return null;
    }

    public boolean contains(String name) {
        return lookup(name) != null;
    }

    public int getCurrentDepth() {
        return scopes.size();
    }

    public List<Symbol> getAllSymbols() {
        List<Symbol> all = new ArrayList<>();
        for (Map<String, Symbol> scope : scopes) {
            all.addAll(scope.values());
        }
        return all;
    }

    private Map<String, Symbol> currentScope() {
        return scopes.get(scopes.size() - 1);
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder("SymbolTable {\n");
        for (int i = 0; i < scopes.size(); i++) {
            sb.append("  Scope ").append(i).append(": ");
            sb.append(scopes.get(i).values());
            sb.append("\n");
        }
        sb.append("}");
        return sb.toString();
    }
}
