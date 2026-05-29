package com.compiler.backend.ast;

public class StringLiteralNode extends AstNode {
    public final String value; // raw lexeme with quotes stripped

    public StringLiteralNode(String value) {
        super("StringLiteral");
        this.value = value;
    }
}
