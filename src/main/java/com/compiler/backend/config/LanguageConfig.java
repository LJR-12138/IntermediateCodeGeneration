package com.compiler.backend.config;

import com.google.gson.Gson;
import com.google.gson.annotations.SerializedName;
import soot.*;

import java.io.FileReader;
import java.io.IOException;
import java.io.Reader;
import java.util.*;

/**
 * Holds all language-specific mappings (production names, tokens, type system).
 *
 * Two construction paths:
 *   1. LanguageConfig.c99Default()  — hardcoded C99 defaults (backward compat)
 *   2. LanguageConfig.parse(path)  — reads a .sem.json config file
 *
 * The config maps "semantic roles" (camelCase, language-neutral) to the actual
 * CST type/token strings produced by yacc for a given grammar.  Swap the .sem.json
 * file alongside .l and .y to support a different language with zero Java changes.
 */
public class LanguageConfig {

    private static final Gson GSON = new Gson();

    // ── Parsed (or default-constructed) raw data ────────────────
    private String language;
    private Map<String, String> productions;
    private RawTokens tokens;
    private Map<String, RawTypeMapping> typeSystem;
    private List<String> builtinFunctions;
    private String outputClassName;

    // ── Derived convenience sets (built after parse/default) ────
    private Set<String> keywordTokenSet;
    private Set<String> typeKeywordSet;
    private Set<String> allBinaryOpTokens;
    private Set<String> allUnaryOpTokens;
    private Set<String> postfixOpTokens;
    private Set<String> structuralTypes;

    // ── Internal JSON structure ─────────────────────────────────

    private static class RawTokens {
        Map<String, String> keywords;
        List<String> typeKeywords;
        @SerializedName("leafTypes")
        Map<String, String> leafTypes;
        List<String> binaryOperatorTokens;
        List<String> unaryOperatorTokens;
        List<String> postfixOperatorTokens;
    }

    private static class RawTypeMapping {
        String sootType;
        List<String> aliases;

        RawTypeMapping() {}
        RawTypeMapping(String sootType, List<String> aliases) {
            this.sootType = sootType;
            this.aliases = aliases;
        }
    }

    // ── Constructors ────────────────────────────────────────────

    private LanguageConfig() {}

    /**
     * Returns a LanguageConfig pre-filled with C99 mappings.
     */
    public static LanguageConfig c99Default() {
        LanguageConfig cfg = new LanguageConfig();
        cfg.language = "c99";

        cfg.productions = new LinkedHashMap<>();
        cfg.productions.put("translationUnit",     "translation_unit");
        cfg.productions.put("functionDefinition",  "function_definition");
        cfg.productions.put("declaration",         "declaration");
        cfg.productions.put("compoundStatement",   "compound_statement");
        cfg.productions.put("selectionStatement",  "selection_statement");
        cfg.productions.put("iterationStatement",  "iteration_statement");
        cfg.productions.put("jumpStatement",       "jump_statement");
        cfg.productions.put("declarator",          "declarator");
        cfg.productions.put("directDeclarator",    "direct_declarator");
        cfg.productions.put("pointer",             "pointer");
        cfg.productions.put("parameterTypeList",   "parameter_type_list");
        cfg.productions.put("parameterList",       "parameter_list");
        cfg.productions.put("parameterDeclaration", "parameter_declaration");
        cfg.productions.put("argumentExpressionList", "argument_expression_list");
        cfg.productions.put("initDeclaratorList",  "init_declarator_list");
        cfg.productions.put("initDeclarator",      "init_declarator");
        cfg.productions.put("initializer",         "initializer");
        cfg.productions.put("initializerList",     "initializer_list");
        cfg.productions.put("externalDeclaration", "external_declaration");

        cfg.tokens = new RawTokens();
        cfg.tokens.keywords = new LinkedHashMap<>();
        cfg.tokens.keywords.put("if",     "IF");
        cfg.tokens.keywords.put("else",   "ELSE");
        cfg.tokens.keywords.put("while",  "WHILE");
        cfg.tokens.keywords.put("for",    "FOR");
        cfg.tokens.keywords.put("return", "RETURN");

        cfg.tokens.typeKeywords = List.of(
            "INT", "CHAR", "VOID", "FLOAT", "DOUBLE",
            "SHORT", "LONG", "SIGNED", "UNSIGNED");

        cfg.tokens.leafTypes = new LinkedHashMap<>();
        cfg.tokens.leafTypes.put("identifier",      "IDENTIFIER");
        cfg.tokens.leafTypes.put("integerConstant", "CONSTANT");
        cfg.tokens.leafTypes.put("stringLiteral",   "STRING_LITERAL");

        cfg.tokens.binaryOperatorTokens = List.of(
            "EQ_OP", "NE_OP", "LE_OP", "GE_OP",
            "AND_OP", "OR_OP", "LEFT_OP", "RIGHT_OP");
        cfg.tokens.unaryOperatorTokens = List.of("INC_OP", "DEC_OP");
        cfg.tokens.postfixOperatorTokens = List.of("INC_OP", "DEC_OP");

        cfg.typeSystem = new LinkedHashMap<>();
        cfg.typeSystem.put("int",    new RawTypeMapping("IntType",
            List.of("signed", "unsigned", "short", "long")));
        cfg.typeSystem.put("char",   new RawTypeMapping("ByteType",   List.of()));
        cfg.typeSystem.put("float",  new RawTypeMapping("FloatType",  List.of()));
        cfg.typeSystem.put("double", new RawTypeMapping("DoubleType", List.of()));
        cfg.typeSystem.put("void",   new RawTypeMapping("VoidType",   List.of()));

        cfg.builtinFunctions = List.of("printf", "scanf");
        cfg.outputClassName = "GeneratedApp";

        cfg.buildDerivedSets();
        return cfg;
    }

