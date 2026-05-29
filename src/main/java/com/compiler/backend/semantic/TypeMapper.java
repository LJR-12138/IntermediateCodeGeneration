package com.compiler.backend.semantic;

import com.compiler.backend.config.LanguageConfig;
import soot.ArrayType;
import soot.Type;

/**
 * Maps C99 AST type annotations (string + pointerDepth) to Soot Type objects.
 *
 * Mapping conventions are now driven by {@link LanguageConfig}, so swapping the
 * .sem.json config file adapts the type system for a different source language.
 *
 * Pointer types (pointerDepth > 0):
 *   C pointers are modelled as Soot ArrayType to support pointer arithmetic
 *   and array subscript operations natively in Jimple. For example:
 *     char *p  → ArrayType.v(ByteType.v(), 1)   = byte[]
 *     int  **q → ArrayType.v(IntType.v(),  2)   = int[][]
 */
public class TypeMapper {

    private final LanguageConfig config;

    public TypeMapper(LanguageConfig config) {
        this.config = config;
    }

    /** Backward-compatible constructor using C99 defaults. */
    public TypeMapper() {
        this(LanguageConfig.c99Default());
    }

    /**
     * Returns the Soot Type for a C declaration with the given type name
     * and pointer depth.
     */
    public Type mapType(String typeName, int pointerDepth) {
        return config.resolveSootType(typeName, pointerDepth);
    }

    /**
     * Returns true if the C type name represents a void type.
     */
    public boolean isVoid(String typeName) {
        return config.isVoidType(typeName);
    }

    /**
     * Returns a human-readable C type descriptor from name + pointer depth.
     */
    public static String toCTypeString(String typeName, int pointerDepth) {
        return typeName + "*".repeat(pointerDepth);
    }
}
