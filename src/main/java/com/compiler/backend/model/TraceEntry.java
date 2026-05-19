package com.compiler.backend.model;

import java.util.List;

public class TraceEntry {
    private int step;
    private int state;
    private int lookaheadId;
    private String lookahead;
    private String action;
    private int productionId;
    private List<Integer> stateStack;
    private List<String> symbolStack;
    private int inputIndex;

    public TraceEntry() {}

    public int getStep() { return step; }
    public void setStep(int step) { this.step = step; }

    public int getState() { return state; }
    public void setState(int state) { this.state = state; }

    public int getLookaheadId() { return lookaheadId; }
    public void setLookaheadId(int lookaheadId) { this.lookaheadId = lookaheadId; }

    public String getLookahead() { return lookahead; }
    public void setLookahead(String lookahead) { this.lookahead = lookahead; }

    public String getAction() { return action; }
    public void setAction(String action) { this.action = action; }

    public int getProductionId() { return productionId; }
    public void setProductionId(int productionId) { this.productionId = productionId; }

    public List<Integer> getStateStack() { return stateStack; }
    public void setStateStack(List<Integer> stateStack) { this.stateStack = stateStack; }

    public List<String> getSymbolStack() { return symbolStack; }
    public void setSymbolStack(List<String> symbolStack) { this.symbolStack = symbolStack; }

    public int getInputIndex() { return inputIndex; }
    public void setInputIndex(int inputIndex) { this.inputIndex = inputIndex; }

    public boolean isShift() { return action != null && action.startsWith("s"); }

    public boolean isReduce() { return action != null && action.startsWith("r"); }

    public boolean isAccept() { return "acc".equals(action); }

    @Override
    public String toString() {
        return "TraceEntry{step=" + step + ", state=" + state + ", lookahead='" + lookahead
                + "', action='" + action + "', productionId=" + productionId + "}";
    }
}
