#include "yacc/lr1/lr1_items.h"

#include <algorithm>
#include <deque>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace seu::yacc {
namespace {

struct LR1ItemKeyHash {
    std::size_t operator()(const LR1Item& item) const noexcept {
        std::size_t h1 = static_cast<std::size_t>(item.production_id * 1315423911u);
        std::size_t h2 = static_cast<std::size_t>(item.dot_pos * 2654435761u);
        std::size_t h3 = static_cast<std::size_t>(item.lookahead_symbol_id * 889523592u);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct LR1ItemKeyEq {
    bool operator()(const LR1Item& a, const LR1Item& b) const noexcept {
        return a.production_id == b.production_id && a.dot_pos == b.dot_pos &&
               a.lookahead_symbol_id == b.lookahead_symbol_id;
    }
};

bool is_valid_symbol_id(const Grammar& grammar, int symbol_id) {
    return symbol_id >= 0 && symbol_id < static_cast<int>(grammar.symbols.size());
}

bool is_valid_production_id(const Grammar& grammar, int production_id) {
    return production_id >= 0 && production_id < static_cast<int>(grammar.productions.size());
}

bool item_less(const LR1Item& a, const LR1Item& b) {
    return std::tie(a.production_id, a.dot_pos, a.lookahead_symbol_id) <
           std::tie(b.production_id, b.dot_pos, b.lookahead_symbol_id);
}

std::vector<int> build_beta_plus_lookahead(
    const Grammar& grammar, const LR1Item& item, int dot_symbol_index_in_rhs) {
    std::vector<int> sequence;
    if (!is_valid_production_id(grammar, item.production_id)) {
        return sequence;
    }
    const auto& production = grammar.productions[item.production_id];
    const int beta_begin = dot_symbol_index_in_rhs + 1;
    for (int i = beta_begin; i < static_cast<int>(production.rhs_symbol_ids.size()); ++i) {
        sequence.push_back(production.rhs_symbol_ids[i]);
    }
    sequence.push_back(item.lookahead_symbol_id);
    return sequence;
}

std::vector<LR1Item> closure_of_items(const Grammar& grammar, const FirstSetResult& first_result,
    const std::vector<LR1Item>& seed_items, std::vector<std::string>* notes) {
    std::vector<LR1Item> items;
    items.reserve(seed_items.size());

    std::unordered_set<LR1Item, LR1ItemKeyHash, LR1ItemKeyEq> visited;
    visited.reserve(seed_items.size() * 4 + 16);

    std::deque<LR1Item> pending;
    for (const LR1Item& seed : seed_items) {
        if (visited.insert(seed).second) {
            items.push_back(seed);
            pending.push_back(seed);
        }
    }

    // 标准 LR(1) 闭包：
    // 若 [A->α·Bβ, a] 在集合中，则对每个 B->γ、每个 b∈FIRST(βa) 加入 [B->·γ, b]。
    while (!pending.empty()) {
        const LR1Item cur = pending.front();
        pending.pop_front();
        if (!is_valid_production_id(grammar, cur.production_id)) {
            continue;
        }

        const auto& production = grammar.productions[cur.production_id];
        if (cur.dot_pos < 0 || cur.dot_pos >= static_cast<int>(production.rhs_symbol_ids.size())) {
            continue;
        }

        const int symbol_after_dot = production.rhs_symbol_ids[cur.dot_pos];
        if (!is_valid_symbol_id(grammar, symbol_after_dot)) {
            continue;
        }
        if (grammar.symbols[symbol_after_dot].kind != SymbolKind::Nonterminal) {
            continue;
        }

        const std::vector<int> beta_a = build_beta_plus_lookahead(grammar, cur, cur.dot_pos);
        const std::set<int> first_beta_a =
            compute_first_of_sequence(grammar, first_result, beta_a, 0);

        auto prod_it = grammar.prod_ids_by_lhs.find(symbol_after_dot);
        if (prod_it == grammar.prod_ids_by_lhs.end()) {
            continue;
        }
        for (int pid : prod_it->second) {
            for (int lookahead_id : first_beta_a) {
                if (lookahead_id == grammar.epsilon_symbol_id) {
                    continue;
                }
                LR1Item next_item{pid, 0, lookahead_id};
                if (!visited.insert(next_item).second) {
                    continue;
                }
                items.push_back(next_item);
                pending.push_back(next_item);

                if (notes != nullptr && notes->size() < 4000) {
                    std::ostringstream oss;
                    oss << "由 " << format_lr1_item(grammar, cur) << " 展开 "
                        << grammar.symbols[symbol_after_dot].name << "，"
                        << "FIRST(beta+a) 含 " << grammar.symbols[lookahead_id].name
                        << "，加入 " << format_lr1_item(grammar, next_item);
                    notes->push_back(oss.str());
                }
            }
        }
    }

    std::sort(items.begin(), items.end(), item_less);
    return items;
}

std::vector<LR1Item> goto_of_items(const Grammar& grammar, const FirstSetResult& first_result,
    const std::vector<LR1Item>& items, int transition_symbol_id) {
    std::vector<LR1Item> moved_items;
    for (const LR1Item& item : items) {
        if (!is_valid_production_id(grammar, item.production_id)) {
            continue;
        }
        const auto& production = grammar.productions[item.production_id];
        if (item.dot_pos < 0 || item.dot_pos >= static_cast<int>(production.rhs_symbol_ids.size())) {
            continue;
        }
        if (production.rhs_symbol_ids[item.dot_pos] != transition_symbol_id) {
            continue;
        }
        moved_items.push_back(LR1Item{item.production_id, item.dot_pos + 1, item.lookahead_symbol_id});
    }
    if (moved_items.empty()) {
        return {};
    }
    return closure_of_items(grammar, first_result, moved_items, nullptr);
}

std::string build_item_set_key(const std::vector<LR1Item>& items) {
    std::ostringstream oss;
    for (const auto& item : items) {
        oss << item.production_id << ":" << item.dot_pos << ":" << item.lookahead_symbol_id << ";";
    }
    return oss.str();
}

std::set<int> collect_transition_symbols(const Grammar& grammar, const std::vector<LR1Item>& items) {
    std::set<int> transition_symbols;
    for (const LR1Item& item : items) {
        if (!is_valid_production_id(grammar, item.production_id)) {
            continue;
        }
        const auto& p = grammar.productions[item.production_id];
        if (item.dot_pos < 0 || item.dot_pos >= static_cast<int>(p.rhs_symbol_ids.size())) {
            continue;
        }
        transition_symbols.insert(p.rhs_symbol_ids[item.dot_pos]);
    }
    return transition_symbols;
}

bool is_item_valid(const Grammar& grammar, const LR1Item& item) {
    if (!is_valid_production_id(grammar, item.production_id)) {
        return false;
    }
    if (!is_valid_symbol_id(grammar, item.lookahead_symbol_id)) {
        return false;
    }
    const auto& p = grammar.productions[item.production_id];
    if (item.dot_pos < 0 || item.dot_pos > static_cast<int>(p.rhs_symbol_ids.size())) {
        return false;
    }
    return true;
}

std::string join_symbol_names(const Grammar& grammar, const std::set<int>& symbol_ids) {
    std::ostringstream oss;
    bool first = true;
    for (int sid : symbol_ids) {
        if (!first) {
            oss << ", ";
        }
        if (is_valid_symbol_id(grammar, sid)) {
            oss << grammar.symbols[sid].name;
        } else {
            oss << "<invalid:" << sid << ">";
        }
        first = false;
    }
    return oss.str();
}

}  // namespace

std::string format_lr1_item(const Grammar& grammar, const LR1Item& item) {
    if (!is_valid_production_id(grammar, item.production_id)) {
        return "[invalid item]";
    }
    const auto& p = grammar.productions[item.production_id];
    std::ostringstream oss;
    oss << "[" << grammar.symbols[p.lhs_symbol_id].name << " ->";
    for (int i = 0; i <= static_cast<int>(p.rhs_symbol_ids.size()); ++i) {
        if (i == item.dot_pos) {
            oss << " ·";
        }
        if (i < static_cast<int>(p.rhs_symbol_ids.size())) {
            const int sid = p.rhs_symbol_ids[i];
            if (is_valid_symbol_id(grammar, sid)) {
                oss << " " << grammar.symbols[sid].name;
            } else {
                oss << " <invalid:" << sid << ">";
            }
        }
    }
    oss << ", ";
    if (is_valid_symbol_id(grammar, item.lookahead_symbol_id)) {
        oss << grammar.symbols[item.lookahead_symbol_id].name;
    } else {
        oss << "<invalid:" << item.lookahead_symbol_id << ">";
    }
    oss << "]";
    return oss.str();
}

LR1Step6Result build_step6_lr1_items(const Grammar& grammar, const FirstSetResult& first_result) {
    LR1Step6Result result;

    if (grammar.productions.empty() || !is_valid_symbol_id(grammar, grammar.eof_symbol_id)) {
        return result;
    }

    // I0 kernel 固定为增广产生式起点项：[S' -> ·S, $]。
    const LR1Item i0_seed{0, 0, grammar.eof_symbol_id};
    result.i0_kernel_items.push_back(i0_seed);

    result.i0_closure_items =
        closure_of_items(grammar, first_result, result.i0_kernel_items, &result.lookahead_derivation_notes);

    // 枚举 I0 中点后符号，计算所有非空 goto(I0, X)。
    const std::set<int> transition_symbols = collect_transition_symbols(grammar, result.i0_closure_items);

    for (int symbol_id : transition_symbols) {
        std::vector<LR1Item> goto_items =
            goto_of_items(grammar, first_result, result.i0_closure_items, symbol_id);
        if (goto_items.empty()) {
            continue;
        }
        result.goto_symbol_ids.push_back(symbol_id);
        result.goto_item_sets.push_back(std::move(goto_items));
    }

    return result;
}

LR1Step6ValidationReport validate_step6_lr1_items(
    const Grammar& grammar, const FirstSetResult& first_result, const LR1Step6Result& result) {
    LR1Step6ValidationReport report;

    if (result.i0_kernel_items.empty()) {
        report.errors.push_back("I0 kernel 为空。");
    }
    if (result.i0_closure_items.empty()) {
        report.errors.push_back("closure(I0) 为空。");
    }
    if (result.goto_symbol_ids.size() != result.goto_item_sets.size()) {
        report.errors.push_back("goto 符号集合与项目集数量不一致。");
    }

    std::unordered_set<LR1Item, LR1ItemKeyHash, LR1ItemKeyEq> i0_unique;
    for (const LR1Item& item : result.i0_closure_items) {
        if (!is_item_valid(grammar, item)) {
            report.errors.push_back("I0 项存在非法字段。");
            break;
        }
        if (!i0_unique.insert(item).second) {
            report.errors.push_back("I0 闭包出现重复项，去重策略异常。");
            break;
        }
    }

    // 闭包完备性检查：对每个 [A->α·Bβ,a]，检查 [B->·γ,b] 是否完整存在。
    for (const LR1Item& item : result.i0_closure_items) {
        if (!is_valid_production_id(grammar, item.production_id)) {
            continue;
        }
        const auto& p = grammar.productions[item.production_id];
        if (item.dot_pos < 0 || item.dot_pos >= static_cast<int>(p.rhs_symbol_ids.size())) {
            continue;
        }
        const int symbol_after_dot = p.rhs_symbol_ids[item.dot_pos];
        if (!is_valid_symbol_id(grammar, symbol_after_dot)) {
            continue;
        }
        if (grammar.symbols[symbol_after_dot].kind != SymbolKind::Nonterminal) {
            continue;
        }

        const std::vector<int> beta_a = build_beta_plus_lookahead(grammar, item, item.dot_pos);
        const std::set<int> first_beta_a =
            compute_first_of_sequence(grammar, first_result, beta_a, 0);
        auto prod_it = grammar.prod_ids_by_lhs.find(symbol_after_dot);
        if (prod_it == grammar.prod_ids_by_lhs.end()) {
            report.errors.push_back("闭包展开目标非终结符缺少产生式：" +
                                    grammar.symbols[symbol_after_dot].name);
            continue;
        }
        for (int pid : prod_it->second) {
            for (int lookahead_id : first_beta_a) {
                if (lookahead_id == grammar.epsilon_symbol_id) {
                    continue;
                }
                LR1Item expected{pid, 0, lookahead_id};
                if (i0_unique.find(expected) == i0_unique.end()) {
                    std::ostringstream oss;
                    oss << "闭包缺失项：" << format_lr1_item(grammar, expected)
                        << "，来源项=" << format_lr1_item(grammar, item)
                        << "，FIRST(beta+a)={" << join_symbol_names(grammar, first_beta_a) << "}";
                    report.errors.push_back(oss.str());
                    if (report.errors.size() > 20) {
                        report.warnings.push_back("闭包缺失项较多，错误列表已截断。");
                        report.passed = false;
                        return report;
                    }
                }
            }
        }
    }

    // goto 集合法性检查。
    for (std::size_t i = 0; i < result.goto_item_sets.size(); ++i) {
        if (!is_valid_symbol_id(grammar, result.goto_symbol_ids[i])) {
            report.errors.push_back("goto 符号 ID 非法。");
            break;
        }
        std::unordered_set<LR1Item, LR1ItemKeyHash, LR1ItemKeyEq> uniq;
        for (const LR1Item& item : result.goto_item_sets[i]) {
            if (!is_item_valid(grammar, item)) {
                report.errors.push_back("goto 项集存在非法项。");
                break;
            }
            if (!uniq.insert(item).second) {
                report.errors.push_back("goto 项集中出现重复项。");
                break;
            }
        }
    }

    if (result.lookahead_derivation_notes.empty()) {
        report.warnings.push_back("当前文法在 I0 无可展开项，lookahead 来源说明为空。");
    }

    report.passed = report.errors.empty();
    return report;
}

LR1Step7Result build_step7_lr1_canonical_collection(
    const Grammar& grammar, const FirstSetResult& first_result) {
    LR1Step7Result result;
    if (grammar.productions.empty() || !is_valid_symbol_id(grammar, grammar.eof_symbol_id)) {
        return result;
    }

    const std::vector<LR1Item> i0_kernel = {LR1Item{0, 0, grammar.eof_symbol_id}};
    const std::vector<LR1Item> i0_closure = closure_of_items(grammar, first_result, i0_kernel, nullptr);
    if (i0_closure.empty()) {
        return result;
    }

    std::unordered_map<std::string, int> state_id_by_key;
    std::deque<int> pending;

    result.states.push_back(LR1State{0, i0_closure});
    state_id_by_key.emplace(build_item_set_key(i0_closure), 0);
    pending.push_back(0);

    while (!pending.empty()) {
        const int from_state_id = pending.front();
        pending.pop_front();
        if (from_state_id < 0 || from_state_id >= static_cast<int>(result.states.size())) {
            continue;
        }

        // 注意：后续可能 push_back 新状态导致 result.states 重新分配，
        // 这里必须复制一份源状态项集，避免悬空引用。
        const auto from_items = result.states[from_state_id].items;
        const std::set<int> transition_symbols = collect_transition_symbols(grammar, from_items);
        for (int symbol_id : transition_symbols) {
            std::vector<LR1Item> target_items = goto_of_items(grammar, first_result, from_items, symbol_id);
            if (target_items.empty()) {
                continue;
            }

            const std::string key = build_item_set_key(target_items);
            int to_state_id = -1;
            auto it = state_id_by_key.find(key);
            if (it == state_id_by_key.end()) {
                to_state_id = static_cast<int>(result.states.size());
                result.states.push_back(LR1State{to_state_id, std::move(target_items)});
                state_id_by_key.emplace(key, to_state_id);
                pending.push_back(to_state_id);
            } else {
                to_state_id = it->second;
            }

            result.transitions.push_back(LR1Transition{from_state_id, symbol_id, to_state_id});
        }
    }

    std::sort(result.transitions.begin(), result.transitions.end(),
        [](const LR1Transition& a, const LR1Transition& b) {
            return std::tie(a.from_state_id, a.symbol_id, a.to_state_id) <
                   std::tie(b.from_state_id, b.symbol_id, b.to_state_id);
        });

    return result;
}

LR1Step7ValidationReport validate_step7_lr1_canonical_collection(
    const Grammar& grammar, const FirstSetResult& first_result, const LR1Step7Result& result) {
    LR1Step7ValidationReport report;
    if (result.states.empty()) {
        report.errors.push_back("状态集合为空。");
        report.passed = false;
        return report;
    }
    if (result.states.front().state_id != 0) {
        report.errors.push_back("初始状态编号必须为 0。");
    }

    std::unordered_set<std::string> state_keys;
    for (std::size_t i = 0; i < result.states.size(); ++i) {
        const auto& state = result.states[i];
        if (state.state_id != static_cast<int>(i)) {
            report.errors.push_back("状态编号与数组下标不一致。");
            break;
        }
        if (state.items.empty()) {
            report.errors.push_back("存在空状态项集。");
            break;
        }

        std::unordered_set<LR1Item, LR1ItemKeyHash, LR1ItemKeyEq> uniq_items;
        for (const auto& item : state.items) {
            if (!is_item_valid(grammar, item)) {
                report.errors.push_back("状态中存在非法 LR(1) 项。");
                break;
            }
            if (!uniq_items.insert(item).second) {
                report.errors.push_back("状态中存在重复 LR(1) 项。");
                break;
            }
        }
        const std::string key = build_item_set_key(state.items);
        if (!state_keys.insert(key).second) {
            report.errors.push_back("存在重复状态（项集判等冲突）。");
            break;
        }
    }

    for (const auto& edge : result.transitions) {
        if (edge.from_state_id < 0 || edge.from_state_id >= static_cast<int>(result.states.size())) {
            report.errors.push_back("存在非法转移源状态编号。");
            break;
        }
        if (edge.to_state_id < 0 || edge.to_state_id >= static_cast<int>(result.states.size())) {
            report.errors.push_back("存在非法转移目标状态编号。");
            break;
        }
        if (!is_valid_symbol_id(grammar, edge.symbol_id)) {
            report.errors.push_back("存在非法转移符号编号。");
            break;
        }

        const auto& from_items = result.states[edge.from_state_id].items;
        const auto& to_items = result.states[edge.to_state_id].items;
        const std::vector<LR1Item> expected_items =
            goto_of_items(grammar, first_result, from_items, edge.symbol_id);
        if (build_item_set_key(expected_items) != build_item_set_key(to_items)) {
            report.errors.push_back("存在 goto 与状态转移不一致的边。");
            break;
        }
    }

    if (result.transitions.empty()) {
        report.warnings.push_back("当前规范族没有可用转移边。");
    }

    report.passed = report.errors.empty();
    return report;
}

}  // namespace seu::yacc
