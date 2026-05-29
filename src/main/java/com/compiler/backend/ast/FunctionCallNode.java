package com.compiler.backend.ast;

import java.util.List;

public class FunctionCallNode extends AstNode {
    public final String functionName;
    public final List<AstNode> arguments;

    public FunctionCallNode(String functionName, List<AstNode> arguments) {
        super("FunctionCall");
        this.functionName = functionName;
        this.arguments = arguments;
    }
}
