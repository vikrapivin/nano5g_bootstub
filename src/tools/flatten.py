#!/usr/bin/env python3
"""flatten.py — resolve preprocessor conditionals in the vendored rockbox
tree for THIS build's configuration.

Rules (macro table = gcc -dM ambient dump + the file's own #defines seen
so far along live paths):
  * dead conditional chains are dropped entirely (content and comments);
  * live SINGLE-branch chains (#ifdef / #ifndef / #if with no #else and
    no #elif) are kept verbatim — include guards and #ifndef NULL-style
    protections depend on include order and must stay dynamic;
  * live multi-branch chains (#else / #elif) are resolved: only the live
    branch's content is kept, the directives go away.
Comments inside kept content stay; comments in dead branches go with the
branch. #include lines are never expanded; code is never rewritten.

Usage: flatten.py AMBIENT.dM FILE...   (files are replaced in place)
"""
import re
import sys

OPEN = re.compile(r'\s*#\s*(ifdef|ifndef|if|elif|else|endif)\b')
DEF = re.compile(r'\s*#\s*define\s+(\w+)(\(.*?\))?\s*(.*)')
UND = re.compile(r'\s*#\s*undef\s+(\w+)')


def load_ambient(path):
    table = {}
    for line in open(path):
        m = re.match(r'#define (\w+)(\(.*?\))?\s*(.*)$', line.rstrip())
        if m:
            table[m.group(1)] = m.group(3)
    return table


class Expr:
    def __init__(self, ambient):
        self.ambient = ambient

    def eval(self, expr, table):
        e = re.sub(r'/\*.*?\*/', ' ', expr, flags=re.S)
        e = re.sub(r'//.*$', '', e).strip()
        e = re.sub(r'defined\s*\(\s*(\w+)\s*\)',
                   lambda m: '1' if m.group(1) in table else '0', e)
        e = re.sub(r'defined\s+(\w+)',
                   lambda m: '1' if m.group(1) in table else '0', e)
        e = re.sub(r'\b(?:__GNUC__|__STDC__)\b', '1', e)

        def subst(m):
            name = m.group(0)
            depth = 0
            while name in table and depth < 16:
                body = table[name].strip()
                if re.fullmatch(r'[-+]?(0[xX][0-9a-fA-F]+|\d+)', body):
                    return int(body, 0)
                try:
                    return self.eval(body, table)
                except SystemExit:
                    return 0
            if re.fullmatch(r'[-+]?(0[xX][0-9a-fA-F]+|\d+)', name):
                return int(name, 0)
            return 0
        e = re.sub(r'\b[A-Za-z_]\w*\b', lambda m: str(subst(m)), e)
        e = re.sub(r'\b0x([0-9a-fA-F]+)[uUlL]*\b', r'0x\1', e)
        e = re.sub(r'\b(\d+)[uUlL]+\b', r'\1', e)
        e = (e.replace('&&', ' and ').replace('||', ' or ')
              .replace('!=', ' != ').replace('==', ' == ')
              .replace('<=', ' <= ').replace('>=', ' >= '))
        e = re.sub(r'!(?!=)', ' not ', e)
        try:
            return 1 if eval(e) else 0
        except Exception:
            raise SystemExit(f"cannot evaluate: #if {expr.strip()}")


class Frame:
    __slots__ = ('kw', 'expr', 'start', 'opener_end', 'simple',
                 'parent_live', 'branches', 'cur', 'taken', 'in_else')

    def __init__(self, kw, expr, start, opener_end, parent_live):
        self.kw, self.expr, self.start = kw, expr, start
        self.opener_end = opener_end
        self.simple = True
        self.parent_live = parent_live
        self.taken = False
        self.in_else = False
        self.branches = []            # [live, [lines...]]

    def new_branch(self, live, dir_span=None):
        self.cur = [live, [], dir_span]
        self.branches.append(self.cur)

    @property
    def cur_live(self):
        return self.cur[0]


