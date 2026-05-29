package com.compiler.backend.ast;

public class ReturnStmtNode extends AstNode {
    public final AstNode value;  // nullable (for bare "return;")

    public ReturnStmtNode(AstNode value) {
        super("ReturnStmt");
        this.value = value;
    }
}
