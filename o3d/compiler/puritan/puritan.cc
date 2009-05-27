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
#include <iterator>
#include <iomanip>
#include "puritan.h"
#include "knobs.h"
#include "structure_gen.h"
#include "exp_gen.h"
#include "puritan_assert.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

namespace Salem
{
namespace Puritan
{
// Counters for code coverage.
IntCoverage cfor_count ("for loop count");
IntCoverage cfor_nesting ("for loop nesting");
IntCoverage cdo_count ("do loop count");
IntCoverage cdo_nesting ("do loop nesting");
IntCoverage cwhile_count ("while loop count");
IntCoverage cwhile_nesting ("while loop nesting");
IntCoverage cif_count ("if count");
IntCoverage cif_nesting ("if nesting");
IntCoverage cblock_count ("block count");
IntCoverage cblock_nesting ("block nesting");
IntCoverage cexp_count ("exp count");
IntCoverage cfunc_count ("func count");
IntCoverage csampler_count ("sampler count");
IntCoverage cuniform_count ("unform count");

const char *assop_names[] = { "*=", "/=", "+=", "-=",  0 };
StrCoverage assops ("Assops", assop_names);

const char *selfmodops_names[] = { "++", "--", 0 };
StrCoverage selfmodops ("SelfModOps", selfmodops_names);

const char *lhs_f4tof4_swizzle_names[] =
{
    "w", "z", "zw", "y",
    "yw", "yz", "yzw", "x",
    "xw", "xz", "xzw", "xy",
    "xyw", "xyz", "xyzw",
    0
};

StrCoverage lhs_f4tof4_swizzles ("Lhs f4tof4 Swizzles", 
                                 lhs_f4tof4_swizzle_names);

const char *lhs_f2tof2_swizzle_names[] =
{
    "xy", "x", "y",
    0
};

StrCoverage lhs_f2tof2_swizzles ("lhs_f2tof2_swizzles", 
                                 lhs_f2tof2_swizzle_names);




// Build the control structurte for each function
//
// Each function starts off with an empty list of tokens describing what to
// do.  Knobs and random numbers insert things into the list in random places
// to mark structure.  Many things have two items in the list, for example, a
// for will have For token and a Close token, marking the point that the for
// scope is opened, and then closed.  If something is inserted into the list
// between the For and the Close, then it becomes nested inside the for when
// the code is output.  If the thing that is inserted is a pair and it has a
// Close istelf and that straddles the Close of the for, then since the Closes
// all look alike, it is still the same as being nested, except the scope of
// the for then stretches out to the end of the new item.


void Gen::create_control_structure ()
{
    // Divide the samplers among the functions, samples_per_func is a list of
    // samplers used in each function.
    std::vector <UList> samples_per_func;

    for (unsigned fidx = 0; fidx < max_funcs; fidx++)
    {
        samples_per_func.push_back (UList ());
    }   

    for (unsigned i = 0; i < n_samplers; i++) 
    {
        for (unsigned j = 0; j < rand.urange (1,2); j++) 
        {
            samples_per_func[rand.urange (0, max_funcs)].push_back (i);
        }
    }

    for (unsigned fidx = 0; fidx < max_funcs; fidx++)
    {
        unsigned for_count 
            = coverage (fidx, knobs.for_count, &cfor_count);
        unsigned for_nesting 
            = coverage (fidx, knobs.for_nesting, &cfor_nesting);
        unsigned block_count 
            = coverage (fidx, knobs.block_count, &cblock_count);
        unsigned block_nesting 
            = coverage (fidx, knobs.block_nesting, &cblock_nesting);
        unsigned while_count 
            = coverage (fidx, knobs.while_count, &cwhile_count);
        unsigned while_nesting 
            = coverage (fidx, knobs.while_nesting, &cwhile_nesting);
        unsigned do_count 
            = coverage (fidx, knobs.do_count, &cdo_count);
        unsigned do_nesting 
            = coverage (fidx, knobs.do_nesting, &cdo_nesting);
        unsigned if_count 
            = coverage (fidx, knobs.if_count, &cif_count);
        unsigned if_nesting 
            = coverage (fidx, knobs.if_nesting, &cif_nesting);
        unsigned asn_count 
            = coverage (fidx, knobs.exp_count, &cexp_count);

        std::vector <BlockSPtr> code_stack;
        std::vector <ForSPtr> for_stack;

        TokenVec tokens;

        unsigned asn_tmp_count = asn_count;

        while (code_nodes < knobs.code_limit.uget()
               && (for_count || block_count || while_count
                   || do_count || if_count  || asn_tmp_count))
        {
            code_nodes ++;
            for_count = ins_stmts (&tokens,
                                   for_count, for_nesting,
                                   TokFor, 1, true);

            block_count= ins_stmts (&tokens,
                                    block_count, block_nesting,
                                    TokBlock, 1, false);

            while_count = ins_stmts (&tokens,
                                     while_count, while_nesting,
                                     TokWhile, 1, true);

            do_count = ins_stmts (&tokens,
                                  do_count, do_nesting,
                                  TokDo, 1, true);

            if_count = ins_stmts (&tokens,
                                  if_count, if_nesting,
                                  TokIf, 1, false);

            if (knobs.selfmod_chance (&rand))
            {
                asn_tmp_count = ins_stmts (&tokens, 
                                           asn_tmp_count, 1, 
                                           TokSelfMod, 0, false);
            } 
            else
            {
                asn_tmp_count = ins_stmts (&tokens, 
                                           asn_tmp_count, 1, 
                                           TokAssign, 0, false);
            }

        }


        // The final statement is always a return, we stick it in an assignment
        // slot

        tokens.push_back (TokReturn);

        asn_count ++;

        // Spread the function calls and samplers over a number of assignments,
        // so that there's always at least one of the right thing per function.
        std::vector <UList> sampler_slots;
        std::vector <FunctionList> caller_slots;

        for (unsigned i = 0; i < asn_count; i++) 
        {
            sampler_slots.push_back (UList ());
            caller_slots.push_back (FunctionList ());
        }

        for (UList::const_iterator sampler = samples_per_func[fidx].begin ();
             sampler != samples_per_func[fidx].end ();
             sampler++)
        {
            for (unsigned j = 0; j < rand.urange (1,3); j++)
            {
                sampler_slots[rand.range (0, asn_count)].push_back (*sampler);
            }
        }

        for (FunctionList::const_iterator 
                 i = callees[fidx].begin ();
             i != callees[fidx].end ();
             i++)
        {
            for (unsigned j = 0; j < rand.urange (1,2); j++)
            {
                caller_slots[rand.range (0, asn_count)].push_back (*i);
            }
        }


        ForSPtr last_for;

        asn_count = 0;

        FunctionSPtr cur = functions[fidx];

        code_stack.push_back (cur);


        for (TokenVec::iterator t = tokens.begin ();
             t != tokens.end ();
             t++)
        {
            Context ctx (cur->idx,
                         last_for, 
                         &sampler_slots[asn_count], 
                         &caller_slots[asn_count]);

            switch (*t)
            {
                case TokBlock:
                {
                    BlockSPtr c (new Block ());
                    code_stack.back ()->add_child (c);
                    code_stack.push_back (c);
                    for_stack.push_back (last_for);
                    break;
                }
                case TokFor:
                {
                    Decl counter 
                        =  new_uninitialized_variable (Scope (Static, cur->idx),
                                                       Int);
                    // mark this variable such that its not assigned anywhere
                    counter.noWrites = true;
                    ForSPtr 
                        f (new For (counter,
                                    rand.range (0,10),
                                    rand.range (11,20)));
                    last_for = f;
                    code_stack.back ()->add_child (f);
                    code_stack.push_back (f);
                    for_stack.push_back (f);
                    break;
                }
                case TokIf:
                {
                    BlockSPtr 
                        c (new IfTemplate (create_expression (this, 
                                                              Float, 
                                                              ctx.relop ()),
                                           knobs.if_elses (&rand)));
                    code_stack.back ()->add_child (c);
                    code_stack.push_back (c);
                    for_stack.push_back (last_for);
                    break;
                }
                case TokWhile:
                {
                    Decl counter 
                        = new_uninitialized_variable (Scope (Static, cur->idx),
                                                      Float);
                    // mark this variable such that its not assigned anywhere
                    counter.noWrites = true;
                    BlockSPtr 
                        c (new While (create_expression (this, 
                                                         Float, 
                                                         ctx.relop ()),
                                      counter,
                                      rand.range (1,10)));
                    code_stack.back ()->add_child (c);
                    code_stack.push_back (c);
                    for_stack.push_back (last_for);
                    break;
                }
                case TokDo:
                {
                    Decl counter 
                        = new_uninitialized_variable (Scope (Static, cur->idx), 
                                                      Float);
                    // mark this variable such that its not assigned anywhere
                    counter.noWrites = true;
                    BlockSPtr 
                        c (new Do (create_expression (this, 
                                                      Float, 
                                                      ctx.relop ()), 
                                   counter,
                                   rand.range (1,10)));
                    code_stack.back ()->add_child (c);
                    code_stack.push_back (c);
                    for_stack.push_back (last_for);
                    break;
                }
                // End of any kind of block
                case TokClose:
                    code_stack.pop_back ();
                    last_for = for_stack.back ();
                    for_stack.pop_back ();
                    break;
                case TokSelfMod:
                {
                    Type ty = random_type ();
                    Decl d = fetch_decl (ty, ctx, Initialized, ReadWrite);
                    ENode lhs = ENode 
                        (new SelfModOp (ty, selfmodops.choose (&rand), 
                                        ENode (new LHSVariable (d, false))));
                    // We can use the assignment template for self mod, since it
                    // takes any expression.
                    code_stack.back ()->add_child 
                        (CodeSPtr (new AssignmentTemplate (lhs)));
                }
                break;
                case TokAssign:
                {
                    ENode rhs;
                    ENode lhs;
                    Type type (NoType);
                    bool assop =  knobs.assop_chance (&rand);
                    bool lhs_swizzle = true;
                    // Inside a for loop we may want to do array assignments.
                    if (ctx.loop.get()
                        && knobs.array_in_for_use (&rand)
                        && !knobs.array_constness (&rand))
                    {
                        Decl decl = gen_array_decl (Float4Array, ctx, Read);
                        rhs = create_expression (this, Float4, ctx);
                        lhs = ENode 
                            (new Index (ENode (new LHSVariable (decl, false)),
                                        ENode (new Constant (Float4, "4"))));
                        type = Float4;
                    }
                    else
                    {
                        type = random_type ();
                        rhs = create_expression (this, type, ctx);
                        // An assignment operator needs a preinitialized lhs.
                        Decl d = fetch_decl (type, ctx,
                                             assop
                                             ? Initialized
                                             : Uninitialized,
                                             assop 
                                             ? ReadWrite 
                                             : Write);
                        if (! d.initializer.get() )
                            lhs_swizzle = false;
                        lhs = ENode (new LHSVariable (d, false));
                    }


                    if (assop) 
                    {
                        rhs = ENode (new AssOp (type,  assops.choose (&rand), 
                                                lhs,
                                                rhs));
                    }
                    else
                    {
                        if (lhs_swizzle)
                        {
                            if (type == Float4 && knobs.lhs_swizzle_chance (&rand))
                            {
                                lhs = ENode
                                    (new Swizzle (type, 
                                                  lhs_f4tof4_swizzles.choose (&rand), 
                                                  lhs));
                            }
                            else if (type == Float2 && knobs.lhs_swizzle_chance (&rand))
                            {
                                lhs = ENode 
                                    (new Swizzle (type, 
                                                  lhs_f2tof2_swizzles.choose (&rand), 
                                                  lhs));
                            }
                        }
                        rhs = ENode (new AssOp (type, "=", lhs, rhs));
                    }

                    code_stack.back ()->add_child 
                        (CodeSPtr (new AssignmentTemplate (rhs)));
                    asn_count ++;
                }
                break;

                case TokReturn:
                {
                    // Add the returns at the bottom, the main function returns a
                    // struct.

                    if (cur->idx == 0)
                    {
                        Decl decl = new_variable (Scope (Static, cur->idx),
                                                  cur->ret_type);
                        EList v;
                        for (TypeList::const_iterator i = cur->ret_type.begin ();
                             i != cur->ret_type.end ();
                             i++)
                        {
                            v.push_back (create_expression (this, *i, ctx));
                        }
                        code_stack.back ()->add_child 
                            (CodeSPtr (new Return (v, decl)));
                    }
                    else
                    {
                        ENode rval1 = create_expression (this, 
                                                         cur->ret_type.front (), 
                                                         ctx);
                        code_stack.back ()->add_child 
                            (CodeSPtr (new Return (rval1)));
                    }
                }
                break;

                case TokBreak:
                {
                    BreakSPtr 
                        b (new Break 
                           (create_expression (this, Float, ctx)));
                    code_stack.back ()->add_child (b);
                }
                break;
                default:
                    PURITAN_ABORT("Illegal token " << static_cast<int>(*t));
                    break;
            }
        }
    }
}


// Create the function template for each function and fill in the argument type
// and return types.

void Gen::create_call_structure ()
{
    for (unsigned i = 0; i < max_funcs; i++)
    {
        DeclList formals;
        TypeList ret_type;

        if (i == 0)
        {
            // first function is main, always returns a struct and takes just
            // float2

            unsigned nrets = rand.urange (1, 5);
            for (unsigned j = 0; j < nrets; j++) 
            {
                ret_type.push_back (knobs.float4_struct_member_chance (&rand)
                                    ? Float4
                                    : Float);

            }

            formals.push_back (
                new_uninitialized_variable (Scope (Argument_I, i), Float2));
        }
        else
        {
            switch (rand.range (0,4))
            {
            case 0:
                ret_type.push_back (Float);
                break;
            case 1:
                ret_type.push_back (Float2);
                break;
            default:
                ret_type.push_back (Float4);
                break;
            }

            unsigned n = knobs.arg_count.random_uint (&rand);

            for (unsigned j = 0; j < n; j++)
            {
                Context ctx (i, ForSPtr (), 0, 0);
                scope_t arg =
                    knobs.arg_in_chance(&rand) 
                  ? Argument_I : knobs.arg_out_chance (&rand) 
                  ? Argument_O : Argument_IO;

                formals.push_back (new_uninitialized_variable (Scope (arg, i),
                                                               random_type ()));
            }
        }
        functions.push_back (FunctionSPtr 
                             (new Function (this, i, ret_type, 
                                            formals, 
                                            knobs.standalone (),
                                            knobs.noinline_chance(&rand))));
    }
    // Different call patterns, so far only one.
    switch (int x = rand.range (0,0)) 
    {
    case 0:
        // Each functions calls the one above, save the last
        for (unsigned i = 0; i < max_funcs; i++)
        {
            FunctionList targets;
            if (i != (max_funcs - 1)) 
            {
                targets.push_back (functions[i+1]);
            }
            callees.push_back (targets);
        }
        break;
    case 1:
        break;
    default:
        PURITAN_ABORT ("Unexpected case " << x);
    }
}

// Add any samplers to the symbol table.
void Gen::declare_samplers ()
{
    for (unsigned i = 0; i < n_samplers; i++)
    {
        new_uninitialized_variable (Scope (Sampler), SamplerFloat4, i);
        new_uninitialized_variable (Scope (Uniform), SamplerSize, i);
    }

    for (unsigned i = 0; i < n_uniforms; i++)
    {
        Decl decl = new_uninitialized_variable (Scope (Uniform), Float4, i);
        ostringstream decl_name;
        decl_name << decl;
        uniforms.push_back 
            (std::pair <Type, string>  (Float4, decl_name.str()));
    }
}

// Insert the statement tokens somewhere randomly.
unsigned Gen::ins_stmts (TokenVec *vec,
                         unsigned count,
                         unsigned nest,
                         Token token,
                         unsigned ends,
                         bool break_p)
{
    nest = std::min (std::max(1U, nest), count);
    size_t start_pos = 0;
    size_t limit = vec->size();
    size_t end_pos = limit;
    for (unsigned j = 0; j < nest; j++)
    {
        start_pos = rand.srange (start_pos, end_pos);
        end_pos = start_pos + 1 < end_pos 
            ? rand.srange (start_pos + 1, end_pos) 
            : end_pos;

        PURITAN_ASSERT (start_pos <= end_pos, "Sanity check");

        vec->insert (vec->begin () + start_pos, token);        

        start_pos ++;
        end_pos++;

        if (break_p && knobs.loop_breaks (&rand))
        {
            vec->insert (vec->begin () + start_pos, TokBreak);
            start_pos ++;
            end_pos++;
        }

        if (ends)
        {
            vec->insert (vec->begin () + start_pos, TokClose);
            start_pos ++;
            end_pos++;
        }

    }
    return count - nest;
}

//////////////////////////////////////////////////////////////////////
// Class containing the scope of a name.
Scope::Scope (scope_t x, unsigned i) : scope (x) , fnc_idx (i)
{

}

bool Scope::visible_in (const Context &ctx, 
                        Dir d) const
{
    switch (scope)
    {
    case Argument_IO:
        return fnc_idx == ctx.func && (d == Read || d == Write);
    case Argument_O:
        return fnc_idx == ctx.func && (d == Write);
    case Argument_I:
        return fnc_idx == ctx.func && (d == Read || d == Write);
    case StaticConstArrays:
    case Sampler:
    case Uniform:
        return d == Read;
    case Static:
        return fnc_idx == ctx.func;
    case NoScope:
    default:
        break;
    }
    PURITAN_ABORT ("Illegal scope " << scope);
}

string Scope::name () const
{
    switch (scope)
    {
    case Sampler:
        return "sampler ";
    case Uniform:
        return "uniform ";
    case Static:
    case StaticConstArrays:
        return "";
    default:
    case NoScope:
    case Argument_I:
    case Argument_O:
    case Argument_IO:
        PURITAN_ABORT ("Argument error " << scope);
    }
}

string Scope::short_name () const
{
    switch (scope)
    {
    case Sampler:
        return "in";
    case Uniform:
        return "u_";
    case Static:
    case StaticConstArrays:
        return "s_";
    case Argument_I:
        return "ai_";
    case Argument_O:
        return "ao_";
    case Argument_IO:
        return "aio_";
    default:
    case NoScope:
        PURITAN_ABORT ("Argument error " << scope);
    }
}

bool Scope::eq (const Scope &other) const
{
    return scope == other.scope && fnc_idx == other.fnc_idx;
}

//////////////////////////////////////////////////////////////////////
// Generator
// Main wrapper and support
// Emit all the declarations at the given scope
void
Gen::output_declarations (ostream &out,
                          Scope s)
{
    DeclVec::const_iterator i = symtable.begin ();
    while (i != symtable.end ())
    {
        const Decl & d = *i;
        if (d.scope.eq (s))
        {
            if (d.type () == Float4Array)
            {
                out << "uniform float4 "
                    << d
                    << "[" << array_size << "];\n";
            }
            else if (d.type () == Float4ConstArray)
            {
                out << "const float4 "
                    << d
                    << "[" << array_size << "] = {\n";
                for (unsigned i = 0; i < array_size * 4; i++)
                {
                    if (i)
                    {
                        out << ",\n";
                    }

                    out << gen_fconstant ();
                }
                out << "};\n";
            }
            else
            {
                out << d.scope.name ();

                if (d.scope.scope != Sampler)
                {
                    out << " " << d.type;
                }

                out << " " << d;

                if (d.initializer.get() )
                {
                    out << " = " << d.initializer;
                }
                out << ";\n";
            }
        }
        i++;
    }
}

// Return a random type.
Type Gen::random_type ()
{
    return (knobs.type_float4_chance (&rand))
        ? Float4
        : knobs.type_float2_chance (&rand)
        ? Float2
        : Float;
}


const char *id_char =
"0123456789" "abcdefghijklmnopqrstuvwxyz" "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char *first_char =
"abcdefghijklmnopqrstuvwxyz" "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char *digit = "012345678";

char Gen::rchar (const char *what)
{
    return what[ (rand.srange (0, strlen (what)))];
}


// Make a random floating point constant.
string Gen::gen_fconstant ()
{
    if (knobs.constant_small (&rand))
    {
        ostringstream out;
        char b[100];
        sprintf (b, "%g", 10.0 / rand.range (1, 10000));
        out << b;
        return out.str ();
    }
    else
    {
        string res;
        unsigned len = rand.range (1, 30);
        unsigned point = rand.range (0,len);

        for (unsigned i = 0; i < len; i++)
        {
            res += rchar (digit);
            if (i == point)
            {
                res += ".";
            }
        }
        return res;
    }
}

ostream & Gen::output_typedefs (ostream &out)
{
    FunctionSPtr main = functions.front ();

    if (knobs.standalone())
    {
        out <<      "\n\nstruct PS_OUTPUT\n{\n";
        unsigned k = 0;
        for (TypeList::const_iterator i = main->ret_type.begin ();
             i != main->ret_type.end ();
             i++)
        {
            out << *i << " color" << k << " :" << "COLOR" << k << ";\n";
            k++;
        }
        out << "};\n";
    }
#if 0
    else 
    {
        out << "#ifdef STANDALONE_DEFS\n"
            << "#define D_IN(t,name) in t name\n"
            << "#define D_OUT(t,name) out t name\n"
            << "#define D_INOUT(t,name) inout t name\n"
            << "#define D_VPOS : VPOS\n"
            << "#define D_COLOR0 : COLOR0\n"
            << "#define D_COLOR1 : COLOR1\n"
            << "#define D_COLOR2 : COLOR2\n"
            << "#define D_COLOR3 : COLOR3\n"
            "\n\nstruct PS_OUTPUT\n{\n";

        unsigned k = 0;
      
        for (TypeList::const_iterator i = main->ret_type.begin ();
             i != main->ret_type.end ();
             i++)
        {
            out << *i << " color" << k << " " << "D_COLOR" << k << ";\n";
            k++;
        }
        out << "};\n"
            << "#endif\n"
            << "#ifdef WRAPPED_DEFS\n"
            << "#define D_IN(t,name)  t name\n"
            << "#define D_OUT(t,name) t&name\n"
            << "#define D_INOUT(t,name) t&name\n"
            << "etc etc\n"
            << "#include \"something here\"\n"
            << "#endif\n";
    }
#endif

    return out;
}

// Create a new and unique name of the right type in the symbol table
Decl Gen::new_variable (Scope scope,
                        Type type, 
                        Kind k,
                        const Context &ctx)
{
    return  k == Initialized 
        ? new_initialized_variable (scope, type, ctx)
        : new_uninitialized_variable (scope, type);
}

Decl Gen::new_variable (Scope scope,
                        const TypeList & type)
{
    Decl newdecl = Decl (scope, Struct);
    // We ignore the type, there's only one struct.
    (void) type;
    symtable.push_back (newdecl);
    return newdecl;
}

Decl Gen::new_uninitialized_variable (Scope scope,
                                      Type type,
                                      unsigned idx)
{
    Decl newdecl = Decl (scope, type, idx);
    symtable.push_back (newdecl);
    return newdecl;
}

Decl Gen::new_uninitialized_variable (Scope scope,
                                      Type type)
{
    Decl newdecl = Decl (scope, type);
    symtable.push_back (newdecl);
    return newdecl;
}

Decl Gen::new_initialized_variable (Scope scope,
                                    Type type,
                                    const Context &ctx)
{
    Decl newdecl = Decl (scope, type, create_expression (this, type, ctx));
    symtable.push_back (newdecl);
    return newdecl;    
}

//////////////////////////////////////////////////////////////////////
// Contexts
// A context is a structure which describes the conditions under something else
// is being run. An expression can use the context to work out if it is in a
// for loop or which function it's in.

Context::Context (unsigned _owner,
                  ForSPtr  _loop,
                  UList *_samplers,
                  FunctionList *_callees)

