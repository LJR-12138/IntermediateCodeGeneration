#include "yacc/lalr/lalr_builder.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace seu::yacc {
namespace {

bool item_less(const LR1Item& a, const LR1Item& b) {
    return std::tie(a.production_id, a.dot_pos, a.lookahead_symbol_id) <
           std::tie(b.production_id, b.dot_pos, b.lookahead_symbol_id);
}

bool state_less(const LR1State& a, const LR1State& b) {
    return a.state_id < b.state_id;
}

bool transition_less(const LR1Transition& a, const LR1Transition& b) {
    return std::tie(a.from_state_id, a.symbol_id, a.to_state_id) <
           std::tie(b.from_state_id, b.symbol_id, b.to_state_id);
}

std::string build_lr0_core_key(const std::vector<LR1Item>& items) {
    std::vector<std::pair<int, int>> core_pairs;
    core_pairs.reserve(items.size());
    for (const auto& item : items) {
        core_pairs.emplace_back(item.production_id, item.dot_pos);
    }
    std::sort(core_pairs.begin(), core_pairs.end());
    core_pairs.erase(std::unique(core_pairs.begin(), core_pairs.end()), core_pairs.end());

    std::ostringstream oss;
    for (const auto& [pid, dot] : core_pairs) {
        oss << pid << ":" << dot << ";";
    }
    return oss.str();
}

std::vector<LR1Item> merge_items_by_lr0_core_and_lookahead(const std::vector<LR1State>& source_states) {
    // key=(production_id,dot_pos), value=lookahead集合
    std::map<std::pair<int, int>, std::set<int>> lookaheads_by_core;
    for (const auto& state : source_states) {
        for (const auto& item : state.items) {
            lookaheads_by_core[{item.production_id, item.dot_pos}].insert(item.lookahead_symbol_id);
        }
    }

    std::vector<LR1Item> merged;
    for (const auto& kv : lookaheads_by_core) {
        const int pid = kv.first.first;
        const int dot = kv.first.second;
        for (int lookahead : kv.second) {
            merged.push_back(LR1Item{pid, dot, lookahead});
        }
    }
    std::sort(merged.begin(), merged.end(), item_less);
    return merged;
}

bool is_valid_state_id(const std::vector<LR1State>& states, int state_id) {
    return state_id >= 0 && state_id < static_cast<int>(states.size());
}

}  // namespace

LR1Step10Result build_step10_lalr_from_lr1(
    const Grammar& grammar, const LR1Step7Result& lr1_step7_result, const LR1Step8Result& lr1_step8_result) {
    LR1Step10Result result;
    result.lr1_state_count = lr1_step7_result.states.size();
    result.lr1_conflict_count = lr1_step8_result.conflicts.size();
    result.lr1_to_lalr_state_id.assign(lr1_step7_result.states.size(), -1);

    if (lr1_step7_result.states.empty()) {
        return result;
    }

    // 1) 先按 LR(0) 核分组。
    std::unordered_map<std::string, int> group_id_by_core;
    std::vector<std::vector<int>> source_state_ids_by_group;
    for (const auto& state : lr1_step7_result.states) {
        const std::string core_key = build_lr0_core_key(state.items);
        auto it = group_id_by_core.find(core_key);
        int group_id = -1;
        if (it == group_id_by_core.end()) {
            group_id = static_cast<int>(source_state_ids_by_group.size());
            group_id_by_core.emplace(core_key, group_id);
            source_state_ids_by_group.push_back({});
        } else {
            group_id = it->second;
        }
        source_state_ids_by_group[group_id].push_back(state.state_id);
    }

    // 2) 为了让 I0 稳定对应 LALR I0，按“是否包含 LR(1) I0 + 最小原状态号”排序分组。
    struct GroupOrder {
        int original_group_id = -1;
        int has_lr1_i0 = 0;
        int min_state_id = 1 << 30;
    };
    std::vector<GroupOrder> order;
    order.reserve(source_state_ids_by_group.size());
    for (std::size_t gid = 0; gid < source_state_ids_by_group.size(); ++gid) {
        GroupOrder row;
        row.original_group_id = static_cast<int>(gid);
        for (int sid : source_state_ids_by_group[gid]) {
            row.min_state_id = std::min(row.min_state_id, sid);
            if (sid == 0) {
                row.has_lr1_i0 = 1;
            }
        }
        order.push_back(row);
    }
    std::sort(order.begin(), order.end(), [](const GroupOrder& a, const GroupOrder& b) {
        if (a.has_lr1_i0 != b.has_lr1_i0) {
            return a.has_lr1_i0 > b.has_lr1_i0;
        }
        if (a.min_state_id != b.min_state_id) {
            return a.min_state_id < b.min_state_id;
        }
        return a.original_group_id < b.original_group_id;
    });

    std::vector<int> new_group_id_by_old(source_state_ids_by_group.size(), -1);
    for (std::size_t new_gid = 0; new_gid < order.size(); ++new_gid) {
        new_group_id_by_old[order[new_gid].original_group_id] = static_cast<int>(new_gid);
    }

    // 3) 组装 merge_groups + 构建 LR(1)->LALR 映射。
    result.merge_groups.resize(order.size());
    for (std::size_t new_gid = 0; new_gid < order.size(); ++new_gid) {
        const auto& row = order[new_gid];
        const auto& source_ids = source_state_ids_by_group[row.original_group_id];

        std::vector<LR1State> source_states;
        source_states.reserve(source_ids.size());
        for (int sid : source_ids) {
            if (!is_valid_state_id(lr1_step7_result.states, sid)) {
                continue;
            }
            source_states.push_back(lr1_step7_result.states[sid]);
            result.lr1_to_lalr_state_id[sid] = static_cast<int>(new_gid);
        }

        auto& group = result.merge_groups[new_gid];
        group.lalr_state_id = static_cast<int>(new_gid);
        group.source_lr1_state_ids = source_ids;
        std::sort(group.source_lr1_state_ids.begin(), group.source_lr1_state_ids.end());
        group.merged_items = merge_items_by_lr0_core_and_lookahead(source_states);
    }

    // 4) 组装 LALR 状态集合。
    result.lalr_step7_result.states.reserve(result.merge_groups.size());
    for (const auto& group : result.merge_groups) {
        result.lalr_step7_result.states.push_back(LR1State{group.lalr_state_id, group.merged_items});
    }
    std::sort(result.lalr_step7_result.states.begin(), result.lalr_step7_result.states.end(), state_less);

    // 5) 组装 LALR 转移：把 LR(1) 边按映射投影后去重。
    std::set<std::tuple<int, int, int>> dedup_edges;
    for (const auto& edge : lr1_step7_result.transitions) {
        if (!is_valid_state_id(lr1_step7_result.states, edge.from_state_id) ||
            !is_valid_state_id(lr1_step7_result.states, edge.to_state_id)) {
            continue;
        }
        const int from_lalr = result.lr1_to_lalr_state_id[edge.from_state_id];
        const int to_lalr = result.lr1_to_lalr_state_id[edge.to_state_id];
        if (from_lalr < 0 || to_lalr < 0) {
            continue;
        }
        dedup_edges.insert({from_lalr, edge.symbol_id, to_lalr});
    }
    for (const auto& t : dedup_edges) {
        result.lalr_step7_result.transitions.push_back(
            LR1Transition{std::get<0>(t), std::get<1>(t), std::get<2>(t)});
    }
    std::sort(
        result.lalr_step7_result.transitions.begin(), result.lalr_step7_result.transitions.end(), transition_less);

    // 6) 基于 LALR 状态机构造新的 Action/Goto 表。
    result.lalr_step8_result = build_step8_lr1_parsing_table(grammar, result.lalr_step7_result);
    result.lalr_state_count = result.lalr_step7_result.states.size();
    result.lalr_conflict_count = result.lalr_step8_result.conflicts.size();
    return result;
}

