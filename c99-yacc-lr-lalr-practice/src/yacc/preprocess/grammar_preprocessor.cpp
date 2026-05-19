#include "yacc/preprocess/grammar_preprocessor.h"

namespace seu::yacc {
namespace {

bool is_symbol_id_valid(const Grammar& grammar, int symbol_id) {
    return symbol_id >= 0 && symbol_id < static_cast<int>(grammar.symbols.size());
}

void push_error(GrammarPreprocessReport& report, const std::string& text) {
    report.errors.push_back(text);
}

void push_warning(GrammarPreprocessReport& report, const std::string& text) {
    report.warnings.push_back(text);
}

}  // namespace

GrammarPreprocessReport preprocess_grammar(Grammar& grammar) {
    GrammarPreprocessReport report;

    // 1) 基础结构检查。
    if (grammar.productions.empty()) {
        push_error(report, "产生式为空，无法构造增广文法。");
        return report;
    }
    if (!is_symbol_id_valid(grammar, grammar.start_symbol_id)) {
        push_error(report, "start_symbol_id 非法。");
        return report;
    }
    if (!is_symbol_id_valid(grammar, grammar.augmented_start_symbol_id)) {
        push_error(report, "augmented_start_symbol_id 非法。");
        return report;
    }
    if (!is_symbol_id_valid(grammar, grammar.eof_symbol_id)) {
        push_error(report, "eof_symbol_id 非法。");
        return report;
    }
    if (!is_symbol_id_valid(grammar, grammar.epsilon_symbol_id)) {
        push_error(report, "epsilon_symbol_id 非法。");
        return report;
    }

    // 2) 产生式编号连续性与索引重建（第 4 步关键：后续算法不再全表扫描）。
    grammar.prod_ids_by_lhs.clear();
    for (size_t i = 0; i < grammar.productions.size(); ++i) {
        const auto& p = grammar.productions[i];
        if (p.id != static_cast<int>(i)) {
            push_error(report, "产生式编号不连续，要求 p.id == 下标。");
            return report;
        }
        if (!is_symbol_id_valid(grammar, p.lhs_symbol_id)) {
            push_error(report, "存在 lhs_symbol_id 越界。");
            return report;
        }
        for (int rhs_id : p.rhs_symbol_ids) {
            if (!is_symbol_id_valid(grammar, rhs_id)) {
                push_error(report, "存在 rhs_symbol_id 越界。");
                return report;
            }
        }
        grammar.prod_ids_by_lhs[p.lhs_symbol_id].push_back(p.id);
    }

    // 3) 增广产生式检查：必须固定在 #0 且满足 S' -> S。
    const auto& augmented = grammar.productions.front();
    report.augmented_production_id = augmented.id;
    if (augmented.id != 0) {
        push_error(report, "增广产生式必须固定为 #0。");
        return report;
    }
    if (augmented.lhs_symbol_id != grammar.augmented_start_symbol_id) {
        push_error(report, "增广产生式左部必须是 augmented_start_symbol。");
        return report;
    }
    if (augmented.rhs_symbol_ids.size() != 1 || augmented.rhs_symbol_ids[0] != grammar.start_symbol_id) {
        push_error(report, "增广产生式必须形如 S' -> start_symbol。");
        return report;
    }

    // 4) 非终结符对应产生式检查（对开始符号必须严格要求，其他给出警告）。
    for (int nonterminal_id : grammar.nonterminal_ids) {
        auto it = grammar.prod_ids_by_lhs.find(nonterminal_id);
        if (it == grammar.prod_ids_by_lhs.end() || it->second.empty()) {
            report.nonterminals_without_productions.push_back(nonterminal_id);
        }
    }
    if (!report.nonterminals_without_productions.empty()) {
        for (int sid : report.nonterminals_without_productions) {
            if (sid == grammar.start_symbol_id) {
                push_error(report, "开始符号没有任何产生式。");
                return report;
            }
            push_warning(report, "存在非终结符没有产生式: " + grammar.symbols[sid].name);
        }
    }

    // 5) epsilon 使用规范检查：内部约定空产生式用 RHS 空序列表示，不应显式出现 epsilon 符号。
    for (const auto& p : grammar.productions) {
        for (int rhs_id : p.rhs_symbol_ids) {
            if (rhs_id == grammar.epsilon_symbol_id) {
                report.productions_with_epsilon_symbol_in_rhs.push_back(p.id);
                break;
            }
        }
    }
    if (!report.productions_with_epsilon_symbol_in_rhs.empty()) {
        for (int pid : report.productions_with_epsilon_symbol_in_rhs) {
            push_warning(report, "产生式 #" + std::to_string(pid) +
                                   " 右部显式使用 epsilon，建议改为空 RHS。");
        }
    }

    // 6) 关键符号类型检查。
    const auto& eof = grammar.symbols[grammar.eof_symbol_id];
    const auto& eps = grammar.symbols[grammar.epsilon_symbol_id];
    if (eof.kind != SymbolKind::Special || eps.kind != SymbolKind::Special) {
        push_error(report, "特殊符号 `$` 或 `epsilon` 类型异常。");
        return report;
    }

    // 7) 通过。
    report.passed = true;
    return report;
}

}  // namespace seu::yacc
