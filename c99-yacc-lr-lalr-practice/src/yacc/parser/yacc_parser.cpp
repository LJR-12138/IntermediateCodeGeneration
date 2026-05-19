#include "yacc/parser/yacc_parser.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace seu::yacc {
namespace {

struct SectionRanges {
    std::string definitions;
    std::string rules;
    std::string user_subroutines;
    int definitions_start_line = 1;
    int rules_start_line = 1;
    int user_start_line = 1;
};

// 规则解析前的中间结构，先保留符号名字，之后统一分配 ID。
struct ProductionDraft {
    std::string lhs_name;
    std::vector<std::string> rhs_symbol_names;
    std::vector<bool> rhs_is_literal_char;
    ActionBlock action;
    int source_line = 0;
};

// 读取整个文件文本。
std::string read_text_file(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("无法打开文件: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool is_identifier_start(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

bool is_identifier_char(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

std::string trim(const std::string& s) {
    size_t begin = 0;
    while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin])) != 0) {
        ++begin;
    }

    size_t end = s.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
        --end;
    }
    return s.substr(begin, end - begin);
}

// 按行切分，并保留换行符，便于 section 拼接后仍保留原始结构。
std::vector<std::string> split_lines_keep_newline(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char ch : text) {
        current.push_back(ch);
        if (ch == '\n') {
            lines.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

// 依据 %% 分割 Definitions / Rules / User 三段。
SectionRanges split_sections(const std::string& text) {
    const std::vector<std::string> lines = split_lines_keep_newline(text);

    int marker_count = 0;
    int first_marker_line = -1;
    int second_marker_line = -1;

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const std::string normalized = trim(lines[i]);
        if (normalized == "%%") {
            ++marker_count;
            if (first_marker_line < 0) {
                first_marker_line = i + 1;
            } else if (second_marker_line < 0) {
                second_marker_line = i + 1;
            }
        }
    }

    if (marker_count < 2 || first_marker_line < 0 || second_marker_line < 0) {
        throw ParseError(1, 1, "Yacc 文件必须包含两个 `%%` 分隔符");
    }

    SectionRanges sections;
    sections.definitions_start_line = 1;
    sections.rules_start_line = first_marker_line + 1;
    sections.user_start_line = second_marker_line + 1;

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const int line_no = i + 1;
        if (line_no < first_marker_line) {
            sections.definitions += lines[i];
        } else if (line_no > first_marker_line && line_no < second_marker_line) {
            sections.rules += lines[i];
        } else if (line_no > second_marker_line) {
            sections.user_subroutines += lines[i];
        }
    }

    return sections;
}

void register_symbol_if_absent(
    Grammar& grammar, const std::string& name, SymbolKind kind, bool is_literal_char = false) {
    if (grammar.symbol_id_by_name.find(name) != grammar.symbol_id_by_name.end()) {
        return;
    }

    Symbol symbol;
    symbol.id = static_cast<int>(grammar.symbols.size());
    symbol.name = name;
    symbol.kind = kind;
    symbol.is_literal_char = is_literal_char;

    grammar.symbols.push_back(symbol);
    grammar.symbol_id_by_name.emplace(name, symbol.id);

    if (kind == SymbolKind::Terminal) {
        grammar.terminal_ids.push_back(symbol.id);
    } else if (kind == SymbolKind::Nonterminal) {
        grammar.nonterminal_ids.push_back(symbol.id);
    }
}

