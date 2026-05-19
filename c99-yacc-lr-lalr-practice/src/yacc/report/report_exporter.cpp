#include "yacc/report/report_exporter.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <queue>
#include <stdexcept>
#include <unordered_set>

namespace seu::yacc {
namespace {

std::string symbol_kind_to_text(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Terminal:
            return "Terminal";
        case SymbolKind::Nonterminal:
            return "Nonterminal";
        case SymbolKind::Special:
            return "Special";
    }
    return "Unknown";
}

void write_text_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("无法写入文件: " + path.string());
    }
    out << content;
}

}  // namespace

GrammarAnalysis analyze_grammar(const Grammar& grammar) {
    GrammarAnalysis result;

    std::unordered_set<int> rhs_used_terminal_ids;
    for (const auto& prod : grammar.productions) {
        if (prod.action.present) {
            ++result.productions_with_actions;
        }
        for (int rhs_id : prod.rhs_symbol_ids) {
            const auto& sym = grammar.symbols[rhs_id];
            if (sym.kind == SymbolKind::Terminal) {
                rhs_used_terminal_ids.insert(rhs_id);
            }
        }
    }

    for (int tid : grammar.terminal_ids) {
        if (rhs_used_terminal_ids.find(tid) == rhs_used_terminal_ids.end()) {
            result.unused_terminal_ids.push_back(tid);
        }
    }

    // 可达性分析：从开始符号出发，沿非终结符展开产生式 RHS 中的非终结符。
    std::unordered_set<int> reachable;
    std::queue<int> q;
    if (grammar.start_symbol_id >= 0) {
        reachable.insert(grammar.start_symbol_id);
        q.push(grammar.start_symbol_id);
    }
    while (!q.empty()) {
        const int cur = q.front();
        q.pop();

        auto it = grammar.prod_ids_by_lhs.find(cur);
        if (it == grammar.prod_ids_by_lhs.end()) {
            continue;
        }
        for (int pid : it->second) {
            const auto& prod = grammar.productions[pid];
            for (int rhs_id : prod.rhs_symbol_ids) {
                const auto& sym = grammar.symbols[rhs_id];
                if (sym.kind == SymbolKind::Nonterminal && reachable.insert(rhs_id).second) {
                    q.push(rhs_id);
                }
            }
        }
    }
    for (int nid : grammar.nonterminal_ids) {
        if (reachable.find(nid) == reachable.end()) {
            result.unreachable_nonterminal_ids.push_back(nid);
        }
    }

    return result;
}

std::string make_default_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step3" / stem).string();
}

std::string make_default_step4_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step4" / stem).string();
}

std::string make_default_step5_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step5" / stem).string();
}

std::string make_default_step6_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step6" / stem).string();
}

std::string make_default_step7_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step7" / stem).string();
}

std::string make_default_step8_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step8" / stem).string();
}

std::string make_default_step9_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step9" / stem).string();
}

std::string make_default_step10_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step10" / stem).string();
}

void export_report(
    const Grammar& grammar, const GrammarAnalysis& analysis, const std::string& output_dir) {
    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";

    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    // 1) summary.txt
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "symbols=" + std::to_string(grammar.symbols.size()) + "\n";
    summary += "terminals=" + std::to_string(grammar.terminal_ids.size()) + "\n";
    summary += "nonterminals=" + std::to_string(grammar.nonterminal_ids.size()) + "\n";
    summary += "productions_with_augmented=" + std::to_string(grammar.productions.size()) + "\n";
    if (grammar.start_symbol_id >= 0 &&
        grammar.start_symbol_id < static_cast<int>(grammar.symbols.size())) {
        summary += "start_symbol=" + grammar.symbols[grammar.start_symbol_id].name + "\n";
    }
    write_text_file(root / "summary.txt", summary);

    // 2) raw/symbols.tsv
    std::string symbols_tsv = "id\tname\tkind\tis_literal_char\n";
    for (const auto& sym : grammar.symbols) {
        symbols_tsv += std::to_string(sym.id) + "\t" + sym.name + "\t" +
                       symbol_kind_to_text(sym.kind) + "\t" +
                       (sym.is_literal_char ? "1" : "0") + "\n";
    }
    write_text_file(raw_dir / "symbols.tsv", symbols_tsv);

    // 3) raw/productions.txt
    std::string productions_text;
    for (const auto& p : grammar.productions) {
        productions_text += "#" + std::to_string(p.id) + " ";
        productions_text += grammar.symbols[p.lhs_symbol_id].name + " ->";
        if (p.rhs_symbol_ids.empty()) {
            productions_text += " epsilon";
        } else {
            for (int rhs_id : p.rhs_symbol_ids) {
                productions_text += " " + grammar.symbols[rhs_id].name;
            }
        }
        productions_text += "    [line=" + std::to_string(p.source_line) + "]";
        if (p.action.present) {
            productions_text += " [action=yes]";
        }
        productions_text += "\n";
    }
    write_text_file(raw_dir / "productions.txt", productions_text);

    // 4) raw/user_subroutines.c
    write_text_file(raw_dir / "user_subroutines.c", grammar.user_subroutines_raw);

    // 5) analysis/report.txt
    std::string report;
    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    if (!analysis.unused_terminal_ids.empty()) {
        report += "unused_terminals:\n";
        for (int id : analysis.unused_terminal_ids) {
            report += "- " + grammar.symbols[id].name + " (id=" + std::to_string(id) + ")\n";
        }
    }
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";
    if (!analysis.unreachable_nonterminal_ids.empty()) {
        report += "unreachable_nonterminals:\n";
        for (int id : analysis.unreachable_nonterminal_ids) {
            report += "- " + grammar.symbols[id].name + " (id=" + std::to_string(id) + ")\n";
        }
    }
    write_text_file(analysis_dir / "report.txt", report);
}

