#include "all.h"

tinfo_t types[1024];
u32 type_len;

static bool cmp_typeinfo(tinfo_t *a, tinfo_t *b) {
	ti_kind a_type = a->kind, b_type = b->kind;

	if (a_type != b_type) {
		return false;
	}

	#define CMP(field) if (a->field != b->field) return false;
	#define CMP_ARR(field) if ((arrlenu(a->field) != arrlenu(b->field)) || (a->field && b->field && memcmp(a->field, b->field, arrlenu(a->field))) != 0) return false;

	switch (a_type) {
		case TYPE_FUNCTION: {
			CMP_ARR(d_fn.args);
			CMP(d_fn.ret);
			break;
		}
		case TYPE_TUPLE: {
			CMP_ARR(d_tuple);
			break;
		}
		case TYPE_STRUCT: {
			CMP_ARR(d_struct.fields);
			break;
		}
		case TYPE_PTR: {
			CMP(d_ptr.ref);
			CMP(d_ptr.is_mut);
			break;
		}
		case TYPE_SLICE: {
			CMP(d_slice.elem);
			break;
		}
		case TYPE_ARRAY: {
			// TODO: compare POD instead?

			if (a->d_array.is_symbol != b->d_array.is_symbol) {
				return false;
			}

			if (a->d_array.is_symbol) {
				return a->d_array.elem == b->d_array.elem && a->d_array.d_symbol == b->d_array.d_symbol;
			} else {
				return a->d_array.elem == b->d_array.elem && a->d_array.d_length == b->d_array.d_length;
			}
		}
		case TYPE_SYMBOL: {
			return a->d_symbol == b->d_symbol;
		}
		default: {
			eprintf("kind: %d\n", a_type);
			assert_not_reached();
		}
	}
	return true;
}

type_t type_new(tinfo_t typeinfo) {
	assert(typeinfo.kind >= _TYPE_CONCRETE_MAX);

	// intern types, there aren't that many in an average program
	// the same types are often used in the same context
	// so iterate backwards instead

	for (u32 i = type_len; i > 0;) {
		i--;
		tinfo_t *nb = &types[i];
		if (cmp_typeinfo(nb, &typeinfo)) {
			return i + _TYPE_CONCRETE_MAX;
		}
	}

	// assert(size <= _TYPE_CONCRETE_MAX || size - _TYPE_CONCRETE_MAX < UINT16_MAX);
	assert(type_len < ARRAYLEN(types));

	type_t type = type_len + _TYPE_CONCRETE_MAX;
	types[type_len++] = typeinfo;
	return type;
}

type_t type_new_inc_mul(type_t type, bool is_mut) {
	tinfo_t typeinfo = {
		.kind = TYPE_PTR,
		.d_ptr.ref = type,
		.d_ptr.is_mut = is_mut,
	};

	return type_new(typeinfo);
}

// safe for comparisions
ti_kind type_kind(type_t type) {
	assert(type != TYPE_INFER);
	if (type < _TYPE_CONCRETE_MAX) {
		return type;
	}

	tinfo_t *typeinfo = type_get(type);
	return typeinfo->kind;
}

// don't unwrap vars
tinfo_t *type_get(type_t type) {
	assert(type != TYPE_INFER);
	assert(type >= _TYPE_CONCRETE_MAX);
	u32 idx = type - _TYPE_CONCRETE_MAX;
	
	tinfo_t *typeinfo = &types[idx];
	return typeinfo;
}

type_t type_array_or_slice_to_slice(type_t type) {
	ti_kind kind = type_kind(type);
	
	assert(kind == TYPE_SLICE || kind == TYPE_ARRAY);
	
	if (kind == TYPE_SLICE) {
		return type;
	}

	tinfo_t typeinfo = {
		.kind = TYPE_SLICE,
		.d_slice.elem = type_get(type)->d_array.elem,
	};

	return type_new(typeinfo);
}

type_t type_array_or_slice_elem(type_t type) {
	ti_kind kind = type_kind(type);
	
	assert(kind == TYPE_SLICE || kind == TYPE_ARRAY);
	
	if (kind == TYPE_SLICE) {
		return type_get(type)->d_slice.elem;
	} else {
		return type_get(type)->d_array.elem;
	}
}

type_t type_underlying(type_t type) {
	ti_kind kind = type_kind(type);

	if (kind == TYPE_SYMBOL) {
		return type_underlying(symbols[type_get(type)->d_symbol].d_type.type);
	}

	return type;
}

type_t type_strip_muls(type_t type) {
	ti_kind kind = type_kind(type);

	if (kind == TYPE_PTR) {
		return type_strip_muls(type_get(type)->d_ptr.ref);
	}

	return type;
}

u32 type_nr_muls(type_t type) {
	ti_kind kind = type_kind(type);

	if (kind == TYPE_PTR) {
		return 1 + type_nr_muls(type_get(type)->d_ptr.ref);
	}

	return 0;
}

static u8 *p;

static const char *ctinfo_str[] = {
	#define X(_, lit) lit,
		TYPE_X_CONCRETE_LIST
	#undef X
};

