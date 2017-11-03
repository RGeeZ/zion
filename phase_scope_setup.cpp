#include "zion.h"
#include <stdarg.h>
#include "phase_scope_setup.h"
#include "logger_decls.h"
#include "utils.h"
#include "compiler.h"
#include "ast.h"
#include "code_id.h"

/*
 * The idea here is that we need a phase that sets up a directed graph of name
 * resolution and adds names to the appropriate scopes.
 */

void scope_setup_error(status_t &status, const ast::item_t &item, const char *format, ...) {
	va_list args;
	va_start(args, format);
	auto str = string_formatv(format, args);
	va_end(args);

	user_error(status, item.token.location, "scope-error: %s", str.c_str());
}


void scope_setup_error(status_t &status, location_t location, const char *format, ...) {
	va_list args;
	va_start(args, format);
	auto str = string_formatv(format, args);
	va_end(args);

	user_error(status, location, "scope-error: %s", str.c_str());
}


unchecked_var_t::ref scope_setup_module_symbol(
		status_t &status,
		const ast::item_t &obj,
		identifier::ref id,
		identifier::ref extends_module,
		module_scope_t::ref module_scope)
{
	program_scope_t::ref program_scope = module_scope->get_program_scope();
	auto unchecked_var = unchecked_var_t::create(id, obj.shared_from_this(), module_scope);
	if (id && !!id->get_name()) {
		if (extends_module) {
			if (!!extend_module) {
				auto name = extend_module->get_name();
				if (name == GLOBAL_SCOPE_NAME) {
					return program_scope->put_unchecked_variable(id->get_name(), unchecked_var);
				} else {
					module_scope = program_scope->get_module_scope(name);
					if (module_scope != nullptr) {
						return module_scope->put_unchecked_variable(id->get_name(), unchecked_var);
					} else {
						scope_setup_error(status, obj, "could not find module " c_module("%s") " to extend with " c_id("%s"),
								name.c_str(),
								id->get_name().c_str());
					}
				}
			} else {
				scope_setup_error(status, extend_module->get_location(), "found an unnamed module extension");
				return nullptr;
			}
		} else {
			return module_scope->put_unchecked_variable(
					id->get_name(),
					unchecked_var_t::create(id, obj.shared_from_this(), module_scope));
		}
	} else {
		scope_setup_error(status, obj, "module-level function definition does not have a name");
		return nullptr;
	}

	assert(!status);
	return nullptr;
}

void scope_setup_type_def(
		status_t &status,
	   	const ast::type_def_t &obj,
	   	ptr<module_scope_t> module_scope)
{
	assert(obj.token.text.find(SCOPE_SEP) == std::string::npos);
	assert(obj.token.text.size() != 0);
	module_scope->put_unchecked_type(
			status,
			unchecked_type_t::create(obj.token.text, obj.shared_from_this(), module_scope));
}

void scope_setup_tag(
		status_t &status,
	   	const ast::tag_t &obj,
	   	ptr<module_scope_t> module_scope)
{
	assert(obj.token.text.find(SCOPE_SEP) == std::string::npos);
	assert(obj.token.text.size() != 0);
	module_scope->put_unchecked_type(
			status,
			unchecked_type_t::create(obj.token.text, obj.shared_from_this(), module_scope));
}

status_t scope_setup_module(compiler_t &compiler, const ast::module_t &obj) {
	status_t status;
	auto module_name = obj.decl->get_canonical_name();

	/* create this module's LLVM IR representation */
	module_scope_t::ref module_scope;

	if (obj.global) {
		module_scope = compiler.get_program_scope();
	} else {
		auto llvm_module = compiler.llvm_get_program_module();
		/* create a new scope for this module */
		module_scope = compiler.get_program_scope()->new_module_scope(
				module_name, llvm_module);
	}

   	compiler.set_module_scope(obj.module_key, module_scope);

	/* add any unchecked tags, types, links, or variables to this module */
	for (auto &tag : obj.tags) {
		scope_setup_tag(status, *tag, module_scope);
	}

	for (auto &type_def : obj.type_defs) {
		scope_setup_type_def(status, *type_def, module_scope);
	}

	for (auto &function : obj.functions) {
		scope_setup_module_symbol(
				status,
			   	*function,
				make_code_id(function->decl->token),
			   	function->decl->extends_module,
				module_scope);
	}

	for (auto &var_decl : obj.var_decls) {
		scope_setup_module_symbol(
				status,
				*var_decl,
				make_code_id(var_decl->token),
				make_code_id(var_decl->extends_module),
				module_scope);
	}

	return status;
}

status_t scope_setup_program(const ast::program_t &obj, compiler_t &compiler) {
	status_t status;

	/* create the outermost scope of the program */
	for (auto &module : obj.modules) {
		assert(module != nullptr);
		status |= scope_setup_module(compiler, *module);
	}
	return status;
}