    /**
     * Parses a .sem.json file at the given path.
     */
    public static LanguageConfig parse(String jsonPath) throws IOException {
        try (Reader reader = new FileReader(jsonPath)) {
            LanguageConfig cfg = GSON.fromJson(reader, LanguageConfig.class);
            cfg.validate();
            cfg.buildDerivedSets();
            return cfg;
        }
    }

    // ── Validation ──────────────────────────────────────────────

    private void validate() {
        if (productions == null) productions = Map.of();
        if (tokens == null)
            throw new IllegalArgumentException("Missing required section: tokens");
        if (tokens.keywords == null) tokens.keywords = Map.of();
        if (tokens.typeKeywords == null) tokens.typeKeywords = List.of();
        if (tokens.leafTypes == null) tokens.leafTypes = Map.of();
        if (tokens.binaryOperatorTokens == null) tokens.binaryOperatorTokens = List.of();
        if (tokens.unaryOperatorTokens == null) tokens.unaryOperatorTokens = List.of();
        if (tokens.postfixOperatorTokens == null) tokens.postfixOperatorTokens = List.of();
        if (typeSystem == null) typeSystem = Map.of();
        if (builtinFunctions == null) builtinFunctions = List.of();
        if (outputClassName == null) outputClassName = "GeneratedApp";
    }

    private void buildDerivedSets() {
        keywordTokenSet = new HashSet<>(tokens.keywords.values());
        typeKeywordSet = new HashSet<>(tokens.typeKeywords);

        // Merge universal punctuation operators with language-specific named tokens
        Set<String> punctBinary = Set.of(
            "'+'", "'-'", "'*'", "'/'", "'%'",
            "'<'", "'>'", "'<='", "'>='", "'=='", "'!='",
            "'&&'", "'||'", "'&'", "'|'", "'^'",
            "'<<'", "'>>'", "'='");
        allBinaryOpTokens = new HashSet<>(punctBinary);
        allBinaryOpTokens.addAll(tokens.binaryOperatorTokens);

        Set<String> punctUnary = Set.of("'*'", "'&'", "'-'", "'!'", "'~'", "'++'", "'--'");
        allUnaryOpTokens = new HashSet<>(punctUnary);
        allUnaryOpTokens.addAll(tokens.unaryOperatorTokens);

        postfixOpTokens = new HashSet<>(tokens.postfixOperatorTokens);

        structuralTypes = Set.of(
            prod("translationUnit"), prod("functionDefinition"), prod("declaration"),
            prod("initDeclaratorList"), prod("initDeclarator"),
            prod("compoundStatement"), prod("selectionStatement"), prod("iterationStatement"),
            prod("jumpStatement"), prod("declarator"), prod("directDeclarator"), prod("pointer"));
    }

