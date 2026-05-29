package com.compiler.backend.ast;

import java.util.List;

public class BlockNode extends AstNode {
    public final List<AstNode> statements;

    public BlockNode(List<AstNode> statements) {
        super("Block");
        this.statements = statements;
    }
}
