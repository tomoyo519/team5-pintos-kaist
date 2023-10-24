cd vm
make clean
make
cd build
source ../../activate
pintos-mkdisk filesys.dsk 10
# pintos --gdb --fs-disk=10 -p tests/userprog/exit:exit -- -q -f run 'exit'
pintos --gdb -v -k -T 600 -m 20   --fs-disk=10 -p tests/vm/pt-write-code2:pt-write-code2 -p ../../tests/vm/sample.txt:sample.txt --swap-disk=4 -- -q   -f run pt-write-code2