// 解析 Definitions 段，仅处理第 1 版范围：%token 和 %start。
void parse_definitions(
    const std::string& definitions, int base_line, std::unordered_set<std::string>& token_names,
    std::string& start_symbol_name) {
    const std::vector<std::string> lines = split_lines_keep_newline(definitions);

    bool in_code_block = false;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const int current_line = base_line + i;
        std::string line = lines[i];
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        const std::string normalized = trim(line);

        if (normalized.empty()) {
            continue;
        }

        if (normalized == "%{") {
            in_code_block = true;
            continue;
        }
        if (normalized == "%}") {
            in_code_block = false;
            continue;
        }
        if (in_code_block) {
            continue;
        }

        if (normalized.rfind("%token", 0) == 0) {
            std::istringstream iss(normalized.substr(6));
            std::string token;
            bool found_any = false;
            while (iss >> token) {
                token_names.insert(token);
                found_any = true;
            }
            if (!found_any) {
                throw ParseError(current_line, 1, "%token 后未找到任何 token");
            }
            continue;
        }

        if (normalized.rfind("%start", 0) == 0) {
            std::istringstream iss(normalized.substr(6));
            std::string start;
            if (!(iss >> start)) {
                throw ParseError(current_line, 1, "%start 后未找到开始符号");
            }
            if (!start_symbol_name.empty() && start_symbol_name != start) {
                throw ParseError(current_line, 1, "检测到多个不同的 %start 声明");
            }
            start_symbol_name = start;
            continue;
        }

        // 其他指令在第一版中允许存在，但不处理。
    }
}

struct RuleCursor {
    const std::string& text;
    size_t pos = 0;
    int line = 1;
    int column = 1;
};

bool is_eof(const RuleCursor& c) {
    return c.pos >= c.text.size();
}

char peek(const RuleCursor& c) {
    return is_eof(c) ? '\0' : c.text[c.pos];
}

char advance(RuleCursor& c) {
    if (is_eof(c)) {
        return '\0';
    }
    char ch = c.text[c.pos++];
    if (ch == '\n') {
        ++c.line;
        c.column = 1;
    } else {
        ++c.column;
    }
    return ch;
}

void skip_spaces(RuleCursor& c) {
    while (!is_eof(c)) {
        char ch = peek(c);
        if (std::isspace(static_cast<unsigned char>(ch)) == 0) {
            break;
        }
        advance(c);
    }
}

[[noreturn]] void fail_here(const RuleCursor& c, const std::string& message, int base_line) {
    throw ParseError(base_line + c.line - 1, c.column, message);
}

std::string parse_identifier(RuleCursor& c, int base_line) {
    if (!is_identifier_start(peek(c))) {
        fail_here(c, "期望标识符", base_line);
    }
    std::string out;
    out.push_back(advance(c));
    while (!is_eof(c) && is_identifier_char(peek(c))) {
        out.push_back(advance(c));
    }
    return out;
}

// 读取字符字面量，如 '(' 或 '\n'，内部按原样保存为完整字面量文本。
std::string parse_char_literal(RuleCursor& c, int base_line) {
    if (peek(c) != '\'') {
        fail_here(c, "期望字符字面量", base_line);
    }
    std::string out;
    out.push_back(advance(c));
    bool escaped = false;
    while (!is_eof(c)) {
        char ch = advance(c);
        out.push_back(ch);
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '\'') {
            return out;
        }
    }
    fail_here(c, "字符字面量未闭合", base_line);
}

// 读取动作块，支持嵌套花括号和字符串/字符常量，防止误判结束位置。
std::string parse_action_block(RuleCursor& c, int base_line) {
    if (peek(c) != '{') {
        fail_here(c, "期望动作块起始 `{`", base_line);
    }
    std::string raw;
    int brace_depth = 0;
    bool in_string = false;
    bool in_char = false;
    bool escaped = false;

    while (!is_eof(c)) {
        const char ch = advance(c);
        raw.push_back(ch);

        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (in_char) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '\'') {
                in_char = false;
            }
            continue;
        }

        if (ch == '"') {
            in_string = true;
            continue;
        }
        if (ch == '\'') {
            in_char = true;
            continue;
        }
        if (ch == '{') {
            ++brace_depth;
            continue;
        }
        if (ch == '}') {
            --brace_depth;
            if (brace_depth == 0) {
                return raw;
            }
        }
    }
    fail_here(c, "动作块未闭合", base_line);
}

