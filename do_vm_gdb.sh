# cd vm
# make clean
# make
# cd build
# source ../../activate
# pintos-mkdisk filesys.dsk 10
# pintos --gdb --fs-disk=10 -p tests/userprog/exit:exit -- -q -f run 'exit'
# pintos --gdb --fs-disk=10 -p tests/userprog/fork-once:fork-once -- -q -f run 'fork-once'
FORK_ONCE="pintos --gdb --fs-disk=10 -p tests/userprog/fork-once:fork-once --swap-disk=4 -- -q   -f run fork-once"
ARGS_NONE="pintos --gdb --fs-disk=10 -p tests/userprog/args-none:args-none --swap-disk=4 -- -q   -f run args-none"
STACK_GROW="pintos --gdb --fs-disk=10 -p tests/vm/pt-grow-stack:pt-grow-stack --swap-disk=4 -- -q   -f run pt-grow-stack"
MMAP_READ="pintos --gdb --fs-disk=10 -p tests/vm/mmap-read:mmap-read -p ../../tests/vm/sample.txt:sample.txt --swap-disk=4 -- -q   -f run mmap-read"
MMAP_CLOSE="pintos --gdb  --fs-disk=10 -p tests/vm/mmap-close:mmap-close -p ../../tests/vm/sample.txt:sample.txt --swap-disk=4 -- -q   -f run mmap-close"
SWAP_FILE="pintos -- gdb --fs-disk=10 -p tests/vm/swap-file:swap-file -p ../../tests/vm/large.txt:large.txt --swap-disk=10 -- -q   -f run swap-file"
SWAP_ANON="pintos -- gdb --fs-disk=10 -p tests/vm/swap-anon:swap-anon --swap-disk=30 -- -q   -f run swap-anon"
SWAP_ITER="pintos -- gdb --fs-disk=10 -p tests/vm/swap-iter:swap-iter -p ../../tests/vm/large.txt:large.txt --swap-disk=50 -- -q   -f run swap-iter"
SWAP_FORK="pintos -- gdb --fs-disk=10 -p tests/vm/swap-fork:swap-fork -p tests/vm/child-swap:child-swap --swap-disk=200 -- -q   -f run swap-fork"
cd vm
make clean
make
cd build
source ../../activate

$SWAP_FILE