#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>

#include <memory.hh>

using namespace mem;
using namespace std;

// *****************************************************************
// load_binary
//
// Carga un binario ELF32 RISC-V en el espacio de memoria emulado.
// Pasos:
//   1. Leer el fichero completo en _binary (vector de bytes del host)
//   2. Interpretar la cabecera ELF y validar que es RISC-V
//   3. Leer la tabla de cabeceras de programa (Program Header Table)
//   4. Para cada segmento PT_LOAD, copiar sus bytes al segmento virtual
// *****************************************************************
void
memory::load_binary(const std::string& binfile)
{
  using ifile_iter = std::istream_iterator<uint8_t>;

  std::ifstream ifile(binfile, std::ios::binary);

  if (ifile.is_open() == false) {
    std::cerr << "Unable to open "<< binfile << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // copy the binary into memory
  // Stop eating new lines in binary mode!!!

  // Paso 1
  // unsetf(skipws): sin esto, el iteradorde flujo se saltaría 
  // los bytes que coinciden con espacios en blanco (\n, \r...)
  ifile.unsetf(std::ios::skipws);

  // Calculamos el tamaño del fichero para reservar el vector exacto
  ifile.seekg(0, std::ios::end);
  const auto ifsize = ifile.tellg();
  ifile.seekg(0, std::ios::beg);

  _binary.reserve(ifsize);
  // istream_iterator<uint8_t> lee byte a byte hasta EOF
  _binary.insert(_binary.begin(),
    ifile_iter(ifile),
    ifile_iter());
  ifile.close();


  // Paso 2
  // reinterpret_cast superpone la estructura Elf32_Ehdr sobre los primeros bytes
  // del fichero. Es válido porque el fichero está en memoria contigua.
  _ehdr = *reinterpret_cast<Elf32_Ehdr*>(_binary.data());

  // e_machine debe ser EM_RISCV para un binario RISC-V
  if(_ehdr.e_machine != EM_RISCV) {
    std::cerr << "Invalid machine type: " << _ehdr.e_machine << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Si no hay ningún segmento de programa no podemos cargar nada
  if(_ehdr.e_phnum < 1) {
    std::cerr << "No program header table" << std::endl;
    std::exit(EXIT_FAILURE);
  } 
  
  cout << "Machine type: " << _ehdr.e_machine << endl;
  cout << "Number of Program Headers: " << _ehdr.e_phnum << endl;
  cout << "Entry Point: 0x" << hex << _ehdr.e_entry << dec << endl;

  // Paso 3
  // La tabla comienza en el offset e_phoff del fichero.
  // Cada entrada tiene tamaño e_phentsize y hay e_phnum entradas.
  
  Elf32_Phdr phdr;
  for (int i = 0; i < _ehdr.e_phnum; i++) {
    // Calculamos la dirección de la i-ésima entrada y la copiamos a phdr
    phdr = *reinterpret_cast<Elf32_Phdr*>(_binary.data() + _ehdr.e_phoff + i * _ehdr.e_phentsize);

    cout << "Segment " << i << ": Type=" << phdr.p_type 
      << ", Offset=" << phdr.p_offset << " Bytes,"
      << " Virtual Address=0x" << hex << phdr.p_vaddr 
      << ", Size=" << dec << phdr.p_filesz << " Bytes"
      << endl;


    _phdr.push_back(phdr);
  }

  
  // Paso 4
  // PT_LOAD indica que el segmento debe mapearse en memoria en tiempo de carga.
  // p_vaddr es la dirección virtual destino.
  // p_offset es dónde empieza el segmento dentro del fichero ELF.
  // p_filesz es cuántos bytes copiar desde el fichero.
  
  segment s;
  for (int i = 0; i < _ehdr.e_phnum; i++) {
    phdr = _phdr[i];
    if(phdr.p_type == PT_LOAD) {
      s._initial_address = phdr.p_vaddr;
      // Copiamos exactamente p_filesz bytes del fichero al segmento
      s._content.insert(s._content.begin(),
              _binary.begin() + phdr.p_offset,
              _binary.begin() + phdr.p_offset + phdr.p_filesz);

      cout << "Loading segment at 0x" << hex << s._initial_address 
        << " with size " << dec << s._content.size() << " Bytes" << endl;

      _segments.push_back(s);
    }
  }
}

// *****************************************************************
// dump_binary
//
// Imprime el contenido de un segmento en hexadecimal, 16 bytes por línea.
// Usado para verificar que la carga es correcta comparando con objdump -d.
// *****************************************************************
void memory::dump_binary(size_t segment_id)const {
  if(segment_id >= _segments.size()) {
    std::cerr << "Invalid segment id" << std::endl;
    return;
  }

  std::cout << "Segment " << segment_id << " at 0x" << hex << _segments[segment_id]._initial_address << dec << std::endl;
  std::cout << "Size: " << _segments[segment_id]._content.size() << " Bytes" << std::endl;

  for(size_t i = 0; i < _segments[segment_id]._content.size(); ++i) {
    if(i % 16 == 0) {
      std::cout << std::endl << "0x" << hex << _segments[segment_id]._initial_address + i << ": ";
    }
    std::cout << hex << static_cast<int>(_segments[segment_id]._content[i]) << " ";
  }
  std::cout << std::endl;
}
