#include <iostream>
#include <map>

#include <instructions.hh>
#include <memory.hh>
#include <processor.hh>

using namespace instrs;
using namespace mem;

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Invalid Syntax: peRISCVcope <program>" << std::endl;
        exit(1);
    }

   memory mem;
   processor proc;

    std::map <uint8_t, instr_emulation> dispatch_map = {
        {0b0000011, instrs::load},
        {0b0100011, instrs::store},
        {0b0010011, instrs::alui},
        {0b0110011, instrs::alur},
        {0b0110111, instrs::lui},
        {0b1101111, instrs::jal},
        {0b1100011, instrs::condbranch},
        {0b1100111, instrs::jalr}
    };

   mem.load_binary(argv[1]);
   mem.dump_binary(1);

   // read the entry point
   // ...

   proc.write_pc(mem.entry_point());

   // set the stack pointer
   // the stack grows downward with the stack pointer always being 16-byte aligned
   proc.write_reg(processor::sp, memory::stack_top);

   address_t pc = 0xDEADBEEF, next_pc = 0xDEADBEEF;

   size_t exec_instrs = 0;

    // Inicialize sp
    proc.write_reg(2, memory::stack_top);
   do
   {
       // main interpreter loop
       // ...
        pc = proc.read_pc();
       uint32_t instr = mem.read<uint32_t>(pc);

       std::cout << "Instrucrion read" << std::endl;

       next_pc = dispatch_map[(instr & 0x7F)](mem, proc, instr);
       proc.write_pc(next_pc);
       exec_instrs++;
   } while (next_pc != pc); // look for while(1) in the code

    std::cout << "Number of executed instructions: " << exec_instrs << std::endl;

  // Mostrar el resultado de la función factorial (registro a0)
   uint32_t a0_result = proc.read_reg(10); // Leer el registro a0 (registro 10)
   std::cout << "Result in a0 (factorial): " << std::dec << a0_result << std::endl;

   // Mostrar el resultado de la suma del arreglo (pila s0 - 20)
   uint32_t s0 = proc.read_reg(8); // Leer el registro s0 (registro 8)
   address_t result_addr = s0 - 20; // Dirección de la variable local que almacena el resultado
   uint32_t stack_result = mem.read<uint32_t>(result_addr); // Leer el valor de la pila
   std::cout << "Result on stack (add_array): " << std::dec << stack_result << std::endl;
}
