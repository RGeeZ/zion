#include "zion.h"
#include "ast.h"
#include "compiler.h"
#include "llvm_utils.h"
#include "llvm_types.h"
#include "code_id.h"
#include "logger.h"

bound_type_t::refs create_bound_types_from_product(
		status_t &status,
		llvm::IRBuilder<> &builder,
		ptr<scope_t> scope,
		types::type_product::ref product)
{
	/* iteratate over a product type and pull out a list of the bound types
	 * within */
	bound_type_t::refs args;
	for (auto dimension : product->dimensions) {
		bound_type_t::ref arg = upsert_bound_type(status, builder, scope,
				dimension);
		if (!!status) {
			args.push_back(arg);
		}
	}
	return args;
}

bound_type_t::ref create_bound_product_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		ptr<scope_t> scope,
		const ptr<const types::type_product> &product)
{
	ptr<program_scope_t> program_scope = scope->get_program_scope();

	switch (product->pk) {
	case pk_ref:
		{
			assert(!scope->get_bound_type(product->get_signature()));
			bound_type_t::ref bound_type = scope->get_bound_type(
					product->dimensions[0]->get_signature());
			if (bound_type != nullptr) {
				return bound_type_t::create(product,
						product->get_location(),
						bound_type->get_llvm_type()->getPointerTo());
			}
						
			/* we've never seen the internal type, so start by registering an
			 * opaque placeholder for the struct's concrete type */
			
			/* first create the opaque type */
			llvm::StructType *llvm_type = llvm::StructType::create(
					builder.getContext(),
					product->dimensions[0]->get_signature().str());
			assert(!llvm_type->isSized());
			assert(llvm_type->isOpaque());

			auto bound_pointer_type = bound_type_t::create(
					product,
					product->get_location(),
					llvm_type->getPointerTo());

			program_scope->put_bound_type(status, bound_pointer_type);

			debug_above(4, log(log_info, "%s", product->dimensions[0]->str().c_str()));
			assert(dyncast<const types::type_product>(product->dimensions[0]) != nullptr);

			return bound_pointer_type;
		}
	case pk_module:
		{
			assert(false);
			break;
		}
	case pk_args:
		{
			assert(false);
			break;
		}
	case pk_native_struct:
		{
			assert(false);
			break;
		}
	case pk_tuple:
		{
#ifdef VAR_T
			if (product->ftv_count() != 0) {
				return program_scope->get_bound_type({BUILTIN_UNREACHABLE_TYPE});
			}

			/* start by registering a placeholder handle for the data ctor's
			 * actual final type */
			assert(!scope->get_bound_type(product->get_signature()));
			auto bound_type_handle = bound_type_t::create_handle(
					product,
					program_scope->get_bound_type({"__var_ref"})->get_llvm_type());

			program_scope->put_bound_type(status, bound_type_handle);

			if (!!status) {
				bound_type_t::refs args;
				for (auto dim : product->dimensions) {
					auto arg = upsert_bound_type(status, builder, scope, dim);

					if (!status) {
						break;
					}
					args.push_back(arg);
				}

				if (!!status) {
					auto bound_type = create_algebraic_data_type(status, builder, scope,
							types::gensym(), args, product->name_index,
							product->get_location(), product);
					if (!!status) {
						bound_type_handle->set_actual(bound_type);
						return bound_type;
					}
				}
			}
#endif
			not_impl();
			break;
		}
	case pk_tag:
		{
			assert(false);
			break;
		}
	}

	assert(!status);
	return nullptr;
}

