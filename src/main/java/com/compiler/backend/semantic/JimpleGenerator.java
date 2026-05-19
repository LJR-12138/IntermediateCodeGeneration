package com.compiler.backend.semantic;

import soot.*;
import soot.jimple.*;
import soot.options.Options;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.*;

public class JimpleGenerator {

    private static final String CLASS_NAME = "Test";
    private static final String METHOD_NAME = "main";

    private SootClass sootClass;
    private SootMethod mainMethod;
    private JimpleBody body;
    private final Map<String, Local> locals;

    public JimpleGenerator() {
        this.locals = new LinkedHashMap<>();
    }

    public void initialize() {
        G.reset();

        Options.v().set_src_prec(Options.src_prec_java);
        Options.v().set_output_format(Options.output_format_jimple);
        Options.v().set_keep_line_number(false);
        Options.v().set_keep_offset(false);

        Scene.v().loadNecessaryClasses();

        sootClass = new SootClass(CLASS_NAME, Modifier.PUBLIC);
        sootClass.setSuperclass(Scene.v().getSootClass("java.lang.Object"));
        Scene.v().addClass(sootClass);

        mainMethod = new SootMethod(METHOD_NAME,
                Collections.singletonList(ArrayType.v(RefType.v("java.lang.String"), 1)),
                VoidType.v(),
                Modifier.PUBLIC | Modifier.STATIC);
        sootClass.addMethod(mainMethod);

        body = Jimple.v().newBody(mainMethod);
        mainMethod.setActiveBody(body);

        Local thisLocal = Jimple.v().newLocal("l0",
                ArrayType.v(RefType.v("java.lang.String"), 1));
        body.getLocals().add(thisLocal);

        Unit paramAssign = Jimple.v().newIdentityStmt(thisLocal,
                Jimple.v().newParameterRef(
                        ArrayType.v(RefType.v("java.lang.String"), 1), 0));
        body.getUnits().add(paramAssign);
    }

    public Local declareLocal(String name, String cType) {
        Type jimpleType = mapType(cType);
        // Jimple local names can't clash; if we already have one, skip
        String localName = name.startsWith("$") ? name : name;
        if (locals.containsKey(localName)) {
            return locals.get(localName);
        }
        Local local = Jimple.v().newLocal(localName, jimpleType);
        body.getLocals().add(local);
        locals.put(localName, local);
        return local;
    }

    public Local generateTempLocal(String cType) {
        String name = "$stack" + locals.size();
        return declareLocal(name, cType);
    }

    public void generateAssignment(String varName, Value rhs) {
        Local lhs = locals.get(varName);
        if (lhs == null) {
            System.err.println("[JimpleGen] Warning: local '" + varName + "' not found for assignment");
            return;
        }
        Unit assign = Jimple.v().newAssignStmt(lhs, rhs);
        body.getUnits().add(assign);
    }

    public Value generateIntConstant(int value) {
        return IntConstant.v(value);
    }

    public Value generateAddExpr(Value left, Value right) {
        return Jimple.v().newAddExpr(left, right);
    }

    public Value generateSubExpr(Value left, Value right) {
        return Jimple.v().newSubExpr(left, right);
    }

    public Value generateMulExpr(Value left, Value right) {
        return Jimple.v().newMulExpr(left, right);
    }

    public Value generateDivExpr(Value left, Value right) {
        return Jimple.v().newDivExpr(left, right);
    }

    public Value generateNegExpr(Value v) {
        return Jimple.v().newNegExpr(v);
    }

    public void addReturn() {
        Unit ret = Jimple.v().newReturnVoidStmt();
        body.getUnits().add(ret);
    }

    public String emitJimple() {
        addReturn();

        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        Printer.v().printTo(body, pw);
        pw.flush();

        String bodyStr = sw.toString();

        StringBuilder sb = new StringBuilder();
        sb.append("public class ").append(CLASS_NAME)
                .append(" extends java.lang.Object\n{\n");

        sb.append("    public void <init>()\n");
        sb.append("    {\n");
        sb.append("        ").append(CLASS_NAME).append(" l0;\n");
        sb.append("        l0 := @this: ").append(CLASS_NAME).append(";\n");
        sb.append("        specialinvoke l0.<java.lang.Object: void <init>()>();\n");
        sb.append("        return;\n");
        sb.append("    }\n\n");

        // Normalize Soot output: strip existing indent, add class-level 4-space indent
        String[] lines = bodyStr.split("\n");
        for (String line : lines) {
            String stripped = line.stripTrailing();
            if (stripped.isBlank()) {
                sb.append("\n");
            } else {
                sb.append("    ").append(stripped.stripLeading()).append("\n");
            }
        }

        sb.append("}\n");
        return sb.toString();
    }

    private Type mapType(String cType) {
        return switch (cType) {
            case "int" -> IntType.v();
            case "float" -> FloatType.v();
            case "double" -> DoubleType.v();
            case "long" -> LongType.v();
            case "char" -> ByteType.v();
            case "short" -> ShortType.v();
            case "void" -> VoidType.v();
            case "bool", "_Bool" -> BooleanType.v();
            default -> RefType.v("java.lang.Object");
        };
    }

    public Map<String, Local> getLocals() { return locals; }
}
