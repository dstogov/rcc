# RCC - Rational C Compiler

RCC is an experimental C front-end to IR framework (https://github.com/dstogov/ir). It’s intended to be a very fast C compiler producing optimized native code of reasonable quality.

It is not finished yet! The work is in progress!

## Building and Playing with RCC

IR Framework and therefore RCC uses libcapstone for disassemble. Please, install it using your package manager.

```
sudo apt-get install libcapstone-dev
sudo dnf install capstone-devel
```
Compile and install IR Framework.

```
git clone https://github.com/dstogov/ir.git
cd ir
make
make test
sudo make install
```
Now compile RCC and test it.
```
git clone https://github.com/dstogov/rcc.git
cd rcc
make
./rcc –help
./rcc test.c -S
./rcc test.c --run
```
