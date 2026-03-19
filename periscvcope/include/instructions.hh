#pragma once

#include <cstdint>
#include <functional>

#include <memory.hh>
#include <processor.hh>

// extracted from the riscv-spec-20191213.pdf

namespace instrs {

enum class type {base, r, i, s, b, u, j};

class instruction {
    protected:
        instrs::type _type;
        uint32_t _bitstream;
    public:
        // check Figure 2.4 from the manual
        constexpr uint32_t bits(size_t lsb, size_t len) const
        {
            return (_bitstream >> lsb) & ((static_cast<uint32_t>(1) << len)-1);
        }
        instruction(uint32_t bitstream, instrs::type type=type::base) : _type(type),
        _bitstream(bitstream) {}

        constexpr instrs::type type() const { return _type; }
        constexpr uint8_t opcode() const { return bits(0,7); }
};

class r_instruction : public instruction {
    public:
        r_instruction(uint32_t bitstream) :
            instruction(bitstream, type::r) {}
        constexpr uint8_t rd() const { return bits(7, 5); }
        constexpr uint8_t funct3() const { return bits(12, 3); }
        constexpr uint8_t rs1() const { return bits(15, 5); }
        constexpr uint8_t rs2() const { return bits(20, 5); }
        constexpr uint32_t funct7() const { return bits(25, 7); }
};

class i_instruction : public instruction {
    public:
        i_instruction(uint32_t bitstream) :
            instruction(bitstream, type::i) {}
        constexpr uint8_t rd() const { return bits(7, 5); }
        constexpr uint8_t funct3() const { return bits(12, 3); }
        constexpr uint8_t rs1() const { return bits(15, 5); }
        constexpr uint32_t imm() const { return (bits(31, 1) == 1) ? 0xFFFFF000 | bits(20, 12) : bits(20, 12); }
};

class s_instruction : public instruction {
    public:
        s_instruction(uint32_t bitstream) :
            instruction(bitstream, type::s) {}
        uint32_t imm_val = (bits(25, 7) << 5) | (bits(7, 5));
        constexpr uint32_t imm() const { return (bits(31, 1) == 1) ? 0xFFFFF000 | imm_val : imm_val; }
        constexpr uint8_t funct3() const { return bits(12, 3); }
        constexpr uint8_t rs1() const { return bits(15, 5); }
        constexpr uint8_t rs2() const { return bits(20, 5); }
};

class b_instruction : public instruction {
    public:
        b_instruction(uint32_t bitstream) :
            instruction(bitstream, type::b) {}
        constexpr uint8_t funct3() const { return bits(12, 3); }
        constexpr uint8_t rs1() const { return bits(15, 5); }
        constexpr uint8_t rs2() const { return bits(20, 5); }
        constexpr int32_t imm() const {
            uint32_t imm_val = (bits(31, 1) << 12) | (bits(7, 1) << 11) | (bits(25, 6) << 5) | (bits(8, 4) << 1);
            // Si el bit de signo (bit 12) está activado, OR con la máscara para extender el signo
            return (imm_val & (1 << 12)) ? (imm_val | 0xFFFFE000) : imm_val;
        }
};

class u_instruction : public instruction {
    public:
        u_instruction(uint32_t bitstream) :
            instruction(bitstream, type::u) {}
        constexpr uint8_t rd() const { return bits(7, 5); }
        constexpr uint32_t imm() const { return bits(12, 20) << 12; }
};

class j_instruction : public instruction {
    public:
        j_instruction(uint32_t bitstream) :
            instruction(bitstream, type::j) {}
        constexpr uint8_t rd() const { return bits(7, 5); }
        constexpr int32_t imm() const {
            uint32_t imm_val = (bits(31, 1) << 20) | (bits(12, 8) << 12) | (bits(20, 1) << 11) | (bits(21, 10) << 1);
            // Si el bit de signo (bit 20) está activado, OR con la máscara para extender el signo
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
