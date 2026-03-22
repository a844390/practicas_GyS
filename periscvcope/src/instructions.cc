#include <cstdint>
#include <instructions.hh>
#include <memory.hh>

using namespace instrs;
using namespace mem;

// *****************************************************************
// INSTRUCCIONES DE CARGA (LOAD)
// La dirección efectiva se calcula como: rs1 + imm (con signo)
// El campo funct3 determina el tamaño y si hay extensión de signo
// *****************************************************************

// assume little endian
// LW (Load Word) - func3 = 0b010
// Lee 4 bytes de memoria y los escribe en rd sin modificar
template<>
void instrs::execute_load<0b010>(mem::memory& mem, processor & proc,
    mem::address_t addr, uint8_t rd)
{
  // La dirección debe estar alineada a 4 bytes (los 2 bits menos significativos deben ser 0)
  assert((addr & 0b11) == 0);
  proc.write_reg(rd, mem.read<uint32_t>(addr));
}

// LBU (Load Byte Unsigned) — funct3 = 0b100
// Lee 1 byte y lo escribe en rd sin extensión de signo (rellena con ceros por arriba)
template<>
void instrs::execute_load<0b100>(mem::memory& mem, processor & proc,
    mem::address_t addr, uint8_t rd)
{
  // No hay extensión de signo: se hace cast directo a uint32_t
  uint32_t val = mem.read<uint8_t>(addr);
  proc.write_reg(rd, val);
}

// LB (Load Byte Signed) — funct3 = 0b000
// Lee 1 byte y lo escribe en rd con extensión de signo a 32 bits
template<>
void instrs::execute_load<0b000>(mem::memory& mem, processor & proc,
    mem::address_t addr, uint8_t rd)
{
  // sign_extend<int32_t, 8> replica el bit 7 en los bits 8..31
  uint32_t val = static_cast<uint32_t>(sign_extend<int32_t, 
		  sizeof(uint8_t)*8>(mem.read<uint8_t>(addr)));
  proc.write_reg(rd, val);
}

// LH (Load Halfword Signed) — funct3 = 0b001
// Lee 2 bytes y los escribe en rd con extensión de signo a 32 bits
template<>
void instrs::execute_load<0b001>(mem::memory& mem, processor & proc,
    mem::address_t addr, uint8_t rd)
{
  // La dirección debe estar alineada a 2 bytes
  assert((addr & 0b1) == 0);
  // sign_extend<int32_t, 16> replica el bit 15 en los bits 16..31
  uint32_t val = static_cast<uint32_t>(sign_extend<int32_t,
           sizeof(uint16_t)*8>(mem.read<uint16_t>(addr)));
  proc.write_reg(rd, val);
}

// LHU (Load Halfword Unsigned) — funct3 = 0b101
// Lee 2 bytes y los escribe en rd sin extensión de signo
template<>
void instrs::execute_load<0b101>(mem::memory& mem, processor & proc,
    mem::address_t addr, uint8_t rd)
{
  assert((addr & 0b1) == 0);
  // Cast directo a uint32_t: los 16 bits superiores quedan a 0
  uint32_t val = mem.read<uint16_t>(addr);
  proc.write_reg(rd, val);
}

// Dispatcher de LOAD: decodifica la instrucción tipo I y llama
// a la especialización de plantilla correcta según funct3
uint32_t instrs::load(memory& mem, processor & proc, uint32_t bitstream) {
  i_instruction ii{bitstream};

  // Dirección efectiva = valor de rs1 + inmediato con signo
  address_t src = proc.read_reg(ii.rs1()) + ii.imm();

  switch(ii.funct3()) {
    case 0b010: execute_load<0b010>(mem, proc, src, ii.rd()); break; // LW
    case 0b000: execute_load<0b000>(mem, proc, src, ii.rd()); break; // LB
    case 0b001: execute_load<0b001>(mem, proc, src, ii.rd()); break; // LH
    case 0b100: execute_load<0b100>(mem, proc, src, ii.rd()); break; // LBU
    case 0b101: execute_load<0b101>(mem, proc, src, ii.rd()); break; // LHU
  }
  // Devuelve PC+4 (siguiente instrucción secuencial)
  return proc.next_pc();
}

// *****************************************************************
// INSTRUCCIONES DE ALMACENAMIENTO (STORE)
// La dirección efectiva se calcula como: rs1 + imm (con signo)
// El valor a escribir siempre viene de rs2
// *****************************************************************

