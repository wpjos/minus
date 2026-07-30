#!/usr/bin/env python3
"""Minimal Kconfig front-end for the Minus kernel.

Supports a small subset of Kconfig syntax sufficient for early
configuration:

  source "file"
  config SYMBOL
      bool | hex | int | string "prompt"
      default VALUE [if SYMBOL]
  if SYMBOL ... endif
  # comments

Subcommands:
  defconfig  -- generate .config from a defconfig file
  oldconfig  -- re-resolve .config against Kconfig defaults
  syncconfig -- alias for oldconfig (regenerates autoconf.h)
"""

import argparse
import os
import re
import sys


def strip_comment(line):
    """Remove a trailing '# comment' from a line."""
    # Kconfig strings can contain '#', but we don't use quoted strings
    # in the parts we care about, so a simple rsplit is enough.
    if '#' in line:
        return line[:line.index('#')]
    return line


def tokenize(path):
    """Return a list of (type, value) tokens for a Kconfig file."""
    tokens = []
    with open(path, 'r') as f:
        for raw in f:
            s = strip_comment(raw.rstrip('\n')).strip()
            if not s:
                continue
            parts = s.split(None, 1)
            first = parts[0]
            if first == 'config':
                tokens.append(('CONFIG', parts[1].strip()))
            elif first == 'if':
                tokens.append(('IF', parts[1].strip() if len(parts) > 1 else ''))
            elif first == 'endif':
                tokens.append(('ENDIF', None))
            elif first == 'source':
                src = parts[1].strip().strip('"') if len(parts) > 1 else ''
                tokens.append(('SOURCE', src))
            else:
                tokens.append(('ATTR', s))
    return tokens


def combine_conditions(base, extra):
    """Combine two boolean conditions with '&&'."""
    if not base:
        return extra
    if not extra:
        return base
    return f"({base}) && ({extra})"


def _strip_config_prefix(sym):
    """Allow both PLAT and CONFIG_PLAT to refer to the same symbol."""
    if sym.startswith('CONFIG_'):
        return sym[len('CONFIG_'):]
    return sym


def eval_condition(expr, values):
    """Evaluate a simple condition made of SYMBOL, !SYMBOL, &&, and =/!=."""
    if not expr:
        return True
    # Normalize spaces and parentheses
    expr = expr.replace('(', ' ').replace(')', ' ')
    for part in expr.split('&&'):
        part = part.strip()
        if not part:
            continue

        # String equality / inequality: SYM = "val" or SYM != val
        m = re.match(r'^(\w+)\s*(!?=)\s*"?([^"]*)"?$', part)
        if m:
            sym, op, expected = m.groups()
            sym = _strip_config_prefix(sym)
            actual = values.get(sym, '')
            if actual.startswith('"') and actual.endswith('"'):
                actual = actual[1:-1]
            match = (actual == expected)
            if op == '!=':
                match = not match
            if not match:
                return False
            continue

        # Boolean: SYMBOL or !SYMBOL
        neg = part.startswith('!')
        if neg:
            part = part[1:].strip()
        part = _strip_config_prefix(part)
        val = values.get(part, 'n')
        active = val not in ('n', '0', '', 'false', 'no')
        if neg:
            active = not active
        if not active:
            return False
    return True


def parse(tokens, configs, condition='', base_dir='.'):
    """Parse tokens and fill the configs dict."""
    i = 0
    while i < len(tokens):
        tok = tokens[i]
        ttype = tok[0]

        if ttype == 'CONFIG':
            name = tok[1]
            if name not in configs:
                configs[name] = {'type': None, 'prompt': None, 'defaults': []}
            cfg = configs[name]
            i += 1
            while i < len(tokens) and tokens[i][0] == 'ATTR':
                attr = tokens[i][1]
                parts = attr.split(None, 1)
                key = parts[0]
                rest = parts[1] if len(parts) > 1 else ''
                if key in ('bool', 'hex', 'int', 'string'):
                    cfg['type'] = key
                    cfg['prompt'] = rest.strip().strip('"')
                elif key == 'default':
                    m = re.match(r'(\S+)(?:\s+if\s+(.+))?$', rest)
                    if m:
                        val, cond = m.groups()
                        # Strip surrounding quotes from string literals so that
                        # .config uses the unquoted form (CONFIG_PLAT=raspi).
                        if len(val) >= 2 and val.startswith('"') and val.endswith('"'):
                            val = val[1:-1]
                        full = combine_conditions(condition, cond)
                        cfg['defaults'].append((val, full))
                i += 1

        elif ttype == 'IF':
            new_cond = combine_conditions(condition, tok[1])
            i += 1
            block = []
            depth = 1
            while i < len(tokens) and depth > 0:
                t = tokens[i]
                if t[0] == 'IF':
                    depth += 1
                elif t[0] == 'ENDIF':
                    depth -= 1
                if depth > 0:
                    block.append(t)
                i += 1
            parse(block, configs, new_cond, base_dir)

        elif ttype == 'SOURCE':
            path = tok[1]
            if not os.path.isabs(path):
                path = os.path.join(base_dir, path)
            sub_tokens = tokenize(path)
            parse(sub_tokens, configs, condition, os.path.dirname(path) or '.')
            i += 1

        else:
            i += 1