bound_type_t::ref bind_type_expansion(
		status_t &status,
	   	llvm::IRBuilder<> &builder,
	   	scope_t::ref scope,
	   	types::type::ref type,
		std::string struct_name)
{
	/* first create the opaque type */
	llvm::StructType *llvm_type = llvm::StructType::create(
			builder.getContext(), struct_name);
	assert(!llvm_type->isSized());
	assert(llvm_type->isOpaque());

	auto bound_type = bound_type_t::create(type, type->get_location(), llvm_type);
	auto program_scope = scope->get_program_scope();
	program_scope->put_bound_type(status, bound_type);

	if (!!status) {
		/* now, we can do whatever it takes to resolve this. let's look up this
		 * type in the environment to see if it resolves to any already known
		 * bound_types. this involves recursion on contained types. */
		if (type != nullptr) {
			debug_above(2, log(log_info, "found unbound type_id in env " c_type("%s") " => %s",
						type->get_signature().c_str(),
						type->str().c_str()));

			if (auto lambda = dyncast<const types::type_lambda>(type)) {
				debug_above(4, log(log_info, "type_id %s expands to type_lambda %s",
							type->str().c_str(),
							lambda->str().c_str()));
				user_error(status, type->get_location(),
						"type %s resolves to a lambda, however we found a reference that does not supply parameters",
						type->str().c_str());
			} else {
				/* cool, we have a term we can recurse on. */
				auto bound_type = upsert_bound_type(status, builder, scope, type);

				if (!!status) {
					llvm::StructType *llvm_struct_type = llvm::dyn_cast<llvm::StructType>(
							bound_type->get_llvm_type());

					if (llvm_struct_type != nullptr) {
						/* we're resolved what the structure looks like,
						 * let's set that as the structure's body */
						llvm_type->setBody(llvm_struct_type->elements());

						/* and we're done instantiating this type in LLVM */
						return bound_type;
					} else {
						user_error(status, type->get_location(),
								"failed to find a structure definition for %s",
								type->str().c_str());
						user_message(log_info, status, type->get_location(),
								"did find: %s %s",
								bound_type->str().c_str(),
								llvm_print_type(*bound_type->get_llvm_type()).c_str());
					}
				}
			}
		} else {
			user_error(status, type->get_location(),
					"unable to find a type definition for %s in " c_id("%s"),
					type->str().c_str(),
					scope->get_name().c_str());
		}
	}

	assert(!status);
	return nullptr;
}

bound_type_t::ref create_bound_id_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		ptr<scope_t> scope,
		const ptr<const types::type_id> &id)
{
	/* this id type does not yet have a bound type. */
	assert(!scope->get_bound_type(id->get_signature()));
	auto env = scope->get_typename_env();
	auto expansion = eval_id(id, env);

	/* however, what it expands to might already have a bound type */
	if (expansion != nullptr) {
		return upsert_bound_type(status, builder, scope, expansion);
		return bind_type_expansion(status, builder, scope, expansion,
			scope->make_fqn(id->get_id()->get_name().str()));
	} else {
		user_error(status, id->get_location(), "no type definition found for %s",
				id->str().c_str());
	}
	assert(!status);
	return nullptr;
}

bound_type_t::ref create_bound_operator_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		ptr<scope_t> scope,
		const ptr<const types::type_operator> &operator_)
{
	debug_above(4, log(log_info, "create_bound_operator_type(..., %s)", operator_->str().c_str()));

	/* the strategy with operator types is to bind a handle for them, then
	 * expand them and recurse by creating all of their contained or subtypes.
	 * once we have an idea of that expansion's type, we set actual on that */

	/* apply the operator */
	auto expansion = eval_apply(operator_->oper, operator_->operand,
			scope->get_typename_env());

	return bind_type_expansion(status, builder, scope, expansion,
			scope->make_fqn(operator_->get_id()->get_name().str()));
}

bound_type_t::ref create_bound_maybe_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		ptr<scope_t> scope,
		const ptr<const types::type_maybe> &maybe)
{
	auto program_scope = scope->get_program_scope();
	bound_type_t::ref bound_just_type = upsert_bound_type(status, builder, scope, maybe->just);
	if (!!status) {
		auto llvm_type = bound_just_type->get_llvm_type();
		if (!llvm_type->isPointerTy()) {
			auto bound_type = bound_type_t::create(
					maybe,
					bound_just_type->get_location(),
					llvm_type->getPointerTo());
			program_scope->put_bound_type(status, bound_type);
			if (!!status) {
				return bound_type;
			}
		} else {
			user_error(status, maybe->get_location(),
				   	"type %s cannot be a " c_type("maybe") " type because the underlying storage is already a pointer (it is %s)",
					maybe->str().c_str(),
					llvm_print_type(*llvm_type).c_str());
		}
	}

	assert(!status);
	return nullptr;
}

bound_type_t::ref create_bound_sum_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		ptr<scope_t> scope,
		const ptr<const types::type_sum> &sum)
{
	assert(!scope->get_bound_type(sum->get_signature()));

	auto bound_type = bound_type_t::create(sum,
			sum->get_location(),
			scope->get_bound_type({"__var_ref"})->get_llvm_type());

	ptr<program_scope_t> program_scope = scope->get_program_scope();
	program_scope->put_bound_type(status, bound_type);
	if (!!status) {
		return bound_type;
	} else {
		return nullptr;
	}
}

