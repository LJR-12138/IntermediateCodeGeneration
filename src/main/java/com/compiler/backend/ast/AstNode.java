package com.compiler.backend.ast;

public abstract class AstNode {
    public final String nodeType;

    protected AstNode(String nodeType) {
        this.nodeType = nodeType;
    }
}
