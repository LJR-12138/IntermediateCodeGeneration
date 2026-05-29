package com.compiler.backend.ast;

public class IdentifierNode extends AstNode {
    public final String name;

    public IdentifierNode(String name) {
        super("Identifier");
        this.name = name;
    }
}
