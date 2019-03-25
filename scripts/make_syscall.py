import os
import argparse

# C language keywords
# additionally, it contatins __user, which is used at kernel
C_KEYWORDS = ('auto', 'break', 'case', 'char', 'const', 'continue', 'default',
              'do', 'double', 'else', 'enum', 'extern', 'float', 'for', 'goto',
              'if', 'int', 'long', 'register', 'return', 'short', 'signed',
              'sizeof', 'static', 'struct', 'switch', 'typedef', 'union',
              'unsigned', 'void', 'volatile', 'while', '__user')

SYSCALL_HEADER = 'include/linux/syscalls.h'
SYSCALL_TEMPLATE0 = 'asmlinkage long sys_name(void);\n'
SYSCALL_TEMPLATE = 'asmlinkage long sys_name(f_args);\n'

UNISTD_HEADER = 'arch/arm64/include/asm/unistd.h'
UNISTD_TEMPLATE = '__NR_compat_syscalls'

UNISTD32_HEADER = 'arch/arm64/include/asm/unistd32.h'
UNISTD32_TEMPLATE = '#define __NR_name number\n__SYSCALL(__NR_name, sys_name)\n'

MAKEFILE = 'kernel/Makefile'
MAKEFILE_TEMPLATE = 'obj-y += name.o\n'

KERNEL_FILE = 'kernel/name.c'
KERNEL_TEMPLATE0 = """#include <linux/syscalls.h>

SYSCALL_DEFINE0(name) {
  return 0;
}"""

KERNEL_TEMPLATE = """#include <linux/syscalls.h>

SYSCALL_DEFINEn_args(name,f_args) {
  return 0;
}"""


def replace(string: str, **replacements):
    for key, value in replacements.items():
        string = string.replace(key, value)
    return string


def main(args):
    # parse argument
    syscall_name = args.name
    verbose = args.verbose

    f_args = args.args
    if f_args == []:
        f_args = ''
        n_args = 0
    else:
        f_args = ' '.join(f_args)
        comma_split = f_args.split(',')
        n_args = len(comma_split) - 1 if comma_split[-1] == '' else len(
            comma_split)

    if n_args > 6:
        raise RuntimeError('Cannot exceed # of argument as 6')

    if syscall_name == '':
        raise NameError('Must assign system call name')

    with open(replace(KERNEL_FILE, name=syscall_name), 'x') as f:
        if n_args == 0:
            replacement = replace(KERNEL_TEMPLATE0, name=syscall_name)
            f.write(replacement)
        else:
            # for SYSCALL_DEFINE, we need extra handle in argument
            separated_args = []  # comma separated
            split = f_args.replace(',', '').split()
            buf = ''
            is_struct = False
            for i in range(len(split)):
                if split[i] in C_KEYWORDS:
                    # keep joining
                    buf += (' ' + split[i])
                    if split[i] == 'struct':
                        is_struct = True
                else:
                    if is_struct:
                        # not C keywords, but defined struct type
                        buf += (' ' + split[i])
                        is_struct = False
                    else:
                        separated_args.append(buf)
                        buf = ''
                        separated_args.append(' ' + split[i])
            replacement = replace(
                KERNEL_TEMPLATE,
                name=syscall_name,
                n_args=str(n_args),
                f_args=','.join(separated_args))
            f.write(replacement)
        if verbose:
            print('Generated %s' % f.name)
            print(replacement)
            print()

    with open(SYSCALL_HEADER, 'r+') as f:
        contents = f.readlines()

        if f_args == '':
            replacement = replace(SYSCALL_TEMPLATE0, name=syscall_name)
            contents.insert(-2, replacement)
        else:
            replacement = replace(
                SYSCALL_TEMPLATE, name=syscall_name, f_args=f_args)
            contents.insert(-2, replacement)

        f.seek(0)
        f.writelines(contents)
        if verbose:
            print('Inserted %s' % f.name)
            print(replacement)

    with open(UNISTD_HEADER, 'r+') as f:
        contents = f.readlines()
        modified = None
        for i, line in enumerate(contents):
            line = line.strip().split()
            if UNISTD_TEMPLATE in line:
                num_syscall = line[-1]
                modified = i
                contents[i] = '#define %s\t\t%d\n' % (UNISTD_TEMPLATE,
                                                      int(num_syscall) + 1)
        f.seek(0)
        f.writelines(contents)
        if verbose:
            print('Modified %s' % f.name)
            print(contents[modified])

    with open(UNISTD32_HEADER, 'r+') as f:
        contents = f.readlines()
        replacement = replace(
            UNISTD32_TEMPLATE, name=syscall_name, number=num_syscall)
        contents.insert(-5, replacement)
        f.seek(0)
        f.writelines(contents)
        if verbose:
            print('Inserted %s' % f.name)
            print(replacement)

    with open(MAKEFILE, 'r+') as f:
        contents = f.readlines()
        replacement = replace(MAKEFILE_TEMPLATE, name=syscall_name)
        contents.insert(13, replacement)
        f.seek(0)
        f.writelines(contents)
        if verbose:
            print('Inserted %s' % f.name)
            print(replacement)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='information of new syscall')
    parser.add_argument('--name', type=str, help='name of syscall', default='')
    parser.add_argument(
        '--args',
        type=str,
        help='function argument of syscall',
        nargs='+',
        default=[])
    parser.add_argument(
        '--verbose',
        help='print out generated source code',
        action='store_true')
    main(parser.parse_args())
