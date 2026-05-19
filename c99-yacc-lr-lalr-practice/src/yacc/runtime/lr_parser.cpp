#include "yacc/runtime/lr_parser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace seu::yacc {
namespace {

std::string trim_copy(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string symbol_name_of(const Grammar& grammar, int symbol_id) {
    if (symbol_id < 0 || symbol_id >= static_cast<int>(grammar.symbols.size())) {
        return "<invalid>";
    }
    return grammar.symbols[symbol_id].name;
}

std::string join_state_stack(const std::vector<int>& state_stack) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < state_stack.size(); ++i) {
        if (i != 0) {
            oss << " ";
        }
        oss << state_stack[i];
    }
    return oss.str();
}

std::string join_symbol_stack(const Grammar& grammar, const std::vector<int>& symbol_stack) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < symbol_stack.size(); ++i) {
        if (i != 0) {
            oss << " ";
        }
        oss << symbol_name_of(grammar, symbol_stack[i]);
    }
    return oss.str();
}

void ensure_eof_token(const Grammar& grammar, std::vector<RuntimeToken>& tokens) {
    if (!tokens.empty() && tokens.back().symbol_id == grammar.eof_symbol_id) {
        return;
    }
    RuntimeToken eof;
    eof.symbol_id = grammar.eof_symbol_id;
    eof.symbol_name = symbol_name_of(grammar, grammar.eof_symbol_id);
    eof.lexeme = "$";
    tokens.push_back(std::move(eof));
}

}  // namespace

std::vector<RuntimeToken> load_runtime_tokens_from_file(
    const Grammar& grammar, const std::string& token_file_path) {
    std::ifstream in(token_file_path);
    if (!in.is_open()) {
        throw std::runtime_error("无法打开 token 文件: " + token_file_path);
    }

    std::vector<RuntimeToken> tokens;
    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::istringstream iss(trimmed);
        RuntimeToken token;
        if (!(iss >> token.symbol_name)) {
            continue;
        }
        auto id_it = grammar.symbol_id_by_name.find(token.symbol_name);
        if (id_it == grammar.symbol_id_by_name.end()) {
            throw std::runtime_error("token 文件第 " + std::to_string(line_no) +
                                     " 行使用了未知符号: " + token.symbol_name);
        }
        token.symbol_id = id_it->second;
        const SymbolKind kind = grammar.symbols[token.symbol_id].kind;
        if (kind == SymbolKind::Nonterminal) {
            throw std::runtime_error("token 文件第 " + std::to_string(line_no) +
                                     " 行使用了非终结符，不能作为输入 token: " + token.symbol_name);
        }
        token.lexeme = token.symbol_name;
        token.line = line_no;
        token.column = 1;

        std::string lexeme;
        if (iss >> lexeme) {
            token.lexeme = lexeme;
        }
        int pos_line = 0;
        if (iss >> pos_line) {
            token.line = pos_line;
            int pos_col = 0;
            if (iss >> pos_col) {
                token.column = pos_col;
            }
        }
        tokens.push_back(std::move(token));
    }

    ensure_eof_token(grammar, tokens);
    return tokens;
}

