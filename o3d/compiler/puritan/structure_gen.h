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



#ifndef PURITAN_STRUCTUREGEN_H
#define PURITAN_STRUCTUREGEN_H

#include "puritan.h"

namespace Salem
{
namespace Puritan
{
typedef ::shared_ptr<class Block> BlockSPtr;
typedef ::shared_ptr<class Break> BreakSPtr;
typedef ::shared_ptr<class Code> CodeSPtr;
typedef ::shared_ptr<class Exp> ENode;
typedef ::shared_ptr<class For> ForSPtr;
typedef ::shared_ptr<class Function> FunctionSPtr;
typedef std::vector <CodeSPtr> CodeVec;
typedef std::vector <FunctionSPtr> FunctionVec;


class Code
{
    virtual void print_code (std::ostream &out) = 0;

public:
    Code ();
    virtual ~Code();

    void print_code_kids (CodeVec &children, std::ostream &out);

    friend class Function;
    friend class While;
    friend class Do;
};

class Block:public Code
{
    void print_code (std::ostream &out);

public:
    CodeVec children;

    void add_child (CodeSPtr x);
};

class Function:public Block
{
public:
    class Gen *gen;
    unsigned idx;
    TypeList ret_type;
    DeclList formals;
    UList samplers;
    FunctionSPtr calls;
    std::string name;
    bool standalone;
    bool noinline;

    Function(Gen *gen, unsigned _idx,
             TypeList ret_type,
             DeclList formals,
             bool standalone,
             bool noinline);

    void new_decl (std::string name,
                   Scope scope,
                   FunctionSPtr func,
                   enum type type,
                   std::string init);

    void  output_code (std::ostream &out);
};



class For : public Block
{

    int from;
    int to;
    
    friend class Index;
    friend class ConstArrayRef;
public:

    Decl counter;

    static unsigned fidx;
    For(Decl counter, int _from, int _to) ;

    void print_code (std::ostream &out);
};

class Break: public Code
{
public:
    ::shared_ptr<Exp> cond;

    Break (ENode _cond) ;
    void print_code (std::ostream &out);
}
;


class While: public Block
{
    ENode cond;
    Decl counter;
    unsigned limit;
public:
    While(ENode _cond, 
          Decl i,
          unsigned limit) ;

    void print_code (std::ostream &out);
};


class Do: public Block
{
    ENode cond;
    Decl counter;
    unsigned limit;

public:
    Do(ENode _cond, Decl counter, unsigned limit) ;

    void print_code (std::ostream &out);
};

class IfTemplate: public Block
{
    ENode cond;
    bool has_else;
    bool toggle;
    CodeVec other_block;

public:
    IfTemplate(ENode _cond, bool has_else);

    void print_code (std::ostream &out);
    void add_child (CodeSPtr x);
};

    

class Return : public Code
{
public:
    unsigned struct_elements;
    EList returns;
    Decl name;

    Return(ENode _reta) ;
    Return(EList _returns, Decl name) ;

    void print_code (std::ostream &out);
};


class AssignmentTemplate : public Code
{
public:
    ENode rp;
    AssignmentTemplate(ENode _terms) ;
    void print_code (std::ostream &out);
};


}
}
#endif
