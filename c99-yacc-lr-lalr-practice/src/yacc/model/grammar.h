#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace seu::yacc {

// 符号类别：终结符、非终结符、特殊符号。
enum class SymbolKind {
    Terminal,
    Nonterminal,
    Special
};

// 语义动作占位。第 3 步只保存原始动作代码，不执行。
struct ActionBlock {
    bool present = false;
    std::string raw_code;
};

// 文法符号定义。
struct Symbol {
    int id = -1;
    std::string name;
    SymbolKind kind = SymbolKind::Special;
    bool is_literal_char = false;
};

// 产生式定义。
struct Production {
    int id = -1;
    int lhs_symbol_id = -1;
    std::vector<int> rhs_symbol_ids;
    ActionBlock action;
    int source_line = 0;
};

// 文法整体结构。后续 First、LR 项目集、分析表都基于该结构。
struct Grammar {
    std::string source_path;

    int start_symbol_id = -1;
    int augmented_start_symbol_id = -1;
    int eof_symbol_id = -1;
    int epsilon_symbol_id = -1;

    std::vector<Symbol> symbols;
    std::vector<int> terminal_ids;
    std::vector<int> nonterminal_ids;
    std::vector<Production> productions;

    std::unordered_map<std::string, int> symbol_id_by_name;
    std::unordered_map<int, std::vector<int>> prod_ids_by_lhs;

    std::string user_subroutines_raw;
};

// 常用特殊符号名字。
inline constexpr const char* kEofSymbolName = "$";
inline constexpr const char* kEpsilonSymbolName = "epsilon";
inline constexpr const char* kAugmentedStartName = "S'";

}  // namespace seu::yacc