// TODO: proper eprintf streamer for string dbg output

static void _type_dbg_str(type_t type, bool inner) {
	#define COMMIT(expr) \
		do { \
			p += (expr); \
		} while (0)
		
	if (type == TYPE_INFER) {
		COMMIT(sprintf((char *)p, "<infer>"));
		return;
	}

	if (type < _TYPE_CONCRETE_MAX) {
		COMMIT(sprintf((char *)p, "%s", ctinfo_str[type]));			
		return;
	}

	tinfo_t *typeinfo = type_get(type);

	switch (typeinfo->kind) {
		case TYPE_TUPLE: {
			COMMIT(sprintf((char *)p, "("));
			for (u32 i = 0, c = arrlenu(typeinfo->d_tuple); i < c; i++) {
				type_t elem = typeinfo->d_tuple[i];
				_type_dbg_str(elem, false);
				if (i + 1 < c) {
					COMMIT(sprintf((char *)p, ", "));
				}
			}
			COMMIT(sprintf((char *)p, ")"));
			break;
		}
		case TYPE_FUNCTION: {
			// TODO: `inner` has no use
			/* if (!inner) {
				COMMIT(sprintf((char *)p, "("));
			} */
			
			COMMIT(sprintf((char *)p, "("));
			for (u32 i = 0, c = arrlenu(typeinfo->d_fn.args); i < c; i++) {
				type_t arg = typeinfo->d_fn.args[i];
				_type_dbg_str(arg, false);
				if (i + 1 < c) {
					COMMIT(sprintf((char *)p, ", "));
				}
			}
			COMMIT(sprintf((char *)p, "): "));
			_type_dbg_str(typeinfo->d_fn.ret, false);
			/* if (!inner) {
				COMMIT(sprintf((char *)p, ")"));
			} */
			break;
		}
		case TYPE_PTR: {
			COMMIT(sprintf((char *)p, "*"));
			if (typeinfo->d_ptr.is_mut) {
				COMMIT(sprintf((char *)p, "'"));
			}
			_type_dbg_str(typeinfo->d_ptr.ref, false);
			break;
		}
		case TYPE_ARRAY: {
			if (typeinfo->d_array.is_symbol) {
				COMMIT(sprintf((char *)p, "[%s]", sv_from(symbols[typeinfo->d_array.d_symbol].key)));
			} else {
				COMMIT(sprintf((char *)p, "[%zu]", typeinfo->d_array.d_length));
			}
			_type_dbg_str(typeinfo->d_array.elem, false);
			break;
		}
		case TYPE_SLICE: {
			COMMIT(sprintf((char *)p, "[]"));
			_type_dbg_str(typeinfo->d_slice.elem, false);
			break;
		}
		case TYPE_SYMBOL: {
			COMMIT(sprintf((char *)p, "%s", sv_from(symbols[typeinfo->d_symbol].key)));
			break;
		}
		case TYPE_STRUCT: {
			COMMIT(sprintf((char *)p, "struct { "));
			for (u32 i = 0, c = arrlenu(typeinfo->d_struct.fields); i < c; i++) {
				tinfo_sf_t *field = &typeinfo->d_struct.fields[i];
				COMMIT(sprintf((char *)p, "%s: ", sv_from(field->field)));
				_type_dbg_str(field->type, false);
				if (i + 1 < c) {
					COMMIT(sprintf((char *)p, ", "));
				}
			}
			COMMIT(sprintf((char *)p, " }"));
			break;
		}
		case TYPE_SUM: {
			for (u32 i = 0, c = arrlenu(typeinfo->d_sum.elems); i < c; i++) {
				type_t elem = typeinfo->d_sum.elems[i];
				_type_dbg_str(elem, false);
				if (i + 1 < c) {
					COMMIT(sprintf((char *)p, " | "));
				}
			}
			break;
		}
		default: {
			assert_not_reached();
		}
	}
}

// checks for literals, everything else doesn't
bool type_is_number(type_t type) {
	return (type >= TYPE_SIGNED_INTEGERS_START && type <= TYPE_SIGNED_INTEGERS_END) ||
		(type >= TYPE_UNSIGNED_INTEGERS_START && type <= TYPE_UNSIGNED_INTEGERS_END) ||
		(type == TYPE_F32 || type == TYPE_F64);
}

bool type_is_integer(type_t type) {
	return (type >= TYPE_SIGNED_INTEGERS_START && type <= TYPE_SIGNED_INTEGERS_END) ||
		(type >= TYPE_UNSIGNED_INTEGERS_START && type <= TYPE_UNSIGNED_INTEGERS_END);
}

bool type_is_float(type_t type) {
	return type == TYPE_F32 || type == TYPE_F64;
}

bool type_is_signed(type_t type) {
	return type_is_float(type) || (type >= TYPE_SIGNED_INTEGERS_START && type <= TYPE_SIGNED_INTEGERS_END);
}

bool type_is_unsigned(type_t type) {
	return (type >= TYPE_UNSIGNED_INTEGERS_START && type <= TYPE_UNSIGNED_INTEGERS_END);
}