def flatten(path, ambient):
    raw = open(path).readlines()
    # the ambient dump was taken with config.h included, so it carries the
    # include-guard macros of the probe chain; a file's OWN guard must not
    # be pre-defined when flattening that file
    own_guard = None
    for line in raw:
        m = re.match(r'\s*#\s*ifndef\s+(\w+)', line)
        if m:
            own_guard = m.group(1)
            break
    ambient = {k: v for k, v in ambient.items() if k != own_guard}
    self_defs = set()
    for line in raw:
        m = DEF.match(line)
        if m:
            self_defs.add(m.group(1))
        m = UND.match(line)
        if m:
            self_defs.discard(m.group(1))
    ambient = {k: v for k, v in ambient.items() if k not in self_defs}
    ex = Expr(ambient)
    table = dict(ambient)

    # join continuations: map raw index -> (kw, expr, last_raw_index);
    # cont_lines = raw indices consumed as directive continuations
    dinfo = {}
    cont = set()
    i = 0
    while i < len(raw):
        m = OPEN.match(raw[i])
        if m:
            kw = m.group(1)
            rest = raw[i][m.end():]
            j = i
            while rest.rstrip('\n').endswith('\\'):
                rest = rest.rstrip('\n')[:-1]
                j += 1
                rest += ' ' + raw[j].strip()
                cont.add(j)
            rest = re.sub(r'\s*/\*.*?\*/\s*$', '', rest,
                          flags=re.S).strip()
            dinfo[i] = (kw, rest, j)
            i = j + 1
        else:
            i += 1

    out = []            # final emitted raw lines
    stack = []

    def region_live():
        return all(f.cur_live for f in stack)

    def note_plain(line):
        m = DEF.match(line)
        if m:
            table[m.group(1)] = m.group(3).strip()
        m = UND.match(line)
        if m:
            table.pop(m.group(1), None)

    for i, line in enumerate(raw):
        if i in cont:
            continue
        if i in dinfo:
            kw, rest, j = dinfo[i]
            parent = region_live()
            if kw in ('ifdef', 'ifndef', 'if'):
                if not parent:
                    cond = False
                elif kw == 'ifdef':
                    cond = rest.split()[0] in table
                elif kw == 'ifndef':
                    cond = rest.split()[0] not in table
                else:
                    cond = bool(ex.eval(rest, table))
                f = Frame(kw, rest, i, j, parent)
                f.new_branch(parent and bool(cond))
                stack.append(f)
            elif kw == 'elif':
                f = stack[-1]
                f.simple = False
                if f.parent_live and not f.taken and not f.in_else:
                    live = bool(ex.eval(rest, table))
                    f.taken = live
                else:
                    live = False
                f.new_branch(live, raw[i:j + 1])
            elif kw == 'else':
                f = stack[-1]
                f.simple = False
                f.taken = f.taken or f.cur_live
                f.new_branch(f.parent_live and not f.taken, raw[i:j + 1])
            else:   # endif
                f = stack.pop()
                # ---- emission decision for the whole chain ----
                result = []
                if f.parent_live:
                    if f.simple:
                        live, lines, _ = f.branches[0]
                        if live:
                            result = (raw[f.start:f.opener_end + 1] + lines +
                                      raw[i:i + 1])
                    else:
                        # keep the directives (they re-evaluate correctly
                        # at compile time); keep only the live branch's
                        # content, drop dead branches' content
                        result = list(raw[f.start:f.opener_end + 1])
                        for k, (live, lines, dspan) in enumerate(f.branches):
                            if k > 0 and dspan:
                                result.extend(dspan)
                            if live:
                                result.extend(lines)
                        result.extend(raw[i:i + 1])
                if stack:
                    stack[-1].cur[1].extend(result)
                else:
                    out.extend(result)
            continue
        # plain line (or a directive like include/define/error)
        if region_live():
            stack[-1].cur[1].append(line) if stack else out.append(line)
            note_plain(line)

    if stack:
        raise SystemExit(f"{path}: unterminated #if")
    open(path, 'w').write(''.join(out))


if __name__ == '__main__':
    amb = load_ambient(sys.argv[1])
    for p in sys.argv[2:]:
        flatten(p, amb)
        print("flattened", p)
