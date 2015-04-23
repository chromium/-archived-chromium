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


#include <iostream>
#include "puritan_assert.h"
#include "exp_gen.h"
#include "structure_gen.h"
#include "knobs.h"

namespace Salem
{
namespace Puritan
{


const char *f2tof_swizzle_names[] =
{
    "x", "y", 0
};

StrCoverage f2tof_swizzles ("F2tof Swizzles", 
                            f2tof_swizzle_names);

const char *f2tof2_swizzle_names[] =
{
    "xx", "yy", "xy", "yx", 0
};

StrCoverage f2tof2_swizzles ("F2tof2 Swizzles", 
                             f2tof2_swizzle_names);

const char *f4tof_swizzle_names[] =
{
    "x", "y", "z", "w", 0
};

StrCoverage f4tof_swizzles ("F4tof Swizzles", 
                            f4tof_swizzle_names);

const char *f4tof2_swizzle_names[] =
{
    "xx", "xy", "xw", "xz",
    "yy", "yx", "yw", "yz",
    "wx", "wy", "ww", "wz",
    "zx", "zy", "zw", "zz",
    0
};

StrCoverage f4tof2_swizzles ("F4tof2 Swizzles", 
                             f4tof2_swizzle_names);

const char *f4tof4_swizzle_names[] =
{
    "x", "y", "z", "w", 0
};

StrCoverage f4tof4_swizzles ("f4tof4 Swizzles", 
                             f4tof4_swizzle_names);

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// This code generates controlled random expressions for Puritan.

// During generation, various knobs and random numbers determine the complexity
// of the expression and push various operands into a list. Then the code
// builds an expression tree removing things from the list until there's
// nothing left.
//

//////////////////////////////////////////////////////////////////////

// "frac" removed because it can cause discrepant CPU vs GPU results
//  - see ticket #3493
const char *unfunc_names[] =
{ "abs", /* "frac", */ "exp2", "log2",
  "rcp", "rsqrt", "sqrt", "!", 0
};

StrCoverage unfuncs ("Unary functions", unfunc_names);

const char *binfunc_names[] =
{ "max", "min", "dot", 0 };

StrCoverage binfuncs ("Binary functions", binfunc_names);

const char *trifunc_names[] =
{ "mad", "opcond", 0};

StrCoverage trifuncs ("Trinary functions", trifunc_names);

const char *unop_names[] =
{ "-", 0 };

StrCoverage unops ("Unops", unop_names);

const char *binop_names[] =
{ "*", "/", "+", "-", 0 };

StrCoverage binops ("Binops", binop_names);

// "eq", "ne" removed because these are very sensitive to precision errors
// and could cause discrepant CPU vs GPU results
const char *relop_names[] =
{
    "lt", "le", "gt", "ge",
    /* "eq", "ne", */ "and", "or",
    0
};

StrCoverage relops ("Relops", relop_names);

const char *special_names[] =
{
    "* 2.0", "* 4.0", "* 8.0",
    "/ 2.0", "/ 4.0", "/ 8.0",
    "1.0 -", "1.0-2.0*", 0
};

StrCoverage special ("Special", special_names);

std::ostream & operator << (std::ostream & out,
                            const ENode & x)
{
    x->print (out);
    return out;
}

void print_e (const ENode &x)
{
    std::cerr << x;
}

std::ostream & operator << (std::ostream & out,
                            const EList & x)
{
    bool need_comma = false;

    for (EList::const_iterator i = x.begin (); i != x.end ();
         i++)
    {
        if (need_comma)
        {
            out << ", ";
        }
        out << *i;
        need_comma = true;
    }
    return out;
}

//////////////////////////////////////////////////////////////////////
// Generate expressions..

// Remove one operand from the stack of expressions and convert it to the
// prevailing type.

ENode one_arg (exp_list * exp, Rand * rand, const Type & type, Gen *gen)
{
    PURITAN_ASSERT (!exp->empty (), "Ran out of expressions");
    ENode arg = exp->back ();

    exp->pop_back ();

    return ENode (Exp::convert (arg, rand, type, gen));
}

// Create an expression tree returning with type RETURN_TYPE.
ENode create_expression (Gen * gen, Type return_type, const Context & ctx)
{
    Rand *rand = &gen->rand;
    static int depth;
    Type type = return_type;
    const Knobs & knobs = gen->knobs;
    exp_list exp;

    depth++;

    // We create some things regardless of the complexity settings.
    if (return_type == Float4 && knobs.float4_chance (rand))
    {
        // Make more args - call create_expression out of the arg list
        // because otherwise the expression eval order will be undefined.
        ENode a0 = (create_expression (gen, Float, ctx.deeper ()));
        ENode a1 = (create_expression (gen, Float, ctx.deeper ()));
        ENode a2 = (create_expression (gen, Float, ctx.deeper ()));
        ENode a3 = (create_expression (gen, Float, ctx.deeper ()));

        exp.push_back (ENode (new Float4Func (Float4, a0, a1, a2, a3)));
    }

    if (return_type == Float2 && knobs.float2_chance (rand))
    {
        ENode a0 = (create_expression (gen, Float, ctx.deeper ()));
        ENode a1 = (create_expression (gen, Float, ctx.deeper ()));

        exp.push_back (ENode (new BiIntrinsicFunc (Float2, "float2", a0, a1)));
    }

    if (ctx.callees && !ctx.callees->empty ())
    {
        FunctionSPtr target = ctx.callees->front ();
        EList actuals;

        ctx.callees->pop_front ();

        for (DeclList::iterator ai = target->formals.begin ();
             ai != target->formals.end (); 
             ai++)
        {
            switch (ai->scope.scope)
            {
                // input argument can be any expression
            case Argument_IO:
            case Argument_I:
            {
                ENode actual = create_expression (gen, ai->type, ctx.deeper ());
                actuals.push_back (actual);
                break;
            }
            case Argument_O:
            {
                ENode actual (new Variable
                              (gen->fetch_decl (ai->type, 
                                                ctx.deeper (), 
                                                Uninitialized, 
                                                Read)));

                actuals.push_back (actual);
                break;
            }

            default:
            case Uniform:
            case Static:
            case StaticConstArrays:
            case Sampler:
            case NoScope:
            {
                PURITAN_ABORT ("Bad scope" << ai->scope.scope);
                break;
            }
            }
        }

        exp.push_back
            (ENode (new FCallTemplate (target->ret_type, target, actuals)));
    }

    if (ctx.samplers && !ctx.samplers->empty ())
    {
        // Have to get the arguments in range
        // tex2D (id<n>, <exp> % id<n>_size)
        unsigned sampler = ctx.samplers->front ();

        ctx.samplers->pop_front ();
        ENode lhs = create_expression (gen, Float4, ctx.deeper ());
        ENode rhs (new SamplerSizeRef (sampler));
        ENode mod (new BiIntrinsicFunc (Float4, "quick_mod", lhs, rhs));

        exp.push_back (ENode (new SamplerRef (sampler, mod)));
    }

    // Build as many terms as we were asked for.
    int l = gen->knobs.expression_depth (rand);

    // Children functions get fewer nodes.
    if (ctx.func)
    {
        l = l * 4 / 6 + 1;
    }

    for (int i = 0; i < l; i++)
    {
        if (knobs.type_change_chance (rand))
        {
            type = gen->random_type ();
        }

        if (knobs.array_use (rand))
        {
            Type t = knobs.array_constness (rand)
                ? Float4ConstArray : Float4Array;

            Decl decl = gen->gen_array_decl (t, ctx.deeper (), Read);
            ENode v (new Variable (decl));
            ENode idx;

            if (knobs.array_index_const (rand) || ctx.loop.get() == NULL)
            {
                idx = ENode (new Constant (Float, 
                                           rand->range (0, array_size)));
            }
            else
            {
                idx = ENode (new Variable (ctx.loop->counter));
            }

            ENode id (new Index (v, idx));
            exp.push_back (id);
        }
        else
        {
            if (knobs.uniform_chance (rand) && gen->n_uniforms)
            {
                size_t uni_idx = rand->srange (0, gen->n_uniforms);
                std::vector <std::pair <Type, string> >::iterator uni
                    = gen->uniforms.begin () + uni_idx;
                ENode e (new UniformRef (uni->first, uni->second));
                exp.push_back (e);
            }
            else if (ctx.depth > 10 || knobs.constant_use (rand))
            {
                ENode e (new Constant (Float, gen->gen_fconstant ()));
                exp.push_back (e);
            }
            else
            {
                Type t
                    = knobs.type_change_chance (rand)
                    ? gen->random_type () : type;

                Decl decl = gen->fetch_decl (t, 
                                             ctx.deeper (), 
                                             Initialized, 
                                             Read);
                ENode v (new Variable (decl));

                exp.push_back (v);
            }
        }

        // Stop making nodes if we've gone too far.
        gen->exp_nodes++;
        if (gen->exp_nodes >= knobs.exp_limit.uget ())
        {
            break;
        }
    }

    // Now add enough operators to consume all but one of the terms, the final
    // term is the result of the expression.

    bool no_more_unarys = false;

    while (exp.size () != 1)
    {
        if ((type == Float
             && knobs.relop_chance (rand))
            || (ctx.relop_p
                && exp.size () < 3 && knobs.relop_cond_chance (rand)))
        {
            exp.push_back 
                (ENode (new Relop (relops.choose (rand),
                                   one_arg (&exp, rand, Float, gen),
                                   one_arg (&exp, rand, Float, gen))));

        }
        else if (knobs.special_chance (rand))
        {
            exp.push_back 
                (ENode (new SpecialPhrase (type,
                                           special.choose (rand),
                                           one_arg (&exp, rand, type, gen))));
        }
        else if (knobs.func_chance (rand))
        {
            if (exp.size () > 2 && knobs.trifunc_chance (rand))
            {
                string name = trifuncs.choose (rand);

                
                // name of opcond function changes by type
                if (name == "opcond")
                {
                    switch (type.type) 
                    {
                    case Float4:
                        name = "opcond4";
                        break;
                    case Float2:
                        name = "opcond2";
                        break;
                    case Float:
                        name = "opcond";
                        break;
                    case NoType:
                    case Int:
                    case Int4:
                    case SamplerFloat4:
                    case SamplerSize:
                    case Struct:
                    case Float4ConstArray:
                    case Float4Array:
                    default:
                        PURITAN_ABORT ("Illegal type for opcond" << type.type) ;
                        break;
                    }
                }
                    
                exp.push_back 
                    (ENode (new TriIntrinsicFunc (type,
                                                  name,
                                                  one_arg (&exp, rand, type, gen),
                                                  one_arg (&exp, rand, type, gen),
                                                  one_arg (&exp, rand, type, gen))));
            }
            else if (exp.size () > 1 && knobs.binfunc_chance (rand))
            {
                string f = binfuncs.choose (rand);
                Type ret_type = f == "dot" ? Float : type;

                ENode lhs = one_arg (&exp, rand, type, gen);
                ENode rhs = one_arg (&exp, rand, type, gen);
                exp.push_back 
                    (ENode (new BiIntrinsicFunc (ret_type,
                                                 f,
                                                 lhs,
                                                 rhs)));
                type = ret_type;
            }
            else
            {
                exp.push_back 
                    (ENode (new UnIntrinsicFunc (type,
                                                 unfuncs.choose (rand),
                                                 one_arg (&exp, rand, type, gen))));
            }
        }
        else if (!no_more_unarys && knobs.unary_chance (rand))
        {
            exp.push_back (ENode (new Unop (type, 
                                            unops.choose (rand), 
                                            one_arg (&exp, rand, type, gen))));
        }
        else if (knobs.swizzle_chance (rand))
        {
            exp.push_back (Exp::convert (one_arg (&exp, rand, type, gen), rand, type, gen));
        }
        else
        {
            exp.push_back 
                (ENode (new Binop (type, binops.choose (rand),
                                   one_arg (&exp, rand, type, gen),
                                   one_arg (&exp, rand, type, gen))));
        }
    }

    return Exp::convert (exp.back (), rand, return_type, gen);

}
//////////////////////////////////////////////////////////////////////
/// ENode Template, base class for all expression nodes.

Exp::Exp (Type _type) :type (_type)
{
}

Exp::~ Exp ()
{
}

bool Exp::is_fconstant () const
{
    return false;
}

// Convert a tree from one type to another, return the new tree.
ENode Exp::convert (ENode from, Rand * rand, Type to, Gen *gen)
{
    string pre;
    string post;
    unsigned n = 1;

    if (from->type == to.type && !gen->knobs.copy_swizzle_chance (rand))
    {
        n = 0;
    }
    else if (from->type == Float && to.type == Float)
    {

    }
    else if (from->type == Float2 && to.type == Float2)
    {
        post = f2tof2_swizzles.choose (rand);
    }
    else if (from->type == Float && to.type == Float2)
    {
        n = 1;
        pre = "float2";
    }
    else if (from->type == Float && to.type == Float4)
    {
        n = 1;
        pre = "float4";
    }
    else if (from->type == Float2 && to.type == Float)
    {
        post = f2tof_swizzles.choose (rand);
    }
    else if (from->type == Float2 && to.type == Float4)
    {
        // Can't spread out a float2
        pre = "float4";
        n = 2;
    }
    else if (from->type == Float4 && to.type == Float)
    {
        post = f4tof_swizzles.choose (rand);
    }
    else if (from->type == Float4 && to.type == Float2)
    {
        post = f4tof2_swizzles.choose (rand);
    }
    else if (from->type == Float4 && to.type == Float4)
    {
        for (unsigned i = 0; i < 4; i++)
            post += f4tof4_swizzles.choose (rand);
    }
    else
    {
        PURITAN_ABORT ("Bad args in convert");
    }

    if (from->is_fconstant ())
    {
        n = 1;
    }
    return ENode (new Convert (to, from->type, pre, post, n, from));
}

//////////////////////////////////////////////////////////////////////
/// Boilerplate

A0::A0 (Type ty) :Exp (ty)
{
}

A1::A1 (Type ty, ENode _child) :Exp (ty), child (_child)
{
}

A2::A2 (Type ty, ENode _lhs, ENode _rhs) :Exp (ty),
                                          lhs (_lhs), rhs (_rhs)
{
}

A3::A3 (Type ty, ENode _a0, ENode _a1,
        ENode _a2) :Exp (ty), a0 (_a0), a1 (_a1),
                    a2 (_a2)
{
}

A4::A4 (Type ty, ENode _a0, ENode _a1, ENode _a2,
        ENode _a3) :Exp (ty), a0 (_a0), a1 (_a1),
                    a2 (_a2), a3 (_a3)
{
}

//////////////////////////////////////////////////////////////////////
/// Unary operations

Unop::Unop (Type ty, string _name, ENode _child) 
    :A1 (ty, _child), name (_name)
{
}

void Unop::print (ostream & out) const
{
    out << " ( " << name << " " << child << ") ";
}

//////////////////////////////////////////////////////////////////////
/// Function calls.

FCallTemplate::FCallTemplate (TypeList ty,
                              FunctionSPtr _target,
                              EList _actuals) 
    :Exp (ty.front ()), target (_target), actuals (_actuals)
{
}

void FCallTemplate::print (ostream & out) const
{
    out << "func" << target->idx << " (" << actuals << ")";
}

//////////////////////////////////////////////////////////////////////
/// Sampler references.

SamplerRef::SamplerRef (unsigned _n, ENode _child) 
    :A1 (Float4, _child), n (_n)
{
}

void SamplerRef::print (ostream & out) const
{
    out << "tex2D (in" << n << ", " << child << ")";
}

//////////////////////////////////////////////////////////////////////
/// Sampler references.

SamplerSizeRef::SamplerSizeRef (unsigned _n) 
    :A0 (Float4), n (_n)
{
}

void SamplerSizeRef::print (ostream & out) const
{
    out << "in" << n << "_size";
}

//////////////////////////////////////////////////////////////////////
/// Uniform references.

UniformRef::UniformRef (Type ty, string v) 
    :A0 (ty), name (v)
{
}

void UniformRef::print (ostream & out) const
{
    out << name;
}

//////////////////////////////////////////////////////////////////////
/// Constants.

Constant::Constant (Type ty, string _val) 
    :A0 (ty), val (_val)
{
}

void Constant::print (ostream & out) const
{
    out << val;
}

Constant::Constant (Type ty, unsigned _val) 
    :A0 (ty)
{
    ostringstream o;

    o << _val;
    val = (o.str ());
}
bool Constant::is_fconstant () const
{
    return true;
}

//////////////////////////////////////////////////////////////////////
/// Variables.

Variable::Variable (Decl _d) 
    :A0 (_d.type ()), decl (_d)
{
}

void Variable::print (ostream & out) const
{
    out << " " << decl;
}

//////////////////////////////////////////////////////////////////////
/// Indexes into arrays.

Index::Index (ENode child, ENode idx) 
    :A2 (Float4, child, idx)
{
}

void Index::print (ostream & out) const
{
    out << lhs << "[" << rhs << "]";
}

//////////////////////////////////////////////////////////////////////
/// Constant array references.

ConstArrayRef::ConstArrayRef (Decl _d) 
    :A0 (Float4), decl (_d)
{
}

void ConstArrayRef::print (ostream & out) const
{
    out << " " << decl;
}

//////////////////////////////////////////////////////////////////////
/// lhs swizzles

Swizzle::Swizzle (Type ty, string _name, ENode _child) 
    : A1 (ty, _child), name (_name)
{

}

void Swizzle::print (ostream & out) const
{
    out << child << "." << name;
}

//////////////////////////////////////////////////////////////////////
/// Variables on the LHS of an assignment.

LHSVariable::LHSVariable (Decl _d, bool _swizzled) 
    :Exp (_d.type ()), decl (_d), swizzled (_swizzled)
{
}

void LHSVariable::print (ostream & out) const
{
    out << " " << decl;
}

//////////////////////////////////////////////////////////////////////
/// Special phrases.

SpecialPhrase::SpecialPhrase (Type t, string _name, ENode _child) 
    :A1 (t, _child), name (_name)
{
}

void SpecialPhrase::print (ostream & out) const
{
    if (name[0] == '1')
    {
        out << " " << name << child;
    }
    else
    {
        out << child << " " + name;
    }
}

//////////////////////////////////////////////////////////////////////
/// Unary intrinsic

UnIntrinsicFunc::UnIntrinsicFunc (Type ty, string _name,
                                  ENode _child) 
    :A1 (ty, _child),
     name (_name)
{
}

void UnIntrinsicFunc::print (ostream & out) const
{
    if (name == "!")
    {
        out << "(!(" << child << "))";
    }
    else
    {
        out << " " << name << " (" << child << ")";
    }
}

//////////////////////////////////////////////////////////////////////
/// Binary intrinsic

BiIntrinsicFunc::BiIntrinsicFunc (Type ty, string _name,
                                  ENode _lhs, ENode _rhs) 
    :A2 (ty, _lhs, _rhs), name (_name)
{
}

void BiIntrinsicFunc::print (ostream & out) const
{
    out << " " << name << " (" << lhs << ", " << rhs << ")";
}

//////////////////////////////////////////////////////////////////////
/// Tri intrinsic

TriIntrinsicFunc::TriIntrinsicFunc (Type ty, 
                                    string _name,
                                    ENode _a1,
                                    ENode _a2, 
                                    ENode _a3) 
    :A3 (ty, _a1, _a2, _a3), name (_name)
{
}

void TriIntrinsicFunc::print (ostream & out) const
{
    out << name << "(" << a0 << ", " << a1 << ", " << a2 << ")";
}

//////////////////////////////////////////////////////////////////////
/// Float4

Float4Func::Float4Func (Type ty, ENode _a1, ENode _a2,
                        ENode _a3, ENode _a4) 
    :A4 (ty, _a1, _a2, _a3, _a4)
{
}

void Float4Func::print (ostream & out) const
{
    out << "float4 (" << a0 << ", " << a1 << ", " << a2 << ", " << a3
        << ")";
}

//////////////////////////////////////////////////////////////////////
/// Convert

Convert::Convert (Type to,
                  Type _from,
                  string _pre, string _post, unsigned _n,
                  ENode _child) 
    :A1 (to, _child), from (_from), pre (_pre), post (_post), n (_n)
{
}

void Convert::print (ostream & out) const
{
    if (n == 0)
    {
        out << child;
    }
    else
    {
        out << pre << " (";
        for (unsigned i = 0; i < n; i++)
        {
            if (i)
            {
                out << ", ";
            }
            out << child;
        }
        out << ")";
        if (!post.empty ())
        {
            out << "." << post;
        }
    }
}

//////////////////////////////////////////////////////////////////////
/// Binop

Binop::Binop (Type ty, string _name, ENode _lhs,
              ENode _rhs) 
    :A2 (ty, _lhs, _rhs),
     name (_name)
{
}

void Binop::print (ostream & out) const
{
    out << lhs << " " << name << " " << rhs;
}

//////////////////////////////////////////////////////////////////////
/// Relop

Relop::Relop (string _name, ENode _lhs, ENode _rhs) 
    :A2 (Float, _lhs, _rhs), 
     name (_name)
{
}

void Relop::print (ostream & out) const
{
    out << "op" << name << type.suffix_name() << "(" << lhs << ", " << rhs << ")";
}

//////////////////////////////////////////////////////////////////////
/// AssOp

AssOp::AssOp (Type ty, string _name, ENode _lhs, ENode _rhs) 
    :A2 (ty, _lhs, _rhs),
     name (_name)
{
}

void AssOp::print (ostream & out) const
{
    out << lhs << " " << name << " " << rhs;
}

//////////////////////////////////////////////////////////////////////
/// SelfModOp

SelfModOp::SelfModOp (Type ty, string _name, ENode _child) 
    :A1 (ty, _child),
     name (_name)
{
}

void SelfModOp::print (ostream & out) const
{
    out << child << " " << name;
}

//////////////////////////////////////////////////////////////////////
///

//////////////////////////////////////////////////////////////////////

}
}
