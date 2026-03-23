#include <iostream>
#include <map>

#include <instructions.hh>
#include <memory.hh>
#include <processor.hh>

using namespace instrs;
using namespace mem;

// *****************************************************************
// Bucle principal del intérprete
//
// Flujo general:
//   1. Cargar el binario ELF en memoria
//   2. Inicializar el PC al entry point del ELF
//   3. Inicializar el stack pointer al tope de la pila
//   4. Ejecutar el bucle fetch-decode-execute hasta que el programa termine
//
// Condición de terminación:
//   El programa acaba con un salto a sí mismo: j 7c (next_pc == pc).
//   Esto corresponde al while(1) del main del binario emulado.
// *****************************************************************
int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Invalid Syntax: peRISCVcope <program>" << std::endl;
        exit(1);
    }

   memory mem; // Espacio de memoria (ya incluye el segmento de pila vacío)
   processor proc; // Estado del procesador (registros a 0, PC indefinido)

    // Cada entrada mapea los 7 bits del opcode RV32I a la función que
    // fetch-decodifica-ejecuta ese tipo de instrucción.
    std::unordered_map <uint8_t, instr_emulation> dispatch_map = {
        {0b0000011, instrs::load}, // LOAD  (LW, LH, LB, LHU, LBU)
        {0b0100011, instrs::store}, // STORE (SW, SH, SB)
        {0b0010011, instrs::alui}, // ALU inmediato
        {0b0110011, instrs::alur}, // ALU registro
        {0b0110111, instrs::lui}, // LUI
        {0b1101111, instrs::jal}, // JAL
        {0b1100011, instrs::condbranch}, // Branch
        {0b1100111, instrs::jalr} // JALR (incluye RET)
    };

   // Carga del binario ELF
   mem.load_binary(argv[1]);
   // dump_binary(1) imprime el segmento 1 (primer PT_LOAD, el .text)
   // útil para comparar con el output de objdump -d
   mem.dump_binary(1);

   // read the entry point
   // e_entry indica la primera instrucción a ejecutar (main del binario emulado)
   proc.write_pc(mem.entry_point());

   // set the stack pointer
   // La pila crece hacia abajo; SP empieza en el byte más alto del segmento de pila.
   // Debe mantenerse alineado a 16 bytes
   proc.write_reg(processor::sp, memory::stack_top);

   // Variables para el bucle principal
   address_t pc = 0xDEADBEEF, next_pc = 0xDEADBEEF; // Inicializadas a valor inválido

   size_t exec_instrs = 0;
   do
   {
       // FETCH: leer la instrucción de 32 bits en la dirección del PC actual
       pc = proc.read_pc();
       uint32_t instr = mem.read<uint32_t>(pc);

    //    std::cout << "PC: 0x" << std::hex << pc << std::endl;
    //    std::cout << "Instr raw: 0x" << std::hex << instr << std::endl;

    //    uint8_t opcode = instr & 0x7F;
    //    std::cout << "Opcode: " << std::hex << (int)opcode << std::endl;

       std::cout << "Instrucrion read" << std::endl;

       // DECODE + EXECUTE:
       //   1. Extraer el opcode (bits [6:0]) para indexar el dispatch_map
       //   2. Llamar a la función correspondiente, que decodifica el resto
       //      de campos y actualiza el register file / memoria según la instrucción
       //   3. La función devuelve la siguiente dirección de PC
       next_pc = dispatch_map[(instr & 0x7F)](mem, proc, instr);

       // WRITEBACK PC: actualizar el PC con la dirección devuelta
       proc.write_pc(next_pc);
       exec_instrs++;
   } while (next_pc != pc); // Fin: salto a sí mismo (while(1) del programa emulado)

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
