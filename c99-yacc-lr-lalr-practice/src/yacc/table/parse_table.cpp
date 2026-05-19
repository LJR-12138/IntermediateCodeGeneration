#include "yacc/table/parse_table.h"

#include <algorithm>
#include <sstream>
#include <tuple>
#include <unordered_map>

namespace seu::yacc {
namespace {

bool is_valid_symbol_id(const Grammar& grammar, int symbol_id) {
    return symbol_id >= 0 && symbol_id < static_cast<int>(grammar.symbols.size());
}

bool is_valid_state_id(const LR1Step7Result& step7_result, int state_id) {
    return state_id >= 0 && state_id < static_cast<int>(step7_result.states.size());
}

bool is_terminal_symbol(const Grammar& grammar, int symbol_id) {
    if (!is_valid_symbol_id(grammar, symbol_id)) {
        return false;
    }
    return grammar.symbols[symbol_id].kind == SymbolKind::Terminal ||
           grammar.symbols[symbol_id].kind == SymbolKind::Special;
}

bool is_nonterminal_symbol(const Grammar& grammar, int symbol_id) {
    if (!is_valid_symbol_id(grammar, symbol_id)) {
        return false;
    }
    return grammar.symbols[symbol_id].kind == SymbolKind::Nonterminal;
}

std::string conflict_type_of(
    const ParseActionEntry& existing, const ParseActionEntry& incoming) {
    if (existing.type == ParseActionType::Shift && incoming.type == ParseActionType::Reduce) {
        return "shift/reduce";
    }
    if (existing.type == ParseActionType::Reduce && incoming.type == ParseActionType::Shift) {
        return "reduce/shift";
    }
    if (existing.type == ParseActionType::Reduce && incoming.type == ParseActionType::Reduce) {
        return "reduce/reduce";
    }
    if (existing.type == ParseActionType::Shift && incoming.type == ParseActionType::Shift) {
        return "shift/shift";
    }
    if (existing.type == ParseActionType::Accept && incoming.type == ParseActionType::Reduce) {
        return "accept/reduce";
    }
    if (existing.type == ParseActionType::Reduce && incoming.type == ParseActionType::Accept) {
        return "reduce/accept";
    }
    if (existing.type == ParseActionType::Shift && incoming.type == ParseActionType::Accept) {
        return "shift/accept";
    }
    if (existing.type == ParseActionType::Accept && incoming.type == ParseActionType::Shift) {
        return "accept/shift";
    }
    if (existing.type == ParseActionType::Accept && incoming.type == ParseActionType::Accept) {
        return "accept/accept";
    }
    return "action/conflict";
}

bool same_action(const ParseActionEntry& a, const ParseActionEntry& b) {
    return std::tie(a.type, a.target_state_id, a.reduce_production_id) ==
           std::tie(b.type, b.target_state_id, b.reduce_production_id);
}

std::vector<LR1Item> collect_related_items(
    const LR1State& state, int symbol_id, const Grammar& grammar) {
    std::vector<LR1Item> related;
    for (const auto& item : state.items) {
        if (item.production_id < 0 || item.production_id >= static_cast<int>(grammar.productions.size())) {
            continue;
        }
        const auto& p = grammar.productions[item.production_id];
        const bool is_reduce_item = item.dot_pos == static_cast<int>(p.rhs_symbol_ids.size());
        const bool has_shift_symbol = item.dot_pos >= 0 &&
                                      item.dot_pos < static_cast<int>(p.rhs_symbol_ids.size()) &&
                                      p.rhs_symbol_ids[item.dot_pos] == symbol_id;
        const bool has_reduce_lookahead = is_reduce_item && item.lookahead_symbol_id == symbol_id;
        if (has_shift_symbol || has_reduce_lookahead) {
            related.push_back(item);
        }
    }
    std::sort(related.begin(), related.end(), [](const LR1Item& a, const LR1Item& b) {
        return std::tie(a.production_id, a.dot_pos, a.lookahead_symbol_id) <
               std::tie(b.production_id, b.dot_pos, b.lookahead_symbol_id);
    });
    return related;
}

void insert_action_entry(const Grammar& grammar, const LR1State& state, int symbol_id,
    const ParseActionEntry& entry, std::unordered_map<int, ParseActionEntry>& action_row,
    std::vector<ParseTableConflict>& conflicts,
    std::vector<ParseTableConflictResolutionLog>& resolution_logs) {
    auto it = action_row.find(symbol_id);
    if (it == action_row.end()) {
        action_row.emplace(symbol_id, entry);
        return;
    }

    if (same_action(it->second, entry)) {
        return;
    }

    ParseTableConflict conflict;
    conflict.state_id = state.state_id;
    conflict.symbol_id = symbol_id;
    const std::string conflict_type = conflict_type_of(it->second, entry);
    const std::string existing_action = format_parse_action_entry(it->second);
    const std::string incoming_action = format_parse_action_entry(entry);
    conflict.conflict_type = conflict_type;
    conflict.existing_action = existing_action;
    conflict.incoming_action = incoming_action;
    conflict.related_items = collect_related_items(state, symbol_id, grammar);
    conflicts.push_back(std::move(conflict));

    // 冲突消解策略（确定性）：
    // 1) shift/reduce：优先 shift（匹配多数 Yacc/Bison 默认行为，含 dangling-else 语义）
    // 2) reduce/reduce：优先产生式编号更小者（稳定、可复现）
    // 3) accept 与其他冲突：优先 accept（只应在 $ 列出现）
    ParseActionEntry resolved = it->second;
    std::string reason = "keep_existing";
    if (it->second.type == ParseActionType::Shift || entry.type == ParseActionType::Shift) {
        if (it->second.type == ParseActionType::Shift) {
            resolved = it->second;
        } else {
            resolved = entry;
        }
        reason = "prefer_shift";
    } else if (it->second.type == ParseActionType::Reduce && entry.type == ParseActionType::Reduce) {
        if (entry.reduce_production_id >= 0 &&
            (it->second.reduce_production_id < 0 ||
                entry.reduce_production_id < it->second.reduce_production_id)) {
            resolved = entry;
        } else {
            resolved = it->second;
        }
        reason = "prefer_smaller_reduce_production_id";
    } else if (it->second.type == ParseActionType::Accept || entry.type == ParseActionType::Accept) {
        if (it->second.type == ParseActionType::Accept) {
            resolved = it->second;
        } else {
            resolved = entry;
        }
        reason = "prefer_accept";
    }

    it->second = resolved;

    ParseTableConflictResolutionLog log;
    log.state_id = state.state_id;
    log.symbol_id = symbol_id;
    log.conflict_type = conflict_type;
    log.existing_action = existing_action;
    log.incoming_action = incoming_action;
    log.resolved_action = format_parse_action_entry(resolved);
    log.reason = reason;
    resolution_logs.push_back(std::move(log));
}

}  // namespace

std::string format_parse_action_entry(const ParseActionEntry& entry) {
    switch (entry.type) {
        case ParseActionType::Shift:
            return "s" + std::to_string(entry.target_state_id);
        case ParseActionType::Reduce:
            return "r" + std::to_string(entry.reduce_production_id);
        case ParseActionType::Accept:
            return "acc";
    }
    return "?";
}

LR1Step8Result build_step8_lr1_parsing_table(
    const Grammar& grammar, const LR1Step7Result& step7_result) {
    LR1Step8Result result;
    result.action_table.resize(step7_result.states.size());
    result.goto_table.resize(step7_result.states.size());

    // 1) 根据状态边填表：终结符/特殊符号 => shift；非终结符 => goto。
    for (const auto& edge : step7_result.transitions) {
        if (!is_valid_state_id(step7_result, edge.from_state_id) ||
            !is_valid_state_id(step7_result, edge.to_state_id) ||
            !is_valid_symbol_id(grammar, edge.symbol_id)) {
            continue;
        }

        if (is_nonterminal_symbol(grammar, edge.symbol_id)) {
            auto& row = result.goto_table[edge.from_state_id];
            auto it = row.find(edge.symbol_id);
            if (it == row.end()) {
                row.emplace(edge.symbol_id, edge.to_state_id);
            } else if (it->second != edge.to_state_id) {
                // goto 理论上不应冲突，若冲突归为结构错误，留给校验阶段报错。
                it->second = edge.to_state_id;
            }
            continue;
        }

        if (is_terminal_symbol(grammar, edge.symbol_id)) {
            ParseActionEntry shift_action;
            shift_action.type = ParseActionType::Shift;
            shift_action.target_state_id = edge.to_state_id;
            shift_action.reduce_production_id = -1;
            insert_action_entry(grammar, step7_result.states[edge.from_state_id], edge.symbol_id, shift_action,
                result.action_table[edge.from_state_id], result.conflicts, result.conflict_resolution_logs);
        }
    }

    // 2) 根据归约项填 Action 表：A -> alpha ·, a => action[state,a] = reduce A->alpha。
    //    特判增广接受项：S' -> S ·, $ => accept。
    for (const auto& state : step7_result.states) {
        if (!is_valid_state_id(step7_result, state.state_id)) {
            continue;
        }
        auto& action_row = result.action_table[state.state_id];
        for (const auto& item : state.items) {
            if (item.production_id < 0 || item.production_id >= static_cast<int>(grammar.productions.size())) {
                continue;
            }
            if (!is_valid_symbol_id(grammar, item.lookahead_symbol_id)) {
                continue;
            }

            const auto& p = grammar.productions[item.production_id];
            if (item.dot_pos != static_cast<int>(p.rhs_symbol_ids.size())) {
                continue;
            }

            ParseActionEntry action;
            if (item.production_id == 0 && item.lookahead_symbol_id == grammar.eof_symbol_id) {
                action.type = ParseActionType::Accept;
                action.target_state_id = -1;
                action.reduce_production_id = -1;
            } else {
                action.type = ParseActionType::Reduce;
                action.target_state_id = -1;
                action.reduce_production_id = item.production_id;
            }
            insert_action_entry(grammar, state, item.lookahead_symbol_id, action, action_row, result.conflicts,
                result.conflict_resolution_logs);
        }
    }

    return result;
}

LR1Step8ValidationReport validate_step8_lr1_parsing_table(const Grammar& grammar,
    const LR1Step7Result& step7_result, const LR1Step8Result& step8_result) {
    LR1Step8ValidationReport report;

    if (step8_result.action_table.size() != step7_result.states.size()) {
        report.errors.push_back("Action 表状态行数与 LR(1) 状态数不一致。");
    }
    if (step8_result.goto_table.size() != step7_result.states.size()) {
        report.errors.push_back("Goto 表状态行数与 LR(1) 状态数不一致。");
    }

    // 预建状态边索引：用于校验 shift/goto 是否与状态图一致。
    std::unordered_map<long long, int> transition_target_by_key;
    transition_target_by_key.reserve(step7_result.transitions.size() * 2 + 16);
    for (const auto& edge : step7_result.transitions) {
        const long long key =
            (static_cast<long long>(edge.from_state_id) << 32) ^ static_cast<unsigned int>(edge.symbol_id);
        transition_target_by_key[key] = edge.to_state_id;
    }

    for (std::size_t state_id = 0; state_id < step8_result.action_table.size(); ++state_id) {
        const auto& row = step8_result.action_table[state_id];
        for (const auto& kv : row) {
            const int symbol_id = kv.first;
            const ParseActionEntry& action = kv.second;
            if (!is_terminal_symbol(grammar, symbol_id)) {
                report.errors.push_back("Action 表出现非终结符列。");
                break;
            }
            if (action.type == ParseActionType::Shift) {
                if (!is_valid_state_id(step7_result, action.target_state_id)) {
                    report.errors.push_back("Shift 动作目标状态非法。");
                    break;
                }
                const long long key =
                    (static_cast<long long>(state_id) << 32) ^ static_cast<unsigned int>(symbol_id);
                auto it = transition_target_by_key.find(key);
                if (it == transition_target_by_key.end() || it->second != action.target_state_id) {
                    report.errors.push_back("Shift 动作与状态转移图不一致。");
                    break;
                }
            } else if (action.type == ParseActionType::Reduce) {
                if (action.reduce_production_id <= 0 ||
                    action.reduce_production_id >= static_cast<int>(grammar.productions.size())) {
                    report.errors.push_back("Reduce 动作产生式编号非法。");
                    break;
                }
                if (!is_valid_state_id(step7_result, static_cast<int>(state_id))) {
                    report.errors.push_back("Reduce 动作所在状态非法。");
                    break;
                }
                bool found = false;
                for (const auto& item : step7_result.states[state_id].items) {
                    if (item.production_id != action.reduce_production_id) {
                        continue;
                    }
                    const auto& p = grammar.productions[item.production_id];
                    if (item.dot_pos == static_cast<int>(p.rhs_symbol_ids.size()) &&
                        item.lookahead_symbol_id == symbol_id) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    report.errors.push_back("Reduce 动作无法在状态项集中找到依据。");
                    break;
                }
            } else if (action.type == ParseActionType::Accept) {
                if (symbol_id != grammar.eof_symbol_id) {
                    report.errors.push_back("Accept 动作只能出现在 `$` 列。");
                    break;
                }
                bool found = false;
                for (const auto& item : step7_result.states[state_id].items) {
                    if (item.production_id != 0) {
                        continue;
                    }
                    const auto& p = grammar.productions[0];
                    if (item.dot_pos == static_cast<int>(p.rhs_symbol_ids.size()) &&
                        item.lookahead_symbol_id == grammar.eof_symbol_id) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    report.errors.push_back("Accept 动作无法在状态项集中找到增广接受项。");
                    break;
                }
            }
        }
    }

    for (std::size_t state_id = 0; state_id < step8_result.goto_table.size(); ++state_id) {
        const auto& row = step8_result.goto_table[state_id];
        for (const auto& kv : row) {
            const int symbol_id = kv.first;
            const int to_state_id = kv.second;
            if (!is_nonterminal_symbol(grammar, symbol_id)) {
                report.errors.push_back("Goto 表出现非非终结符列。");
                break;
            }
            if (!is_valid_state_id(step7_result, to_state_id)) {
                report.errors.push_back("Goto 表目标状态非法。");
                break;
            }
            const long long key =
                (static_cast<long long>(state_id) << 32) ^ static_cast<unsigned int>(symbol_id);
            auto it = transition_target_by_key.find(key);
            if (it == transition_target_by_key.end() || it->second != to_state_id) {
                report.errors.push_back("Goto 表项与状态转移图不一致。");
                break;
            }
        }
    }

    if (!step8_result.conflicts.empty()) {
        report.warnings.push_back(
            "检测到分析表冲突，详见 raw/parse_table_conflicts.tsv（课程要求应保留冲突报告）。");
    }
    if (step8_result.conflict_resolution_logs.size() != step8_result.conflicts.size()) {
        report.errors.push_back("冲突数量与冲突消解日志数量不一致。");
    }

    report.passed = report.errors.empty();
    return report;
}

}  // namespace seu::yacc
