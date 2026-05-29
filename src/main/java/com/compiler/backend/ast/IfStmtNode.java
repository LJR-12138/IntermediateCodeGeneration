package com.compiler.backend.ast;

public class IfStmtNode extends AstNode {
    public final AstNode condition;
    public final AstNode thenBranch;
    public final AstNode elseBranch; // nullable

    public IfStmtNode(AstNode condition, AstNode thenBranch, AstNode elseBranch) {
        super("IfStmt");
        this.condition = condition;
        this.thenBranch = thenBranch;
        this.elseBranch = elseBranch;
    }
}
