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

#include <iostream>
#include <string>
#include <vector>

#include "ecoff.hh"
#include "object_file.hh"
#include "str.hh"
#include "symtab.hh"

Tick curTick;

int
main(int argc, char *argv[])
{
    EcoffObject obj;
    if (argc != 3) {
        cout << "usage: " << argv[0] << " <filename> <symbol>\n";
        return 1;
    }

    if (!obj.open(argv[1])) {
        cout << "file not found\n";
        return 1;
    }

    SymbolTable symtab;
    obj.loadGlobals(&symtab);

    string symbol = argv[2];
    Addr address;

    if (symbol[0] == '0' && symbol[1] == 'x') {
        if (to_number(symbol, address) && symtab.findSymbol(address, symbol))
            cout << "address = 0x" << hex << address
                 << ", symbol = " << symbol << "\n";
        else
            cout << "address = 0x" << hex << address << " was not found\n";
    } else {
        if (symtab.findAddress(symbol, address))
            cout << "symbol = " << symbol << ", address = 0x" << hex
                 << address << "\n";
        else
            cout << "symbol = " << symbol << " was not found\n";
    }

    return 0;
}
