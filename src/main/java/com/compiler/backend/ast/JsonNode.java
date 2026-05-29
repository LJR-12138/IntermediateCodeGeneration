package com.compiler.backend.ast;

import java.util.List;

/**
 * Maps one node object in the flat ast_lalr.json "nodes" array.
 *
 * Leaf nodes:     production_id == -1, lexeme non-empty, children empty.
 * Interior nodes: production_id >= 0, lexeme empty, children hold child IDs.
 */
public class JsonNode {
    public int id;
    public String type;
    public String lexeme;
    public int production_id;
    public int line;
    public int column;
    public List<Integer> children;

    public boolean isLeaf() {
        return production_id == -1;
    }
}
