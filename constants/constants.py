#!/usr/bin/env python3

import argparse
import yaml

# process command-line arguments

parser = argparse.ArgumentParser(description='Generate memory map')
parser.add_argument('--lang', type=str, help='Choose language: c, asm or asm_c', choices=['c', 'asm', 'asm_c'], required=True)
args = parser.parse_args()

# load yaml

initial_pos = 0x10000
with open('constants.yaml', 'r') as f:
    constants = yaml.safe_load(f)

# process data

pos = initial_pos
calculated = []
str_sz = 0
for df in reversed(constants):
    if 'name' in df:
        if 'size' in df:
            df['addr'] = pos - df['size']
            pos -= df['size']
        str_sz = max(len(df['name']), str_sz)
str_sz += 1

# create file

if args.lang == 'c':
    print('#ifndef MEMORY_MAP_H_')
    print('#define MEMORY_MAP_H_')
elif args.lang == 'asm_c':
    print('#ifndef RETROLAB_DEF_H_')
    print('#define RETROLAB_DEF_H_')
    print('const char* retrolab_def = ')

for df in constants:
    if args.lang == 'c':
        if 'name' in df:
            print('#define ' + df['name'].ljust(str_sz, ' ') + ' ' + hex(df['addr']).upper() + '  ', end='')
            if 'comments' in df:
                for i, cm in enumerate(df['comments']):
                    if i == 0:
                        print('// ' + cm)
                    else:
                        print(' ' * (8 + str_sz + 9), end='')
                        print('//   ' + cm)
            else:
                print()
        elif 'comments' in df:
            print()
            for cm in df['comments']:
                print('// ' + cm)
            print()
    elif args.lang == 'asm':
        if 'name' in df:
            print(df['name'].ljust(str_sz, ' ') + '= ' + hex(df['addr']).upper() + '  ', end='')
            if 'comments' in df:
                for i, cm in enumerate(df['comments']):
                    if i == 0:
                        print('; ' + cm)
                    else:
                        print(' ' * (str_sz + 10), end='')
                        print(';   ' + cm)
            else:
                print()
        elif 'comments' in df:
            print()
            for cm in df['comments']:
                print('; ' + cm)
            print()
    elif args.lang == 'asm_c':
        if 'name' in df:
            print('"' + df['name'].ljust(str_sz, ' ') + '= ' + hex(df['addr']).upper() + '\\n"')

if args.lang == 'c':
    print('#endif')
elif args.lang == 'asm':
    print()
elif args.lang == 'asm_c':
    print(';')
    print('#endif')