    // ── Public accessors ────────────────────────────────────────

    public String getLanguage() { return language; }

    /** Returns the CST production name for a semantic role. */
    public String prod(String role) {
        return productions.getOrDefault(role, role);
    }

    /** Returns the CST keyword token for a semantic role (e.g. "if" → "IF"). */
    public String kw(String keyword) {
        return tokens.keywords.getOrDefault(keyword, keyword.toUpperCase());
    }

    public boolean isKeywordToken(String tokenType) {
        return keywordTokenSet.contains(tokenType);
    }

    public boolean isTypeKeyword(String tokenType) {
        return typeKeywordSet.contains(tokenType);
    }

    public Set<String> getTypeKeywords() { return typeKeywordSet; }

    public String identifierLeafType() {
        return tokens.leafTypes.getOrDefault("identifier", "IDENTIFIER");
    }

    public String integerConstantLeafType() {
        return tokens.leafTypes.getOrDefault("integerConstant", "CONSTANT");
    }

    public String stringLiteralLeafType() {
        return tokens.leafTypes.getOrDefault("stringLiteral", "STRING_LITERAL");
    }

    public boolean isBinaryOperatorToken(String tokenType) {
        return allBinaryOpTokens.contains(tokenType);
    }

    public boolean isUnaryOperatorToken(String tokenType) {
        return allUnaryOpTokens.contains(tokenType);
    }

    public boolean isPostfixOperatorToken(String tokenType) {
        return postfixOpTokens.contains(tokenType);
    }

    /** Production CST names that should NOT be skip-wrapped. */
    public Set<String> getStructuralTypes() { return structuralTypes; }

    public boolean isBuiltin(String name) {
        return builtinFunctions.contains(name);
    }

    public String getOutputClassName() { return outputClassName; }

    public boolean isVoidType(String cstTypeLexeme) {
        RawTypeMapping m = typeSystem.get(cstTypeLexeme);
        if (m != null && "VoidType".equals(m.sootType)) return true;
        return typeSystem.values().stream()
                .filter(t -> "VoidType".equals(t.sootType))
                .anyMatch(t -> t.aliases != null && t.aliases.contains(cstTypeLexeme));
    }

    /**
     * Resolves a CST type lexeme (e.g. "int", "unsigned") to a Soot Type,
     * optionally wrapping in ArrayType for the given pointer depth.
     */
    public soot.Type resolveSootType(String cstTypeLexeme, int pointerDepth) {
        soot.Type base = resolveBaseSootType(cstTypeLexeme);
        if (pointerDepth > 0) {
            return ArrayType.v(base, pointerDepth);
        }
        return base;
    }

    private soot.Type resolveBaseSootType(String cstTypeLexeme) {
        // Direct match
        RawTypeMapping mapping = typeSystem.get(cstTypeLexeme);
        if (mapping != null) {
            return sootTypeFromName(mapping.sootType);
        }
        // Alias match
        for (RawTypeMapping m : typeSystem.values()) {
            if (m.aliases != null && m.aliases.contains(cstTypeLexeme)) {
                return sootTypeFromName(m.sootType);
            }
        }
        return IntType.v(); // fallback
    }

    private static soot.Type sootTypeFromName(String name) {
        return switch (name) {
            case "IntType"    -> IntType.v();
            case "ByteType"   -> ByteType.v();
            case "FloatType"  -> FloatType.v();
            case "DoubleType" -> DoubleType.v();
            case "VoidType"   -> VoidType.v();
            default           -> IntType.v();
        };
    }
}
