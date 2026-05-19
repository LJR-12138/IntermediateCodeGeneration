#pragma once

#include <string>
#include <vector>

#include "yacc/model/grammar.h"

namespace seu::yacc {

// 第 4 步预处理报告：用于确认文法已满足后续 First/LR 算法输入要求。
struct GrammarPreprocessReport {
    bool passed = false;
    int augmented_production_id = -1;

    std::vector<int> nonterminals_without_productions;
    std::vector<int> productions_with_epsilon_symbol_in_rhs;

    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

// 第 4 步核心接口：
// 1) 重建 lhs -> productions 索引；
// 2) 校验增广文法结构；
// 3) 补充后续算法需要的结构一致性检查。
GrammarPreprocessReport preprocess_grammar(Grammar& grammar);

}  // namespace seu::yacc
