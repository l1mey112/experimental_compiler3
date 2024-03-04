import sys
import re
import os

# wrapper over print stderr
def fatal(*args, **kwargs):
	print(*args, file=sys.stderr, **kwargs)
	sys.exit(1)

if __name__ != '__main__':
	fatal(f'error: seagen.py is not a module')

if len(sys.argv) < 2:
	print(f'usage: {sys.argv[0]} folder', file=sys.stderr)
	sys.exit(1)

FOLDER = sys.argv[1]
SEA = None
RULES = {}

for entry in os.listdir(FOLDER):
	fp = FOLDER + '/' + entry

	def isw(s):
		if not re.match(r'^\w+$', s):
			fatal(f'error: `{s}` is not a valid prefix')

	if entry.endswith('.sea'):
		if SEA is not None:
			fatal(f'error: multiple .sea files found')
		with open(fp) as f:
			SEA = f.read()

	if entry.endswith('.rules'):
		with open(fp) as f:
			prefix = entry[:-len('.rules')]
			isw(prefix)
			RULES[prefix] = {
				'content': f.read(),
				'rules': []
			}

if SEA is None:
	fatal(f'error: no .sea file found')

def sexp(sexp):
	term_regex = r'''(?mx)
		\s*(?:
			(?P<comment>\#[^\n]*) |
			(?P<sqc>(\(\{)[^)}]*(\}\))) |
			(?P<sq>["'][^"']*["']) |
			(?P<sqand>&&[^&]*=>) |
			(?P<bracklw>\w+:\() |
			(?P<brackl>\() |
			(?P<brackr>\)) |
			(?P<tup>\[\w+[^\]]*\]) |
			(?P<num>\-?\d+\.\d+|\-?\d+) |
			(?P<s>[^(^)\s]+)
		)'''

	nstack = []
	stack = []
	out = []
	for termtypes in re.finditer(term_regex, sexp):
		term, value = [(t,v) for t,v in termtypes.groupdict().items() if v][0]

		if term == 'comment':
			continue

		if term == 'brackl':
			stack.append(out)
			nstack.append(None)
			out = []
		elif term == 'bracklw':
			stack.append(out)
			nstack.append(value[:-2])
			out = []
		elif term == 'brackr':
			assert stack, "unbalanced ) brace"

			name = nstack.pop()

			tmpout, out = out, stack.pop()

			if name is not None:
				out.append((name, tmpout))
			else:
				out.append(tmpout)
		elif term == 'tup':
			# [name value...]
			cont = value[1:-1].split(' ', 1)
			out.append((cont[0], cont[1].strip()))
		elif term == 'num':
			v = float(value)
			if v.is_integer(): v = int(v)
			out.append(v)
		elif term == 'sqand':
			out.append("&&")
			out.append(value[2:-2].strip())
			out.append("=>")
		elif term == 'sq':
			out.append(value[1:-1])
		elif term == 'sqc':
			out.append(value[2:-2].strip())
		elif term == 's':
			out.append(value)
		else:
			raise NotImplementedError("Error: %r" % (term, value))

	assert not stack, "unbalanced ( brace"
	return out[0]

SEA_NODES = {}
SEA_GROUPS = {}

has_unknown = False

for toplevel in sexp('(\n' + SEA + '\n)'):
	match toplevel:
		case ['group', name, nodes]:
			if name in SEA_GROUPS:
				fatal(f"group `{name}` already exists")

			if len(nodes) == 0:
				fatal(f"group `{name}` is empty")

			# ensure existance and same length for all
			size = SEA_NODES[nodes[0]]['inputs']
			for node in nodes:
				if node not in SEA_NODES:
					fatal(f"in group `{name}`, node `{node}` does not exist")
				if SEA_NODES[node]['inputs'] != size:
					fatal(f"in group `{name}`, node `{node}` doesn't have {size} inputs")

			SEA_GROUPS[name] = nodes
		case ['op', inputs, name]:
			if name in SEA_NODES:
				fatal(f"node `{name}` already exists")
			inputs = int(inputs)
			SEA_NODES[name] = {'body': None, 'inputs': inputs}
		case ['opb', inputs, name, body]:
			if name in SEA_NODES:
				fatal(f"node `{name}` already exists")
			inputs = int(inputs)
			SEA_NODES[name] = {'body': body, 'inputs': inputs}
		case _:
			print("unknown:", toplevel, file=sys.stderr)
			has_unknown = True

if has_unknown:
	sys.exit(1)

