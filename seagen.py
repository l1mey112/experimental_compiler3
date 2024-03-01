import sys
import re

if len(sys.argv) < 2 or len(sys.argv) == 3 and sys.argv[2] != '-v':
	print(f'usage: {sys.argv[0]} graph.sea', file=sys.stderr)
	print(f'usage: {sys.argv[0]} graph.sea -v', file=sys.stderr)
	sys.exit(1)

file = sys.argv[1]
is_verbose = len(sys.argv) == 3

def verbose(*args, **kwargs):
	if is_verbose:
		print(*args, **kwargs, file=sys.stderr)

try:
	with open(file) as f:
		text = f.read()
except FileNotFoundError:
	print(f'error: file `{file}` not found', file=sys.stderr)
	sys.exit(1)

NODES = {}

def rule_node(line_nr, col, match):
	name, c0, c1, c2, body = match.groups()

	verbose(f"rule_node: {name} {c0} {c1} {c2} {body}")

	if name in NODES:
		print(f"{file}:{line_nr}:{col}: error: node `{name}` already exists", file=sys.stderr)
		sys.exit(1)

	# make sure name is lowercase
	if not name.islower():
		print(f"{file}:{line_nr}:{col}: error: node `{name}` should be lowercase", file=sys.stderr)
		sys.exit(1)

	# [ ] -> none
	# [x] -> has
	# [!] -> todo

	peepholes = [c0, c1, c2]

	NODES[name] = {
		'peepholes': peepholes,
		'body': body,
	}

	pass

def rule_print(match):
	pass

rules = {
	# this is a comment
	r'^#.*$': None,

	# node [ ] [x] [!] { u64 value; }
	r'(?s)(?P<name>\w+)\s+(?P<c0>\[[ !x]\])\s*(?P<c1>\[[ !x]\])\s*(?P<c2>\[[ !x]\])\s*(?P<body>\{[^}]+\})': rule_node,
}

verbose(f'{file}: parsing...')

pos = 0
line_nr = 1
col = 1
while pos < len(text):
	# skip whitespace
	while text[pos].isspace():
		if text[pos] == '\n':
			line_nr += 1
			col = 1
		else:
			col += 1
		pos += 1

		if pos >= len(text):
			break
	else:
		# match rules
		for rule in rules:
			match = re.match(rule, text[pos:], re.MULTILINE)

			if match:
				verbose(f"{file}:{line_nr}:{col}: matched rule {rule}")

				if rules[rule]:
					rules[rule](line_nr, col, match)

				match_len = len(match.group(0))
				line_nr += text.count('\n', pos, match_len)
				col = match_len - text.rfind('\n', pos, match_len)

				pos += match_len
				break
		else:
			print(f"{file}:{line_nr}:{col}: no rule matched at position {pos}", file=sys.stderr)
			sys.exit(1)

verbose(f'{file}: parsed')
verbose(f'{NODES}')

# now the graph.sea is parsed
# let's generate the code

print("typedef enum p9_kind_t p9_kind_t;")
print("typedef struct p9_t p9_t;")
print()
print("enum p9_kind_t : u8 {")
for name in NODES:
	print(f"\tP9_{name.upper()},")
print("};")
print()
print("struct p9_t {")
print("\tp9_kind_t kind;")
print()
print("\t// ordered def-use")
print("\tp9_t **exprs;")
print()
print("\tu8 extra[];")
print("}")
print()

nodes_with_body = [name for name in NODES if NODES[name]['body']]

for name in nodes_with_body:
	print(f"typedef struct p9_ex_{name}_t p9_{name}_t;")

print()

for name in NODES:
	node = NODES[name]
	if not node['body']:
		continue

	print(f"struct p9_ex_{name}_t {node['body']}")
	
	# print(f"\tP9_{node.upper()},")
