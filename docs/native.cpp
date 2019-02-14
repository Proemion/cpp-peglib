#include <emscripten/bind.h>
#include "../peglib.h"
#include <cstdio>
#include <functional>
#include <iomanip>
#include <sstream>

// https://stackoverflow.com/questions/7724448/simple-json-string-escape-for-c/33799784#33799784
std::string escape_json(const std::string& s) {
  std::ostringstream o;
  for (auto c : s) {
    if (c == '"' || c == '\\' || ('\x00' <= c && c <= '\x1f')) {
      o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
    } else {
      o << c;
    }
  }
  return o.str();
}

std::function<void(size_t, size_t, const std::string&)> makeJSONFormatter(std::string& json) {
  auto init = std::make_shared<bool>(true);

  return [&json, init](size_t ln, size_t col, const std::string& msg) mutable {
    if (!init) {
      json += ",";
    }
    json += "{";
    json += R"("ln":)" + std::to_string(ln) + ",";
    json += R"("col":)" + std::to_string(col) + ",";
    json += R"("msg":")" + escape_json(msg) + R"(")";
    json += "}";
    *init = false;
  };
}

bool parse_grammar(const std::string& text, peg::parser& peg, std::string& json) {
  peg.log = makeJSONFormatter(json);
  json += "[";
  auto ret = peg.load_grammar(text.data(), text.size());
  json += "]";
  return ret;
}

bool parse_code(const std::string& text, peg::parser& peg, std::string& json,
                std::shared_ptr<peg::Ast>& ast) {
  peg.enable_ast();
  peg.log = makeJSONFormatter(json);
  json += "[";
  auto ret = peg.parse_n(text.data(), text.size(), ast);
  json += "]";
  return ret;
}

std::string lint(const std::string& grammarText, const std::string& codeText) {
  std::string grammarResult;
  std::string codeResult;
  std::string astResult;
  std::string astResultOptimized;

  peg::parser peg;
  auto ret = parse_grammar(grammarText, peg, grammarResult);

  if (ret && peg) {
    std::shared_ptr<peg::Ast> ast;
    if (parse_code(codeText, peg, codeResult, ast)) {
      astResult = escape_json(peg::ast_to_s(ast));
      astResultOptimized =
          escape_json(peg::ast_to_s(peg::AstOptimizer(true).optimize(ast)));
    }
  }

  std::string json;
  json += "{";
  json += "\"grammar\":" + grammarResult;
  if (!codeResult.empty()) {
    json += ",\"code\":" + codeResult;
    json += ",\"ast\":\"" + astResult + "\"";
    json += ",\"astOptimized\":\"" + astResultOptimized + "\"";
  }
  json += "}";

  return json;
}

EMSCRIPTEN_BINDINGS(native) { emscripten::function("lint", &lint); }