// SB (Store Byte) — funct3 = 0b000
// Escribe el byte menos significativo de rs2 en memoria
template<>
void instrs::execute_store<0b000>(mem::memory& mem, processor& proc,
    mem::address_t addr, uint8_t rs2)
{
  // Truncamos a 8 bits con el cast a uint8_t
  mem.write<uint8_t>(addr, static_cast<uint8_t>(proc.read_reg(rs2)));
}

// SH (Store Halfword) — funct3 = 0b001
// Escribe los 2 bytes menos significativos de rs2 en memoria
template<>
void instrs::execute_store<0b001>(mem::memory& mem, processor& proc,
    mem::address_t addr, uint8_t rs2)
{
  assert((addr & 0b1) == 0); // Requiere alineación a 2 bytes
  mem.write<uint16_t>(addr, static_cast<uint16_t>(proc.read_reg(rs2)));
}

// SW (Store Word) — funct3 = 0b010
// Escribe los 4 bytes de rs2 en memoria
template<>
void instrs::execute_store<0b010>(mem::memory& mem, processor& proc,
    mem::address_t addr, uint8_t rs2)
{
  assert((addr & 0b11) == 0); // Requiere alineación a 4 bytes
  mem.write<uint32_t>(addr, proc.read_reg(rs2));
}

// Dispatcher de STORE: decodifica la instrucción tipo S y llama
// a la especialización correcta según funct3
uint32_t instrs::store(memory& mem, processor & proc, uint32_t bitstream) {
  s_instruction si{bitstream};

  // Dirección efectiva = rs1 + imm (el inmediato en tipo S está partido en dos campos)
  address_t src = proc.read_reg(si.rs1()) + si.imm();

  switch(si.funct3()) {
    case 0b000: execute_store<0b000>(mem, proc, src, si.rs2()); break; // SB
    case 0b001: execute_store<0b001>(mem, proc, src, si.rs2()); break; // SH
    case 0b010: execute_store<0b010>(mem, proc, src, si.rs2()); break; // SW
  }
  return proc.next_pc();
}

// *****************************************************************
// ALU CON INMEDIATO (ALUI)
// Opera entre el valor de rs1 y un inmediato con signo
// *****************************************************************
uint32_t instrs::alui(mem::memory &, processor &proc, uint32_t bitstream) {
  i_instruction ii(bitstream);
  uint32_t val = proc.read_reg(ii.rs1()); // Operando fuente

  switch (ii.funct3()) {
    case 0b000: // ADDI -> suma rs1 + imm con signo
      proc.write_reg(ii.rd(), val + ii.imm());
      break;
    case 0b001: // SLLI -> desplazamiento lógico a la izquierda
      // Solo los 5 bits bajos del inmediato indican el número de posiciones (0-31)
      proc.write_reg(ii.rd(), val << (ii.imm() & 0x1F));
      break;
    case 0b111: // ANDI -> AND bit a bit con inmediato
      // Usado por soft_mul para aislar el bit menos significativo: andi a5,a5,1
      proc.write_reg(ii.rd(), val & ii.imm());
      break;
    case 0b101: // SRLI o SRAI -> desplazamiento lógico o aritmético a la derecha 
      // El bit 10 del inmediato distingue las dos variantes:
      //   0 -> SRLI: rellena con ceros (lógico)
      //   1 -> SRAI: replica el bit de signo (aritmético)
      if ((ii.imm() >> 10) & 1) {
        // SRAI: cast a int32_t para que el desplazamiento sea aritmético
        proc.write_reg(ii.rd(), (int32_t)val >> (ii.imm() & 0x1F));
      } else {
        // SRLI: desplazamiento lógico (rellena con 0s)
        proc.write_reg(ii.rd(), val >> (ii.imm() & 0x1F));
      }
      break;
  }
  return proc.next_pc();
}

