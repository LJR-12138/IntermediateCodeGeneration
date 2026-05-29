package com.compiler.backend.ast;

public class VarDeclNode extends AstNode {
    public final String typeName;     // e.g. "int", "char", "float", "void"
    public final String varName;
    public final int pointerDepth;    // 0 = no pointer, 1 = *, 2 = **, ...
    public final AstNode initExpr;    // nullable

    public VarDeclNode(String typeName, String varName, int pointerDepth, AstNode initExpr) {
        super("VarDecl");
        this.typeName = typeName;
        this.varName = varName;
        this.pointerDepth = pointerDepth;
        this.initExpr = initExpr;
    }
}
