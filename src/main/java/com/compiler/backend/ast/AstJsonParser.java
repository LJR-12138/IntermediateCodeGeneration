package com.compiler.backend.ast;

import com.google.gson.Gson;
import com.google.gson.annotations.SerializedName;
import com.google.gson.reflect.TypeToken;
import java.io.FileReader;
import java.io.IOException;
import java.io.Reader;
import java.lang.reflect.Type;
import java.util.HashMap;
import java.util.List;

/**
 * Parses the flat ast_lalr.json into a HashMap of JsonNode keyed by id.
 */
public class AstJsonParser {

    private static final Gson GSON = new Gson();

    public static class Result {
        public final int rootId;
        public final HashMap<Integer, JsonNode> nodes;

        Result(int rootId, HashMap<Integer, JsonNode> nodes) {
            this.rootId = rootId;
            this.nodes = nodes;
        }
    }

    private static class RawDocument {
        int root;
        List<JsonNode> nodes;
    }

    /**
     * Reads ast_lalr.json from the given path and returns a Result with the root ID
     * and a map of all nodes keyed by id.
     */
    public static Result parse(String jsonPath) throws IOException {
        try (Reader reader = new FileReader(jsonPath)) {
            RawDocument doc = GSON.fromJson(reader, RawDocument.class);
            HashMap<Integer, JsonNode> map = new HashMap<>();
            for (JsonNode node : doc.nodes) {
                map.put(node.id, node);
            }
            return new Result(doc.root, map);
        }
    }

    /**
     * Returns the JsonNode for the given id, or null if absent.
     */
    public static JsonNode getNode(HashMap<Integer, JsonNode> nodes, int id) {
        return nodes.get(id);
    }

    /**
     * Returns child JsonNodes (in order) for the given parent node.
     */
    public static List<JsonNode> getChildren(HashMap<Integer, JsonNode> nodes, JsonNode parent) {
        java.util.List<JsonNode> result = new java.util.ArrayList<>();
        for (int childId : parent.children) {
            JsonNode child = nodes.get(childId);
            if (child != null) {
                result.add(child);
            }
        }
        return result;
    }
}