LR1Step10ValidationReport validate_step10_lalr_from_lr1(const Grammar& grammar,
    const LR1Step7Result& lr1_step7_result, const LR1Step8Result& lr1_step8_result,
    const LR1Step10Result& step10_result) {
    LR1Step10ValidationReport report;

    if (step10_result.lr1_to_lalr_state_id.size() != lr1_step7_result.states.size()) {
        report.errors.push_back("LR(1)->LALR 映射大小与 LR(1) 状态数不一致。");
    }

    for (std::size_t sid = 0; sid < step10_result.lr1_to_lalr_state_id.size(); ++sid) {
        const int mapped = step10_result.lr1_to_lalr_state_id[sid];
        if (mapped < 0 || mapped >= static_cast<int>(step10_result.lalr_step7_result.states.size())) {
            report.errors.push_back("存在未映射或非法映射的 LR(1) 状态。");
            break;
        }
    }

    if (step10_result.lalr_step7_result.states.empty()) {
        report.errors.push_back("LALR 状态集合为空。");
    } else if (step10_result.lalr_step7_result.states.front().state_id != 0) {
        report.errors.push_back("LALR 初始状态编号必须为 0。");
    }

    for (const auto& group : step10_result.merge_groups) {
        if (group.source_lr1_state_ids.empty()) {
            report.errors.push_back("存在空合并组。");
            break;
        }
        if (group.merged_items.empty()) {
            report.errors.push_back("存在合并后空状态。");
            break;
        }
    }

    // 检查转移确定性：同一 from+symbol 不应对应多个 to。
    std::unordered_map<long long, int> target_by_key;
    for (const auto& edge : step10_result.lalr_step7_result.transitions) {
        const long long key =
            (static_cast<long long>(edge.from_state_id) << 32) ^ static_cast<unsigned int>(edge.symbol_id);
        auto it = target_by_key.find(key);
        if (it == target_by_key.end()) {
            target_by_key.emplace(key, edge.to_state_id);
        } else if (it->second != edge.to_state_id) {
            report.errors.push_back("LALR 转移存在同一 from+symbol 指向多个目标状态。");
            break;
        }
    }

    const auto lalr_step8_validation =
        validate_step8_lr1_parsing_table(grammar, step10_result.lalr_step7_result, step10_result.lalr_step8_result);
    if (!lalr_step8_validation.passed) {
        report.errors.push_back("LALR Action/Goto 表校验失败。");
        for (const auto& e : lalr_step8_validation.errors) {
            report.errors.push_back("  - " + e);
            if (report.errors.size() > 30) {
                report.warnings.push_back("LALR 构表错误较多，已截断。");
                break;
            }
        }
    }

    if (step10_result.lalr_conflict_count > step10_result.lr1_conflict_count) {
        report.warnings.push_back("LALR 冲突数高于 LR(1)，需重点分析合并后的歧义传播。");
    }
    if (step10_result.lalr_state_count >= step10_result.lr1_state_count &&
        step10_result.lr1_state_count > 0) {
        report.warnings.push_back("LALR 状态数未下降，需检查文法是否几乎无可合并核。");
    }

    if (step10_result.lr1_conflict_count != lr1_step8_result.conflicts.size()) {
        report.warnings.push_back("记录的 LR(1) 冲突数与 step8 输入不一致（以 step8 输入为准）。");
    }

    report.passed = report.errors.empty();
    return report;
}

}  // namespace seu::yacc