bound_type_t::ref create_bound_function_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		ptr<scope_t> scope,
		const ptr<const types::type_function> &function)
{
	bound_type_t::refs args = create_bound_types_from_product(status,
			builder, scope, function->args);
	bound_type_t::ref return_type = upsert_bound_type(
			status, builder, scope, function->return_type);

	if (!!status) {
		auto signature = function->get_signature();
		auto bound_type = scope->get_bound_type(signature);
		if (bound_type) {
			return bound_type;
		} else {
			auto *llvm_fn_type = llvm_create_function_type(status,
					builder, args, return_type);
			if (!!status) {
				bound_type = bound_type_t::create(function,
						function->get_location(), llvm_fn_type);
				ptr<program_scope_t> program_scope = scope->get_program_scope();
				program_scope->put_bound_type(status, bound_type);

				if (!!status) {
					return bound_type;
				} else {
					return nullptr;
				}
			}
		}
	}

	assert(!status);
	return nullptr;
}

bound_type_t::ref create_bound_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		ptr<scope_t> scope,
		types::type::ref type)
{
	assert(!!status);

	indent_logger indent(3,
		string_format("attempting to create a bound type for %s in scope " c_id("%s"),
			type->str().c_str(), scope->get_name().c_str()));

    auto program_scope = scope->get_program_scope();

	if (auto id = dyncast<const types::type_id>(type)) {
		return create_bound_id_type(status, builder, scope, id);
    } else if (auto maybe = dyncast<const types::type_maybe>(type)) {
		return create_bound_maybe_type(status, builder, scope, maybe);
	} else if (auto product = dyncast<const types::type_product>(type)) {
		return create_bound_product_type(status, builder, scope, product);
	} else if (auto function = dyncast<const types::type_function>(type)) {
		return create_bound_function_type(status, builder, scope, function);
	} else if (auto sum = dyncast<const types::type_sum>(type)) {
		return create_bound_sum_type(status, builder, scope, sum);
	} else if (auto operator_ = dyncast<const types::type_operator>(type)) {
		return create_bound_operator_type(status, builder, scope, operator_);
	} else if (auto variable = dyncast<const types::type_variable>(type)) {
		user_error(status, variable->get_location(), "unable to resolve type for %s", variable->str().c_str());
	}

	assert(!status);
	return nullptr;
}

bound_type_t::ref upsert_bound_type(
		status_t &status,
	   	llvm::IRBuilder<> &builder,
		ptr<scope_t> scope,
	   	types::type::ref type)
{
	type = type->rebind(scope->get_type_variable_bindings());

	auto signature = type->get_signature();
	auto bound_type = scope->get_bound_type(signature);
	if (bound_type != nullptr) {
		return bound_type;
	} else {
		/* we believe that this type does not exist. let's build it */
		bound_type = create_bound_type(status, builder, scope, type);

		if (!!status) {
			return bound_type;
		}

		user_error(status, type->get_location(),
			   	"unable to find a definition for %s in scope " c_id("%s"),
				type->str().c_str(),
                scope->get_name().c_str());
	}

	assert(!status);
	return nullptr;
}

bound_type_t::ref get_function_return_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		const ast::item &obj,
		scope_t::ref scope,
		bound_type_t::ref function_type)
{
	if (auto type_function = dyncast<const types::type_function>(function_type->get_type())) {
		auto return_type_sig = type_function->return_type->get_signature();
		auto return_type = scope->get_bound_type(return_type_sig);

		/* this should exist, otherwise how was the function type built in the
		 * first place? */
		assert(return_type != nullptr);
		debug_above(8, log(log_info, "got function return type %s", return_type->str().c_str()));
		return return_type;
	} else {
		panic("expected a function");
		return nullptr;
	}
}

