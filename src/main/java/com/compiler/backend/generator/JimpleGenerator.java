package com.compiler.backend.generator;

import com.compiler.backend.ast.*;
import com.compiler.backend.config.LanguageConfig;
import com.compiler.backend.semantic.SemanticException;
import com.compiler.backend.semantic.SymbolTable;
import com.compiler.backend.semantic.TypeMapper;

import soot.*;
import soot.jimple.*;
import soot.options.Options;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.*;

public class JimpleGenerator {

    private final SymbolTable symbolTable;
    private final LanguageConfig config;
    private SootClass sootClass;
    private JimpleBody currentBody;
    private SootMethod currentMethod;
    private int tempCounter;
    private boolean mainMethodEnsured;
    private final Map<String, SootMethod> builtinMethodCache = new HashMap<>();
    /** Top-level (file-scope) C global/static variables mapped to SootFields. */
    private final Map<String, SootField> globalFields = new HashMap<>();

    public JimpleGenerator(SymbolTable symbolTable, LanguageConfig config) {
        this.symbolTable = symbolTable;
        this.config = config;
        this.tempCounter = 0;
        this.mainMethodEnsured = false;
    }

    /** Backward-compatible constructor using C99 defaults. */
    public JimpleGenerator(SymbolTable symbolTable) {
        this(symbolTable, LanguageConfig.c99Default());
    }

    public JimpleGenerator() {
        this(new SymbolTable(), LanguageConfig.c99Default());
    }

    // ======== Top-level entry ========

    public void generate(TranslationUnitNode root) {
        soot.G.reset();
        soot.options.Options.v().set_prepend_classpath(true);
        soot.options.Options.v().set_keep_line_number(true);
        soot.Scene.v().loadBasicClasses();

        sootClass = new SootClass(config.getOutputClassName(), Modifier.PUBLIC);
        sootClass.setSuperclass(Scene.v().getSootClass("java.lang.Object"));
        Scene.v().addClass(sootClass);
        sootClass.setApplicationClass();

        // Phase 1: declare global variables as class-level static fields.
        // This must happen before generating any function bodies so that
        // functions can reference globals declared later in the source.
        for (AstNode decl : root.declarations) {
            if (decl instanceof VarDeclNode varDecl) {
                generateGlobalVarDecl(varDecl);
            }
        }

        // Phase 2: generate function bodies (which may reference globals).
        for (AstNode decl : root.declarations) {
            if (decl instanceof FunctionDefNode func) {
                generateFunction(func);
            }
        }

        if (mainMethodEnsured) {
            currentBody.getUnits().add(Jimple.v().newReturnVoidStmt());
        }
    }

    private void ensureMainMethod() {
        if (mainMethodEnsured) return;
        mainMethodEnsured = true;

        SootMethod mainMethod = new SootMethod("main",
                Collections.singletonList(ArrayType.v(RefType.v("java.lang.String"), 1)),
                VoidType.v(),
                Modifier.PUBLIC | Modifier.STATIC);
        sootClass.addMethod(mainMethod);

        currentBody = Jimple.v().newBody(mainMethod);
        mainMethod.setActiveBody(currentBody);
        currentMethod = mainMethod;

        Local argsLocal = Jimple.v().newLocal("args",
                ArrayType.v(RefType.v("java.lang.String"), 1));
        currentBody.getLocals().add(argsLocal);
        currentBody.getUnits().add(Jimple.v().newIdentityStmt(argsLocal,
                Jimple.v().newParameterRef(
                        ArrayType.v(RefType.v("java.lang.String"), 1), 0)));
    }

    // ======== Function Definition ========

    private void generateFunction(FunctionDefNode func) {
        symbolTable.enterScope();

        Type returnType = config.resolveSootType(func.returnType, 0);
        List<Type> paramTypes = new ArrayList<>();
        for (VarDeclNode p : func.parameters) {
            paramTypes.add(config.resolveSootType(p.typeName, p.pointerDepth));
        }

        SootMethod method = new SootMethod(func.name, paramTypes, returnType,
                Modifier.PUBLIC | Modifier.STATIC);
        sootClass.addMethod(method);

        JimpleBody body = Jimple.v().newBody(method);
        method.setActiveBody(body);

        currentBody = body;
        currentMethod = method;
        PatchingChain<Unit> units = body.getUnits();

        for (int i = 0; i < func.parameters.size(); i++) {
            VarDeclNode p = func.parameters.get(i);
            Type paramType = config.resolveSootType(p.typeName, p.pointerDepth);
            Local paramLocal = Jimple.v().newLocal(p.varName, paramType);
            body.getLocals().add(paramLocal);

            symbolTable.put(p.varName, paramLocal);

            units.add(Jimple.v().newIdentityStmt(
                    paramLocal,
                    Jimple.v().newParameterRef(paramType, i)));
        }

        generateBlock(func.body);

        if (config.isVoidType(func.returnType)) {
            boolean hasReturn = false;
            for (Unit u : units) {
                if (u instanceof ReturnStmt || u instanceof ReturnVoidStmt) {
                    hasReturn = true;
                    break;
                }
            }
            if (!hasReturn) {
                units.add(Jimple.v().newReturnVoidStmt());
            }
        }

        symbolTable.exitScope();
        currentBody = null;
        currentMethod = null;
    }

