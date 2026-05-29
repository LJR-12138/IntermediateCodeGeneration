package com.compiler.backend.ast;

import static org.junit.jupiter.api.Assertions.*;

import java.util.HashMap;
import java.util.List;
import org.junit.jupiter.api.Test;

class AstJsonParserTest {

    @Test
    void parseSampleJson() throws Exception {
        String path = getClass().getResource("/sample_ast_lalr.json").getPath();
        AstJsonParser.Result result = AstJsonParser.parse(path);

        assertNotNull(result);
        assertTrue(result.rootId >= 0, "root id should be non-negative");
        assertFalse(result.nodes.isEmpty(), "nodes should not be empty");

        // root node exists
        JsonNode root = result.nodes.get(result.rootId);
        assertNotNull(root, "root node must exist");
        assertEquals("translation_unit", root.type);
        assertFalse(root.children.isEmpty(), "root should have children");

        // verify leaf vs interior node properties
        for (JsonNode node : result.nodes.values()) {
            if (node.isLeaf()) {
                assertEquals(-1, node.production_id);
                assertFalse(node.lexeme.isEmpty(), "leaf must have lexeme: " + node.id);
                assertTrue(node.children.isEmpty(), "leaf must have no children: " + node.id);
            } else {
                assertTrue(node.production_id >= 0, "interior node must have production_id: " + node.id);
                assertTrue(node.lexeme.isEmpty(), "interior node must have empty lexeme: " + node.id + " type=" + node.type);
            }
        }

        // verify child references are valid
        for (JsonNode node : result.nodes.values()) {
            for (int childId : node.children) {
                assertTrue(result.nodes.containsKey(childId),
                    "child " + childId + " of node " + node.id + " must exist in map");
            }
        }

        System.out.println("=== AST JSON Parser Test ===");
        System.out.println("Root ID: " + result.rootId);
        System.out.println("Total nodes: " + result.nodes.size());
        long leafCount = result.nodes.values().stream().filter(JsonNode::isLeaf).count();
        long interiorCount = result.nodes.size() - leafCount;
        System.out.println("Leaf nodes: " + leafCount);
        System.out.println("Interior nodes: " + interiorCount);

        // Test getChildren helper
        List<JsonNode> rootChildren = AstJsonParser.getChildren(result.nodes, root);
        System.out.println("Root children: " + rootChildren.size());
        for (JsonNode child : rootChildren) {
            System.out.println("  child id=" + child.id + " type=" + child.type);
        }
    }
}
