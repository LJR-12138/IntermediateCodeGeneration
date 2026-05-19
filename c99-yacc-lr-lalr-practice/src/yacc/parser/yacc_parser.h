#pragma once

#include <stdexcept>
#include <string>

#include "yacc/model/grammar.h"

namespace seu::yacc {

// 解析错误类型，携带行列信息，方便定位输入文件问题。
class ParseError : public std::runtime_error {
public:
    ParseError(int line, int column, const std::string& message);

    int line() const noexcept { return line_; }
    int column() const noexcept { return column_; }

private:
    int line_;
    int column_;
};

// 第 3 步核心接口：读取并解析 Yacc 输入文件。
Grammar parse_yacc_file(const std::string& path);

}  // namespace seu::yacc

