/*
 * Copyright (c) 2003 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ELF_OBJECT_HH__
#define __ELF_OBJECT_HH__

/* Because of the -Wundef flag we have to do this */
#define __LIBELF_INTERNAL__     0
#define __LIBELF64_LINUX        1
#define __LIBELF_NEED_LINK_H    0

#include <libelf/libelf.h>
#include <libelf/gelf.h>
#include "base/loader/object_file.hh"

class ElfObject : public ObjectFile
{
  protected:

    ElfObject(const std::string &_filename, int _fd,
              size_t _len, uint8_t *_data,
              Arch _arch, OpSys _opSys);

  public:
    virtual ~ElfObject() {}

    virtual bool loadSections(FunctionalMemory *mem,
                              bool loadPhys = false);
    virtual bool loadGlobalSymbols(SymbolTable *symtab);
    virtual bool loadLocalSymbols(SymbolTable *symtab);

    static ObjectFile *tryFile(const std::string &fname, int fd,
                               size_t len, uint8_t *data);
};

#endif // __ELF_OBJECT_HH__
