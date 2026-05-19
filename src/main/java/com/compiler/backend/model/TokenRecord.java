package com.compiler.backend.model;

public class TokenRecord {
    private int index;
    private String type;
    private String lexeme;
    private int line;
    private int col;

    public TokenRecord() {}

    public int getIndex() { return index; }
    public void setIndex(int index) { this.index = index; }

    public String getType() { return type; }
    public void setType(String type) { this.type = type; }

    public String getLexeme() { return lexeme; }
    public void setLexeme(String lexeme) { this.lexeme = lexeme; }

    public int getLine() { return line; }
    public void setLine(int line) { this.line = line; }

    public int getCol() { return col; }
    public void setCol(int col) { this.col = col; }

    @Override
    public String toString() {
        return "Token{" + index + ": " + type + "='" + lexeme + "'}";
    }
}
