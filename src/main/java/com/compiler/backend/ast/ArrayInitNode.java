package com.compiler.backend.ast;

import java.util.List;

/**
 * Represents a brace-enclosed initializer list like {3, 8, 2, 7, 10}.
 * Used for array and struct initialization.
 */
public class ArrayInitNode extends AstNode {
    public final List<AstNode> values;

    public ArrayInitNode(List<AstNode> values) {
        super("ArrayInit");
        this.values = values;
    }
}