from dataclasses import dataclass

# (match _)
@dataclass
class Ignore: pass

# (match var)
@dataclass
class FreeVar: name: str

@dataclass
class Node: name: str

@dataclass
class Expr:
	term: Ignore | FreeVar | Node # free var only applies on rhs
	body: list("Expr")
	named: None | str
	constraints: list((str, str)) # [value 0]

# as sexpr
def expr_str(expr):
	def term_str(term):
		match term:
			case Ignore():
				return "_"
			case FreeVar(name):
				return name
			case Node(name):
				return name

	is_bare = len(expr.body) == 0 and expr.named is None and len(expr.constraints) == 0

	st = ""

	if expr.named:
		st += f"{expr.named}:"

	if not is_bare:
		st += "("

	st += f"{term_str(expr.term)}"

	for nexpr in expr.body:
		st += " " + expr_str(nexpr)

	for field, value in expr.constraints:
		st += f" [{field} {value}]"

	if not is_bare:
		st += ")"
	return st

def parse_match(exi_sym, sexp, is_lhs):
	# if lhs then we are defining the symbols
	# if rhs then we are using the symbols
	
	named = None

	if not isinstance(sexp, list):
		sexp = [sexp]

	# something:()
	if isinstance(sexp[0], tuple):
		named = sexp[0][0]
		sexp = sexp[0][1]

	if len(sexp) == 0:
		fatal(f'error: empty list in rule, try `_` instead of `()`')
	
	# can only define free vars in lhs
	if named and not is_lhs:
		fatal(f'error: named match in right hand side of rule')
	
	if named not in exi_sym:
		if is_lhs:
			exi_sym.add(named)
		else:
			fatal(f'error: variable `{head}` already defined in left hand side of rule')

	head, *tail = sexp

	if head == '_':
		if not is_lhs:
			fatal(f'error: `_` in right hand side of rule')
		term = Ignore()
	elif head in SEA_NODES:
		term = Node(head)
	else:
		if is_lhs:
			if head in exi_sym:
				# duplicate definition
				fatal(f'error: variable `{head}` already defined in left hand side of rule')
			else:
				exi_sym.add(head)
		elif not head in exi_sym:
			fatal(f'error: unknown variable `{head}` in rule (lhs: {is_lhs})')

		term = FreeVar(head)

	# rest are [Expr..., Cond...]

	matches = []
	conts = []

	in_matches = True
	for match in tail:
		if isinstance(match, tuple) and isinstance(match[0], str) and isinstance(match[1], str):
			in_matches = False
			conts.append(match)
		else:
			if not in_matches:
				fatal(f'error: match after condition in rule')
			matches.append(parse_match(exi_sym, match, is_lhs))

	if isinstance(term, Node) and len(matches) != SEA_NODES[term.name]['inputs']:
		fatal(f'error: wrong number of inputs for node `{term.name}` in rule')

	return Expr(term, matches, named, conts)

for rule in RULES:
	content = RULES[rule]['content']
	toplevels = sexp('(\n' + content + '\n)')

	i = 0
	while i + 2 < len(toplevels):
		toplevel = toplevels[i:i+3]
		match toplevel:
			case [lhs, '=>', rhs]:
				existing_symbols = set()
				lhs = parse_match(existing_symbols, lhs, True)

				# TODO: handle groups
				if not isinstance(lhs.term, Node):
					fatal(f'error: left hand side of rule must be a node to match')

				rhs = parse_match(existing_symbols, rhs, False)
				RULES[rule]['rules'].append((lhs, rhs))
				i += 3
			case _:
				print("unknown:", toplevel, file=sys.stderr)
				sys.exit(1)
	if i != len(toplevels):
		print("error: unhandled rule at EOF", file=sys.stderr)
		sys.exit(1)

h = open(f'{FOLDER}/p9.h', 'w')
c = open(f'{FOLDER}/p9.c', 'w')

h.write(
	"#pragma once\n"
	"#include \"all.h\"\n"
	"\n"
)

c.write(
	"#include \"all.h\"\n"
	f"#include \"p9.h\"\n"
	"\n"
)

# write node enum and node itself

h.write(f"enum p9_kind_t : u8 {{\n")
for name in SEA_NODES:
	h.write(f"	P9_{name.upper()},\n")