    : func (_owner), loop (_loop) ,  depth (0), relop_p (false),
      samplers (_samplers),
      callees (_callees)
{
}

Context Context::deeper () const
{
    Context res = *this;
    res.depth++;
    return res;
}

Context Context::relop () const
{
    Context res =deeper ();
    res.relop_p = true;
    return res;
}

Decl Gen::gen_array_decl (Type t,
                          const Context &ctx, 
                          Dir d)
{
    if (knobs.array_reuse (&rand))
    {
        DeclVec::iterator i = existing_decl (t, ctx, d);
        if (i != symtable.end ())
        {
            return *i;
        }
    }
    return new_uninitialized_variable (Scope (Uniform), t);
}

Decl Gen::fetch_decl (Type  t, const Context & ctx, Kind k, Dir dir)
{
    if (knobs.variable_reuse (&rand))
    {
        DeclVec::iterator i = existing_decl (t, ctx, dir);
        if (i != symtable.end ())
        {
            return *i;
        }
    }
    return new_variable (Scope (Static, ctx.func), t, k, ctx);
}

//////////////////////////////////////////////////////////////////////
// Simply reformat the output to make it look pretty.

class Reformat
{
    string res;
    string pending_line;
    std::vector <unsigned> indent_stack;
    unsigned last_indent;
    bool pending_nl;
    char prev_char;
    char this_char;
    bool had_open_p;
    bool had_eq;
    unsigned raw_line_length;
    string tab ()
    {
        string t;
        unsigned i;
        for (i = 0; i < last_indent; i++)
        {
            t += " ";
        }
        return t;
    }
    void write (string x)
    {
        pending_line += x;
    }
    unsigned pos_on_line ()
    {
        return last_indent + pending_line.size ();
    }
    void flush_pending_line ()
    {
        if (!pending_line.empty ())
        {
            res += tab ();
            res += pending_line;
            res += "\n";
            pending_line = "";
        }
        pending_nl = false;
        had_open_p = false;
        raw_line_length = 0;  
        last_indent = indent_stack.back ();
    }
    void tabout_and_writeln (string x)
    {
        tab ();
        write (x);
        write ("\n");
    }
public:
    Reformat (string src) : res (""), 
                            pending_line (""), 
                            last_indent (0), 
                            pending_nl (false), 
                            prev_char (' '), 
                            this_char (' '), 
                            had_open_p (false),
                            had_eq (false),
                            raw_line_length (0)

