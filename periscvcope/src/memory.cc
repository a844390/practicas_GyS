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
    ifile.unsetf(std::ios::skipws);

    ifile.seekg(0, std::ios::end);
    const auto ifsize = ifile.tellg();
    ifile.seekg(0, std::ios::beg);

    _binary.reserve(ifsize);
    _binary.insert(_binary.begin(),
	    ifile_iter(ifile),
	    ifile_iter());
    ifile.close();


    // read elf header
    _ehdr = *reinterpret_cast<Elf32_Ehdr*>(_binary.data());

    // ensure riscv32
    if(_ehdr.e_machine != EM_RISCV) {
      std::cerr << "Invalid machine type: " << _ehdr.e_machine << std::endl;
      std::exit(EXIT_FAILURE);
    }

    // ensure the binary has a correct program table
    if(_ehdr.e_phnum < 1) {
      std::cerr << "No program header table" << std::endl;
      std::exit(EXIT_FAILURE);
    } 
    
    cout << "Machine type: " << _ehdr.e_machine << endl;
    cout << "Number of Program Headers: " << _ehdr.e_phnum << endl;
    cout << "Entry Point: 0x" << hex << _ehdr.e_entry << dec << endl;

    // entry point

    // read ELF program header table
    //  _ehdr.e_phnum = numero entradas en la tabla de encabezados
    //  _ehdr.e_phoff = offset de la tabla de encabezados
    //  _ehdr.e_phentsize = tamaño de cada entrada en la tabla de encabezados
    
    Elf32_Phdr phdr;
    for (int i = 0; i < _ehdr.e_phnum; i++) {
      phdr = *reinterpret_cast<Elf32_Phdr*>(_binary.data() + _ehdr.e_phoff + i * _ehdr.e_phentsize);

      cout << "Segment " << i << ": Type=" << phdr.p_type 
        << ", Offset=" << phdr.p_offset << " Bytes,"
        << " Virtual Address=0x" << hex << phdr.p_vaddr 
        << ", Size=" << dec << phdr.p_filesz << " Bytes"
        << endl;


      _phdr.push_back(phdr);
    }

    
    // load segments in memory
    //  PT_LOAD = Indica que el segmento debe ser cargado en memoria
    //  phdr.p_vaddr = dirección virtual donde se cargará el segmento
    //  phdr.p_offset = offset en el archivo donde se encuentra el segmento
    //  phdr.p_filesz = tamaño del segmento en el archivo
    
    segment s;
    for (int i = 0; i < _ehdr.e_phnum; i++) {
      phdr = _phdr[i];
      if(phdr.p_type == PT_LOAD) {
        s._initial_address = phdr.p_vaddr;
        s._content.insert(s._content.begin(), _binary.begin() + phdr.p_offset, _binary.begin() + phdr.p_offset + phdr.p_filesz);

        cout << "Loading segment at 0x" << hex << s._initial_address 
          << " with size " << dec << s._content.size() << " Bytes" << endl;

        _segments.push_back(s);
        }
    }
}

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
