package com.compiler.backend.semantic;

import com.compiler.backend.model.ReductionRule;
import com.compiler.backend.model.TokenRecord;
import com.compiler.backend.model.TraceEntry;

import java.util.*;

public class TranslationEngine {

    private final List<TokenRecord> tokens;
    private final Map<Integer, ReductionRule> ruleMap;
    private final Stack<SemanticValue> semanticStack;
    private final SymbolTable symbolTable;
    private final JimpleGenerator jimpleGen;
    private final List<String> actionLog;
    private boolean jimpleInitialized;

    public TranslationEngine(List<TokenRecord> tokens, List<ReductionRule> rules) {
        this(tokens, rules, new JimpleGenerator());
    }

    public TranslationEngine(List<TokenRecord> tokens, List<ReductionRule> rules, JimpleGenerator jimpleGen) {
        this.tokens = tokens;
        this.ruleMap = new HashMap<>();
        for (ReductionRule r : rules) {
            ruleMap.put(r.getId(), r);
        }
        this.semanticStack = new Stack<>();
        this.symbolTable = new SymbolTable();
        this.jimpleGen = jimpleGen;
        this.actionLog = new ArrayList<>();
        this.jimpleInitialized = false;
    }

    public void process(List<TraceEntry> trace) {
        if (!jimpleInitialized) {
            jimpleGen.initialize();
            jimpleInitialized = true;
        }
        for (TraceEntry entry : trace) {
            if (entry.isShift()) {
                handleShift(entry);
            } else if (entry.isReduce()) {
                handleReduce(entry);
            }
            // acc = accept, nothing to do
        }
    }

    // ---- Shift: push token semantic value onto stack ----
    private void handleShift(TraceEntry entry) {
        int idx = entry.getInputIndex();
        if (idx < tokens.size()) {
            TokenRecord tok = tokens.get(idx);
            SemanticValue val = tokenToSemanticValue(tok);
            semanticStack.push(val);
            log("SHIFT  %s('%s') -> push %s", tok.getType(), tok.getLexeme(), val);
        } else {
            log("SHIFT  (no token at index %d)", idx);
        }
    }

    private SemanticValue tokenToSemanticValue(TokenRecord tok) {
        String type = tok.getType();
        String lexeme = tok.getLexeme();

        return switch (type) {
            case "INT", "FLOAT", "DOUBLE", "CHAR", "SHORT", "LONG", "VOID",
                 "SIGNED", "UNSIGNED", "BOOL", "COMPLEX", "IMAGINARY",
                 "STRUCT", "UNION", "ENUM", "TYPE_NAME" ->
                    SemanticValue.typeValue(lexeme);

            case "IDENTIFIER" ->
                    SemanticValue.identifierValue(lexeme);

            case "CONSTANT", "STRING_LITERAL" ->
                    SemanticValue.constantValue(lexeme);

            default ->
                    SemanticValue.punctuationValue(stripQuotes(lexeme));
        };
    }

    // ---- Reduce: pop RHS, compute semantic value, push result ----
    private void handleReduce(TraceEntry entry) {
        int ruleId = entry.getProductionId();
        ReductionRule rule = ruleMap.get(ruleId);

        if (rule == null) {
            log("REDUCE #%d (unknown rule) - skipping", ruleId);
            return;
        }

        String lhs = rule.getLhs();
        List<String> rhs = rule.getRhs();
        int n = rhs.size();

        List<SemanticValue> popped = new ArrayList<>();
        for (int i = 0; i < n; i++) {
            if (!semanticStack.isEmpty()) {
                popped.add(0, semanticStack.pop()); // prepend to preserve order
            }
        }

        SemanticValue result = computeSemantic(lhs, rhs, popped);
        semanticStack.push(result);

        log("REDUCE #%d %s -> %s | pop(%d)=%s push=%s",
                ruleId, lhs, rhs, n, popped, result);
    }

