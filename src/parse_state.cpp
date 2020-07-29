#include "parse_state.h"

#include <cstdarg>

#include "ast.h"
#include "builtins.h"
#include "compiler.h"
#include "dbg.h"
#include "disk.h"
#include "logger_decls.h"
#include "parser.h"
#include "tld.h"
#include "types.h"
#include "zion.h"

namespace zion {

namespace parser {

BoundVarLifetimeTracker::BoundVarLifetimeTracker(ParseState &ps)
    : ps(ps), mutable_vars_saved(ps.mutable_vars), term_map_saved(ps.term_map) {
}

BoundVarLifetimeTracker::~BoundVarLifetimeTracker() {
  ps.mutable_vars = mutable_vars_saved;
  ps.term_map = term_map_saved;
}

const ast::Expr *BoundVarLifetimeTracker::escaped_parse_expr(
    bool allow_for_comprehensions) {
  /* pop out of the current parsing scope to allow the parser to harken back to
   * prior scopes */
  auto mutable_vars = ps.mutable_vars;
  ps.mutable_vars = mutable_vars_saved;

  auto term_map = ps.term_map;
  ps.term_map = term_map_saved;

  const ast::Expr *expr = parse_expr(ps, allow_for_comprehensions);
  ps.term_map = term_map;
  ps.mutable_vars = mutable_vars;
  return expr;
}

ParseState::ParseState(std::string filename,
                       std::string module_name,
                       Lexer &lexer,
                       std::vector<Token> &comments,
                       std::set<LinkIn> &link_ins,
                       SymbolExports &symbol_exports,
                       SymbolImports &symbol_imports,
                       const std::map<std::string, int> &builtin_arities)
    : filename(filename),
      module_name(module_name.size() != 0
                      ? module_name
                      : strip_zion_extension(leaf_from_file_path(filename))),
      builtin_arities(builtin_arities), lexer(lexer), comments(comments),
      link_ins(link_ins), symbol_exports(symbol_exports),
      symbol_imports(symbol_imports) {
  advance();
}

bool ParseState::advance() {
  debug_lexer(log(log_info, "advanced from %s %s", tkstr(token.tk),
                  token.text.c_str()[0] != '\n' ? token.text.c_str() : ""));
  prior_token = token;
  return lexer.get_token(token, newline, &comments);
}

Token ParseState::token_and_advance() {
  advance();
  return prior_token;
}

Identifier ParseState::identifier_and_advance(bool map_id) {
  if (token.tk != tk_identifier) {
    throw user_error(token.location, "expected an identifier here");
  }
  advance();
  auto id = Identifier{prior_token.text, prior_token.location};
  return map_id ? id_mapped(id) : id;
}

Identifier ParseState::id_mapped(Identifier id) {
  if (tld::is_fqn(id.name, true /*default_special*/)) {
    /* this has already been mapped */
    return id;
  }
  auto iter = term_map.find(id.name);
  if (iter != term_map.end()) {
    return Identifier{iter->second, id.location};
  } else {
    if (id.name == "offset") {
      for (auto pair : term_map) {
        log("term_map = %s -> %s", pair.first.c_str(), pair.second.c_str());
      }
    }
    return id;
  }
}

void ParseState::error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  auto error = user_error(token.location, format, args);
  va_end(args);
  if (lexer.eof()) {
    error.add_info(token.location, "encountered end-of-file");
  }
  throw error;
}

void ParseState::add_term_map(Location location,
                              std::string key,
                              std::string value,
                              bool allow_override) {
  // log("adding %s to term map => %s", key.c_str(), value.c_str());
  if (!allow_override && in(key, term_map)) {
    throw user_error(location, "symbol %s imported twice", key.c_str())
        .add_info(location, "%s was already mapped to %s", key.c_str(),
                  term_map.at(key).c_str());
  }
  term_map[key] = value;
}

} // namespace parser
} // namespace zion