def resolve(kconfig_path, defconfig_path):
    """Resolve Kconfig defaults merged with a defconfig."""
    configs = {}
    tokens = tokenize(kconfig_path)
    parse(tokens, configs, '', os.path.dirname(kconfig_path) or '.')

    values = {}
    if defconfig_path and os.path.exists(defconfig_path):
        with open(defconfig_path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                m = re.match(r'CONFIG_(\w+)=(.+)$', line)
                if m:
                    values[m.group(1)] = m.group(2)

    # Apply defaults for any symbol not already set.
    for name, cfg in configs.items():
        if name in values:
            continue
        for val, cond in cfg['defaults']:
            if eval_condition(cond, values):
                values[name] = val
                break

    return configs, values


def write_config(configs, values, path):
    """Write a Linux-style .config file."""
    os.makedirs(os.path.dirname(path) if os.path.dirname(path) else '.', exist_ok=True)
    with open(path, 'w') as f:
        for name in configs:
            if name in values:
                f.write(f'CONFIG_{name}={values[name]}\n')
            else:
                f.write(f'# CONFIG_{name} is not set\n')


def write_autoconf(configs, config_path, header_path):
    """Generate include/generated/autoconf.h from .config."""
    os.makedirs(os.path.dirname(header_path), exist_ok=True)
    with open(config_path, 'r') as f:
        lines = f.readlines()

    with open(header_path, 'w') as f:
        f.write('/* Automatically generated by scripts/kconfig.py; do not edit */\n')
        f.write('#ifndef __MINUS_AUTOCONF_H__\n')
        f.write('#define __MINUS_AUTOCONF_H__\n\n')
        for line in lines:
            line = line.strip()
            if not line:
                continue
            if line.startswith('# CONFIG_') and 'is not set' in line:
                name = line[len('# CONFIG_'):].split()[0]
                f.write(f'/* CONFIG_{name} is not set */\n')
            elif line.startswith('CONFIG_'):
                name, val = line[len('CONFIG_'):].split('=', 1)
                cfg_type = configs.get(name, {}).get('type')
                if cfg_type == 'string':
                    f.write(f'#define CONFIG_{name} "{val}"\n')
                elif val == 'y':
                    f.write(f'#define CONFIG_{name} 1\n')
                elif val == 'n':
                    f.write(f'/* CONFIG_{name} is not set */\n')
                elif re.match(r'0[xX][0-9a-fA-F]+$', val):
                    f.write(f'#define CONFIG_{name} {val}ULL\n')
                else:
                    f.write(f'#define CONFIG_{name} {val}\n')
        f.write('\n#endif /* __MINUS_AUTOCONF_H__ */\n')


def main():
    ap = argparse.ArgumentParser(description='Minimal Kconfig front-end')
    ap.add_argument('command', choices=['defconfig', 'oldconfig', 'syncconfig'])
    ap.add_argument('--kconfig', default='Kconfig')
    ap.add_argument('--defconfig', default=None)
    ap.add_argument('--config', default='.config')
    ap.add_argument('--autoconf', default='include/generated/autoconf.h')
    args = ap.parse_args()

    if args.command == 'defconfig':
        configs, values = resolve(args.kconfig, args.defconfig)
        write_config(configs, values, args.config)
        write_autoconf(configs, args.config, args.autoconf)
        print(f'Generated {args.config} and {args.autoconf}')
    elif args.command in ('oldconfig', 'syncconfig'):
        if not os.path.exists(args.config):
            print(f'{args.config} not found, running defconfig', file=sys.stderr)
            configs, values = resolve(args.kconfig, args.defconfig)
        else:
            configs, values = resolve(args.kconfig, args.config)
        write_config(configs, values, args.config)
        write_autoconf(configs, args.config, args.autoconf)
        print(f'Generated {args.config} and {args.autoconf}')


if __name__ == '__main__':
    main()