    {
        indent_stack.push_back (0);
        prev_char = '\n';
        for (string::iterator p = src.begin (); p != src.end (); p++)
        {
            this_char = *p;

            if (raw_line_length == 0)
            {
                raw_line_length = last_indent;
                for (string::iterator q = p;
                     q != src.end () && *q != '\n';
                     q++) 
                {
                    raw_line_length++;
                }
            }
                  
            switch (this_char)
            {
            case '{':
                flush_pending_line ();
                pending_line = "{";
                flush_pending_line ();
                prev_char = this_char;
                last_indent = indent_stack.back () + 2;
                indent_stack.push_back (last_indent);
                break;
                // fall 
            case '(':
                if (raw_line_length > 80 && pos_on_line () > 70)
                {
                    flush_pending_line ();
                }

                if (pos_on_line () < 30) 
                {
                    indent_stack.push_back (pos_on_line ());
                }
                else
                {
                    indent_stack.push_back (indent_stack.back ()+1);
                }
                pending_line += this_char;
                prev_char = this_char;

                break;

            case '}':

                flush_pending_line ();
                indent_stack.pop_back ();
                pending_line = "}";
                last_indent = indent_stack.back ();
                flush_pending_line ();


                prev_char = '\n';
                break;

            case ')':
                prev_char = this_char;
                indent_stack.pop_back ();
                pending_line += this_char;
                break;
            case '\n':
                pending_nl = false;
                flush_pending_line ();
                prev_char = this_char;
                break;
            case ' ':
                if (prev_char == ' '
                    || prev_char == '('
                    || prev_char == '\n'
                    || prev_char == '{')
                {
                    break;
                }
                pending_line += this_char;
                prev_char = this_char;
                break;

            case ';':
                pending_line += this_char;
                prev_char = this_char;
                if (had_eq)
                {
                    had_eq = false;
                    indent_stack.pop_back ();
                }
                break;

            case '=':
                pending_line += this_char;
                prev_char = this_char;
                if (!had_eq) 
                {
                    had_eq = true;
                    indent_stack.push_back (indent_stack.back () + 4);
                }

                break;

            default:
                // no need to break line if we know it will fit

                if (raw_line_length > 80) {
                    if (prev_char == ' '
                        && (!isalpha (this_char))
                        && (!isdigit (this_char))
                        && pos_on_line () > 60)
                    {
                        flush_pending_line ();
                    }
                    else
                        if ( (prev_char == ' '
                              && pos_on_line () > 100)
                             || pending_nl)
                        {
                            flush_pending_line ();
                        }
                }
                if (prev_char == ';')
                {
                    pending_line += ' ';
                }

                pending_line += this_char;
                prev_char = this_char;
                break;
            }
        }
        flush_pending_line ();
    }
    string str ()
    {
        return res;
    }
};

ostream &
Gen::output_boiler_plate (ostream &out) const
{
    // isnan functions
    out <<
        "float isnan1 (float a)\n"
        "{\n"
        "return ((a<0) && (a>0));"
        "}\n"
        "float2 isnan2 (float2 a)\n"
        "{\n"
        "return ((a<0) && (a>0));"
        "}\n"
        "float4 isnan4 (float4 a)\n"
        "{\n"
        "return ((a<0) && (a>0));"
        "}\n";

    out << 
        "float4 quick_mod (float4 a, float4 b)\n"
        "{\n"
        "float4 d = a / b;\n"
        "float4 q = d - frac (d);\n"
        "float4 r = a - q * b;\n"
        "r -= frac(r);\n"
        "return isnan4 (r) ? 0 : r;\n"
        "}\n"
        "float opcond(float x, float y, float z)\n"
        "{\n"
        "return x ? y : z;\n"
        "}\n"
        "float2 opcond2(float2 x, float2 y, float2 z)\n"
        "{\n"
        "return x ? y : z;\n"
        "}\n"
        "float4 opcond4(float4 x, float4 y, float4 z)\n"
        "{\n"
        "return x ? y : z;\n"
        "}\n";

    static char * floats[2][4] =
        { { "float", "float2",   "float4", 0},
          {"1",      "2",        "4",      0} };
    static char * ops[2][9] = 
        { {"lt", "le", "gt", "ge", "eq", "ne", "and", "or", 0},
          {"<",  "<=", ">",  ">=", "==", "!=", "&&",  "||", 0} };

    for (unsigned i = 0; floats[0][i]; i++)
    {
         for (unsigned t = 0; ops[0][t]; t++)
         {
             out << floats[0][i] << " op" << ops[0][t] << floats[1][i] 
                 << "(" << floats[0][i] << " a, " << floats[0][i]  << " b) "
                 << "{ return isnan" << floats[1][i] << " (a) ? 0 : (isnan"
                 << floats[1][i] << " (b) ? 0 : (a" << ops[1][t] << "b));\n}";
        }

    }
    return out;
}

std::ostream & operator << (std::ostream &out, const FunctionSPtr&x)
{
    x->output_code (out);
    return out;
}

std::ostream & operator << (std::ostream &out, 
                            const Program &functions)
{
    std::copy (functions.rbegin (), 
               functions.rend (),
               std::ostream_iterator<FunctionSPtr> (out,"\n"));
    return out;
}

std::ostream & operator << (std::ostream &out,  gspair x)
{
    x.first->output_declarations (out, x.second);
    return out;
}

static OutputInfo::ArgSize translate_type (Type t)
{
    switch (t.type)
    {
    case Float4:
        return OutputInfo::Float4;
    case Float2:
        return OutputInfo::Float2;
    case Float:
        return OutputInfo::Float1;
    case NoType:
    case Int:
    case Int4:
    case SamplerFloat4:
    case SamplerSize:
    case Struct:
    case Float4ConstArray:
    case Float4Array:
    default:
        PURITAN_ABORT ("Unexpected type " << t.type);
        break;
    }
}

string Gen::generate (OutputInfo *output_info)
{
    ostringstream output_stream;
    ostringstream comments;

    // Reset the declaration counter so names start at 0 for each new file.
    Decl::reset ();

    declare_samplers ();
    create_call_structure ();
    create_control_structure ();

    output_typedefs (output_stream);

    output_stream << gspair (this, Scope (Sampler))
                  << gspair (this, Scope (Uniform))
                  << gspair (this, Scope (StaticConstArrays));

    output_boiler_plate (output_stream);

    output_stream << functions
                  << "/* cn=" << code_nodes << " en=" << exp_nodes << "*/\n";

    comments << "/*\n"  << knobs << "*/\n"
             << "/*\n Coverage to this point\n"
             << *Coverage::head
             << "*/\n";

    Reformat reformatted (output_stream.str ());
    

    // Fill in the output description with what we've been up to.
    if (output_info)
    {
        output_info->n_samplers = n_samplers;

        PURITAN_ASSERT (output_info->uniforms.empty(),"Incorrect start state");
        PURITAN_ASSERT (output_info->returns.empty(), "Incorrect start state");

        FunctionSPtr func = functions[0];

        for (TypeList::const_iterator i = func->ret_type.begin();
             i != func->ret_type.end();
             i++)
        {
            output_info->returns.push_front (translate_type (*i));
        }

        for (std::vector<std::pair <Type, string> >::const_iterator 
             i = uniforms.begin();
             i != uniforms.end(); 
             i++)
        {
            std::pair <OutputInfo::ArgSize, std::string> 
                x(translate_type (i->first), i->second);
            output_info->uniforms.push_front (x);
        }
    }
    return comments.str () + reformatted.str ();
}

Gen::Gen (Knobs & _config)
    :
    rand (_config.seed.get ()),
    knobs (_config),
    exp_nodes (0),
    code_nodes (0),
    max_funcs (coverage (knobs.func_count, &cfunc_count)),
    n_samplers (coverage (knobs.sampler_count, &csampler_count)),
    n_uniforms (coverage (knobs.uniform_count, &cuniform_count))
{

}


// Return iterator pointing to an existing decl with the suggested type and
// visible in the context.  If it's an argument formal make sure that the
// direction of intended action is ok.
DeclVec::iterator Gen::existing_decl (Type  t,
                                      const Context &ctx,
                                      Dir d)
{
  size_t length = symtable.size ();
  size_t scan_from = rand.srange (0, length);

  // Scan from the middle somewhere, and if that doesn't find
  // anything, start again at the start.

  DeclVec::iterator starts[2] = 
    { symtable.begin () + scan_from, symtable.begin()};
  DeclVec::iterator ends[2] = 
    { symtable.end(), symtable.begin() + scan_from };

  for (unsigned i = 0; i < 2; i++)
    {
      for (DeclVec::iterator res = starts[i];
           res != ends[i];
           res++)
        {
          // It's usable if we're writing, or it's initialized, or an
          // incoming argument. Also, check if assignments it are allowed, if
          // we are writing.
          if (res->type == t 
              && res->scope.visible_in (ctx, d)
              && (res->initializer.get()
                  || d == Write
                  || res->scope.scope == Argument_I
                  || res->scope.scope == Argument_IO)
              && (!res->noWrites || d == Read))
            {
              return res;
            }
        }
    }
  return symtable.end ();
}


unsigned Decl::counter = 0;

Decl::Decl () : scope (NoScope), type (NoType), idx (0), noWrites (false)
{
}

Decl::Decl (bool) : scope (NoScope), type (NoType), idx (0), noWrites (false)
{
}

Decl::Decl (Scope _scope,
            Type _type)
    : scope (_scope), type (_type),   idx (++counter), noWrites (false)
{
}

Decl::Decl (Scope _scope,
            Type _type,
            unsigned _idx)
    : scope (_scope), type (_type),   idx (_idx), noWrites (false)
{
}


// Return a number according to the knob range, and increment the coverage
// counter for it.
int Gen::coverage (const RangeKnob &knob,
                   IntCoverage *marker) 
{
    int r = knob (&rand);
    marker->increment (r);
    return r;
}

// As above, but the value is scaled so that minor functions get smaller 
// values.
int Gen::coverage (unsigned fidx,
                   const RangeKnob &knob,
                   IntCoverage *marker)
{
    int r = knob (&rand);
    if (fidx > 0) 
    {
        r = (static_cast<int> ( (static_cast<double> (r)) 
                                * knobs.func_trim.get ()));
    }
    marker->increment (r);
    return r;
}

//////////////////////////////////////////////////////////////////////
// Decl class
// Knows the scope, type, name and initializer of every variable.

Decl::Decl (Scope _scope,
            Type _type,
            ENode _init) 
    : scope (_scope), 
      type (_type), 
      initializer (_init), 
      idx (++counter)
{
}

void Decl::reset ()
{
    Decl::counter = 0;
}

Scope::Scope (scope_t _scope) : scope (_scope), fnc_idx (0)
{
}

std::ostream & operator << (std::ostream &out, const Decl&x)
{
    if (x.type == SamplerSize)
    {
        out << "in" << x.idx << "_size";
    }
    else
    {
        out << x.scope.short_name () << x.type.short_name () << x.idx;
    }
    return out;
}

//////////////////////////////////////////////////////////////////////
// Type

Type::Type () : type (NoType) {}
Type::Type (enum type _type) : type (_type) {}

enum type Type::operator () () const
{
    return type;
}

bool Type::array_p ()
{
    return type == Float4ConstArray
        || type == Float4Array;
}


std::ostream & operator << (std::ostream &out,  Type x)
{
    const char *w;
    switch (x.type)
    {
    case Int:
        w= "int";
        break;
    case Int4:
        w="int4";
        break;
    case Float:
        w= "float";
        break;
    case Float2:
        w= "float2";
        break;
    case Float4:
    case SamplerSize:
        w= "float4";
        break;
    case Float4ConstArray:
        w ="floatc4x40";
        break;
    case Float4Array:
        w = "floatv4x40";
        break;
    case Struct:
        w = "struct PS_OUTPUT";
        break;
    default:
    case NoType:
    case SamplerFloat4:
        PURITAN_ABORT ("Argument error" << x.type);
    }
    return (out << w);

}

string Type::short_name () const
{
    switch (type)
    {
    case Int:
        return "i_";
    case Int4:
        return "i4_";
    case Float:
        return "f1_";
    case Float2:
        return "f2_";
    case Float4:
        return "f4_";
    case Float4ConstArray:
        return "carray_";
    case Float4Array:
        return "farray_";
    case SamplerFloat4:
        return "";
    case Struct:
        return "s_";
    default:
    case NoType:
    case SamplerSize:
        PURITAN_ABORT ("Argument error:" << type);
    }
}

string Type::suffix_name () const
{
    switch (type)
    {
    case Int:
        return "i";
    case Int4:
        return "i4";
    case Float:
        return "1";
    case Float2:
        return "2";
    case Float4:
        return "4";
    case Float4ConstArray:
        return "c";
    case Float4Array:
        return "f";
    case SamplerFloat4:
        return "";
    case Struct:
        return "s";
    default:
    case NoType:
    case SamplerSize:
        PURITAN_ABORT ("Argument error:" << type);
    }
}

bool Type::operator != (Type x) const
{
    return x.type != type;
}

bool Type::operator == (Type x) const
{
    return x.type == type;
}

std::ostream & operator << (std::ostream &out, const TypeList &x) 
{
    PURITAN_ASSERT (x.size () == 1, "Should never be called with a non unary list");
    out << x.front ();
    return out;
}


}
}
 