    // ======== Block ========

    private void generateBlock(BlockNode block) {
        symbolTable.enterScope();
        for (AstNode stmt : block.statements) {
            generateStmt(stmt);
        }
        symbolTable.exitScope();
    }

    // ======== Statement dispatch ========

    private void generateStmt(AstNode node) {
        if (node instanceof VarDeclNode vd) {
            generateVarDecl(vd);
        } else if (node instanceof IfStmtNode ifStmt) {
            generateIfStmt(ifStmt);
        } else if (node instanceof WhileStmtNode whileStmt) {
            generateWhileStmt(whileStmt);
        } else if (node instanceof ReturnStmtNode ret) {
            generateReturnStmt(ret);
        } else if (node instanceof BlockNode block) {
            generateBlock(block);
        } else if (node instanceof BinaryExprNode) {
            evaluateExpr(node);
        } else if (node instanceof UnaryExprNode) {
            evaluateExpr(node);
        } else if (node instanceof FunctionCallNode) {
            evaluateExpr(node);
        }
    }

    // ======== Variable Declaration ========

    private void generateVarDecl(VarDeclNode vd) {
        Type mappedType = config.resolveSootType(vd.typeName, vd.pointerDepth);
        Local local = Jimple.v().newLocal(vd.varName, mappedType);
        currentBody.getLocals().add(local);

        symbolTable.put(vd.varName, local);

        if (vd.initExpr instanceof ArrayInitNode arrayInit) {
            Type elementType = config.resolveSootType(vd.typeName,
                    Math.max(0, vd.pointerDepth - 1));
            int size = arrayInit.values.size();
            Value newArr = Jimple.v().newNewArrayExpr(elementType, IntConstant.v(size));
            currentBody.getUnits().add(Jimple.v().newAssignStmt(local, newArr));

            for (int i = 0; i < size; i++) {
                Value val = evaluateExpr(arrayInit.values.get(i));
                ArrayRef arrRef = Jimple.v().newArrayRef(local, IntConstant.v(i));
                currentBody.getUnits().add(Jimple.v().newAssignStmt(arrRef, val));
            }
        } else if (vd.initExpr != null) {
            Value initValue = evaluateExpr(vd.initExpr);
            currentBody.getUnits().add(Jimple.v().newAssignStmt(local, initValue));
        }
    }

    /**
     * Creates a class-level static field for a C file-scope (global) variable.
     * Global variables in C have static storage duration, which maps naturally
     * to {@code private static} fields in the Jimple class.
     */
    private void generateGlobalVarDecl(VarDeclNode vd) {
        Type mappedType = config.resolveSootType(vd.typeName, vd.pointerDepth);
        SootField field = new SootField(vd.varName, mappedType,
                Modifier.PRIVATE | Modifier.STATIC);
        sootClass.addField(field);
        globalFields.put(vd.varName, field);

        // Handle initializer if present.
        // For global variables, the initializer must be a compile-time constant
        // (C99 §6.7.8); we store it as a Soot tag for later retrieval.
        if (vd.initExpr instanceof IntLiteralNode lit) {
            field.addTag(new soot.tagkit.IntegerConstantValueTag(lit.value));
        } else if (vd.initExpr instanceof StringLiteralNode str) {
            field.addTag(new soot.tagkit.StringConstantValueTag(str.value));
        }
        // For array / expression initializers at global scope, a <clinit> static
        // initializer block would be needed — not yet implemented.
    }

    // ======== Expression evaluation (returns Value) ========

