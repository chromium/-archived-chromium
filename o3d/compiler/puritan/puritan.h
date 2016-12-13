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


#ifndef PURITAN_PURITAN_H
#define PURITAN_PURITAN_H

#include <string>
#include <sstream>
#include <vector>
#include <list>
#include "puritan_assert.h"
#include "rand.h"

#include "puritan_shared_ptr.h"

namespace Salem
{
namespace Puritan
{

class Decl;
class Exp;
class For;
class Function;
class Gen;
class IntCoverage;
class RangeKnob;
class Scope;
class Type;

typedef ::shared_ptr<Exp> ENode;
typedef ::shared_ptr<For> ForSPtr;
typedef ::shared_ptr<Function> FunctionSPtr;
typedef std::list <Decl> DeclList;
typedef std::list <ENode> EList;
typedef std::list <FunctionSPtr> FunctionList;
typedef std::list <Type> TypeList;
typedef std::list <unsigned> UList;
typedef std::ostream ostream;
typedef std::ostringstream ostringstream;
typedef std::pair <Gen *, Scope> gspair;
typedef std::string string;
typedef std::vector <Decl> DeclVec;
typedef std::vector <FunctionSPtr> Program;

ostream & operator << (ostream &out, Type x);
ostream & operator << (ostream &out, const Decl& x);
ostream & operator << (ostream &out, const TypeList & x) ;
ostream & operator << (ostream &out, gspair x);

const unsigned int array_size = 3;

enum type 
{
    NoType,
    Int, Int4,
    Float, Float2, Float4, 
    SamplerFloat4,
    SamplerSize,
    Struct,
    Float4ConstArray, Float4Array
};

class Type
{
public:
    enum type type;
    string name () const;
    string short_name () const;
    string suffix_name () const;
    bool array_p ();
    enum type operator () () const;
    Type (enum type);
    Type ();                    // default ctor for use with STL containers
    bool operator == (Type) const;
    bool operator != (Type) const;
    unsigned struct_elements ();
};

typedef enum 
{ 
    NoScope, 
    StaticConstArrays, 
    Sampler, 
    Uniform, 
    Static, 
    Argument_I,
    Argument_O,
    Argument_IO
} scope_t;

typedef enum 
{
    Initialized,
    Uninitialized 
} Kind;

typedef enum 
{
    Read,
    Write,
    ReadWrite
} Dir;

// Scope of a name.
class Scope
{
public:
    scope_t scope;
    unsigned fnc_idx;

    Scope (scope_t x) ;
    Scope (scope_t x, unsigned fnc_idx) ;

    bool eq (const Scope&other) const;
    string name () const;
    string short_name () const;
    bool visible_in (const class Context & ctx, Dir d) const;
};

// Declarations - scope, type and name.
class Decl
{
    static unsigned counter;

public:
    Scope scope;
    Type type;
    ENode initializer;

    // use the creation index as the root of the name
    unsigned idx;

    // certain variables should be made unavailable for general assignments
    // - for example, loop counters are modified by special
    // increment/decrement statements and should not be modified elsewhere
    bool noWrites;

    Decl (Scope, Type type);
    Decl (Scope, Type type, ENode init);
    Decl (Scope, Type type, unsigned idx);
    Decl (bool);
    Decl ();                    // default ctor for use with STL containers
    bool same ();
    static void reset ();
};

// A context passes information about what we're doing down through the
// recursive decent generator 

class Context
{
public:
    unsigned func;
    ForSPtr loop;
    unsigned depth;
    bool relop_p;

    UList *samplers;
    FunctionList *callees;

    Context relop () const;
    Context deeper () const;

    Context (unsigned _owner,
             ForSPtr _loop,
             UList *samplers,
             FunctionList *callees);
};

class Gen
{
private:
    friend class Exp;
    typedef enum { TokFor, TokWhile, TokDo, TokAssign, 
                   TokSelfMod, TokReturn, TokIf, TokBlock, 
                   TokClose, TokBreak } Token;
    typedef std::vector <Token> TokenVec;
    Rand rand;
    class Knobs & knobs;
    std::vector < std::pair < Type, std::string > > uniforms;
    unsigned exp_nodes;
    unsigned code_nodes;
    unsigned max_funcs;
    unsigned n_samplers;
    unsigned n_uniforms;

    Program functions;
    std::vector <FunctionList> callees;
    DeclVec symtable;

    void create_control_structure ();
    void create_call_structure ();
    void declare_samplers ();


    Decl gen_array_decl (Type t, const Context &ctx, Dir d);
    Decl fetch_decl (Type t,const Context &ctx, Kind k, Dir d);
    Decl new_variable (Scope scope, Type type, Kind k, const Context &c);
    Decl new_variable (Scope scope, const TypeList & type);
    Decl new_initialized_variable (Scope scope, Type type,  const Context &c);
    Decl new_uninitialized_variable (Scope s, Type c, unsigned idx);
    Decl new_uninitialized_variable (Scope s, Type c);
    DeclVec::iterator existing_decl (Type t, const Context &ctx, Dir d);

    char rchar (const char *what);
    ostream & output_boiler_plate (ostream &out) const;
    int coverage (const RangeKnob &knob, IntCoverage *marker);
    int coverage (unsigned fidx, const RangeKnob &knob, IntCoverage *marker) ;
    unsigned ins_stmts (TokenVec *v, unsigned c, 
                        unsigned n, Token t, 
                        unsigned e, bool break_p);
    void output_code (ostream &out, const Program &functions);
    void output_declarations (ostream &out, Scope s);
    ostream &output_typedefs (ostream &out);

    string gen_fconstant ();
    Type random_type ();

    friend ostream & operator << (ostream &out, gspair x);
    friend ENode create_expression (Gen *gen, Type rtype, const Context &ctx);
public:
    Gen (Knobs & _config); 
    string generate (class OutputInfo *output);
};

}
}

#endif
