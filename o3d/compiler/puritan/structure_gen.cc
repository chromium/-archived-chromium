/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
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



#include <sstream>
#include "structure_gen.h"
#include "exp_gen.h"
namespace Salem
{
namespace Puritan
{
class Context;

//////////////////////////////////////////////////////////////////////
// Blocks

void Block::print_code (ostream &out)
{
    print_code_kids (children, out);
}

void Block::add_child (CodeSPtr x)
{
    children.push_back (x);
}

//////////////////////////////////////////////////////////////////////
// Functions

Function::Function(Gen *_gen, 
                   unsigned _idx, 
                   TypeList _ret_type, 
                   DeclList _formals,
                   bool _standalone,
                   bool _noinline) 
    : gen (_gen), 
      idx (_idx),
      ret_type (_ret_type), 
      formals (_formals), 
      standalone (_standalone),
      noinline (_noinline)
{

}

void Function::output_code (ostream &out)
{
    // main always returns a struct
    if (idx == 0)
    {
        if (standalone)
        {
            out << "struct ";
        }

        out << "PS_OUTPUT main (";
    }
    else 
    {
        if (noinline)
        {
            out << "noinline ";

        }
        out << ret_type << " func" << idx << "(";
    }
    unsigned arg_idx = 0;

    for (DeclList::iterator i = formals.begin();
         i != formals.end();
         i++, arg_idx++)
    {
        if (arg_idx != 0)
        {
            out << ", ";
        }

        out << (*i).type << " " << (*i);

        if (standalone && idx == 0)
        {
            out << ":VPOS";
        }
    }

    out << ")\n";


    ostringstream code;

    for (CodeVec::iterator x = children.begin();
         x != children.end();
         x++)
    {
        (*x)->print_code (code);
    }

    // emit the declarations at the top of the function and then the guts.
    out << "{"
        << gspair (gen, Scope(Static, idx))
        << code.str()
        << "}";
}

//////////////////////////////////////////////////////////////////////
/// For loops 

For::For(Decl _counter, int _from, int _to) 
    : from (_from), to (_to), counter (_counter)
{

}

void For::print_code (ostream &out)
{
    out << "for (" << counter << " = " << from << ";" 
        << counter << " < " << to <<"; "
        << counter << "++)\n";
    print_code_kids (children, out);
}

//////////////////////////////////////////////////////////////////////
/// Breaks in loops 

Break::Break (ENode _cond)
    : cond (_cond)
{
}

void Break::print_code (ostream &out)
{
    out << "if (" << cond << ")\n{ break;}\n";
}

//////////////////////////////////////////////////////////////////////
/// While loops

While::While(ENode _cond, 
             Decl _index, 
             unsigned _limit)
    : cond(_cond),
      counter (_index),
      limit (_limit)
{
}

void While::print_code (ostream &out)
{
    out << counter << " = " << limit << ";\n"
        << "while (" << counter << "> 0 &&"
        << cond << ")\n"
        << "{" << "--" << counter << ";\n";

    for (CodeVec::iterator x = children.begin();
         x != children.end();
         x++)
    {
        (*x)->print_code (out);
    }

    out << "}";
}

//////////////////////////////////////////////////////////////////////
/// Do loops

Do::Do(ENode _cond, Decl _counter, unsigned _limit)
    : cond(_cond),
      counter (_counter),
      limit (_limit)
{
}

void Do::print_code (ostream &out)
{
    out << counter << " = " << limit << ";\n"
        << "do {";

    for (CodeVec::iterator x = children.begin();
         x != children.end();
         x++)
    {
        (*x)->print_code (out);
    }

    out << "--" << counter << ";\n"
        << "} while (" << counter << "> 0 &&"
        << cond << ");\n";
}

//////////////////////////////////////////////////////////////////////
// Ifs

IfTemplate::IfTemplate(ENode _cond, bool _has_else) 
    : cond (_cond),
      has_else (_has_else),
      toggle (false)
{
}

void IfTemplate::print_code (ostream &out)
{
    out << "if (" << cond << ")\n";

    print_code_kids (children, out);
    if (has_else) 
    {
        out << "else\n";
        print_code_kids (other_block, out);
    }

}

void IfTemplate::add_child (CodeSPtr x)
{
    if (toggle && has_else)
    {
        other_block.push_back (x);
    }
    else
    {
        children.push_back (x);
    }

    toggle = ! toggle;
}

//////////////////////////////////////////////////////////////////////
// Return

Return::Return(EList  _returns, Decl _name) 
    : returns (_returns),
      name (_name)
{
}

Return::Return(ENode _reta) : name(false)
{
    returns.push_back (_reta);
}

void Return::print_code (ostream &out)
{
    if (name.type != NoType)
    {
        const char *ext[] = {".color0", ".color1", ".color2", ".color3" };
        unsigned k = 0;
        for (EList::const_iterator i = returns.begin();
             i != returns.end();
             i++, k++)
        {
            out << name << ext[k] << " = " << *i << ";\n";
        }
        out << "return " << name << ";\n";
    }
    else
    {
        out << "return " << returns.front() << ";\n";
    }
}

//////////////////////////////////////////////////////////////////////
// Assignments

AssignmentTemplate::AssignmentTemplate(ENode _terms) 
    : rp (_terms)
{
}

void AssignmentTemplate::print_code (ostream &out)
{
    out << rp << ";\n";
}


//////////////////////////////////////////////////////////////////////
// Skeleton base class for all above
Code::Code()
{
}

Code::~Code()
{
}

void Code::print_code_kids (CodeVec &children, ostream &out)
{
    out << "{";
    for (CodeVec::iterator x = children.begin();
         x != children.end();
         x++)
    {
        (*x)->print_code (out);
    }
    out << "}";
}

}
}
