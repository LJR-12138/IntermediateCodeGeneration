package com.compiler.backend.ast;

public class IntLiteralNode extends AstNode {
    public final int value;

    public IntLiteralNode(int value) {
        super("IntLiteral");
        this.value = value;
    }
}
