package com.compiler.backend.ast;

public class ArraySubscriptNode extends AstNode {
    public final AstNode array;
    public final AstNode index;

    public ArraySubscriptNode(AstNode array, AstNode index) {
        super("ArraySubscript");
        this.array = array;
        this.index = index;
    }
}
