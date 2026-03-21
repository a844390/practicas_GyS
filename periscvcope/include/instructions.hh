#pragma once

#include <cstdint>
#include <functional>

#include <memory.hh>
#include <processor.hh>

// extracted from the riscv-spec-20191213.pdf

// *****************************************************************
// Módulo de instrucciones de peRISCVcope
//
// RISC-V RV32I define 6 formatos de instrucción de 32 bits.
// Todos comparten los 7 bits menos significativos como opcode.
// Las clases de este fichero encapsulan la decodificación de cada formato
// usando el método bits(lsb, len) heredado de instruction.
// *****************************************************************

namespace instrs {

// Enumeración de los 6 tipos de instrucción RV32I
enum class type {base, r, i, s, b, u, j};

// ** Clase base
// Almacena el bitstream completo de 32 bits y proporciona el método
// bits() para extraer campos arbitrarios sin desplazamientos manuales
class instruction {
    protected:
        instrs::type _type;
        uint32_t _bitstream; // Los 32 bits crudos de la instrucción
    public:
        // check Figure 2.4 from the manual
        // Extrae 'len' bits a partir de la posición 'lsb' (bit menos significativo)
        // Ejemplo: bits(12, 3) extrae funct3 de los bits [14:12]
        constexpr uint32_t bits(size_t lsb, size_t len) const
        {
            return (_bitstream >> lsb) & ((static_cast<uint32_t>(1) << len)-1);
        }
        instruction(uint32_t bitstream, instrs::type type=type::base) : _type(type),
        _bitstream(bitstream) {}