void append_production_from_buffer(const std::string& lhs_name, const std::vector<std::string>& rhs_names,
    const std::vector<bool>& rhs_is_literal, const ActionBlock& action, int source_line,
    std::vector<ProductionDraft>& out_drafts) {
    ProductionDraft draft;
    draft.lhs_name = lhs_name;
    draft.rhs_symbol_names = rhs_names;
    draft.rhs_is_literal_char = rhs_is_literal;
    draft.action = action;
    draft.source_line = source_line;
    out_drafts.push_back(std::move(draft));
}

// 解析 Rules 段，产出产生式草稿及 lhs 集合。
void parse_rules(const std::string& rules, int base_line, std::vector<ProductionDraft>& drafts,
    std::unordered_set<std::string>& lhs_names) {
    RuleCursor c{rules, 0, 1, 1};

    while (!is_eof(c)) {
        skip_spaces(c);
        if (is_eof(c)) {
            break;
        }

        const int rule_line = base_line + c.line - 1;
        const std::string lhs_name = parse_identifier(c, base_line);
        lhs_names.insert(lhs_name);

        skip_spaces(c);
        if (peek(c) != ':') {
            fail_here(c, "产生式左部后缺少 `:`", base_line);
        }
        advance(c);  // consume :

        std::vector<std::string> rhs_buffer;
        std::vector<bool> rhs_literal_buffer;
        ActionBlock action_buffer;

        while (!is_eof(c)) {
            skip_spaces(c);
            if (is_eof(c)) {
                fail_here(c, "规则未正常结束，缺少 `;`", base_line);
            }

            const char ch = peek(c);
            if (ch == '|') {
                append_production_from_buffer(
                    lhs_name, rhs_buffer, rhs_literal_buffer, action_buffer, rule_line, drafts);
                rhs_buffer.clear();
                rhs_literal_buffer.clear();
                action_buffer = ActionBlock{};
                advance(c);  // consume |
                continue;
            }
            if (ch == ';') {
                append_production_from_buffer(
                    lhs_name, rhs_buffer, rhs_literal_buffer, action_buffer, rule_line, drafts);
                advance(c);  // consume ;
                break;
            }
            if (ch == '{') {
                if (action_buffer.present) {
                    fail_here(c, "同一候选式中不允许多个动作块", base_line);
                }
                action_buffer.present = true;
                action_buffer.raw_code = parse_action_block(c, base_line);
                continue;
            }
            if (ch == '\'') {
                const std::string lit = parse_char_literal(c, base_line);
                rhs_buffer.push_back(lit);
                rhs_literal_buffer.push_back(true);
                continue;
            }
            if (ch == '%' || ch == '<') {
                fail_here(c, "第一版暂不支持该语法元素（如 %prec 或类型标签）", base_line);
            }
            if (is_identifier_start(ch)) {
                rhs_buffer.push_back(parse_identifier(c, base_line));
                rhs_literal_buffer.push_back(false);
                continue;
            }

            fail_here(c, std::string("无法识别的规则符号: `") + ch + "`", base_line);
        }
    }
}