// allocates using alloc_* functions
const char *type_dbg_str(type_t type) {
	// u8 *p = alloc_scratch(0);
	//u32 len = _type_dbg_str(p, type);

	
	// ARGHHHHH
	p = alloc_scratch(256);
	u8 *oldp = p;

	_type_dbg_str(type, true);

	u32 nwritten = p - oldp;

	assert(nwritten < 256);

	p[nwritten] = '\0';

	return (const char *)oldp;
}

bool type_is_diverging(type_t type) {
	// TODO: search through structs etc
	
	return type == TYPE_BOTTOM;
}

// relies on ABI
u32 type_sizeof(arch_t *arch, type_t type) {
	ti_kind kind = type_kind(type);

	// TODO: store type size on typeinfo
	// TODO: search for ! inside types
	
	switch (kind) {
		case TYPE_I8: return 1;
		case TYPE_I16: return 2;
		case TYPE_I32: return 4;
		case TYPE_I64: return 8;
		case TYPE_ISIZE: return arch->ptr_size;
		case TYPE_U8: return 1;
		case TYPE_U16: return 2;
		case TYPE_U32: return 4;
		case TYPE_U64: return 8;
		case TYPE_USIZE: return arch->ptr_size;
		case TYPE_F32: return 4;
		case TYPE_F64: return 8;
		case TYPE_BOOL: return 1;
		//
		case TYPE_UNIT: return 0;
		case TYPE_BOTTOM: {
			assert_not_reached();
		}
		// TYPE_TUPLE
		// TYPE_FUNCTION
		// TYPE_CLOSURE
		// TYPE_CLOSURE_UNION
		// TYPE_PTR
		// TYPE_SLICE
		// TYPE_ARRAY
		// TYPE_SYMBOL
		case TYPE_ARRAY: {
			// TODO: proper alignment and everything
			tinfo_t *info = type_get(type);
			assert(!info->d_array.is_symbol);
			return type_sizeof(arch, info->d_array.elem) * info->d_array.d_length;
		}
		default: {
			// TODO: please fix this
			eprintf("type: %s\n", type_dbg_str(type));
			assert_not_reached();
		}
	}
}

// relies on ABI
u32 type_alignof(arch_t *arch, type_t type) {
	ti_kind kind = type_kind(type);

	// TODO: 32 bit systems and below
	
	switch (kind) {
		case TYPE_I8: return 1;
		case TYPE_I16: return 2;
		case TYPE_I32: return 4;
		case TYPE_I64: return 8;
		case TYPE_ISIZE: return arch->ptr_size;
		case TYPE_U8: return 1;
		case TYPE_U16: return 2;
		case TYPE_U32: return 4;
		case TYPE_U64: return 8;
		case TYPE_USIZE: return arch->ptr_size;
		case TYPE_F32: return 4;
		case TYPE_F64: return 8;
		case TYPE_BOOL: return 1;
		//
		// TYPE_TUPLE
		// TYPE_FUNCTION
		// TYPE_CLOSURE
		// TYPE_CLOSURE_UNION
		// TYPE_PTR
		// TYPE_SLICE
		// TYPE_ARRAY
		// TYPE_SYMBOL
		case TYPE_UNIT: return 1;
		case TYPE_BOTTOM: {
			assert_not_reached();
		}
		default: {
			// TODO: please fix this
			assert_not_reached();
		}
	}
}

// taken from rustc ABI/consteval overflow errors

u64 type_abi_truncate(arch_t *arch, type_t type, u64 lit) {
	u32 bits = type_sizeof(arch, type) * 8;

	if (bits == 0) {
		return 0;
	}

	u32 shift = 64 - bits;

	return (lit << shift) >> shift;
}

i64 type_abi_sign_extend(arch_t *arch, type_t type, u64 lit) {
	u32 bits = type_sizeof(arch, type) * 8;

	if (bits == 0) {
		return 0;
	}

	u32 shift = 64 - bits;

	return (i64)(lit << shift) >> shift;
}

i64 type_abi_signed_int_min(arch_t *arch, type_t type) {
	u32 bits = type_sizeof(arch, type) * 8;

	assert(bits != 0);

	if (bits == 64) {
		return ~0ULL >> 1;
	}

	return -(1LL << (bits - 1));
}

i64 type_abi_signed_int_max(arch_t *arch, type_t type) {
	u32 bits = type_sizeof(arch, type) * 8;

	assert(bits != 0);

	return (1LL << (bits - 1)) - 1;
}

u64 type_abi_unsigned_int_max(arch_t *arch, type_t type) {
	u32 bits = type_sizeof(arch, type) * 8;

	assert(bits != 0);

	return (1ULL << bits) - 1;
}

// checks integer size
// ensure type is numeric
bool type_abi_integer_fits_in(arch_t *arch, u64 lit, type_t type) {
	assert(type_is_integer(type));

	u64 trunc = type_abi_truncate(arch, type, lit);

	bool fits_in;
	if (type_is_signed(type)) {
		fits_in = type_abi_sign_extend(arch, type, trunc) == (i64)lit;
	} else {
		fits_in = trunc == lit;
	}

	return fits_in;
}