        constexpr instrs::type type() const { return _type; }
        // El opcode son los 7 bits menos significativos [6:0]
        constexpr uint8_t opcode() const { return bits(0,7); }
};

// ** Tipo R 
// Formato: funct7[31:25] | rs2[24:20] | rs1[19:15] | funct3[14:12] | rd[11:7] | opcode[6:0]
// Usado para operaciones ALU registro-registro: ADD, SUB, MUL, AND...
// funct7 + funct3 determinan la operación exacta
class r_instruction : public instruction {
    public:
        r_instruction(uint32_t bitstream) :
            instruction(bitstream, type::r) {}
        constexpr uint8_t rd() const { return bits(7, 5); } // Registro destino
        constexpr uint8_t funct3() const { return bits(12, 3); } // Selecciona operación
        constexpr uint8_t rs1() const { return bits(15, 5); } // Primer operando
        constexpr uint8_t rs2() const { return bits(20, 5); } // Segundo operando
        constexpr uint32_t funct7() const { return bits(25, 7); } // Distingue ADD, SUB, MUL...
};

// ** Tipo I 
// Formato: imm[31:20] | rs1[19:15] | funct3[14:12] | rd[11:7] | opcode[6:0]
// Usado para: LOAD, operaciones ALU con inmediato (ADDI, ANDI...), JALR
// El inmediato de 12 bits se extiende con signo a 32 bits
class i_instruction : public instruction {
    public:
        i_instruction(uint32_t bitstream) :
            instruction(bitstream, type::i) {}
        constexpr uint8_t rd() const { return bits(7, 5); }
        constexpr uint8_t funct3() const { return bits(12, 3); }
        constexpr uint8_t rs1() const { return bits(15, 5); }
        // Extensión de signo: si el bit 31 es 1, rellena los bits [31:12] con 1
        constexpr uint32_t imm() const { return (bits(31, 1) == 1) ? 0xFFFFF000 | bits(20, 12) : bits(20, 12); }
};

// ** Tipo S 
// Formato: imm[11:5][31:25] | rs2[24:20] | rs1[19:15] | funct3[14:12] | imm[4:0][11:7] | opcode[6:0]
// El inmediato está partido en dos campos para que rs2 siempre esté en la misma posición
// Usado para: SW, SH, SB
class s_instruction : public instruction {
    public:
        s_instruction(uint32_t bitstream) :
            instruction(bitstream, type::s) {}
        // Reconstituye el inmediato uniendo los dos campos: bits[11:5] y bits[4:0]
        uint32_t imm_val = (bits(25, 7) << 5) | (bits(7, 5));
        // Extensión de signo igual que en tipo I
        constexpr uint32_t imm() const { return (bits(31, 1) == 1) ? 0xFFFFF000 | imm_val : imm_val; }
        constexpr uint8_t funct3() const { return bits(12, 3); }
        constexpr uint8_t rs1() const { return bits(15, 5); }
        constexpr uint8_t rs2() const { return bits(20, 5); }
};

// ** Tipo B 
// Formato: imm[12|10:5][31:25] | rs2[24:20] | rs1[19:15] | funct3[14:12] | imm[4:1|11][11:7] | opcode[6:0]
// El inmediato codifica un offset de 13 bits (bit 0 siempre 0, instrucciones alineadas)
// Los bits del inmediato están reordenados para que rs1/rs2 coincidan con tipo S
// Usado para: BEQ, BNE, BLT, BGE...
class b_instruction : public instruction {
    public:
        b_instruction(uint32_t bitstream) :
            instruction(bitstream, type::b) {}
        constexpr uint8_t funct3() const { return bits(12, 3); }
        constexpr uint8_t rs1() const { return bits(15, 5); }
        constexpr uint8_t rs2() const { return bits(20, 5); }
        // Reensambla los bits del offset en orden: [12|11|10:5|4:1] con bit 0 implícito = 0
        constexpr int32_t imm() const {
            uint32_t imm_val = (bits(31, 1) << 12) | (bits(7, 1) << 11)
                             | (bits(25, 6) << 5)  | (bits(8, 4) << 1);
            // Extensión de signo desde el bit 12
            return (imm_val & (1 << 12)) ? (imm_val | 0xFFFFE000) : imm_val;
        }
};

// ** Tipo U 
// Formato: imm[31:12] | rd[11:7] | opcode[6:0]
// El inmediato ocupa los 20 bits superiores; los 12 inferiores quedan a 0
// Usado para: LUI (Load Upper Immediate)
class u_instruction : public instruction {
    public:
        u_instruction(uint32_t bitstream) :
            instruction(bitstream, type::u) {}
        constexpr uint8_t rd() const { return bits(7, 5); }
        // Los 20 bits se desplazan a su posición final [31:12]
        constexpr uint32_t imm() const { return bits(12, 20) << 12; }
};

// ** Tipo J 
// Formato: imm[20|10:1|11|19:12] | rd[11:7] | opcode[6:0]
// El offset de 21 bits tiene sus bits desordenados (diseño para reutilizar decodificadores)
// Bit 0 implícito = 0 (saltos siempre a dirección par)
// Usado para: JAL (rango ±1 MB)
class j_instruction : public instruction {
    public:
        j_instruction(uint32_t bitstream) :
            instruction(bitstream, type::j) {}
        constexpr uint8_t rd()  const { return bits(7, 5); }
        // Reensambla el offset: [20|19:12|11|10:1] con bit 0 implícito = 0
        constexpr int32_t imm() const {
            uint32_t imm_val = (bits(31, 1) << 20) | (bits(12, 8) << 12)
                             | (bits(20, 1) << 11)  | (bits(21, 10) << 1);
            // Extensión de signo desde el bit 20
            return (imm_val & (1 << 20)) ? (imm_val | 0xFFE00000) : imm_val;
        }
        
};

// sign extension
// Se toma un valor de B bits y se extiende a T bits
template <typename T, unsigned B>
inline T sign_extend(const T x)
{
	  struct { T x:B; } s;
	    return s.x = x;
}

//load
template<uint8_t funct3>
void execute_load(mem::memory& mem, processor& proc,
    mem::address_t addr, uint8_t rd);

//store
template<uint8_t funct3>
void execute_store(mem::memory& mem, processor& proc,
    mem::address_t addr, uint8_t rs2);

// ** Tipo función para el dispatch map
using instr_emulation = std::function<uint32_t(mem::memory& mem, processor& proc, uint32_t)>;

uint32_t load(mem::memory& mem, processor& proc, uint32_t bitstream);
uint32_t store(mem::memory& mem, processor& proc, uint32_t bitstream);

// Arithmetic
// Inmediate
uint32_t alui(mem::memory&, processor& proc, uint32_t bitstream);
// Register
uint32_t alur(mem::memory&, processor& proc, uint32_t bitstream);

// Load Upper Immediate
uint32_t lui(mem::memory&, processor& proc, uint32_t bitstream);

// Jump and Link
uint32_t jal(mem::memory&, processor& proc, uint32_t bitstream);

// Jump and Link Register
uint32_t jalr(mem::memory&, processor& proc, uint32_t bitstream);

// Branch
uint32_t condbranch(mem::memory&, processor& proc, uint32_t bitstream);

} // namespace instrs