void finalize_grammar(Grammar& grammar, const std::unordered_set<std::string>& token_names,
    const std::unordered_set<std::string>& lhs_names, const std::vector<ProductionDraft>& drafts,
    const std::string& start_symbol_name) {
    // 1) 注册 token 终结符。
    for (const auto& token : token_names) {
        register_symbol_if_absent(grammar, token, SymbolKind::Terminal, false);
    }

    // 2) 注册非终结符（以 lhs 为准）。
    for (const auto& lhs : lhs_names) {
        register_symbol_if_absent(grammar, lhs, SymbolKind::Nonterminal, false);
    }

    // 3) 注册特殊符号。
    register_symbol_if_absent(grammar, kEofSymbolName, SymbolKind::Special, false);
    register_symbol_if_absent(grammar, kEpsilonSymbolName, SymbolKind::Special, false);
    register_symbol_if_absent(grammar, kAugmentedStartName, SymbolKind::Special, false);
    grammar.eof_symbol_id = grammar.symbol_id_by_name.at(kEofSymbolName);
    grammar.epsilon_symbol_id = grammar.symbol_id_by_name.at(kEpsilonSymbolName);
    grammar.augmented_start_symbol_id = grammar.symbol_id_by_name.at(kAugmentedStartName);

    if (start_symbol_name.empty()) {
        throw ParseError(1, 1, "缺少 %start 声明");
    }
    auto start_it = grammar.symbol_id_by_name.find(start_symbol_name);
    if (start_it == grammar.symbol_id_by_name.end()) {
        throw ParseError(1, 1, "开始符号未在规则左部出现: " + start_symbol_name);
    }
    grammar.start_symbol_id = start_it->second;

    // 4) 增广产生式放在第 0 条：S' -> start_symbol
    Production augmented;
    augmented.id = 0;
    augmented.lhs_symbol_id = grammar.augmented_start_symbol_id;
    augmented.rhs_symbol_ids = {grammar.start_symbol_id};
    augmented.source_line = 1;
    grammar.productions.push_back(augmented);

    // 5) 录入普通产生式。
    int next_prod_id = 1;
    for (const auto& draft : drafts) {
        Production p;
        p.id = next_prod_id++;
        p.source_line = draft.source_line;
        p.action = draft.action;

        auto lhs_it = grammar.symbol_id_by_name.find(draft.lhs_name);
        if (lhs_it == grammar.symbol_id_by_name.end()) {
            throw ParseError(draft.source_line, 1, "未知的产生式左部: " + draft.lhs_name);
        }
        p.lhs_symbol_id = lhs_it->second;

        for (size_t i = 0; i < draft.rhs_symbol_names.size(); ++i) {
            const std::string& name = draft.rhs_symbol_names[i];
            const bool is_literal = draft.rhs_is_literal_char[i];

            if (is_literal) {
                register_symbol_if_absent(grammar, name, SymbolKind::Terminal, true);
            } else {
                if (token_names.find(name) != token_names.end()) {
                    register_symbol_if_absent(grammar, name, SymbolKind::Terminal, false);
                } else if (lhs_names.find(name) != lhs_names.end()) {
                    register_symbol_if_absent(grammar, name, SymbolKind::Nonterminal, false);
                } else {
                    throw ParseError(
                        draft.source_line, 1, "规则右部出现未定义符号: " + name);
                }
            }
            p.rhs_symbol_ids.push_back(grammar.symbol_id_by_name.at(name));
        }

        grammar.productions.push_back(std::move(p));
    }

    // 6) 构建 lhs -> 产生式索引，供后续 Closure 快速访问。
    for (const auto& prod : grammar.productions) {
        grammar.prod_ids_by_lhs[prod.lhs_symbol_id].push_back(prod.id);
    }
}

}  // namespace

ParseError::ParseError(int line, int column, const std::string& message)
    : std::runtime_error(
          "ParseError at line " + std::to_string(line) + ", column " + std::to_string(column) +
          ": " + message),
      line_(line),
      column_(column) {}

Grammar parse_yacc_file(const std::string& path) {
    const std::string content = read_text_file(path);
    const SectionRanges sections = split_sections(content);

    std::unordered_set<std::string> token_names;
    std::string start_symbol_name;
    parse_definitions(
        sections.definitions, sections.definitions_start_line, token_names, start_symbol_name);

    std::vector<ProductionDraft> drafts;
    std::unordered_set<std::string> lhs_names;
    parse_rules(sections.rules, sections.rules_start_line, drafts, lhs_names);

    Grammar grammar;
    grammar.source_path = path;
    grammar.user_subroutines_raw = sections.user_subroutines;
    finalize_grammar(grammar, token_names, lhs_names, drafts, start_symbol_name);
    return grammar;
}

}  // namespace seu::yacc

