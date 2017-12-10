#pragma once
#include <string>
#include "lexer.h"
#include "logger_decls.h"
#include "status.h"
#include "ptr.h"
#include "identifier.h"

namespace types {
	struct type_t;
}

typedef std::map<std::string, ptr<const types::type_t>> type_macros_t;

struct parse_state_t {
	typedef log_level_t parse_error_level_t;
	parse_error_level_t pel_error = log_error;
	parse_error_level_t pel_warning = log_warning;

	parse_state_t(
			status_t &status,
			std::string filename,
			zion_lexer_t &lexer,
			type_macros_t type_macros,
			std::vector<token_t> *comments=nullptr,
			std::set<token_t> *link_ins=nullptr);

	bool advance();
	void warning(const char *format, ...);
	void error(const char *format, ...);

	bool line_broke() const { return newline || prior_token.tk == tk_semicolon; }
	token_t token;
	token_t prior_token;
	std::string filename;
	identifier::ref module_id;
	zion_lexer_t &lexer;
	status_t &status;
	type_macros_t type_macros;
	std::vector<token_t> *comments;
	std::set<token_t> *link_ins;

	/* keep track of the current function declaration parameter position */
	int argument_index;

private:
	bool newline = false;
};
