import os
import argparse

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

SYSCALL_DEFINEn_args(name, f_args) {
  return 0;
}"""


def replace(string: str, **replacements):
    for key, value in replacements.items():
        string = string.replace(key, value)
    return string


def main(args):
    # parse argument
    syscall_name = args.name

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
        print('Cannt exceed # of argument as 6')
        return

    if syscall_name == '':
        print('Must assign system call name')
        return

    with open(replace(KERNEL_FILE, name=syscall_name), 'x') as f:
        if n_args == 0:
            f.write(replace(KERNEL_TEMPLATE0, name=syscall_name))
        else:
            # for SYSCALL_DEFINE, we need extra handle in argument
            separated_args = []  # comma separated
            split = f_args.replace(',', '').split()
            buf = ''
            for i in range(len(split)):
                if split[i] == 'struct':
                    # defer writing
                    buf += split[i]
                else:
                    if buf != '':
                        buf += (' ' + split[i])
                    else:
                        buf += split[i]
                    separated_args.append(buf)
                    buf = ''
            f.write(
                replace(
                    KERNEL_TEMPLATE,
                    name=syscall_name,
                    n_args=str(n_args),
                    f_args=', '.join(separated_args)))

    with open(SYSCALL_HEADER, 'r+') as f:
        contents = f.readlines()

        if f_args == '':
            contents.insert(-2, replace(SYSCALL_TEMPLATE0, name=syscall_name))
        else:
            contents.insert(
                -2, replace(
                    SYSCALL_TEMPLATE, name=syscall_name, f_args=f_args))

        f.seek(0)
        f.writelines(contents)

    with open(UNISTD_HEADER, 'r+') as f:
        contents = f.readlines()
        for i, line in enumerate(contents):
            line = line.strip().split()
            if UNISTD_TEMPLATE in line:
                num_syscall = line[-1]
                contents[i] = '#define %s\t\t%d\n' % (UNISTD_TEMPLATE,
                                                      int(num_syscall) + 1)
        f.seek(0)
        f.writelines(contents)

    with open(UNISTD32_HEADER, 'r+') as f:
        contents = f.readlines()
        contents.insert(
            -5,
            replace(UNISTD32_TEMPLATE, name=syscall_name, number=num_syscall))
        f.seek(0)
        f.writelines(contents)

    with open(MAKEFILE, 'r+') as f:
        contents = f.readlines()
        contents.insert(13, replace(MAKEFILE_TEMPLATE, name=syscall_name))
        f.seek(0)
        f.writelines(contents)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='information of new syscall')
    parser.add_argument('--name', type=str, help='name of syscall', default='')
    parser.add_argument(
        '--args',
        type=str,
        help='function argument of syscall',
        nargs='+',
        default=[])
    main(parser.parse_args())