void export_step4_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const std::string& output_dir) {
    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";

    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    // 1) summary.txt
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "step=4\n";
    summary += "symbols=" + std::to_string(grammar.symbols.size()) + "\n";
    summary += "terminals=" + std::to_string(grammar.terminal_ids.size()) + "\n";
    summary += "nonterminals=" + std::to_string(grammar.nonterminal_ids.size()) + "\n";
    summary += "productions_with_augmented=" + std::to_string(grammar.productions.size()) + "\n";
    summary += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    if (grammar.start_symbol_id >= 0 &&
        grammar.start_symbol_id < static_cast<int>(grammar.symbols.size())) {
        summary += "start_symbol=" + grammar.symbols[grammar.start_symbol_id].name + "\n";
    }
    if (grammar.augmented_start_symbol_id >= 0 &&
        grammar.augmented_start_symbol_id < static_cast<int>(grammar.symbols.size())) {
        summary += "augmented_start_symbol=" + grammar.symbols[grammar.augmented_start_symbol_id].name + "\n";
    }
    summary += "augmented_production_id=" + std::to_string(preprocess_report.augmented_production_id) + "\n";
    write_text_file(root / "summary.txt", summary);

    // 2) raw/symbols.tsv
    std::string symbols_tsv = "id\tname\tkind\tis_literal_char\n";
    for (const auto& sym : grammar.symbols) {
        symbols_tsv += std::to_string(sym.id) + "\t" + sym.name + "\t" +
                       symbol_kind_to_text(sym.kind) + "\t" +
                       (sym.is_literal_char ? "1" : "0") + "\n";
    }
    write_text_file(raw_dir / "symbols.tsv", symbols_tsv);

    // 3) raw/productions.txt
    std::string productions_text;
    for (const auto& p : grammar.productions) {
        productions_text += "#" + std::to_string(p.id) + " ";
        productions_text += grammar.symbols[p.lhs_symbol_id].name + " ->";
        if (p.rhs_symbol_ids.empty()) {
            productions_text += " epsilon";
        } else {
            for (int rhs_id : p.rhs_symbol_ids) {
                productions_text += " " + grammar.symbols[rhs_id].name;
            }
        }
        productions_text += "    [line=" + std::to_string(p.source_line) + "]";
        if (p.action.present) {
            productions_text += " [action=yes]";
        }
        productions_text += "\n";
    }
    write_text_file(raw_dir / "productions.txt", productions_text);

    // 4) raw/augmented_grammar.txt
    std::string aug;
    if (!grammar.productions.empty()) {
        const auto& p0 = grammar.productions.front();
        aug += "augmented_production=#" + std::to_string(p0.id) + "\n";
        aug += grammar.symbols[p0.lhs_symbol_id].name + " ->";
        for (int rhs_id : p0.rhs_symbol_ids) {
            aug += " " + grammar.symbols[rhs_id].name;
        }
        aug += "\n";
    }
    write_text_file(raw_dir / "augmented_grammar.txt", aug);

    // 5) raw/prod_index_by_lhs.tsv
    std::string index_tsv = "lhs_id\tlhs_name\tproduction_ids\n";
    for (int lhs_id : grammar.nonterminal_ids) {
        index_tsv += std::to_string(lhs_id) + "\t" + grammar.symbols[lhs_id].name + "\t";
        auto it = grammar.prod_ids_by_lhs.find(lhs_id);
        if (it != grammar.prod_ids_by_lhs.end()) {
            for (size_t i = 0; i < it->second.size(); ++i) {
                index_tsv += std::to_string(it->second[i]);
                if (i + 1 < it->second.size()) {
                    index_tsv += ",";
                }
            }
        }
        index_tsv += "\n";
    }
    write_text_file(raw_dir / "prod_index_by_lhs.tsv", index_tsv);

    // 6) raw/user_subroutines.c
    write_text_file(raw_dir / "user_subroutines.c", grammar.user_subroutines_raw);

    // 7) analysis/report.txt
    std::string report;
    report += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    report += "preprocess_errors_count=" + std::to_string(preprocess_report.errors.size()) + "\n";
    if (!preprocess_report.errors.empty()) {
        report += "preprocess_errors:\n";
        for (const auto& e : preprocess_report.errors) {
            report += "- " + e + "\n";
        }
    }
    report += "preprocess_warnings_count=" + std::to_string(preprocess_report.warnings.size()) + "\n";
    if (!preprocess_report.warnings.empty()) {
        report += "preprocess_warnings:\n";
        for (const auto& w : preprocess_report.warnings) {
            report += "- " + w + "\n";
        }
    }
    report += "nonterminals_without_productions_count=" +
              std::to_string(preprocess_report.nonterminals_without_productions.size()) + "\n";
    report += "productions_with_epsilon_symbol_in_rhs_count=" +
              std::to_string(preprocess_report.productions_with_epsilon_symbol_in_rhs.size()) + "\n";

    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";

    write_text_file(analysis_dir / "report.txt", report);
}

void export_step5_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const std::string& output_dir) {
    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";

    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    // 1) summary.txt
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "step=5\n";
    summary += "symbols=" + std::to_string(grammar.symbols.size()) + "\n";
    summary += "terminals=" + std::to_string(grammar.terminal_ids.size()) + "\n";
    summary += "nonterminals=" + std::to_string(grammar.nonterminal_ids.size()) + "\n";
    summary += "productions_with_augmented=" + std::to_string(grammar.productions.size()) + "\n";
    summary += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    summary += "first_converged=" + std::string(first_result.converged ? "true" : "false") + "\n";
    summary += "first_iterations=" + std::to_string(first_result.iteration_count) + "\n";
    summary += "nullable_nonterminals_count=" +
               std::to_string(first_result.nullable_nonterminal_ids.size()) + "\n";
    summary += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    write_text_file(root / "summary.txt", summary);

    // 2) raw/symbols.tsv
    std::string symbols_tsv = "id\tname\tkind\tis_literal_char\n";
    for (const auto& sym : grammar.symbols) {
        symbols_tsv += std::to_string(sym.id) + "\t" + sym.name + "\t" +
                       symbol_kind_to_text(sym.kind) + "\t" +
                       (sym.is_literal_char ? "1" : "0") + "\n";
    }
    write_text_file(raw_dir / "symbols.tsv", symbols_tsv);

    // 3) raw/productions.txt
    std::string productions_text;
    for (const auto& p : grammar.productions) {
        productions_text += "#" + std::to_string(p.id) + " ";
        productions_text += grammar.symbols[p.lhs_symbol_id].name + " ->";
        if (p.rhs_symbol_ids.empty()) {
            productions_text += " epsilon";
        } else {
            for (int rhs_id : p.rhs_symbol_ids) {
                productions_text += " " + grammar.symbols[rhs_id].name;
            }
        }
        productions_text += "    [line=" + std::to_string(p.source_line) + "]";
        if (p.action.present) {
            productions_text += " [action=yes]";
        }
        productions_text += "\n";
    }
    write_text_file(raw_dir / "productions.txt", productions_text);

    // 4) raw/augmented_grammar.txt
    std::string aug;
    if (!grammar.productions.empty()) {
        const auto& p0 = grammar.productions.front();
        aug += "augmented_production=#" + std::to_string(p0.id) + "\n";
        aug += grammar.symbols[p0.lhs_symbol_id].name + " ->";
        for (int rhs_id : p0.rhs_symbol_ids) {
            aug += " " + grammar.symbols[rhs_id].name;
        }
        aug += "\n";
    }
    write_text_file(raw_dir / "augmented_grammar.txt", aug);

    // 5) raw/prod_index_by_lhs.tsv
    std::string index_tsv = "lhs_id\tlhs_name\tproduction_ids\n";
    for (int lhs_id : grammar.nonterminal_ids) {
        index_tsv += std::to_string(lhs_id) + "\t" + grammar.symbols[lhs_id].name + "\t";
        auto it = grammar.prod_ids_by_lhs.find(lhs_id);
        if (it != grammar.prod_ids_by_lhs.end()) {
            for (size_t i = 0; i < it->second.size(); ++i) {
                index_tsv += std::to_string(it->second[i]);
                if (i + 1 < it->second.size()) {
                    index_tsv += ",";
                }
            }
        }
        index_tsv += "\n";
    }
    write_text_file(raw_dir / "prod_index_by_lhs.tsv", index_tsv);

    // 6) raw/first_sets.tsv
    std::string first_tsv = "symbol_id\tsymbol_name\tkind\tfirst_set\n";
    for (const auto& sym : grammar.symbols) {
        first_tsv += std::to_string(sym.id) + "\t" + sym.name + "\t" + symbol_kind_to_text(sym.kind) + "\t";
        if (sym.id >= 0 && sym.id < static_cast<int>(first_result.first_sets_by_symbol_id.size())) {
            const auto& first_set = first_result.first_sets_by_symbol_id[sym.id];
            bool first_item = true;
            for (int member_id : first_set) {
                if (!first_item) {
                    first_tsv += ",";
                }
                first_tsv += grammar.symbols[member_id].name;
                first_item = false;
            }
        }
        first_tsv += "\n";
    }
    write_text_file(raw_dir / "first_sets.tsv", first_tsv);

    // 7) raw/first_sequence_examples.txt
    std::ostringstream sequence_examples;
    sequence_examples << "examples_for_first_of_sequence\n";
    if (!grammar.productions.empty()) {
        const auto& p0 = grammar.productions.front();
        sequence_examples << "prod#0_rhs:";
        for (int sid : p0.rhs_symbol_ids) {
            sequence_examples << " " << grammar.symbols[sid].name;
        }
        sequence_examples << "\nFIRST(prod#0_rhs)={";
        const std::set<int> first_p0_rhs = compute_first_of_sequence(grammar, first_result, p0.rhs_symbol_ids, 0);
        bool first_item = true;
        for (int sid : first_p0_rhs) {
            if (!first_item) {
                sequence_examples << ", ";
            }
            sequence_examples << grammar.symbols[sid].name;
            first_item = false;
        }
        sequence_examples << "}\n";
    }
    write_text_file(raw_dir / "first_sequence_examples.txt", sequence_examples.str());

    // 8) raw/user_subroutines.c
    write_text_file(raw_dir / "user_subroutines.c", grammar.user_subroutines_raw);

    // 9) analysis/report.txt
    std::string report;
    report += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    report += "preprocess_errors_count=" + std::to_string(preprocess_report.errors.size()) + "\n";
    report += "preprocess_warnings_count=" + std::to_string(preprocess_report.warnings.size()) + "\n";
    report += "first_converged=" + std::string(first_result.converged ? "true" : "false") + "\n";
    report += "first_iterations=" + std::to_string(first_result.iteration_count) + "\n";
    report += "nullable_nonterminals_count=" +
              std::to_string(first_result.nullable_nonterminal_ids.size()) + "\n";
    if (!first_result.nullable_nonterminal_ids.empty()) {
        report += "nullable_nonterminals:\n";
        for (int sid : first_result.nullable_nonterminal_ids) {
            report += "- " + grammar.symbols[sid].name + " (id=" + std::to_string(sid) + ")\n";
        }
    }
    report += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    report += "first_validation_errors_count=" + std::to_string(first_validation.errors.size()) + "\n";
    if (!first_validation.errors.empty()) {
        report += "first_validation_errors:\n";
        for (const auto& e : first_validation.errors) {
            report += "- " + e + "\n";
        }
    }
    report += "first_validation_warnings_count=" + std::to_string(first_validation.warnings.size()) + "\n";
    if (!first_validation.warnings.empty()) {
        report += "first_validation_warnings:\n";
        for (const auto& w : first_validation.warnings) {
            report += "- " + w + "\n";
        }
    }
    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";

    write_text_file(analysis_dir / "report.txt", report);
}

