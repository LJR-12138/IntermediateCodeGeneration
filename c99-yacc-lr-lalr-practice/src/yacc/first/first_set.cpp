#include "yacc/first/first_set.h"

#include <algorithm>

namespace seu::yacc {
namespace {

bool is_valid_symbol_id(const Grammar& grammar, int symbol_id) {
    return symbol_id >= 0 && symbol_id < static_cast<int>(grammar.symbols.size());
}

void add_non_epsilon_from_source_to_target(
    std::set<int>& target, const std::set<int>& source, int epsilon_symbol_id, bool& changed) {
    for (int sid : source) {
        if (sid == epsilon_symbol_id) {
            continue;
        }
        const bool inserted = target.insert(sid).second;
        if (inserted) {
            changed = true;
        }
    }
}

}  // namespace

FirstSetResult compute_first_sets(const Grammar& grammar) {
    FirstSetResult result;
    result.first_sets_by_symbol_id.resize(grammar.symbols.size());

    if (grammar.symbols.empty()) {
        return result;
    }

    // 1) 初始化：终结符/输入结束符的 First 为其自身，epsilon 的 First 为 epsilon。
    for (int tid : grammar.terminal_ids) {
        if (is_valid_symbol_id(grammar, tid)) {
            result.first_sets_by_symbol_id[tid].insert(tid);
        }
    }
    if (is_valid_symbol_id(grammar, grammar.eof_symbol_id)) {
        result.first_sets_by_symbol_id[grammar.eof_symbol_id].insert(grammar.eof_symbol_id);
    }
    if (is_valid_symbol_id(grammar, grammar.epsilon_symbol_id)) {
        result.first_sets_by_symbol_id[grammar.epsilon_symbol_id].insert(grammar.epsilon_symbol_id);
    }

    // 2) 不动点迭代：FIRST(A) <- FIRST(alpha)。
    constexpr int kMaxIterations = 20000;
    for (int iter = 1; iter <= kMaxIterations; ++iter) {
        bool changed = false;

        for (const auto& production : grammar.productions) {
            if (!is_valid_symbol_id(grammar, production.lhs_symbol_id)) {
                continue;
            }
            std::set<int>& first_lhs = result.first_sets_by_symbol_id[production.lhs_symbol_id];

            // 空 RHS 等价于 A -> epsilon。
            if (production.rhs_symbol_ids.empty()) {
                const bool inserted = first_lhs.insert(grammar.epsilon_symbol_id).second;
                if (inserted) {
                    changed = true;
                }
                continue;
            }

            bool all_nullable = true;
            for (int rhs_symbol_id : production.rhs_symbol_ids) {
                if (!is_valid_symbol_id(grammar, rhs_symbol_id)) {
                    all_nullable = false;
                    break;
                }
                const std::set<int>& first_rhs = result.first_sets_by_symbol_id[rhs_symbol_id];
                add_non_epsilon_from_source_to_target(
                    first_lhs, first_rhs, grammar.epsilon_symbol_id, changed);
                if (first_rhs.find(grammar.epsilon_symbol_id) == first_rhs.end()) {
                    all_nullable = false;
                    break;
                }
            }

            if (all_nullable) {
                const bool inserted = first_lhs.insert(grammar.epsilon_symbol_id).second;
                if (inserted) {
                    changed = true;
                }
            }
        }

        result.iteration_count = iter;
        if (!changed) {
            result.converged = true;
            break;
        }
    }

    // 3) 收集可空非终结符（FIRST(A) 含 epsilon）。
    for (int nid : grammar.nonterminal_ids) {
        if (!is_valid_symbol_id(grammar, nid)) {
            continue;
        }
        const auto& first_n = result.first_sets_by_symbol_id[nid];
        if (first_n.find(grammar.epsilon_symbol_id) != first_n.end()) {
            result.nullable_nonterminal_ids.push_back(nid);
        }
    }
    std::sort(result.nullable_nonterminal_ids.begin(), result.nullable_nonterminal_ids.end());

    return result;
}

std::set<int> compute_first_of_sequence(const Grammar& grammar, const FirstSetResult& result,
    const std::vector<int>& symbol_ids, std::size_t start_index) {
    std::set<int> first_sequence;

    if (start_index >= symbol_ids.size()) {
        first_sequence.insert(grammar.epsilon_symbol_id);
        return first_sequence;
    }

    for (std::size_t i = start_index; i < symbol_ids.size(); ++i) {
        const int sid = symbol_ids[i];
        if (!is_valid_symbol_id(grammar, sid) ||
            sid >= static_cast<int>(result.first_sets_by_symbol_id.size())) {
            continue;
        }
        const auto& first_sid = result.first_sets_by_symbol_id[sid];
        for (int token_id : first_sid) {
            if (token_id != grammar.epsilon_symbol_id) {
                first_sequence.insert(token_id);
            }
        }
        if (first_sid.find(grammar.epsilon_symbol_id) == first_sid.end()) {
            return first_sequence;
        }
    }

    first_sequence.insert(grammar.epsilon_symbol_id);
    return first_sequence;
}

FirstSetValidationReport validate_first_sets(const Grammar& grammar, const FirstSetResult& result) {
    FirstSetValidationReport report;

    if (!result.converged) {
        report.errors.push_back("First 集迭代未收敛。");
    }
    if (result.first_sets_by_symbol_id.size() != grammar.symbols.size()) {
        report.errors.push_back("First 集数组大小与符号表大小不一致。");
    }

    // 规则 1：每个终结符 t 应满足 FIRST(t) = {t}。
    for (int tid : grammar.terminal_ids) {
        if (!is_valid_symbol_id(grammar, tid) ||
            tid >= static_cast<int>(result.first_sets_by_symbol_id.size())) {
            report.errors.push_back("终结符 ID 非法，无法校验 FIRST(t)。");
            continue;
        }
        const auto& set_t = result.first_sets_by_symbol_id[tid];
        if (set_t.size() != 1 || set_t.find(tid) == set_t.end()) {
            report.errors.push_back("存在终结符不满足 FIRST(t)={t}: " + grammar.symbols[tid].name);
        }
    }

    // 规则 2：空产生式 A->epsilon 必须使 epsilon 属于 FIRST(A)。
    for (const auto& production : grammar.productions) {
        if (!production.rhs_symbol_ids.empty()) {
            continue;
        }
        if (!is_valid_symbol_id(grammar, production.lhs_symbol_id) ||
            production.lhs_symbol_id >= static_cast<int>(result.first_sets_by_symbol_id.size())) {
            continue;
        }
        const auto& first_lhs = result.first_sets_by_symbol_id[production.lhs_symbol_id];
        if (first_lhs.find(grammar.epsilon_symbol_id) == first_lhs.end()) {
            report.errors.push_back("空产生式未传播 epsilon 到 FIRST(lhs): #" +
                                    std::to_string(production.id));
        }
    }

    // 规则 3：若产生式 A->X...，则 FIRST(X)-{epsilon} 应并入 FIRST(A)。
    for (const auto& production : grammar.productions) {
        if (production.rhs_symbol_ids.empty()) {
            continue;
        }
        const int lhs = production.lhs_symbol_id;
        const int first_rhs_symbol = production.rhs_symbol_ids.front();
        if (!is_valid_symbol_id(grammar, lhs) || !is_valid_symbol_id(grammar, first_rhs_symbol) ||
            lhs >= static_cast<int>(result.first_sets_by_symbol_id.size()) ||
            first_rhs_symbol >= static_cast<int>(result.first_sets_by_symbol_id.size())) {
            continue;
        }
        const auto& first_lhs = result.first_sets_by_symbol_id[lhs];
        const auto& first_rhs = result.first_sets_by_symbol_id[first_rhs_symbol];
        for (int sid : first_rhs) {
            if (sid == grammar.epsilon_symbol_id) {
                continue;
            }
            if (first_lhs.find(sid) == first_lhs.end()) {
                report.errors.push_back("First 传播缺失: #" + std::to_string(production.id));
                break;
            }
        }
    }

    // 告警：可空非终结符过多时提醒后续 LR(1) 状态可能显著膨胀。
    if (result.nullable_nonterminal_ids.size() > 32) {
        report.warnings.push_back("可空非终结符数量较多，后续 LR(1) 状态规模可能偏大。");
    }

    report.passed = report.errors.empty();
    return report;
}

}  // namespace seu::yacc