    private Value evaluateExpr(AstNode node) {
        if (node instanceof IntLiteralNode lit) {
            return IntConstant.v(lit.value);
        }
        if (node instanceof StringLiteralNode str) {
            return StringConstant.v(str.value);
        }
        if (node instanceof IdentifierNode id) {
            // Local variable / parameter takes precedence (C scoping rules).
            if (symbolTable.contains(id.name)) {
                return symbolTable.lookup(id.name);
            }
            // Fallback: global / file-scope variable → static field reference.
            SootField globalField = globalFields.get(id.name);
            if (globalField != null) {
                return Jimple.v().newStaticFieldRef(globalField.makeRef());
            }
            throw new SemanticException("undeclared variable '" + id.name + "'");
        }
        if (node instanceof BinaryExprNode bin) {
            return evaluateBinaryExpr(bin);
        }
        if (node instanceof ArraySubscriptNode sub) {
            Value arrayVal = evaluateExpr(sub.array);
            Value indexVal = evaluateExpr(sub.index);
            Value arrayRef = Jimple.v().newArrayRef(arrayVal, indexVal);
            Type elementType = arrayRef.getType();
            Local tempLocal = newTempLocal(elementType);
            currentBody.getUnits().add(Jimple.v().newAssignStmt(tempLocal, arrayRef));
            return tempLocal;
        }
        if (node instanceof FunctionCallNode call) {
            List<Value> argValues = new ArrayList<>();
            for (AstNode arg : call.arguments) {
                argValues.add(evaluateExpr(arg));
            }
            SootMethod callee;
            if (symbolTable.isBuiltin(call.functionName)) {
                callee = getOrCreateBuiltin(call.functionName, argValues);
            } else {
                callee = sootClass.getMethodByName(call.functionName);
            }
            Value invokeExpr = Jimple.v().newStaticInvokeExpr(
                    callee.makeRef(), argValues);
            if (callee.getReturnType() instanceof soot.VoidType) {
                currentBody.getUnits().add(Jimple.v().newInvokeStmt(invokeExpr));
                return IntConstant.v(0);
            }
            Local temp = newTempLocal(callee.getReturnType());
            currentBody.getUnits().add(Jimple.v().newAssignStmt(temp, invokeExpr));
            return temp;
        }
        if (node instanceof UnaryExprNode un) {
            return evaluateUnaryExpr(un);
        }
        throw new SemanticException("Cannot evaluate expression of type: " + node.nodeType);
    }

    private Value evaluateBinaryExpr(BinaryExprNode bin) {
        Value left = evaluateExpr(bin.left);
        Value right = evaluateExpr(bin.right);

        if ("=".equals(bin.operator)) {
            if (bin.left instanceof IdentifierNode lhs) {
                Local lhsLocal = symbolTable.lookup(lhs.name);
                currentBody.getUnits().add(Jimple.v().newAssignStmt(lhsLocal, right));
                return lhsLocal;
            }
            throw new SemanticException("Left-hand side of assignment must be an identifier, got: "
                    + bin.left.nodeType);
        }

        Value expr = switch (bin.operator) {
            case "+"  -> Jimple.v().newAddExpr(left, right);
            case "-"  -> Jimple.v().newSubExpr(left, right);
            case "*"  -> Jimple.v().newMulExpr(left, right);
            case "/"  -> Jimple.v().newDivExpr(left, right);
            case "%"  -> Jimple.v().newRemExpr(left, right);
            case "==" -> Jimple.v().newEqExpr(left, right);
            case "!=" -> Jimple.v().newNeExpr(left, right);
            case "<"  -> Jimple.v().newLtExpr(left, right);
            case ">"  -> Jimple.v().newGtExpr(left, right);
            case "<=" -> Jimple.v().newLeExpr(left, right);
            case ">=" -> Jimple.v().newGeExpr(left, right);
            case "&&" -> Jimple.v().newAndExpr(left, right);
            case "||" -> Jimple.v().newOrExpr(left, right);
            case "&"  -> Jimple.v().newAndExpr(left, right);
            case "|"  -> Jimple.v().newOrExpr(left, right);
            case "^"  -> Jimple.v().newXorExpr(left, right);
            case "<<" -> Jimple.v().newShlExpr(left, right);
            case ">>" -> Jimple.v().newShrExpr(left, right);
            default ->
                throw new SemanticException("Unknown binary operator: " + bin.operator);
        };

        Type exprType = expr.getType();
        Local tempLocal = newTempLocal(exprType);
        currentBody.getUnits().add(Jimple.v().newAssignStmt(tempLocal, expr));
        return tempLocal;
    }

