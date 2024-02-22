#pragma once

typedef struct p9_t p9_t;

#include "all.h"

struct p9_t {
	enum {
		// literals
		EXPR_INTEGER,
		EXPR_BOOL,
		EXPR_ARRAY,
		EXPR_TUPLE_UNIT,
		EXPR_TUPLE,  // (exprs...) -> type
		EXPR_STRUCT, // (exprs...) -> type       (canonical representation)
		//
		EXPR_INFIX,
		EXPR_PREFIX,
		//
		EXPR_ASSIGN, // (memory, lhs, rhs) -> memory       (memory region must encompass entire lhs/lvalue)
		//
		EXPR_DEREF,
		EXPR_ADDR_OF,
		EXPR_INDEX,
		EXPR_SLICE,
		EXPR_CAST,
		EXPR_CALL,
		// EXPR_VOIDING, // evaluates expr, possible effects. discards return value, returning ()
		// EXPR_SIZEOF_TYPE, // is usize
		EXPR_LET,
		// EXPR_UNREACHABLE, // TODO: unreachable
		EXPR_FIELD, // named field and tuple field
		// EXPR_LAMBDA,
		// EXPR_MATCH,
		//
		// vars
		EXPR_LOCAL,
		EXPR_SYM,
		// parser abstractions
		EXPR_PARSE_BRK, // always !
		EXPR_PARSE_REP, // always !
		EXPR_PARSE_RET, // always !
		EXPR_PARSE_ASSIGN, // (lhs, rhs) -> type
		EXPR_PARSE_POSTFIX,
		EXPR_PARSE_NAMED_STRUCT, // (args...: d_types...)  -> type
		EXPR_PARSE_IDENT,
		EXPR_PARSE_DO_BLOCK,
		EXPR_PARSE_LOOP,
		EXPR_PARSE_IF,
	} kind;

	type_t type;
	loc_t loc;

	// ordered def-use
	p9_t** exprs;

	union {
		u64 d_integer;
		bool d_bool;
		rlocal_t d_local;
		rsym_t d_sym;
		struct {
			loc_t type_loc; // type is stored inside `type`
		} d_cast;
		struct {
			u16 field_idx; // (default -1) set for tuples, for fields after checking both are set 
			istr_t field;  // set for fields
		} d_field;
		struct {
			p9_t *ref;
			bool is_mut;
		} d_addr_of;
		struct {
			p9_t *expr;
			enum : u8 {
				EXPR_K_INC,
				EXPR_K_DEC,
			} op;
		} d_postfix;
		struct {
			p9_t *expr;
			 enum : u8 {
				EXPR_K_NOT,
				EXPR_K_SUB,
			} op;
		} d_prefix;
		struct {
			p9_t *exprs; // all do blocks have at least one expr (before transformations), unless they become a EXPR_TUPLE_UNIT
			bool forward; // brk, set by parser meaning the last expr isn't a `brk` yet
			bool backward; // rep
			bool no_branch; // doesn't participate as branch target
		} d_do_block;
		struct {
			p9_t *expr;
			bool forward;
			bool backward;
		} d_loop;
		struct {
			p9_t *lhs;
			p9_t *rhs;
			tok_t kind; // TODO: create enum kind
		} d_assign;
		struct {
			p9_t *lhs;
			p9_t *rhs;
			tok_t kind; // TODO: create enum kind
		} d_infix;
		/* struct {
			hir_rvar_t *args;
			p9_t *expr;
		} d_lambda; */
		struct {
			p9_t *expr;
			pattern_t *patterns;
			p9_t *exprs;
		} d_match;
		struct {
			pattern_t pattern;
			p9_t *expr;
		} d_let;
		struct {
			p9_t *f;
			p9_t *args; // f(...args)
		} d_call;
		p9_t *d_tuple;
		p9_t *d_array;
		struct {
			p9_t *expr;
			u32 branch_level;
		} d_break;
		struct {
			u32 branch_level;
		} d_continue;
		struct {
			p9_t *expr;
		} d_return;
		struct {
			p9_t *cond;
			p9_t *then;
			p9_t *els; // can be none
		} d_if;
		struct {
			enum : u8 {
				UNREACHABLE_ASSERTION, // full debuginfo - generates an assertion, deleted on production
				UNREACHABLE_HEURISTIC, // no debuginfo - doesn't generate an assertion at all
				UNREACHABLE_UD2, // minimal debuginfo - unreachable, generates a trap always
				//
				//                         +-------------------------+-------------------------+
				//                         | debug                   | prod                    |
				// +-----------------------+-------------------------+-------------------------+
				// | UNREACHABLE_ASSERTION | assert(0)               | __builtin_unreachable() |
				// | UNREACHABLE_HEURISTIC | __builtin_unreachable() | __builtin_unreachable() |
				// | UNREACHABLE_UD2       | __builtin_trap()        | __builtin_trap()        |
				// +-----------------------+-------------------------+-------------------------+
			} kind;
		} d_unreachable;
		struct {
			hir_sf_t *fields; // type is expr->type
		} d_struct;
		struct {
			type_t struc;
			p9_t *exprs;
		} d_struct_positional;
	};
};