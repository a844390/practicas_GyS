#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <cstddef>

// *****************************************************************
// Estado arquitectónico del procesador RISC-V RV32I
//
// Modela el estado visible del procesador:
//   - Register file: 32 registros de 32 bits (x0..x31)
//   - PC: program counter de 32 bits
//
// Convenciones ABI RV32I relevantes:
//   x0  (zero): siempre 0, escrituras ignoradas
//   x1  (ra): return address
//   x2  (sp): stack pointer (inicializado a stack_top)
//   x8  (s0/fp): frame pointer
//   x10 (a0): primer argumento / valor de retorno
// *****************************************************************

class processor {


  private:
    constexpr static size_t num_regs = 32; // exactamente 32 registros
    std::array<uint32_t, num_regs> _reg_file; // Los 32 registros de propósito general
    uint32_t _pc; // Program Counter

  public:
    // Alias para el stack pointer (x2) usado en main.cc al inicializar la pila
    constexpr static size_t sp = 2;

    // Constructor: pone todos los registros a 0 (incluyendo x0, que siempre será 0)
    processor()
    {
    for(auto& e: _reg_file) {
      e = 0;
    }
    }

    // FIXME use contracts C++20
    // devuelve el valor del registro i
    constexpr uint32_t read_reg(size_t i) const { assert(i<32); return _reg_file[i]; }
    // escribe val en el registro i, EXCEPTO si i==0 
    void write_reg(size_t i, uint32_t val) { assert(i<32); if(i>0) { _reg_file[i] = val; } }

    // devuelve el PC actual
    constexpr uint32_t read_pc() const { return _pc; }
    // avanza el PC en 4 bytes y devuelve el nuevo valor.
    uint32_t next_pc() { _pc+=4; return _pc; }
    // establece el PC a un valor arbitrario
    void write_pc(uint32_t val) { _pc = val; };
};