// *****************************************************************
// ALU ENTRE REGISTROS (ALUR)
// funct3 selecciona la operación; funct7 distingue variantes (ADD, SUB, MUL)
// *****************************************************************
uint32_t instrs::alur(mem::memory &, processor &proc, uint32_t bitstream) {
  r_instruction ri(bitstream);

  uint32_t val1 = proc.read_reg(ri.rs1());
  uint32_t val2 = proc.read_reg(ri.rs2());

  switch (ri.funct3()) {
    case 0b000: // ADD o MUL
      if (ri.funct7() == 0b0000000) {
        // ADD: suma sin signo
        proc.write_reg(ri.rd(), val1 + val2);
      } else if (ri.funct7() == 0b0000001) {
        // MUL: multiplicación
        proc.write_reg(ri.rd(), val1 * val2);
      } else if (ri.funct7() == 0b0100000) {
        // SUB: resta. También cubre NEG cuando rs1 == x0
        proc.write_reg(ri.rd(), val1 - val2);
      }
      break;
  }
  return proc.next_pc();
}

// *****************************************************************
// LUI (Load Upper Immediate) opcode = (0110111)
// Carga los 20 bits superiores del inmediato en rd; los 12 bits bajos quedan a 0
// Usado para construir constantes de 32 bits junto con ADDI
// *****************************************************************
uint32_t instrs::lui(mem::memory &, processor &proc, uint32_t bitstream) {
  u_instruction ui(bitstream);

  // ui.imm() ya devuelve el valor desplazado 12 posiciones a la izquierda
  proc.write_reg(ui.rd(), ui.imm());
  return proc.next_pc();
}

// *****************************************************************
// JAL (Jump And Link) opcode = (1101111)
// Salto incondicional con offset relativo al PC actual
// Guarda la dirección de retorno (PC+4) en rd
// Usado para llamadas a función: jal ra, <funcion>
// *****************************************************************
uint32_t instrs::jal(mem::memory &, processor &proc, uint32_t bitstream) {
  j_instruction ji(bitstream);

  uint32_t current_pc = proc.read_pc(); //PC actual
  // Guarda PC+4 en rd para poder volver después
  proc.write_reg(ji.rd(), current_pc + 4);

  // El destino del salto es PC + offset con signo
  // El offset siempre tiene el bit 0 a 0 (instrucciones alineadas a 2 bytes)
  address_t addr = current_pc + ji.imm();

  // std::cout << "JAL from " << current_pc 
  //         << " to " << (addr) << std::endl;

  return addr;
}

// *****************************************************************
// JALR (Jump And Link Register) opcode = (1100111)
// Salto incondicional a una dirección absoluta: (rs1 + imm) & ~1
// Guarda la dirección de retorno (PC+4) en rd
// Usado para: retorno de función (ret = jalr x0, ra, 0)
//             llamada a función a través de puntero
// *****************************************************************
uint32_t instrs::jalr(mem::memory &, processor &proc, uint32_t bitstream) {
  i_instruction ii(bitstream);

  uint32_t current_pc = proc.read_pc();
  // Guarda PC+4 en rd (si rd == x0 simplemente se descarta, como en ret)
  proc.write_reg(ii.rd(), current_pc + 4);

    // Destino = (rs1 + imm) con el bit 0 forzado a 0 (alineación a 2 bytes)
  // Esto permite que el valor base en rs1 sea impar sin problema
  address_t addr = (proc.read_reg(ii.rs1()) + ii.imm()) & ~1;

  // std::cout << "JALR return to " << addr << std::endl;
  // std::cout << "JALR: rs1=" << (int)ii.rs1()
  //         << " ra=" << std::hex << proc.read_reg(ii.rs1())
  //         << " target=" << addr << std::endl;

  return addr;
}

// Conditional Branch
uint32_t instrs::condbranch(mem::memory &, processor & proc, uint32_t bitstream) {
  b_instruction bi(bitstream);

  uint32_t current_pc = proc.read_pc();
  uint32_t val1 = proc.read_reg(bi.rs1());
  uint32_t val2 = proc.read_reg(bi.rs2());

  bool take_branch = false;
  switch (bi.funct3()) {
    case 0b000: // BEQ
      take_branch = (val1 == val2);
      break;
    case 0b001: // BNE
      take_branch = (val1 != val2);
      break;
    case 0b100: // BLT
      take_branch = ((int32_t)val1 < (int32_t)val2);
      break;
    case 0b101: // BGE
      take_branch = ((int32_t)val1 >= (int32_t)val2);
      break;
    case 0b110: // BLTU
      take_branch = (val1 < val2);
      break;
    case 0b111: // BGEU
      take_branch = (val1 >= val2);
      break;
  }

  // Si se cumple la condición, se suma el offset (imm) al siguiente PC
  address_t addr = take_branch ? (current_pc + bi.imm()) : proc.next_pc();
  return addr;
}

