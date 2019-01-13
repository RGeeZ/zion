#pragma once
#include <string>
#include "lexer.h"
#include "logger_decls.h"
#include "user_error.h"
#include "ptr.h"
#include "identifier.h"
#include "scope.h"
#include "types.h"

struct parse_state_t {
	typedef log_level_t parse_error_level_t;
	parse_error_level_t pel_error = log_error;

	parse_state_t(
			std::string filename,
			std::string module_name,
			zion_lexer_t &lexer,
			std::vector<token_t> &comments,
			std::set<token_t> &link_ins);

	bool advance();
	token_t token_and_advance();
	identifier_t identifier_and_advance();
	void error(const char *format, ...);
	void add_term_map(location_t, std::string, std::string);
	void add_type_map(location_t, std::string, std::string);
	bool line_broke() const { return newline || prior_token.tk == tk_semicolon; }
	identifier_t id_mapped(identifier_t id);
	token_t token;
	token_t prior_token;
	std::string filename;
	std::string module_name;
	zion_lexer_t &lexer;

	/* top-level term remapping from "get" statements */
	std::unordered_map<std::string, std::string> term_map;

	std::vector<token_t> &comments;
	std::set<token_t> &link_ins;
	ctor_id_map_t ctor_id_map;
	data_ctors_map_t data_ctors_map;
	std::set<identifier_t> newtypes;


	/* scoped expression contexts */
	std::list<scope_t> scopes;

	bool is_mutable_var(std::string name);

	/* keep track of the current function declaration parameter position */
	int argument_index;

private:
	bool newline = false;
};