    // ---- Semantic computation by pattern matching on LHS/RHS ----
    private SemanticValue computeSemantic(String lhs, List<String> rhs, List<SemanticValue> vals) {
        String rhsKey = String.join(" ", rhs);

        // -- type_specifier -> INT | FLOAT | CHAR | VOID | ... --
        if (lhs.equals("type_specifier")) {
            return vals.get(0); // pass through the type value
        }

        // -- declaration_specifiers -> type_specifier | storage_class_specifier | type_qualifier --
        if (lhs.equals("declaration_specifiers") && rhs.size() == 1) {
            return vals.get(0); // pass through
        }
        if (lhs.equals("declaration_specifiers") && rhs.size() == 2) {
            return vals.get(0).isType() ? vals.get(0) : vals.get(1);
        }

        // -- direct_declarator -> IDENTIFIER --
        if (lhs.equals("direct_declarator") && rhsKey.equals("IDENTIFIER")) {
            return vals.get(0); // pass through identifier
        }

        // -- declarator -> direct_declarator | pointer direct_declarator --
        if (lhs.equals("declarator")) {
            SemanticValue declarator = vals.get(vals.size() - 1); // last val is direct_declarator
            return declarator; // pass through for now (skip pointer for Step 3)
        }

        // -- init_declarator -> declarator --
        if (lhs.equals("init_declarator") && rhs.size() == 1) {
            return vals.get(0); // pass through identifier
        }
        // init_declarator -> declarator '=' initializer (skip for now)
        if (lhs.equals("init_declarator") && rhs.size() == 3) {
            return vals.get(0); // just return the declarator
        }

        // -- init_declarator_list -> init_declarator --
        if (lhs.equals("init_declarator_list") && rhs.size() == 1) {
            SemanticValue v = vals.get(0);
            // Convert single identifier to list
            if (v.isIdentifier()) {
                return SemanticValue.singleDeclList(v.getStringValue());
            }
            return v;
        }
        // -- init_declarator_list -> init_declarator_list ',' init_declarator --
        if (lhs.equals("init_declarator_list") && rhs.size() == 3) {
            SemanticValue listVal = vals.get(0); // existing list
            SemanticValue newDecl = vals.get(2); // new init_declarator (identifier)
            if (newDecl.isIdentifier()) {
                listVal.addToDeclList(newDecl.getStringValue());
            }
            return listVal;
        }

        // -- declaration -> declaration_specifiers init_declarator_list ';' --
        if (lhs.equals("declaration") && rhsKey.contains("init_declarator_list")) {
            SemanticValue typeVal = vals.get(0);
            SemanticValue declList = vals.get(1);
            String typeName = typeVal.getStringValue();

            if (declList.isDeclList() && declList.getIdentifiers() != null) {
                for (String name : declList.getIdentifiers()) {
                    symbolTable.insert(name, typeName, SymbolTable.Symbol.SymbolKind.VARIABLE);
                    jimpleGen.declareLocal(name, typeName);
                    log("  DECLARE: %s %s [Jimple local created]", typeName, name);
                }
            }
            return SemanticValue.emptyValue();
        }
        // -- declaration -> declaration_specifiers ';' --
        if (lhs.equals("declaration") && rhsKey.equals("declaration_specifiers ';'")) {
            return SemanticValue.emptyValue();
        }

        // -- external_declaration -> declaration | function_definition --
        if (lhs.equals("external_declaration")) {
            return vals.get(0); // pass through
        }

        // -- translation_unit -> external_declaration --
        // -- translation_unit -> translation_unit external_declaration --
        if (lhs.equals("translation_unit")) {
            return vals.size() > 0 ? vals.get(vals.size() - 1) : SemanticValue.emptyValue();
        }

        // -- primary_expression -> IDENTIFIER | CONSTANT --
        if (lhs.equals("primary_expression")) {
            return vals.get(0); // pass through
        }

        // -- expression chains: pass through --
        if (isChainRule(lhs, rhs)) {
            return vals.get(0);
        }

        // Default: pass-through
        log("  (default pass-through for %s)", lhs);
        return vals.isEmpty() ? SemanticValue.emptyValue() : vals.get(0);
    }

    private boolean isChainRule(String lhs, List<String> rhs) {
        return rhs.size() == 1 && !isTerminal(rhs.get(0));
    }

    private boolean isTerminal(String symbol) {
        return symbol.startsWith("'") || Character.isUpperCase(symbol.charAt(0));
    }

    private String stripQuotes(String s) {
        if (s.length() >= 2 && s.startsWith("'") && s.endsWith("'")) {
            return s.substring(1, s.length() - 1);
        }
        return s;
    }

    private void log(String fmt, Object... args) {
        actionLog.add(String.format(fmt, args));
    }

    // ---- Accessors ----
    public SymbolTable getSymbolTable() { return symbolTable; }

    public List<String> getActionLog() { return actionLog; }

    public String getJimpleOutput() { return jimpleGen.emitJimple(); }

    public void printActionLog() {
        for (String line : actionLog) {
            System.out.println("  " + line);
        }
    }
}
