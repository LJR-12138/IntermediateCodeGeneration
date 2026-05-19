package com.compiler.backend.model;

import java.util.List;

public class ReductionRule {
    private int id;
    private int sequence;
    private String lhs;
    private List<String> rhs;

    public ReductionRule() {}

    public int getId() { return id; }
    public void setId(int id) { this.id = id; }

    public int getSequence() { return sequence; }
    public void setSequence(int sequence) { this.sequence = sequence; }

    public String getLhs() { return lhs; }
    public void setLhs(String lhs) { this.lhs = lhs; }

    public List<String> getRhs() { return rhs; }
    public void setRhs(List<String> rhs) { this.rhs = rhs; }

    public int getRhsSize() { return rhs != null ? rhs.size() : 0; }

    @Override
    public String toString() {
        return "ReductionRule{seq=" + sequence + ", id=" + id + ", " + lhs + " -> " + rhs + "}";
    }
}
