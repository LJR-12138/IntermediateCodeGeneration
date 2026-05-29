package com.compiler.backend.ast;

import java.util.List;

public class FunctionDefNode extends AstNode {
    public final String returnType;   // e.g. "int", "void", "char"
    public final String name;         // function name
    public final List<VarDeclNode> parameters;
    public final BlockNode body;

    public FunctionDefNode(String returnType, String name,
                           List<VarDeclNode> parameters, BlockNode body) {
        super("FunctionDef");
        this.returnType = returnType;
        this.name = name;
        this.parameters = parameters;
        this.body = body;
    }
}
