cd vm
make clean
make
cd build
source ../../activate
pintos-mkdisk filesys.dsk 10
# pintos --gdb --fs-disk=10 -p tests/userprog/exit:exit -- -q -f run 'exit'
pintos --gdb --fs-disk=10 -p tests/vm/pt-grow-stack:pt-grow-stack --swap-disk=4 -- -q   -f run pt-grow-stack