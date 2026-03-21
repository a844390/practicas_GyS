#pragma once

#include <cassert>
#include <cstdint>
#include <elf.h>
#include <string>
#include <vector>
#include <iostream>

// *****************************************************************
// Módulo de memoria de peRISCVcope
//
// Modela el espacio de direcciones del sistema emulado mediante una lista
// de segmentos. Cada segmento tiene una dirección base y un vector de bytes.
// También incluye un segmento de pila preinicializado.
//
// El acceso a memoria es siempre por dirección virtual (address_t).
// find_segment() localiza qué segmento contiene la dirección pedida.
// *****************************************************************

namespace mem {
// Tipo para las direcciones virtuales del sistema emulado (32 bits)
using address_t = uint32_t;

// Segmento de memoria
// Representa una región contigua del espacio de direcciones.
// Corresponde a un segmento PT_LOAD del ELF o a la pila.
struct segment
{
    address_t _initial_address; // Dirección virtual del primer byte
    std::vector<uint8_t> _content; // Contenido del segmento (bytes)

    segment() : _initial_address(0), _content() {}
    segment(address_t initial_address, size_t size) : _initial_address(initial_address), _content(size) {}
};

class memory
{
  private:
    std::vector<uint8_t> _binary; // Copia cruda del fichero ELF en memoria del host
    Elf32_Ehdr _ehdr; // ELF header
    std::vector<Elf32_Phdr> _phdr; // Program header table, may contain multiple entries (una por segmento)
    std::vector<segment> _segments; // Segmentos cargados en el espacio virtual del emulado

  public:
    // La pila crece hacia abajo desde stack_top.
    // El SP se inicializa a stack_top y debe mantenerse alineado a 16 bytes
    // 128 MB — dirección más alta
    constexpr static size_t stack_top = 128 * 1024 * 1024; // the stack segment is always the first
    // 1 MB de espacio de pila
    constexpr static size_t stack_size = 1024 * 1024; // the stack segment is always the first

    // Constructor: crea el segmento de pila antes de cargar ningún binario.
    // El segmento de pila ocupa [stack_top - stack_size, stack_top)
    // y siempre es el segmento 0 en _segments.
    memory() : _binary(), _ehdr(), _phdr(), _segments() {
        // initialize the stack
        _segments.push_back(segment(stack_top-stack_size, stack_size)); // initial 1MB stack
    }

    // Busca el segmento que contiene la dirección 'addr'.
    // Devuelve el índice del segmento, o -1 si no se encuentra.
    ssize_t find_segment(address_t addr) const {
        size_t i = 0;
        for(; i < _segments.size(); ++i){
            auto begin = _segments[i]._initial_address;
            auto end = begin + _segments[i]._content.size();
            // La dirección pertenece al segmento si begin <= addr < end
            if((begin <= addr) && (addr < end)) {
                return static_cast<ssize_t>(i);
            }
        }
        return -1; // Dirección no mapeada, assert en read/write abortará
    }

  // Lee un valor de tipo T (uint8_t, uint16_t o uint32_t) de la dirección 'addr'.
  template<typename T>
  T read(address_t addr)
  {
    // find the segment
    auto seg_idx = find_segment(addr);
    std::cerr << "Attempt to read in address: " << std::hex << addr << std::endl;
    assert(seg_idx != -1); // Dirección no mapeada: fallo de segmentación emulado

    // read the data
    auto pos = addr - _segments[seg_idx]._initial_address; // Offset dentro del segmento
    // ensure alignment
    assert ((pos & (sizeof(T) - 1)) == 0); // Verificación de alineación

    // Acceso directo al vector de bytes interpretando como T
    return *reinterpret_cast<T*>(&_segments[seg_idx]._content[pos]);
  }

  // Escribe un valor de tipo T en la dirección 'addr'.
  template<typename T>
  void write(address_t addr, T value)
    {
      // find the segment
      auto seg_idx = find_segment(addr);
      assert(seg_idx != -1);

      // read the data
      auto pos = addr - _segments[seg_idx]._initial_address;
      // ensure alignment
      assert ((pos & (sizeof(T) - 1)) == 0);

      // Descomposición en bytes: itera sizeof(T) veces extrayendo el byte bajo
      for(size_t i = 0; i < sizeof(T); ++i) {
        _segments[seg_idx]._content[pos+i]=value & 0xFF;
        value = value >> 8; // Desplaza para procesar el siguiente byte
      }
    }

  // Carga el binario ELF desde disco, valida la cabecera y copia los segmentos PT_LOAD
  void load_binary(const std::string& binfile);
  // Imprime el contenido de un segmento en hexadecimal (para verificar la carga)
  void dump_binary(size_t segment_id) const;
  
  // Devuelve el punto de entrada del programa (campo e_entry del ELF header)
  // Es la primera dirección que ejecutará el bucle principal
  address_t entry_point() const { return _ehdr.e_entry; }
};

} // namespace mem
