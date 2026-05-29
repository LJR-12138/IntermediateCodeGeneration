package com.compiler.backend.semantic;

/**
 * Thrown on semantic errors: redefinition, undeclared variable, type mismatch, etc.
 */
public class SemanticException extends RuntimeException {
    public final int line;
    public final int column;

    public SemanticException(String message) {
        super(message);
        this.line = 0;
        this.column = 0;
    }

    public SemanticException(String message, int line, int column) {
        super(message + " at line " + line + ":" + column);
        this.line = line;
        this.column = column;
    }
}