h.write(
	"};\n"
	"\n"
	f"typedef enum p9_kind_t p9_kind_t;\n"
	f"typedef struct p9_t p9_t;\n"
	"\n"
	f"struct p9_t {{\n"
	f"	p9_kind_t kind;\n"
	"\n"
	"	// ordered def-use\n"
	f"	p9_t **inputs;\n"
	"\n"
	"	u8 extra[];\n"
	"};\n"
	"\n"
	"#define P9_EXT(n, opc) ((p9_ex_##opc##_t*)((n)->extra))\n"
	"\n"
)

# write node extra data

nodes_with_body = [name for name in SEA_NODES if SEA_NODES[name]['body']]

for name in nodes_with_body:
	h.write(f"typedef struct p9_ex_{name}_t p9_ex_{name}_t;\n")

h.write("\n")

for name in nodes_with_body:
	node = SEA_NODES[name]
	h.write(f"struct p9_ex_{name}_t {{ {node['body']} }};\n")

h.write("\n")

# forward declare dispatch functions
# TODO: need to place each rule rewriters in a different C file
for rule_name in RULES:
	rules = RULES[rule_name]['rules']

	# PYTHON WOW WHAT??? NO WAY !!
	rule_nodes = {name: [rule for rule in rules if rule[0].term.name == name] for name in SEA_NODES if any(rule[0].term.name == name for rule in rules)}
	
	for name in rule_nodes:
		c.write(f"static p9_t *p9_{rule_name}_rule_{name}(p9_t *n);\n")

	c.write("\n")

	rules = RULES[rule_name]['rules']

	# TODO: handle groups
	#assert isinstance(lhs.term, Node)

	h.write(f"p9_t *p9_{rule_name}(p9_t *n);\n")

	c.write(
		f"p9_t *p9_{rule_name}(p9_t *n) {{\n"
		"	switch (n->kind) {\n"
	)

	for name in rule_nodes:
		# these switch cases will get long
		# give the register allocator a chance to breathe
		# put these in a different function
		
		# TODO: handle groups, will need to dispatch on multiple
		#       functions instead of just one here
		c.write(f"		case P9_{name.upper()}:\n")
		c.write(f"			return p9_{rule_name}_rule_{name}(n);\n")

	c.write(
		"		default:\n"
		"			break;\n"
		"	}\n"
		"\n"
		"	return NULL;\n"
		"}\n"
		"\n"
	)

	for name in rule_nodes:
		c.write(
			f"p9_t *p9_{rule_name}_rule_{name}(p9_t *_n) {{\n"
		)

		for rule in rule_nodes[name]:
			lhs, rhs = rule

			# generate lhs
			# you can assume the LHS Node() is checked,
			# but everything else needs to be

			# from pprint import pprint
			# pprint(lhs)

			rule_str = expr_str(lhs) + " => " + expr_str(rhs)

			# generated rule
			print(rule_str)

			c.write(
				f"	// {rule_str}\n"
				"	{\n"
			)


			# ugh, these are apparently `global`s and not `nonlocal`s
			# should have created a main() function i guess
			cond = ""
			header = ""

			def walk_lhs(vself, expr):
				global cond
				global header

				match expr.term:
					case Ignore():
						pass
					case Node(name):
						if vself == '_n':
							pass # already checked
						else:
							cond += f" && {vself}->kind == P9_{name.upper()}"
					case FreeVar(name):
						header += f"		p9_t *{name} = {vself};\n"

				# TODO: shouldn't really be a case with FreeVar and named?
				if expr.named:
					header += f"		p9_t *{expr.named} = {vself};\n"

				# TODO: need better constraints, even ways to call functions inside
				#       though that would be better delegated to && actually...
				for field, value in expr.constraints:
					# TODO: hack, how else do we match though?
					assert isinstance(expr.term, Node)
					cond += f" && P9_EXT({vself}, {expr.term.name})->{field} == {value}"

				for i, subexpr in enumerate(expr.body):
					walk_lhs(f"{vself}->inputs[{i}]", subexpr)

			walk_lhs('_n', lhs)
			c.write(header)
			c.write(
				f"		if ({cond[4:]}) {{\n"
			)

			# generate rhs

			#def walk_rhs(vself, expr):
			#	match expr.term:
			#		case FreeVar(name):
			#		case Node(name):
			#
			#		case _:
			#			# Ignore can't be on rhs
			#			assert False

			if isinstance(rhs.term, FreeVar):
				c.write(f"			return {rhs.term.name};\n")
			else:
				raise NotImplementedError("TODO")			

			c.write(
				"		}\n"
				"	}\n"
			)

		c.write(
			"	return NULL;\n"
			"}\n"
		)
