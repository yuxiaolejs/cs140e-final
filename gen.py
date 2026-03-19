import os

USE_BIN_OUTPUT = True

GCC="tcc"

abs_path="/dmode"

allowed_source_dir=["final", "lib"]

black_list = ["final/gpio.c", "final/irq.c", "final/ptable.c", "final/debugger_lab.c"]

CXX_FLAGS=f"-I{abs_path}/final/include -I{abs_path}/lib/include -I{abs_path}/gnu -nostdlib -static  -DUSE_PHYSICAL_TTY=1" # + " -DDISABLE_PRINTK=1"

c_files = []
for root, dirs, files in os.walk("."):
    for file in files:
        if file.endswith(".c") and any(root.startswith(f"./{d}") for d in allowed_source_dir) and not any(os.path.join(root, file)[2:].endswith(bl) for bl in black_list):
            c_files.append(os.path.join(root, file))

S_files = []
for root, dirs, files in os.walk("."):
    for file in files:
        if file.endswith(".S") and any(root.startswith(f"./{d}") for d in allowed_source_dir) and not any(os.path.join(root, file)[2:].endswith(bl) for bl in black_list):
            S_files.append(os.path.join(root, file))
# S_files = []

linkable_objs = []
num_files = len(c_files) + len(S_files)
with open("make.sh", "w") as f:
    f.write("#!/bin/bash\n")
    f.write("set -euo pipefail\n")
    count = 0
    for c_file in c_files:
        count += 1
        f.write(f"{GCC} {CXX_FLAGS} -c {abs_path}/{c_file[2:]} -o {abs_path}/{c_file[2:-2]}.o\n")
        f.write(f"echo Compiled {c_file} {count}/{num_files}\n")
        linkable_objs.append(f"{abs_path}/{c_file[2:-2]}.o")
    for S_file in S_files:
        count += 1
        f.write(f"{GCC} {CXX_FLAGS} -c {abs_path}/{S_file[2:]} -o {abs_path}/{S_file[2:-2]}.o\n")
        f.write(f"echo Compiled {S_file} {count}/{num_files}\n")
        linkable_objs.append(f"{abs_path}/{S_file[2:-2]}.o")
        
    # link
    f.write(f"{GCC} {CXX_FLAGS}  -nostdlib -static -Wl,-Ttext=c0008000 -Wl,-e=_start -Wl,-section-alignment=4 {'-Wl,-oformat=binary' if USE_BIN_OUTPUT else ''} {' '.join(linkable_objs)} -o {abs_path}/kernel.elf\n")
    
    # /sh.elf /dmode/make.sh
