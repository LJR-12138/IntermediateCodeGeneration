package com.compiler.backend.semantic;

import java.util.ArrayList;
import java.util.List;
import java.util.StringJoiner;

public class SemanticValue {
    public enum Kind {
        TYPE,        // type specifier (int, float, etc.)
        IDENTIFIER,  // variable/function name
        CONSTANT,    // numeric/string constant
        DECLARATOR,  // processed declarator
        DECL_LIST,   // list of identifiers in a declaration
        STATEMENT,   // a statement
        PUNCTUATION, // terminals like ';', ',', '(', etc. - no semantic value
        EMPTY        // epsilon / empty production
    }

    private Kind kind;
    private String stringValue;    // For TYPE, IDENTIFIER, CONSTANT
    private List<String> identifiers; // For DECL_LIST (list of variable names)

    public SemanticValue(Kind kind) {
        this.kind = kind;
    }

    public static SemanticValue typeValue(String typeName) {
        SemanticValue v = new SemanticValue(Kind.TYPE);
        v.stringValue = typeName;
        return v;
    }

    public static SemanticValue identifierValue(String name) {
        SemanticValue v = new SemanticValue(Kind.IDENTIFIER);
        v.stringValue = name;
        return v;
    }

    public static SemanticValue constantValue(String value) {
        SemanticValue v = new SemanticValue(Kind.CONSTANT);
        v.stringValue = value;
        return v;
    }

    public static SemanticValue punctuationValue(String lexeme) {
        SemanticValue v = new SemanticValue(Kind.PUNCTUATION);
        v.stringValue = lexeme;
        return v;
    }

    public static SemanticValue emptyValue() {
        return new SemanticValue(Kind.EMPTY);
    }

    // DECL_LIST helpers
    public static SemanticValue declListValue(List<String> names) {
        SemanticValue v = new SemanticValue(Kind.DECL_LIST);
        v.identifiers = new ArrayList<>(names);
        return v;
    }

    public static SemanticValue singleDeclList(String name) {
        List<String> names = new ArrayList<>();
        names.add(name);
        return declListValue(names);
    }

    public void addToDeclList(String name) {
        if (identifiers == null) {
            identifiers = new ArrayList<>();
        }
        identifiers.add(name);
    }

    public Kind getKind() { return kind; }

    public String getStringValue() { return stringValue; }

    public List<String> getIdentifiers() { return identifiers; }

    public boolean isType() { return kind == Kind.TYPE; }

    public boolean isIdentifier() { return kind == Kind.IDENTIFIER; }

    public boolean isPunctuation() { return kind == Kind.PUNCTUATION; }

    public boolean isDeclList() { return kind == Kind.DECL_LIST; }

    @Override
    public String toString() {
        switch (kind) {
            case TYPE:
                return "Type(" + stringValue + ")";
            case IDENTIFIER:
                return "Id(" + stringValue + ")";
            case CONSTANT:
                return "Const(" + stringValue + ")";
            case DECL_LIST:
                StringJoiner sj = new StringJoiner(", ");
                if (identifiers != null) identifiers.forEach(sj::add);
                return "DeclList[" + sj + "]";
            case PUNCTUATION:
                return "Punct(" + stringValue + ")";
            case EMPTY:
                return "Empty";
            default:
                return "???";
        }
    }
}