void export_step6_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_result,
    const LR1Step6ValidationReport& lr1_validation, const std::string& output_dir) {
    // 复用 step5 导出作为基础，再追加第 6 步文件，保证目录结构稳定。
    export_step5_report(
        grammar, analysis, preprocess_report, first_result, first_validation, output_dir);

    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";

    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    // 1) summary.txt：覆盖为 step6 信息。
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "step=6\n";
    summary += "symbols=" + std::to_string(grammar.symbols.size()) + "\n";
    summary += "terminals=" + std::to_string(grammar.terminal_ids.size()) + "\n";
    summary += "nonterminals=" + std::to_string(grammar.nonterminal_ids.size()) + "\n";
    summary += "productions_with_augmented=" + std::to_string(grammar.productions.size()) + "\n";
    summary += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    summary += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    summary += "i0_kernel_items=" + std::to_string(lr1_result.i0_kernel_items.size()) + "\n";
    summary += "i0_closure_items=" + std::to_string(lr1_result.i0_closure_items.size()) + "\n";
    summary += "i0_goto_edges=" + std::to_string(lr1_result.goto_symbol_ids.size()) + "\n";
    summary += "lr1_step6_validation_passed=" + std::string(lr1_validation.passed ? "true" : "false") + "\n";
    write_text_file(root / "summary.txt", summary);

    // 2) raw/lr1_i0_items.txt
    std::ostringstream i0_items;
    i0_items << "[I0 kernel]\n";
    for (const auto& item : lr1_result.i0_kernel_items) {
        i0_items << format_lr1_item(grammar, item) << "\n";
    }
    i0_items << "\n[I0 closure]\n";
    for (const auto& item : lr1_result.i0_closure_items) {
        i0_items << format_lr1_item(grammar, item) << "\n";
    }
    write_text_file(raw_dir / "lr1_i0_items.txt", i0_items.str());

    // 3) raw/lr1_i0_goto.tsv
    std::string goto_tsv = "symbol_id\tsymbol_name\titem_count\n";
    for (std::size_t i = 0; i < lr1_result.goto_symbol_ids.size(); ++i) {
        const int sid = lr1_result.goto_symbol_ids[i];
        const std::string name =
            (sid >= 0 && sid < static_cast<int>(grammar.symbols.size())) ? grammar.symbols[sid].name
                                                                          : "<invalid>";
        goto_tsv += std::to_string(sid) + "\t" + name + "\t" +
                    std::to_string(lr1_result.goto_item_sets[i].size()) + "\n";
    }
    write_text_file(raw_dir / "lr1_i0_goto.tsv", goto_tsv);

    // 4) raw/lr1_i0_goto_items.txt
    std::ostringstream goto_items_text;
    for (std::size_t i = 0; i < lr1_result.goto_symbol_ids.size(); ++i) {
        const int sid = lr1_result.goto_symbol_ids[i];
        const std::string name =
            (sid >= 0 && sid < static_cast<int>(grammar.symbols.size())) ? grammar.symbols[sid].name
                                                                          : "<invalid>";
        goto_items_text << "[goto(I0, " << name << ") items]\n";
        for (const auto& item : lr1_result.goto_item_sets[i]) {
            goto_items_text << format_lr1_item(grammar, item) << "\n";
        }
        goto_items_text << "\n";
    }
    write_text_file(raw_dir / "lr1_i0_goto_items.txt", goto_items_text.str());

    // 5) raw/lr1_lookahead_derivation.txt
    std::ostringstream lookahead_notes;
    lookahead_notes << "lookahead_derivation_notes_count="
                    << lr1_result.lookahead_derivation_notes.size() << "\n";
    for (const auto& line : lr1_result.lookahead_derivation_notes) {
        lookahead_notes << "- " << line << "\n";
    }
    write_text_file(raw_dir / "lr1_lookahead_derivation.txt", lookahead_notes.str());

    // 6) analysis/report.txt：覆盖为 step6 诊断结果。
    std::string report;
    report += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    report += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    report += "lr1_i0_kernel_items=" + std::to_string(lr1_result.i0_kernel_items.size()) + "\n";
    report += "lr1_i0_closure_items=" + std::to_string(lr1_result.i0_closure_items.size()) + "\n";
    report += "lr1_i0_goto_edges=" + std::to_string(lr1_result.goto_symbol_ids.size()) + "\n";
    report += "lr1_lookahead_derivation_notes_count=" +
              std::to_string(lr1_result.lookahead_derivation_notes.size()) + "\n";
    report += "lr1_step6_validation_passed=" + std::string(lr1_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step6_errors_count=" + std::to_string(lr1_validation.errors.size()) + "\n";
    if (!lr1_validation.errors.empty()) {
        report += "lr1_step6_errors:\n";
        for (const auto& e : lr1_validation.errors) {
            report += "- " + e + "\n";
        }
    }
    report += "lr1_step6_warnings_count=" + std::to_string(lr1_validation.warnings.size()) + "\n";
    if (!lr1_validation.warnings.empty()) {
        report += "lr1_step6_warnings:\n";
        for (const auto& w : lr1_validation.warnings) {
            report += "- " + w + "\n";
        }
    }
    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";

    write_text_file(analysis_dir / "report.txt", report);
}

void export_step7_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const std::string& output_dir) {
    // 复用 step6 导出作为基础，再追加 step7 产物。
    export_step6_report(grammar, analysis, preprocess_report, first_result, first_validation, lr1_step6_result,
        lr1_step6_validation, output_dir);

    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";

    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    // 1) summary.txt：覆盖为 step7 信息。
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "step=7\n";
    summary += "symbols=" + std::to_string(grammar.symbols.size()) + "\n";
    summary += "terminals=" + std::to_string(grammar.terminal_ids.size()) + "\n";
    summary += "nonterminals=" + std::to_string(grammar.nonterminal_ids.size()) + "\n";
    summary += "productions_with_augmented=" + std::to_string(grammar.productions.size()) + "\n";
    summary += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    summary += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    summary += "lr1_step6_validation_passed=" + std::string(lr1_step6_validation.passed ? "true" : "false") + "\n";
    summary += "lr1_states=" + std::to_string(lr1_step7_result.states.size()) + "\n";
    summary += "lr1_transitions=" + std::to_string(lr1_step7_result.transitions.size()) + "\n";
    summary += "lr1_step7_validation_passed=" + std::string(lr1_step7_validation.passed ? "true" : "false") + "\n";
    write_text_file(root / "summary.txt", summary);

    // 2) raw/lr1_states.tsv
    std::string states_tsv = "state_id\titem_count\n";
    for (const auto& state : lr1_step7_result.states) {
        states_tsv += std::to_string(state.state_id) + "\t" + std::to_string(state.items.size()) + "\n";
    }
    write_text_file(raw_dir / "lr1_states.tsv", states_tsv);

    // 3) raw/lr1_state_items.txt
    std::ostringstream state_items_text;
    for (const auto& state : lr1_step7_result.states) {
        state_items_text << "[state " << state.state_id << "]\n";
        for (const auto& item : state.items) {
            state_items_text << format_lr1_item(grammar, item) << "\n";
        }
        state_items_text << "\n";
    }
    write_text_file(raw_dir / "lr1_state_items.txt", state_items_text.str());

    // 4) raw/lr1_transitions.tsv
    std::string transitions_tsv = "from_state\tsymbol_id\tsymbol_name\tto_state\n";
    for (const auto& edge : lr1_step7_result.transitions) {
        const std::string symbol_name =
            (edge.symbol_id >= 0 && edge.symbol_id < static_cast<int>(grammar.symbols.size()))
                ? grammar.symbols[edge.symbol_id].name
                : "<invalid>";
        transitions_tsv += std::to_string(edge.from_state_id) + "\t" + std::to_string(edge.symbol_id) + "\t" +
                           symbol_name + "\t" + std::to_string(edge.to_state_id) + "\n";
    }
    write_text_file(raw_dir / "lr1_transitions.tsv", transitions_tsv);

    // 5) raw/lr1_predecessors.tsv：便于追踪状态来源。
    std::vector<std::vector<std::string>> predecessor_rows(lr1_step7_result.states.size());
    for (const auto& edge : lr1_step7_result.transitions) {
        if (edge.to_state_id < 0 || edge.to_state_id >= static_cast<int>(predecessor_rows.size())) {
            continue;
        }
        const std::string symbol_name =
            (edge.symbol_id >= 0 && edge.symbol_id < static_cast<int>(grammar.symbols.size()))
                ? grammar.symbols[edge.symbol_id].name
                : "<invalid>";
        predecessor_rows[edge.to_state_id].push_back(
            std::to_string(edge.from_state_id) + "--" + symbol_name + "-->" + std::to_string(edge.to_state_id));
    }
    std::string predecessor_tsv = "state_id\tpredecessors\n";
    for (std::size_t state_id = 0; state_id < predecessor_rows.size(); ++state_id) {
        predecessor_tsv += std::to_string(state_id) + "\t";
        for (std::size_t i = 0; i < predecessor_rows[state_id].size(); ++i) {
            predecessor_tsv += predecessor_rows[state_id][i];
            if (i + 1 < predecessor_rows[state_id].size()) {
                predecessor_tsv += ",";
            }
        }
        predecessor_tsv += "\n";
    }
    write_text_file(raw_dir / "lr1_predecessors.tsv", predecessor_tsv);

    // 6) analysis/report.txt：覆盖为 step7 诊断结果。
    std::string report;
    report += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    report += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step6_validation_passed=" + std::string(lr1_step6_validation.passed ? "true" : "false") + "\n";
    report += "lr1_states=" + std::to_string(lr1_step7_result.states.size()) + "\n";
    report += "lr1_transitions=" + std::to_string(lr1_step7_result.transitions.size()) + "\n";
    report += "lr1_step7_validation_passed=" + std::string(lr1_step7_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step7_errors_count=" + std::to_string(lr1_step7_validation.errors.size()) + "\n";
    if (!lr1_step7_validation.errors.empty()) {
        report += "lr1_step7_errors:\n";
        for (const auto& e : lr1_step7_validation.errors) {
            report += "- " + e + "\n";
        }
    }
    report += "lr1_step7_warnings_count=" + std::to_string(lr1_step7_validation.warnings.size()) + "\n";
    if (!lr1_step7_validation.warnings.empty()) {
        report += "lr1_step7_warnings:\n";
        for (const auto& w : lr1_step7_validation.warnings) {
            report += "- " + w + "\n";
        }
    }
    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";

    write_text_file(analysis_dir / "report.txt", report);
}

void export_step8_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const LR1Step8Result& lr1_step8_result,
    const LR1Step8ValidationReport& lr1_step8_validation, const std::string& output_dir) {
    // 复用 step7 导出作为基础，再追加 step8 产物。
    export_step7_report(grammar, analysis, preprocess_report, first_result, first_validation, lr1_step6_result,
        lr1_step6_validation, lr1_step7_result, lr1_step7_validation, output_dir);

    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";
    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    std::size_t action_entries = 0;
    for (const auto& row : lr1_step8_result.action_table) {
        action_entries += row.size();
    }
    std::size_t goto_entries = 0;
    for (const auto& row : lr1_step8_result.goto_table) {
        goto_entries += row.size();
    }

    // 1) summary.txt：覆盖为 step8 信息。
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "step=8\n";
    summary += "symbols=" + std::to_string(grammar.symbols.size()) + "\n";
    summary += "terminals=" + std::to_string(grammar.terminal_ids.size()) + "\n";
    summary += "nonterminals=" + std::to_string(grammar.nonterminal_ids.size()) + "\n";
    summary += "productions_with_augmented=" + std::to_string(grammar.productions.size()) + "\n";
    summary += "lr1_states=" + std::to_string(lr1_step7_result.states.size()) + "\n";
    summary += "lr1_transitions=" + std::to_string(lr1_step7_result.transitions.size()) + "\n";
    summary += "action_entries=" + std::to_string(action_entries) + "\n";
    summary += "goto_entries=" + std::to_string(goto_entries) + "\n";
    summary += "parse_table_conflicts=" + std::to_string(lr1_step8_result.conflicts.size()) + "\n";
    summary += "parse_table_conflict_resolutions=" +
               std::to_string(lr1_step8_result.conflict_resolution_logs.size()) + "\n";
    summary += "lr1_step8_validation_passed=" + std::string(lr1_step8_validation.passed ? "true" : "false") + "\n";
    write_text_file(root / "summary.txt", summary);

    // 2) raw/action_table.tsv
    std::string action_tsv = "state_id\tterminal_id\tterminal_name\taction\ttarget\n";
    for (std::size_t state_id = 0; state_id < lr1_step8_result.action_table.size(); ++state_id) {
        std::vector<std::pair<int, ParseActionEntry>> entries;
        entries.reserve(lr1_step8_result.action_table[state_id].size());
        for (const auto& kv : lr1_step8_result.action_table[state_id]) {
            entries.push_back(kv);
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        for (const auto& kv : entries) {
            const int symbol_id = kv.first;
            const auto& action = kv.second;
            const std::string symbol_name =
                (symbol_id >= 0 && symbol_id < static_cast<int>(grammar.symbols.size()))
                    ? grammar.symbols[symbol_id].name
                    : "<invalid>";
            std::string target = "-";
            if (action.type == ParseActionType::Shift) {
                target = std::to_string(action.target_state_id);
            } else if (action.type == ParseActionType::Reduce) {
                target = std::to_string(action.reduce_production_id);
            }
            action_tsv += std::to_string(state_id) + "\t" + std::to_string(symbol_id) + "\t" + symbol_name + "\t" +
                          format_parse_action_entry(action) + "\t" + target + "\n";
        }
    }
    write_text_file(raw_dir / "action_table.tsv", action_tsv);

    // 3) raw/goto_table.tsv
    std::string goto_tsv = "state_id\tnonterminal_id\tnonterminal_name\tto_state\n";
    for (std::size_t state_id = 0; state_id < lr1_step8_result.goto_table.size(); ++state_id) {
        std::vector<std::pair<int, int>> entries;
        entries.reserve(lr1_step8_result.goto_table[state_id].size());
        for (const auto& kv : lr1_step8_result.goto_table[state_id]) {
            entries.push_back(kv);
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        for (const auto& kv : entries) {
            const int symbol_id = kv.first;
            const int to_state = kv.second;
            const std::string symbol_name =
                (symbol_id >= 0 && symbol_id < static_cast<int>(grammar.symbols.size()))
                    ? grammar.symbols[symbol_id].name
                    : "<invalid>";
            goto_tsv += std::to_string(state_id) + "\t" + std::to_string(symbol_id) + "\t" + symbol_name + "\t" +
                        std::to_string(to_state) + "\n";
        }
    }
    write_text_file(raw_dir / "goto_table.tsv", goto_tsv);

    // 4) raw/parse_table_conflicts.tsv
    std::string conflict_tsv =
        "state_id\tsymbol_id\tsymbol_name\tconflict_type\texisting_action\tincoming_action\trelated_items\n";
    for (const auto& c : lr1_step8_result.conflicts) {
        const std::string symbol_name =
            (c.symbol_id >= 0 && c.symbol_id < static_cast<int>(grammar.symbols.size()))
                ? grammar.symbols[c.symbol_id].name
                : "<invalid>";
        std::ostringstream related_items_text;
        for (std::size_t i = 0; i < c.related_items.size(); ++i) {
            related_items_text << format_lr1_item(grammar, c.related_items[i]);
            if (i + 1 < c.related_items.size()) {
                related_items_text << " || ";
            }
        }
        conflict_tsv += std::to_string(c.state_id) + "\t" + std::to_string(c.symbol_id) + "\t" + symbol_name + "\t" +
                        c.conflict_type + "\t" + c.existing_action + "\t" + c.incoming_action + "\t" +
                        related_items_text.str() + "\n";
    }
    write_text_file(raw_dir / "parse_table_conflicts.tsv", conflict_tsv);

    // 5) raw/parse_table_conflict_resolution.tsv
    std::string resolution_tsv =
        "state_id\tsymbol_id\tsymbol_name\tconflict_type\texisting_action\tincoming_action\tresolved_action\treason\n";
    for (const auto& log : lr1_step8_result.conflict_resolution_logs) {
        const std::string symbol_name =
            (log.symbol_id >= 0 && log.symbol_id < static_cast<int>(grammar.symbols.size()))
                ? grammar.symbols[log.symbol_id].name
                : "<invalid>";
        resolution_tsv += std::to_string(log.state_id) + "\t" + std::to_string(log.symbol_id) + "\t" + symbol_name +
                          "\t" + log.conflict_type + "\t" + log.existing_action + "\t" + log.incoming_action + "\t" +
                          log.resolved_action + "\t" + log.reason + "\n";
    }
    write_text_file(raw_dir / "parse_table_conflict_resolution.tsv", resolution_tsv);

    // 6) analysis/report.txt：覆盖为 step8 诊断结果。
    std::string report;
    report += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    report += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step6_validation_passed=" + std::string(lr1_step6_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step7_validation_passed=" + std::string(lr1_step7_validation.passed ? "true" : "false") + "\n";
    report += "action_entries=" + std::to_string(action_entries) + "\n";
    report += "goto_entries=" + std::to_string(goto_entries) + "\n";
    report += "parse_table_conflicts=" + std::to_string(lr1_step8_result.conflicts.size()) + "\n";
    report += "parse_table_conflict_resolutions=" +
              std::to_string(lr1_step8_result.conflict_resolution_logs.size()) + "\n";
    report += "lr1_step8_validation_passed=" + std::string(lr1_step8_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step8_errors_count=" + std::to_string(lr1_step8_validation.errors.size()) + "\n";
    if (!lr1_step8_validation.errors.empty()) {
        report += "lr1_step8_errors:\n";
        for (const auto& e : lr1_step8_validation.errors) {
            report += "- " + e + "\n";
        }
    }
    report += "lr1_step8_warnings_count=" + std::to_string(lr1_step8_validation.warnings.size()) + "\n";
    if (!lr1_step8_validation.warnings.empty()) {
        report += "lr1_step8_warnings:\n";
        for (const auto& w : lr1_step8_validation.warnings) {
            report += "- " + w + "\n";
        }
    }
    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";

    write_text_file(analysis_dir / "report.txt", report);
}

void export_step9_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const LR1Step8Result& lr1_step8_result,
    const LR1Step8ValidationReport& lr1_step8_validation, const LRParseRunResult& lr1_parse_result,
    const LRParseRunResult& lalr_parse_result, const std::vector<RuntimeToken>& input_tokens,
    const std::string& token_source,
    const std::string& output_dir) {
    // 复用 step8 导出作为基础，再追加 step9 产物。
    export_step8_report(grammar, analysis, preprocess_report, first_result, first_validation, lr1_step6_result,
        lr1_step6_validation, lr1_step7_result, lr1_step7_validation, lr1_step8_result, lr1_step8_validation,
        output_dir);

    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";
    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    // 1) summary.txt：覆盖为 step9 信息。
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "step=9\n";
    summary += "token_source=" + token_source + "\n";
    summary += "input_tokens=" + std::to_string(input_tokens.size()) + "\n";
    summary += "lr1_parse_accepted=" + std::string(lr1_parse_result.accepted ? "true" : "false") + "\n";
    summary += "lr1_parse_total_steps=" + std::to_string(lr1_parse_result.total_steps) + "\n";
    summary += "lr1_parse_consumed_tokens=" + std::to_string(lr1_parse_result.consumed_tokens) + "\n";
    summary += "lr1_parse_reductions=" + std::to_string(lr1_parse_result.reduction_production_ids.size()) + "\n";
    summary += "lr1_parse_trace_rows=" + std::to_string(lr1_parse_result.trace_rows.size()) + "\n";
    summary += "lalr_parse_accepted=" + std::string(lalr_parse_result.accepted ? "true" : "false") + "\n";
    summary += "lalr_parse_total_steps=" + std::to_string(lalr_parse_result.total_steps) + "\n";
    summary += "lalr_parse_consumed_tokens=" + std::to_string(lalr_parse_result.consumed_tokens) + "\n";
    summary += "lalr_parse_reductions=" + std::to_string(lalr_parse_result.reduction_production_ids.size()) + "\n";
    summary += "lalr_parse_trace_rows=" + std::to_string(lalr_parse_result.trace_rows.size()) + "\n";
    write_text_file(root / "summary.txt", summary);

    // 2) raw/parse_input_tokens.tsv
    std::string token_tsv = "index\tsymbol_id\tsymbol_name\tlexeme\tline\tcolumn\n";
    for (std::size_t i = 0; i < input_tokens.size(); ++i) {
        const auto& tk = input_tokens[i];
        token_tsv += std::to_string(i) + "\t" + std::to_string(tk.symbol_id) + "\t" + tk.symbol_name + "\t" +
                     tk.lexeme + "\t" + std::to_string(tk.line) + "\t" + std::to_string(tk.column) + "\n";
    }
    write_text_file(raw_dir / "parse_input_tokens.tsv", token_tsv);

    // 3) raw/parse_trace.tsv
    std::string trace_tsv =
        "step\tstate\tlookahead_id\tlookahead\taction\tproduction_id\tstate_stack\tsymbol_stack\tinput_index\n";
    for (const auto& row : lr1_parse_result.trace_rows) {
        trace_tsv += std::to_string(row.step_no) + "\t" + std::to_string(row.state_id) + "\t" +
                     std::to_string(row.lookahead_symbol_id) + "\t" + row.lookahead_symbol_name + "\t" +
                     row.action_text + "\t" + std::to_string(row.production_id) + "\t" + row.state_stack_text +
                     "\t" + row.symbol_stack_text + "\t" + std::to_string(row.input_index) + "\n";
    }
    write_text_file(raw_dir / "parse_trace.tsv", trace_tsv);

    // 3b) raw/parse_trace_lalr.tsv
    std::string trace_lalr_tsv =
        "step\tstate\tlookahead_id\tlookahead\taction\tproduction_id\tstate_stack\tsymbol_stack\tinput_index\n";
    for (const auto& row : lalr_parse_result.trace_rows) {
        trace_lalr_tsv += std::to_string(row.step_no) + "\t" + std::to_string(row.state_id) + "\t" +
                          std::to_string(row.lookahead_symbol_id) + "\t" + row.lookahead_symbol_name + "\t" +
                          row.action_text + "\t" + std::to_string(row.production_id) + "\t" + row.state_stack_text +
                          "\t" + row.symbol_stack_text + "\t" + std::to_string(row.input_index) + "\n";
    }
    write_text_file(raw_dir / "parse_trace_lalr.tsv", trace_lalr_tsv);

    // 4) raw/parse_reductions.txt
    std::string reductions_text;
    for (std::size_t i = 0; i < lr1_parse_result.reduction_production_ids.size(); ++i) {
        const int pid = lr1_parse_result.reduction_production_ids[i];
        reductions_text += std::to_string(i + 1) + ". #" + std::to_string(pid) + " ";
        if (pid >= 0 && pid < static_cast<int>(grammar.productions.size())) {
            const auto& p = grammar.productions[pid];
            reductions_text += grammar.symbols[p.lhs_symbol_id].name + " ->";
            if (p.rhs_symbol_ids.empty()) {
                reductions_text += " epsilon";
            } else {
                for (int rhs_id : p.rhs_symbol_ids) {
                    reductions_text += " " + grammar.symbols[rhs_id].name;
                }
            }
        }
        reductions_text += "\n";
    }
    write_text_file(raw_dir / "parse_reductions.txt", reductions_text);

    // 4b) raw/parse_reductions_lalr.txt
    std::string reductions_lalr_text;
    for (std::size_t i = 0; i < lalr_parse_result.reduction_production_ids.size(); ++i) {
        const int pid = lalr_parse_result.reduction_production_ids[i];
        reductions_lalr_text += std::to_string(i + 1) + ". #" + std::to_string(pid) + " ";
        if (pid >= 0 && pid < static_cast<int>(grammar.productions.size())) {
            const auto& p = grammar.productions[pid];
            reductions_lalr_text += grammar.symbols[p.lhs_symbol_id].name + " ->";
            if (p.rhs_symbol_ids.empty()) {
                reductions_lalr_text += " epsilon";
            } else {
                for (int rhs_id : p.rhs_symbol_ids) {
                    reductions_lalr_text += " " + grammar.symbols[rhs_id].name;
                }
            }
        }
        reductions_lalr_text += "\n";
    }
    write_text_file(raw_dir / "parse_reductions_lalr.txt", reductions_lalr_text);

    // 5) raw/parse_error.txt
    std::string error_text;
    if (lr1_parse_result.error.has_error) {
        error_text += "step=" + std::to_string(lr1_parse_result.error.step_no) + "\n";
        error_text += "input_index=" + std::to_string(lr1_parse_result.error.input_index) + "\n";
        error_text += "state=" + std::to_string(lr1_parse_result.error.state_id) + "\n";
        error_text += "lookahead_id=" + std::to_string(lr1_parse_result.error.lookahead_symbol_id) + "\n";
        error_text += "lookahead=" + lr1_parse_result.error.lookahead_symbol_name + "\n";
        error_text += "message=" + lr1_parse_result.error.message + "\n";
        error_text += "expected=";
        for (std::size_t i = 0; i < lr1_parse_result.error.expected_terminal_ids.size(); ++i) {
            const int sid = lr1_parse_result.error.expected_terminal_ids[i];
            if (sid >= 0 && sid < static_cast<int>(grammar.symbols.size())) {
                error_text += grammar.symbols[sid].name;
            } else {
                error_text += "<invalid>";
            }
            if (i + 1 < lr1_parse_result.error.expected_terminal_ids.size()) {
                error_text += ", ";
            }
        }
        error_text += "\n";
    } else {
        error_text = "no_error\n";
    }
    write_text_file(raw_dir / "parse_error.txt", error_text);

    // 5b) raw/parse_error_lalr.txt
    std::string error_lalr_text;
    if (lalr_parse_result.error.has_error) {
        error_lalr_text += "step=" + std::to_string(lalr_parse_result.error.step_no) + "\n";
        error_lalr_text += "input_index=" + std::to_string(lalr_parse_result.error.input_index) + "\n";
        error_lalr_text += "state=" + std::to_string(lalr_parse_result.error.state_id) + "\n";
        error_lalr_text += "lookahead_id=" + std::to_string(lalr_parse_result.error.lookahead_symbol_id) + "\n";
        error_lalr_text += "lookahead=" + lalr_parse_result.error.lookahead_symbol_name + "\n";
        error_lalr_text += "message=" + lalr_parse_result.error.message + "\n";
        error_lalr_text += "expected=";
        for (std::size_t i = 0; i < lalr_parse_result.error.expected_terminal_ids.size(); ++i) {
            const int sid = lalr_parse_result.error.expected_terminal_ids[i];
            if (sid >= 0 && sid < static_cast<int>(grammar.symbols.size())) {
                error_lalr_text += grammar.symbols[sid].name;
            } else {
                error_lalr_text += "<invalid>";
            }
            if (i + 1 < lalr_parse_result.error.expected_terminal_ids.size()) {
                error_lalr_text += ", ";
            }
        }
        error_lalr_text += "\n";
    } else {
        error_lalr_text = "no_error\n";
    }
    write_text_file(raw_dir / "parse_error_lalr.txt", error_lalr_text);

    // 6) analysis/report.txt：覆盖为 step9 诊断结果。
    std::string report;
    report += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    report += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step6_validation_passed=" + std::string(lr1_step6_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step7_validation_passed=" + std::string(lr1_step7_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step8_validation_passed=" + std::string(lr1_step8_validation.passed ? "true" : "false") + "\n";
    report += "lr1_parse_accepted=" + std::string(lr1_parse_result.accepted ? "true" : "false") + "\n";
    report += "lr1_parse_error=" + std::string(lr1_parse_result.error.has_error ? "true" : "false") + "\n";
    report += "lr1_parse_total_steps=" + std::to_string(lr1_parse_result.total_steps) + "\n";
    report += "lr1_parse_reductions=" + std::to_string(lr1_parse_result.reduction_production_ids.size()) + "\n";
    report += "lalr_parse_accepted=" + std::string(lalr_parse_result.accepted ? "true" : "false") + "\n";
    report += "lalr_parse_error=" + std::string(lalr_parse_result.error.has_error ? "true" : "false") + "\n";
    report += "lalr_parse_total_steps=" + std::to_string(lalr_parse_result.total_steps) + "\n";
    report += "lalr_parse_reductions=" + std::to_string(lalr_parse_result.reduction_production_ids.size()) + "\n";
    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";
    if (lr1_parse_result.error.has_error) {
        report += "lr1_parse_error_message=" + lr1_parse_result.error.message + "\n";
    }
    if (lalr_parse_result.error.has_error) {
        report += "lalr_parse_error_message=" + lalr_parse_result.error.message + "\n";
    }

    write_text_file(analysis_dir / "report.txt", report);
}

void export_step10_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const LR1Step8Result& lr1_step8_result,
    const LR1Step8ValidationReport& lr1_step8_validation, const LR1Step10Result& step10_result,
    const LR1Step10ValidationReport& step10_validation, const std::string& output_dir) {
    // 复用 step8 导出作为基础，再追加 step10 产物。
    export_step8_report(grammar, analysis, preprocess_report, first_result, first_validation, lr1_step6_result,
        lr1_step6_validation, lr1_step7_result, lr1_step7_validation, lr1_step8_result, lr1_step8_validation,
        output_dir);

    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";
    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    std::size_t lalr_action_entries = 0;
    for (const auto& row : step10_result.lalr_step8_result.action_table) {
        lalr_action_entries += row.size();
    }
    std::size_t lalr_goto_entries = 0;
    for (const auto& row : step10_result.lalr_step8_result.goto_table) {
        lalr_goto_entries += row.size();
    }

    // 1) summary.txt：覆盖为 step10 信息。
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "step=10\n";
    summary += "lr1_states=" + std::to_string(step10_result.lr1_state_count) + "\n";
    summary += "lalr_states=" + std::to_string(step10_result.lalr_state_count) + "\n";
    summary += "state_merged=" +
               std::to_string(static_cast<long long>(step10_result.lr1_state_count) -
                              static_cast<long long>(step10_result.lalr_state_count)) +
               "\n";
    summary += "lr1_conflicts=" + std::to_string(step10_result.lr1_conflict_count) + "\n";
    summary += "lalr_conflicts=" + std::to_string(step10_result.lalr_conflict_count) + "\n";
    summary += "lalr_action_entries=" + std::to_string(lalr_action_entries) + "\n";
    summary += "lalr_goto_entries=" + std::to_string(lalr_goto_entries) + "\n";
    summary += "lr1_step10_validation_passed=" + std::string(step10_validation.passed ? "true" : "false") + "\n";
    write_text_file(root / "summary.txt", summary);

    // 2) raw/lr1_to_lalr_state_map.tsv
    std::string state_map_tsv = "lr1_state\tlalr_state\n";
    for (std::size_t lr1_state = 0; lr1_state < step10_result.lr1_to_lalr_state_id.size(); ++lr1_state) {
        state_map_tsv += std::to_string(lr1_state) + "\t" +
                         std::to_string(step10_result.lr1_to_lalr_state_id[lr1_state]) + "\n";
    }
    write_text_file(raw_dir / "lr1_to_lalr_state_map.tsv", state_map_tsv);

    // 3) raw/lalr_merge_groups.tsv
    std::string merge_tsv = "lalr_state\tsource_lr1_states\titem_count\n";
    for (const auto& group : step10_result.merge_groups) {
        merge_tsv += std::to_string(group.lalr_state_id) + "\t";
        for (std::size_t i = 0; i < group.source_lr1_state_ids.size(); ++i) {
            merge_tsv += std::to_string(group.source_lr1_state_ids[i]);
            if (i + 1 < group.source_lr1_state_ids.size()) {
                merge_tsv += ",";
            }
        }
        merge_tsv += "\t" + std::to_string(group.merged_items.size()) + "\n";
    }
    write_text_file(raw_dir / "lalr_merge_groups.tsv", merge_tsv);

    // 4) raw/lalr_state_items.txt
    std::ostringstream state_items_text;
    for (const auto& state : step10_result.lalr_step7_result.states) {
        state_items_text << "[state " << state.state_id << "]\n";
        for (const auto& item : state.items) {
            state_items_text << format_lr1_item(grammar, item) << "\n";
        }
        state_items_text << "\n";
    }
    write_text_file(raw_dir / "lalr_state_items.txt", state_items_text.str());

    // 5) raw/lalr_transitions.tsv
    std::string transitions_tsv = "from_state\tsymbol_id\tsymbol_name\tto_state\n";
    for (const auto& edge : step10_result.lalr_step7_result.transitions) {
        const std::string symbol_name =
            (edge.symbol_id >= 0 && edge.symbol_id < static_cast<int>(grammar.symbols.size()))
                ? grammar.symbols[edge.symbol_id].name
                : "<invalid>";
        transitions_tsv += std::to_string(edge.from_state_id) + "\t" + std::to_string(edge.symbol_id) + "\t" +
                           symbol_name + "\t" + std::to_string(edge.to_state_id) + "\n";
    }
    write_text_file(raw_dir / "lalr_transitions.tsv", transitions_tsv);

    // 6) raw/lalr_action_table.tsv
    std::string action_tsv = "state_id\tterminal_id\tterminal_name\taction\ttarget\n";
    for (std::size_t state_id = 0; state_id < step10_result.lalr_step8_result.action_table.size(); ++state_id) {
        std::vector<std::pair<int, ParseActionEntry>> entries;
        entries.reserve(step10_result.lalr_step8_result.action_table[state_id].size());
        for (const auto& kv : step10_result.lalr_step8_result.action_table[state_id]) {
            entries.push_back(kv);
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        for (const auto& kv : entries) {
            const int symbol_id = kv.first;
            const auto& action = kv.second;
            const std::string symbol_name =
                (symbol_id >= 0 && symbol_id < static_cast<int>(grammar.symbols.size()))
                    ? grammar.symbols[symbol_id].name
                    : "<invalid>";
            std::string target = "-";
            if (action.type == ParseActionType::Shift) {
                target = std::to_string(action.target_state_id);
            } else if (action.type == ParseActionType::Reduce) {
                target = std::to_string(action.reduce_production_id);
            }
            action_tsv += std::to_string(state_id) + "\t" + std::to_string(symbol_id) + "\t" + symbol_name + "\t" +
                          format_parse_action_entry(action) + "\t" + target + "\n";
        }
    }
    write_text_file(raw_dir / "lalr_action_table.tsv", action_tsv);

    // 7) raw/lalr_goto_table.tsv
    std::string goto_tsv = "state_id\tnonterminal_id\tnonterminal_name\tto_state\n";
    for (std::size_t state_id = 0; state_id < step10_result.lalr_step8_result.goto_table.size(); ++state_id) {
        std::vector<std::pair<int, int>> entries;
        entries.reserve(step10_result.lalr_step8_result.goto_table[state_id].size());
        for (const auto& kv : step10_result.lalr_step8_result.goto_table[state_id]) {
            entries.push_back(kv);
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        for (const auto& kv : entries) {
            const int symbol_id = kv.first;
            const int to_state = kv.second;
            const std::string symbol_name =
                (symbol_id >= 0 && symbol_id < static_cast<int>(grammar.symbols.size()))
                    ? grammar.symbols[symbol_id].name
                    : "<invalid>";
            goto_tsv += std::to_string(state_id) + "\t" + std::to_string(symbol_id) + "\t" + symbol_name + "\t" +
                        std::to_string(to_state) + "\n";
        }
    }
    write_text_file(raw_dir / "lalr_goto_table.tsv", goto_tsv);

    // 8) raw/lalr_parse_table_conflicts.tsv
    std::string conflict_tsv =
        "state_id\tsymbol_id\tsymbol_name\tconflict_type\texisting_action\tincoming_action\trelated_items\n";
    for (const auto& c : step10_result.lalr_step8_result.conflicts) {
        const std::string symbol_name =
            (c.symbol_id >= 0 && c.symbol_id < static_cast<int>(grammar.symbols.size()))
                ? grammar.symbols[c.symbol_id].name
                : "<invalid>";
        std::ostringstream related_items_text;
        for (std::size_t i = 0; i < c.related_items.size(); ++i) {
            related_items_text << format_lr1_item(grammar, c.related_items[i]);
            if (i + 1 < c.related_items.size()) {
                related_items_text << " || ";
            }
        }
        conflict_tsv += std::to_string(c.state_id) + "\t" + std::to_string(c.symbol_id) + "\t" + symbol_name + "\t" +
                        c.conflict_type + "\t" + c.existing_action + "\t" + c.incoming_action + "\t" +
                        related_items_text.str() + "\n";
    }
    write_text_file(raw_dir / "lalr_parse_table_conflicts.tsv", conflict_tsv);

    // 9) raw/lalr_parse_table_conflict_resolution.tsv
    std::string resolution_tsv =
        "state_id\tsymbol_id\tsymbol_name\tconflict_type\texisting_action\tincoming_action\tresolved_action\treason\n";
    for (const auto& log : step10_result.lalr_step8_result.conflict_resolution_logs) {
        const std::string symbol_name =
            (log.symbol_id >= 0 && log.symbol_id < static_cast<int>(grammar.symbols.size()))
                ? grammar.symbols[log.symbol_id].name
                : "<invalid>";
        resolution_tsv += std::to_string(log.state_id) + "\t" + std::to_string(log.symbol_id) + "\t" + symbol_name +
                          "\t" + log.conflict_type + "\t" + log.existing_action + "\t" + log.incoming_action + "\t" +
                          log.resolved_action + "\t" + log.reason + "\n";
    }
    write_text_file(raw_dir / "lalr_parse_table_conflict_resolution.tsv", resolution_tsv);

    // 10) analysis/report.txt：覆盖为 step10 诊断结果。
    std::string report;
    report += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    report += "first_converged=" + std::string(first_result.converged ? "true" : "false") + "\n";
    report += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step6_validation_passed=" + std::string(lr1_step6_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step7_validation_passed=" + std::string(lr1_step7_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step8_validation_passed=" + std::string(lr1_step8_validation.passed ? "true" : "false") + "\n";
    report += "lr1_states=" + std::to_string(step10_result.lr1_state_count) + "\n";
    report += "lalr_states=" + std::to_string(step10_result.lalr_state_count) + "\n";
    report += "state_merged=" +
              std::to_string(static_cast<long long>(step10_result.lr1_state_count) -
                             static_cast<long long>(step10_result.lalr_state_count)) +
              "\n";
    report += "lr1_conflicts=" + std::to_string(step10_result.lr1_conflict_count) + "\n";
    report += "lalr_conflicts=" + std::to_string(step10_result.lalr_conflict_count) + "\n";
    report += "lr1_step10_validation_passed=" + std::string(step10_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step10_errors_count=" + std::to_string(step10_validation.errors.size()) + "\n";
    if (!step10_validation.errors.empty()) {
        report += "lr1_step10_errors:\n";
        for (const auto& e : step10_validation.errors) {
            report += "- " + e + "\n";
        }
    }
    report += "lr1_step10_warnings_count=" + std::to_string(step10_validation.warnings.size()) + "\n";
    if (!step10_validation.warnings.empty()) {
        report += "lr1_step10_warnings:\n";
        for (const auto& w : step10_validation.warnings) {
            report += "- " + w + "\n";
        }
    }
    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";
    write_text_file(analysis_dir / "report.txt", report);
}

}  // namespace seu::yacc