    private Value evaluateUnaryExpr(UnaryExprNode un) {
        Value operand = evaluateExpr(un.operand);

        Value expr = switch (un.operator) {
            case "-" -> Jimple.v().newNegExpr(operand);
            case "!" -> Jimple.v().newEqExpr(operand, IntConstant.v(0));
            default ->
                throw new SemanticException("Unknown unary operator: " + un.operator);
        };

        Type exprType = expr.getType();
        Local tempLocal = newTempLocal(exprType);
        currentBody.getUnits().add(Jimple.v().newAssignStmt(tempLocal, expr));
        return tempLocal;
    }

    // ======== Return Statement ========

    private void generateReturnStmt(ReturnStmtNode ret) {
        if (ret.value != null) {
            Value val = evaluateExpr(ret.value);
            currentBody.getUnits().add(Jimple.v().newReturnStmt(val));
        } else {
            currentBody.getUnits().add(Jimple.v().newReturnVoidStmt());
        }
    }

    // ======== If Statement ========

    private void generateIfStmt(IfStmtNode ifStmt) {
        PatchingChain<Unit> units = currentBody.getUnits();
        Value condition = evaluateExpr(ifStmt.condition);

        Unit thenLabel = Jimple.v().newNopStmt();
        Unit elseLabel = Jimple.v().newNopStmt();
        Unit mergeLabel = Jimple.v().newNopStmt();

        // if condition == 0 (false) goto elseLabel
        units.add(Jimple.v().newIfStmt(
                Jimple.v().newEqExpr(condition, IntConstant.v(0)), elseLabel));

        // Then branch
        units.add(thenLabel);
        generateStmt(ifStmt.thenBranch);
        units.add(Jimple.v().newGotoStmt(mergeLabel));

        // Else branch
        units.add(elseLabel);
        if (ifStmt.elseBranch != null) {
            generateStmt(ifStmt.elseBranch);
        }

        units.add(mergeLabel);
    }

    // ======== While Statement ========

    private void generateWhileStmt(WhileStmtNode whileStmt) {
        PatchingChain<Unit> units = currentBody.getUnits();

        Unit startLabel = Jimple.v().newNopStmt();
        Unit endLabel = Jimple.v().newNopStmt();

        units.add(startLabel);

        Value condition = evaluateExpr(whileStmt.condition);

        // if condition == 0 (false) goto endLabel
        units.add(Jimple.v().newIfStmt(
                Jimple.v().newEqExpr(condition, IntConstant.v(0)), endLabel));

        generateStmt(whileStmt.body);

        units.add(Jimple.v().newGotoStmt(startLabel));
        units.add(endLabel);
    }

    // ======== Helpers ========

    /**
     * Lazily creates a dummy SootMethod for C builtins (printf, scanf).
     * The cache key includes full parameter types so calls with different
     * argument lists (e.g. printf(str,int) vs printf(str,int,int)) get
     * distinct stubs.  If a method with matching name + param types already
     * exists on sootClass, it is reused instead of creating a duplicate.
     */
    private SootMethod getOrCreateBuiltin(String name, List<Value> argValues) {
        List<Type> paramTypes = new ArrayList<>();
        for (Value v : argValues) {
            paramTypes.add(v.getType());
        }
        // Cache key includes actual types, not just arg count
        String cacheKey = name + "/" + paramTypes;
        SootMethod cached = builtinMethodCache.get(cacheKey);
        if (cached != null) return cached;

        // Reuse an existing method with the same name and parameter types
        for (SootMethod m : sootClass.getMethods()) {
            if (m.getName().equals(name) && m.getParameterTypes().equals(paramTypes)) {
                builtinMethodCache.put(cacheKey, m);
                return m;
            }
        }

        SootMethod method = new SootMethod(name, paramTypes, IntType.v(),
                Modifier.PUBLIC | Modifier.STATIC);
        sootClass.addMethod(method);

        JimpleBody body = Jimple.v().newBody(method);
        method.setActiveBody(body);
        body.getUnits().add(Jimple.v().newReturnStmt(IntConstant.v(0)));

        builtinMethodCache.put(cacheKey, method);
        return method;
    }

    private Local newTempLocal(Type type) {
        String name = "$t" + (tempCounter++);
        Local temp = Jimple.v().newLocal(name, type);
        currentBody.getLocals().add(temp);
        return temp;
    }

    // ======== Output ========

    public SootClass getSootClass() {
        return sootClass;
    }

    public String emitJimple() {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        Printer.v().printTo(sootClass, pw);
        pw.flush();
        return sw.toString();
    }

    public SymbolTable getSymbolTable() {
        return symbolTable;
    }
}