bound_type_t::ref get_or_create_tuple_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		scope_t::ref scope,
		identifier::ref id,
		bound_type_t::refs args,
		const ast::item::ref &node)
{
	atom name = id->get_name();

	/* get the type of this tuple type */
	types::type::ref type = get_tuple_type(get_types(args));
	auto data_type = scope->get_bound_type(type->get_signature());

	if (data_type != nullptr) {
		return data_type;
	} else {
		auto program_scope = scope->get_program_scope();

		/* build the llvm specific type */
		llvm::Type *llvm_tuple_type = llvm_create_tuple_type(
				builder, program_scope, name, args);

		assert(!"need to treat native types");
		llvm::Type *llvm_wrapped_tuple_type = llvm_wrap_type(builder, program_scope,
				name, llvm_tuple_type);

		/* display the new type */
		llvm::Type *llvm_obj_struct_type = llvm::cast<llvm::PointerType>(llvm_wrapped_tuple_type)->getElementType();
		debug_above(5, log(log_info, "created LLVM wrapped tuple type %s", llvm_print_type(*llvm_obj_struct_type).c_str()));

		/* get the bound type of the data ctor's value */
		bound_type_t::ref data_type = bound_type_t::create(
				type,
				node->token.location,
				llvm_wrapped_tuple_type,
				args);

		/* put the type for the data type */
		program_scope->put_bound_type(status, data_type);

		if (!!status) {
			return data_type;
		} else {
			return nullptr;
		}
	}
}

bound_type_t::ref get_or_create_algebraic_data_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		scope_t::ref scope,
		identifier::ref id,
		bound_type_t::refs args,
		atom::map<int> name_index,
		struct location location,
		types::type_product::ref type)
{
	assert(type != nullptr);

	debug_above(5, log(log_info, "get_or_create_algebraic_data_type looking for %s",
			type->get_signature().c_str()));

	bound_type_t::ref data_type = scope->get_bound_type(type->get_signature());

	if (data_type != nullptr) {
		return data_type;
	} else {
		return create_algebraic_data_type(status, builder, scope, id, args,
				name_index, location, type);
	}
}

bound_type_t::ref create_algebraic_data_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		scope_t::ref scope,
		identifier::ref id,
		bound_type_t::refs args,
		atom::map<int> name_index,
		struct location location,
		types::type::ref type)
{
	assert(id != nullptr);
	auto program_scope = scope->get_program_scope();

	/* build the llvm return type */
	llvm::Type *llvm_tuple_type = llvm_create_tuple_type(
			builder, program_scope, id->get_name(), args);

#ifdef VAR_T
	// TODO: Some condition that leads to knowing whether to wrap the type and
	// return a pointer, which i don't think is correct, since pointers should
	// be dealt with above ...
	//
	if (type->pk == pk_tuple) {
		// TODO: consider injecting the appropriate "args" before the passed in
		// "args" to track object lifetime
		llvm_tuple_type = llvm_wrap_type(builder, program_scope, id->get_name(),
				llvm_tuple_type);
	} else {
		assert(false);
	}
#endif

	/* display the new type */
	debug_above(5, log(log_info, "created LLVM type %s", llvm_print_type(*llvm_tuple_type).c_str()));

	types::type_product::ref product = dyncast<const types::type_product>(type);

	/* get the bound type of the data ctor's value */
	auto data_type = bound_type_t::create(
			type,
			location,
			llvm_tuple_type,
			args,
			name_index);

	/* put the type for the data type. the scope can handle the case where the
	 * type already exists. */
	program_scope->put_bound_type(status, data_type);

	if (!!status) {
		return data_type;
	} else {
		return nullptr;
	}
}

std::pair<bound_var_t::ref, bound_type_t::ref> instantiate_tuple_ctor(
		status_t &status, 
		llvm::IRBuilder<> &builder,
		scope_t::ref scope,
		types::type::ref type_fn_context,
		bound_type_t::refs args,
		identifier::ref id,
		const ast::item::ref &node)
{
#ifdef VAR_T
	/* this is a tuple constructor function */
	if (!!status) {
		program_scope_t::ref program_scope = scope->get_program_scope();

		bound_type_t::ref data_type = get_or_create_tuple_type(status, builder, scope,
				id, args, node);

		if (!!status) {
			bound_var_t::ref tuple_ctor = get_or_create_tuple_ctor(status, builder,
					scope, type_fn_context, data_type, id, node);

			if (!!status) {
				return {tuple_ctor, data_type};
			}
		}
	}
#endif

	assert(!status);
	return {nullptr, nullptr};
}

