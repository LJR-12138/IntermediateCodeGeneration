package com.compiler.backend.ast;

import java.util.List;

public class TranslationUnitNode extends AstNode {
    public final List<AstNode> declarations;

    public TranslationUnitNode(List<AstNode> declarations) {
        super("TranslationUnit");
        this.declarations = declarations;
    }
}
