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



#ifndef PURITAN_EXPGEN_H
#define PURITAN_EXPGEN_H
#include "puritan.h"

// Expression trees.
namespace Salem
{
namespace Puritan
{
class Exp;
class Gen;
class Type;
class Context;
typedef ::shared_ptr<Exp> ENode;
typedef std::vector <ENode> exp_list;
// Make a tree.
ENode create_expression (Gen *gen, Type return_type, const Context &ctx);

// Print it out.
std::ostream & operator<< (std::ostream &out, const ENode &x);

class Exp
{
public:
    Type type;
    Exp (Type _type) ;
    virtual ~Exp();

    virtual void print (ostream &out) const = 0;
    virtual bool is_fconstant() const;

    static ENode convert (ENode from, Rand *rand, Type to, Gen *gen);
};

class A0 : public Exp
{
public:
    A0 (Type ty);
};

class A1 : public Exp
{
public:
    ENode child;
    A1 (Type ty, ENode child);
};

class A2 : public Exp
{
public:
    ENode lhs;
    ENode rhs;
    A2(Type ty,
       ENode lhs,
       ENode rhs);
};

class A3 : public Exp
{
public:
    ENode a0;
    ENode a1;
    ENode a2;

    A3(Type ty,
       ENode a0,
       ENode a1,
       ENode a2);
};

class A4 : public Exp
{
public:
    ENode a0;
    ENode a1;
    ENode a2;
    ENode a3;

    A4(Type ty,
       ENode a0,
       ENode a1,
       ENode a2,
       ENode a3);
};

class Unop : public A1
{
    string name;

public:
    Unop(Type ty, string _name, ENode child) ;
    void print (ostream &out) const;
};

class FCallTemplate : public Exp
{
public:
    FunctionSPtr target;
    EList actuals;

    FCallTemplate (TypeList ty, FunctionSPtr f, EList actuals);
    void print (ostream &out) const;
};

class SamplerRef : public A1
{
    unsigned n;

public:
    SamplerRef (unsigned _n, ENode _child) ;
    void print (ostream &out) const;
};

class SamplerSizeRef : public A0
{
    unsigned n;

public:
    SamplerSizeRef (unsigned _n);
    void print (ostream &out) const;
};

class UniformRef : public A0
{
    string name;

public:
    UniformRef (Type ty, string v);
    void print (ostream &out) const;
};

class Constant : public A0
{
    string val;
public:
    Constant (Type ty, string v);
    Constant (Type ty, unsigned v);
    bool is_fconstant() const;
    void print (ostream &out) const;
};

class Variable : public A0
{
    Decl decl;

public:
    Variable (Decl _d) ;
    void print (ostream &out) const;
};


class Index : public A2
{
public:
    Index (ENode child, ENode index);
    void print (ostream &out) const;

};

class ConstArrayRef : public A0
{
    Decl decl;
public:
    ConstArrayRef (Decl _d);
    void print (ostream &out) const;
};

class LHSVariable : public Exp
{
    Decl decl;
    bool swizzled;

public:
    LHSVariable (Decl _d, bool _swizzled) ;

    void print (ostream &out) const;
};

class SpecialPhrase : public A1
{
    string name;

public:
    SpecialPhrase (Type , string name, ENode _child);

    void print (ostream &out) const;
};

class UnIntrinsicFunc : public A1
{
    string name;

public:
    UnIntrinsicFunc(Type ty, string _name, ENode) ;

    void print (ostream &out) const;
};

class BiIntrinsicFunc : public A2
{
    string name;

public:
    BiIntrinsicFunc(Type ty, string _name, ENode _lhs, ENode _rhs) ;

    void print (ostream &out) const;
};

class TriIntrinsicFunc : public A3
{
    string name;
public:
    TriIntrinsicFunc(Type ty, string _name,  ENode _a1, ENode _a2, ENode _a3);

    void print (ostream &out) const;
};

class Float4Func : public A4
{
public:
    Float4Func(Type ty, ENode _a1, ENode _a2, ENode _a3, ENode _a4);

    void print (ostream &out) const;
};

class Convert : public A1
{
    Type from;
    string pre;
    string post;
    unsigned n;

public:
    Convert (Type _to,
             Type _from,
             string pre,
             string post,
             unsigned n,
             ENode _child);

    void print (ostream &out) const;
};


class Swizzle : public A1
{
    string name;
public:
    Swizzle (Type ty, string _name, ENode _child) ;

    void print (ostream &out) const;
};

class Binop : public A2
{
    string name;

public:
    Binop(Type ty, string _name, ENode _lhs, ENode _rhs) ;

    void print (ostream &out) const;
};

class Relop : public A2
{
    string name;

public:
    Relop(string _name, ENode _lhs, ENode _rhs) ;
    void print (ostream &out) const;
};

class AssOp : public A2
{
    string name;

public:
    AssOp(Type ty,
          string _name,
          ENode lhs,
          ENode rhs);

    void print (ostream &out) const;
};

class SelfModOp : public A1
{
    string name;

public:
    SelfModOp(Type ty, string _name, ENode lhs);
    void print (ostream &out) const;
};

}
}
#endif