std::pair<bound_var_t::ref, bound_type_t::ref> instantiate_tagged_tuple_ctor(
		status_t &status, 
		llvm::IRBuilder<> &builder,
		scope_t::ref scope,
		types::type::ref type_fn_context,
		identifier::ref id,
		const ast::item::ref &node,
		types::type::ref type)
{
	assert(id != nullptr);
	assert(type != nullptr);

	/* this is a tuple constructor function */
	if (!!status) {
		program_scope_t::ref program_scope = scope->get_program_scope();

		auto ctor_args_type = eval(type, scope->get_typename_env());
		debug_above(4, log(log_info, "instantiating with type %s -> %s",
					ctor_args_type->str().c_str(), type->str().c_str()));

		auto product = dyncast<const types::type_product>(ctor_args_type);
		assert(product != nullptr);

		bound_type_t::ref data_type = upsert_bound_type(status, builder, scope, type);

		if (!!status) {
			bound_var_t::ref tagged_tuple_ctor = get_or_create_tuple_ctor(status, builder,
					scope, type_fn_context, data_type, id, node);

			if (!!status) {
				return {tagged_tuple_ctor, data_type};
			}
		}
	}

	assert(!status);
	return {nullptr, nullptr};
}

bound_var_t::ref get_or_create_tuple_ctor(
		status_t &status,
		llvm::IRBuilder<> &builder,
		scope_t::ref scope,
		types::type::ref type_fn_context,
		bound_type_t::ref data_type,
		identifier::ref id,
		const ast::item::ref &node)
{
	atom name = id->get_name();

	auto program_scope = scope->get_program_scope();

	auto ctor_args_type = eval(data_type->get_type(), scope->get_typename_env());
	debug_above(4, log(log_info, "get_or_create_tuple_ctor instantiating with type %s -> %s",
				ctor_args_type->str().c_str(), data_type->str().c_str()));

	auto product = dyncast<const types::type_product>(ctor_args_type);
	assert(product != nullptr);

	bound_type_t::refs args = create_bound_types_from_product(status,
			builder, scope, product);

	/* save and later restore the current branch insertion point */
	llvm::IRBuilderBase::InsertPointGuard ipg(builder);
	auto function = llvm_start_function(status, builder, scope,
			node, type_fn_context, args, data_type, name);

	if (!!status) {
		bound_var_t::ref mem_alloc_var = program_scope->get_bound_variable(
				status, node, "__create_var");

		assert(!!status);
		assert(mem_alloc_var != nullptr);

		llvm::Value *llvm_sizeof_tuple = llvm_sizeof_type(builder,
			   	llvm_deref_type(data_type->get_llvm_type()));

		auto signature = get_function_return_type(function->type->get_type())->get_signature();
		debug_above(5, log(log_info, "mapping type " c_type("%s") " to typeid %d",
					signature.c_str(), signature.iatom));

		llvm::Value *llvm_create_var_call_value = llvm_create_call_inst(
				status, builder, *node,
				mem_alloc_var,
				{
					/* name this variable */
					builder.CreateGlobalStringPtr(name.str()),

					/* no mark function yet */
					llvm::Constant::getNullValue(
							program_scope->get_bound_type({"__mark_fn"})->get_llvm_type()),

					/* the type_id */
					builder.getInt32(signature.iatom),

					/* allocation size */
					llvm_sizeof_tuple
				});

		if (!!status) {
			assert(data_type->get_llvm_type() != nullptr);
			if (data_type->get_llvm_type()->isPointerTy()) {
				/* we've allocated enough space for the object type, let's get our allocation as such */
				llvm::Value *llvm_final_obj = builder.CreatePointerBitCastOrAddrSpaceCast(
						llvm_create_var_call_value, 
						data_type->get_llvm_type());

				int index = 0;

				llvm::Function *llvm_function = (llvm::Function *)function->llvm_value;
				llvm::Function::arg_iterator args_iter = llvm_function->arg_begin();
				while (args_iter != llvm_function->arg_end()) {
					llvm::Value *llvm_param = args_iter++;
					/* get the location we should store this datapoint in */
					llvm::Value *llvm_gep = builder.CreateInBoundsGEP(llvm_final_obj,
							{builder.getInt32(0), builder.getInt32(1), builder.getInt32(index++)});
					debug_above(5, log(log_info, "store %s at %s", llvm_print_value(*llvm_param).c_str(),
								llvm_print_value(*llvm_gep).c_str()));
					builder.CreateStore(llvm_param, llvm_gep);
				}

				/* create a return statement for the final object. NB: the returned
				 * type is a generic object type according to LLVM. LLVM's type system
				 * does not support Zion types so we are basically using a generic obj
				 * pointer for all GC'd types. */
				builder.CreateRet(llvm_create_var_call_value);

				llvm_verify_function(status, llvm_function);

				if (!!status) {
					/* bind the ctor to the program scope */
					scope->get_program_scope()->put_bound_variable(status, name, function);

					if (!!status) {
						debug_above(10, log(log_info, "module so far is:\n" c_ir("%s"), llvm_print_module(
										*llvm_get_module(builder)).c_str()));
						return function;
					}
				}
			} else {
				user_error(status, node->get_location(),
					   	"data type %s is not a pointer type",
						data_type->str().c_str());
			}
		}
	}

	assert(!status);
	return nullptr;
}