LRParseRunResult run_step9_lr_parse(const Grammar& grammar, const LR1Step8Result& step8_result,
    const std::vector<RuntimeToken>& input_tokens, int max_steps) {
    LRParseRunResult result;
    if (step8_result.action_table.empty() || step8_result.goto_table.empty()) {
        result.error.has_error = true;
        result.error.message = "第8步分析表为空，无法执行第9步解析。";
        return result;
    }
    if (input_tokens.empty()) {
        result.error.has_error = true;
        result.error.message = "输入 token 序列为空。";
        return result;
    }
    if (max_steps <= 0) {
        max_steps = 1;
    }

    std::vector<int> state_stack;
    std::vector<int> symbol_stack;
    state_stack.push_back(0);

    int input_index = 0;
    int step_no = 0;
    while (step_no < max_steps) {
        ++step_no;
        if (input_index < 0 || input_index >= static_cast<int>(input_tokens.size())) {
            result.error.has_error = true;
            result.error.step_no = step_no;
            result.error.input_index = input_index;
            result.error.message = "读取 lookahead 越界。";
            break;
        }
        if (state_stack.empty()) {
            result.error.has_error = true;
            result.error.step_no = step_no;
            result.error.input_index = input_index;
            result.error.message = "状态栈为空。";
            break;
        }

        const int state_id = state_stack.back();
        if (state_id < 0 || state_id >= static_cast<int>(step8_result.action_table.size())) {
            result.error.has_error = true;
            result.error.step_no = step_no;
            result.error.input_index = input_index;
            result.error.state_id = state_id;
            result.error.message = "当前状态越界。";
            break;
        }

        const RuntimeToken& lookahead = input_tokens[input_index];
        LRParseTraceRow row;
        row.step_no = step_no;
        row.state_id = state_id;
        row.lookahead_symbol_id = lookahead.symbol_id;
        row.lookahead_symbol_name = lookahead.symbol_name;
        row.state_stack_text = join_state_stack(state_stack);
        row.symbol_stack_text = join_symbol_stack(grammar, symbol_stack);
        row.input_index = input_index;

        const auto& action_row = step8_result.action_table[state_id];
        auto action_it = action_row.find(lookahead.symbol_id);
        if (action_it == action_row.end()) {
            result.error.has_error = true;
            result.error.step_no = step_no;
            result.error.input_index = input_index;
            result.error.state_id = state_id;
            result.error.lookahead_symbol_id = lookahead.symbol_id;
            result.error.lookahead_symbol_name = lookahead.symbol_name;
            result.error.message = "Action 表无可用动作，语法错误。";

            std::set<int> expected;
            for (const auto& kv : action_row) {
                expected.insert(kv.first);
            }
            result.error.expected_terminal_ids.assign(expected.begin(), expected.end());
            row.action_text = "error";
            result.trace_rows.push_back(std::move(row));
            break;
        }

        const ParseActionEntry& action = action_it->second;
        row.action_text = format_parse_action_entry(action);

        if (action.type == ParseActionType::Shift) {
            if (action.target_state_id < 0 ||
                action.target_state_id >= static_cast<int>(step8_result.action_table.size())) {
                result.error.has_error = true;
                result.error.step_no = step_no;
                result.error.input_index = input_index;
                result.error.state_id = state_id;
                result.error.lookahead_symbol_id = lookahead.symbol_id;
                result.error.lookahead_symbol_name = lookahead.symbol_name;
                result.error.message = "Shift 目标状态越界。";
                result.trace_rows.push_back(std::move(row));
                break;
            }
            symbol_stack.push_back(lookahead.symbol_id);
            state_stack.push_back(action.target_state_id);
            ++input_index;
            result.trace_rows.push_back(std::move(row));
            continue;
        }

        if (action.type == ParseActionType::Reduce) {
            const int production_id = action.reduce_production_id;
            if (production_id <= 0 || production_id >= static_cast<int>(grammar.productions.size())) {
                result.error.has_error = true;
                result.error.step_no = step_no;
                result.error.input_index = input_index;
                result.error.state_id = state_id;
                result.error.lookahead_symbol_id = lookahead.symbol_id;
                result.error.lookahead_symbol_name = lookahead.symbol_name;
                result.error.message = "Reduce 产生式编号非法。";
                row.production_id = production_id;
                result.trace_rows.push_back(std::move(row));
                break;
            }

            const Production& production = grammar.productions[production_id];
            const int pop_count = static_cast<int>(production.rhs_symbol_ids.size());
            if (static_cast<int>(state_stack.size()) < pop_count + 1 ||
                static_cast<int>(symbol_stack.size()) < pop_count) {
                result.error.has_error = true;
                result.error.step_no = step_no;
                result.error.input_index = input_index;
                result.error.state_id = state_id;
                result.error.lookahead_symbol_id = lookahead.symbol_id;
                result.error.lookahead_symbol_name = lookahead.symbol_name;
                result.error.message = "栈深不足，无法执行归约。";
                row.production_id = production_id;
                result.trace_rows.push_back(std::move(row));
                break;
            }

            for (int i = 0; i < pop_count; ++i) {
                state_stack.pop_back();
                symbol_stack.pop_back();
            }
            const int goto_from_state = state_stack.back();
            if (goto_from_state < 0 || goto_from_state >= static_cast<int>(step8_result.goto_table.size())) {
                result.error.has_error = true;
                result.error.step_no = step_no;
                result.error.input_index = input_index;
                result.error.state_id = goto_from_state;
                result.error.lookahead_symbol_id = lookahead.symbol_id;
                result.error.lookahead_symbol_name = lookahead.symbol_name;
                result.error.message = "Goto 起点状态越界。";
                row.production_id = production_id;
                result.trace_rows.push_back(std::move(row));
                break;
            }
            const auto& goto_row = step8_result.goto_table[goto_from_state];
            auto goto_it = goto_row.find(production.lhs_symbol_id);
            if (goto_it == goto_row.end()) {
                result.error.has_error = true;
                result.error.step_no = step_no;
                result.error.input_index = input_index;
                result.error.state_id = goto_from_state;
                result.error.lookahead_symbol_id = lookahead.symbol_id;
                result.error.lookahead_symbol_name = lookahead.symbol_name;
                result.error.message = "归约后 Goto 缺失。";
                row.production_id = production_id;
                result.trace_rows.push_back(std::move(row));
                break;
            }

            symbol_stack.push_back(production.lhs_symbol_id);
            state_stack.push_back(goto_it->second);
            row.production_id = production_id;
            result.reduction_production_ids.push_back(production_id);
            result.trace_rows.push_back(std::move(row));
            continue;
        }

        // accept
        result.accepted = true;
        result.trace_rows.push_back(std::move(row));
        break;
    }

    if (!result.accepted && !result.error.has_error && step_no >= max_steps) {
        result.error.has_error = true;
        result.error.step_no = step_no;
        result.error.input_index = input_index;
        result.error.message = "达到最大步骤限制，已中止。";
    }

    result.total_steps = step_no;
    result.consumed_tokens = std::max(0, input_index);
    return result;
}

}  // namespace seu::yacc