void ast::type_alias::register_type(
		status_t &status,
		llvm::IRBuilder<> &builder,
		identifier::ref supertype_id,
		identifier::refs type_variables,
	   	scope_t::ref scope) const
{
	debug_above(5, log(log_info, "creating type alias for %s", str().c_str()));

	if (type_variables.size() != 0) {
		user_error(status, token.location, "found type variables in type alias - not yet impl");
	} else {
		user_error(status, token.location, "type aliasing is not yet impl");
	}
}

bound_var_t::ref type_check_get_item_with_int_literal(
		status_t &status,
		llvm::IRBuilder<> &builder,
		scope_t::ref scope,
		const ast::item::ref &node,
		bound_var_t::ref lhs,
		identifier::ref index_id,
		int subscript_index)
{
	if (!!status) {
		bound_var_t::ref index = bound_var_t::create(
				INTERNAL_LOC(),
				"temp_deref_index",
				scope->get_program_scope()->get_bound_type({INT_TYPE}),
				llvm_create_int(builder, subscript_index),
				index_id,
				false/*is_lhs*/);

		/* get or instantiate a function we can call on these arguments */
		return call_program_function(status, builder, scope, "__getitem__",
				node, {lhs, index});
	}

	assert(!status);
	return nullptr;
}

bound_var_t::ref call_const_subscript_operator(
		status_t &status,
		llvm::IRBuilder<> &builder,
		scope_t::ref scope,
		const ast::item::ref &node,
		bound_var_t::ref lhs,
		identifier::ref index_id,
		int subscript_index)
{
	debug_above(6, log(log_info, "generating dereference %s[%d]", lhs->str().c_str(), subscript_index));
	if (subscript_index < 0) {
		user_error(status, *node, "constant subscripts must be positive");
	} else {
		/* do some checks on the lhs */
		auto lhs_type = lhs->type;
		if (lhs_type->get_dimensions().size() != 0) {
			if (lhs_type->get_dimensions().size() > subscript_index) {
				/* ok, we're in range */
				debug_above(6, log(log_info, "generating dereference %s[%d]", lhs->str().c_str(), subscript_index));

				bound_type_t::ref data_type = lhs_type->get_dimensions()[subscript_index];
				assert(data_type != nullptr);
				assert(lhs_type->get_llvm_type() != nullptr);

				/* get the tuple */
				llvm::Value *llvm_lhs = llvm_resolve_alloca(builder, lhs->llvm_value);

				if (lhs_type->get_llvm_type()) {
					llvm::Value *llvm_lhs_subtype = builder.CreatePointerBitCastOrAddrSpaceCast(
							llvm_lhs,
							lhs_type->get_llvm_type());

					debug_above(5, log(log_info, "creating GEP for %s", llvm_print_value(*llvm_lhs_subtype).c_str()));
					llvm::Value *llvm_gep = builder.CreateInBoundsGEP(llvm_lhs_subtype,
							{builder.getInt32(0), builder.getInt32(1), builder.getInt32(subscript_index)});
					llvm::Value *llvm_value = builder.CreateLoad(llvm_gep);
					return bound_var_t::create(
							INTERNAL_LOC(),
							"temp_deref_subscript",
							data_type,
							llvm_value,
							make_code_id(node->token),
							false/*is_lhs*/);
				} else {
					user_error(status, node->get_location(),
						   	"lhs type %s is not a pointer type",
							lhs_type->str().c_str());
				}
			} else {
				user_error(status, *node, "index out of range");
			}
		} else {
			return type_check_get_item_with_int_literal(status, builder, scope,
					node, lhs, index_id, subscript_index);
		}
	}

	assert(!status);
	return nullptr;
}
