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


#include "knobs.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include "rand.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

// Knobs control the behavior of Puritan and keep track of what's happened.

namespace Salem
{
namespace Puritan
{
static Knob *link;

Knobs::Knobs (void)
    :
    top_level_statements 
("--top-level-statements",
 "\t\tNumber of statements at the top level\n",
 1,
 3),

    for_count 
    ("--for-count",
     "\t\tNumber of for loops in code\n",
     0,
     2),

    while_count 
    ("--while-count",
     "\t\tNumber of while loops in code\n",
     0,
     2),

    do_count 
    ("--do-count",
     "\t\tNumber of do loops in code\n",
     0,
     1),

    if_count 
    ("--if-count",
     "\t\tNumber of ifs in code\n",
     0,
     5),

    block_count 
    ("--block-count",
     "\t\tNumber of blocks in code\n",
     0,
     3),

    code_limit
    ("--code-limit",
     "\t\tMaximum number of code nodes\n",
     200),

    exp_limit
    ("--exp-limit",
     "\t\tMaximum number of expression nodes\n",
     100),

    exp_count
    ("--exp-count",
     "\t\tNumber of expressions in code\n",
     6,
     16),

    func_count 
    ("--func-count",
     "\t\tNumber of functions in code\n",
     1,
     9),

    func_trim 
    ("--func-trim",
     "\t\tSize factor to apply to callee functions\n",
     0.7),
    
    arg_in_chance
    ("--args-in-chance",
     "\t\tChance argument may be just input\n",
     0.4),

    arg_out_chance
    ("--args-out-chance",
     "\t\tAfter input, chance argument may be just output\n",
     0.4),

    for_nesting 
    ("--for-nest",
     "\t\tNumber of for loops inside for loops\n",
     0,
     9),

    while_nesting 
    ("--while-nest",
     "\t\tNumber of while loops inside while loops\n",
     0,
     4),

    block_nesting 
    ("--block-nest",
     "\t\tNumber of blocks inside blocks\n",
     0,
     1),

    do_nesting 
    ("--do-nest",
     "\t\tNumber of do loops inside do loops\n",
     0,
     4),

    if_nesting 
    ("--if-nest",
     "\t\tNumber of ifs inside ifs\n",
     0,
     9),

    block_length 
    ("--block-length",
     "\t\tNumber of statements in a block.\n",
     2,
     7),

    expression_depth 
    ("--exp-depth",
     "\t\tRange of expression complexity\n",
     1,
     7),

    special_chance 
    ("--special-chance",
     "\t\tProbability at each expression point of a special phrase.\n",
     0.2),

    float4_chance
    ("--float4-chance",
     "\t\tProbability that an expression will have a float4 constructor.\n",
     .03),

    float4_struct_member_chance
    ("--float4-struct-member-chance",
     "\t\tProbability that an element of the output struct will be float4.\n",
     .90),

    float2_chance
    ("--float2-chance",
     "\t\tProbability that an expression will have a float2 constructor.\n",
     0.03),

    func_chance 
    ("--func-chance",
     "\t\tAfter special, probability of an intrinsic function.\n",
     0.1),

    term_chance 
    ("--terminal-chance",
     "\t\tAfter func, probability a terminal.\n",
     0.2),

    unary_chance 
    ("--unary-chance",
     "\t\tAfter terminal, probabaility of a unary op.\n",
     0.05),

    swizzle_chance 
    ("--swizzle-chance",
     "\t\tAfter unrary, chance that term will be a swizzle.\n",
     0.35),

    fcall_chance 
    ("--fcall-chance",
     "\t\tAfter swizzle, probability of a function call.\n",
     0.25),

    copy_swizzle_chance 
    ("--copy-swizzle-chance",
     "\t\tChance a simple non converting copy will be a swizzle.\n",
     0.3),

    noinline_chance 
    ("--fnoinline-chance",
     "\t\tProbability a function will be declared noinline.\n",
     0.25),

    lhs_swizzle_chance 
    ("--lhs-swizzle-chance",
     "\t\tProbabaility of a swizzle as an lval.\n",
     1.0),

    assop_chance 
    ("--assop-chance",
     "\t\tProbability of a fancy assignment operator as assignment.\n",
     0.1),

    selfmod_chance 
    ("--selfmod-chance",
     "\t\tProbability that an assignment will really be a selfmodify.\n",
     0.1),

    trifunc_chance 
    ("--trifunc-chance",
     "\t\tProbability of a trinary intrinsic.\n",
     0.2),

    binfunc_chance 
    ("--binary-chance",
     "\t\tAfter trinary, probabaility of a binary intrinsic instead of unary.\n",
     0.8),

    relop_chance 
    ("--relop-chance",
     "\t\tIf there's going to be a binop,"
     "probability that it will be a relation.\n",
     0.1),

    relop_cond_chance 
    ("--relop-cond-chance",
     "\t\tNear the end of an expression in a test, "
     "chance that the op will be a compare\n",
     0.9),

    type_change_chance 
    ("--type-change-chance",
     "\t\tProbability that the type of a subexpression will be different "
     "to the expression.\n",
     0.01),

    type_float4_chance 
    ("--type-float4-chance",
     "\t\tProbability that a subexpression type will be float4.\n",
     0.9),

    type_float2_chance 
    ("--type-float2-chance",
     "\t\tAfter float4,"
     "probability that a subexpression type will be float2 instead of float.\n",
     0.9),

    sampler_count 
    ("--sampler-count",
     "\t\tNumber of samplers used\n",
     0,
     9),

    sampler_chance 
    ("--sampler-chance",
     "\t\tProbability that an expression will use a sampler\n",
     .4),

    uniform_count 
    ("--uniform-count",
     "\t\tNumber of unforms used\n",
     0,
     9),

    uniform_chance 
    ("--unform-chance",
     "\t\tProbability that an expression will use a uniform\n",
     .4),

    arg_count 
    ("--arg-count",
     "\t\tNumber of arguments to functions\n",
     1,
     3),

    static_initializer_depth 
    ("--static-initializer-depth",
     "\t\tRange of static initializer expression complexity\n",
     1,
     2),

    standalone 
    ("--standalone",
     "\t\tIf the output should work outside the framework\n",
     false),

    seed 
    ("--seed",
     "\t\tSeed for the random number generator\n",
     0),

    variable_reuse 
    ("--variable-reuse",
     "\t\tRatio of variables reused to created in expressions\n",
     0.90),

    array_use 
    ("--array-use",
     "\t\tRatio of array terms in terms in expressions\n",
     0.2),

    array_reuse 
    ("--array-reuse",
     "\t\tRatio of array terms reused to created in expressions\n",
     0.95),

    array_constness 
    ("--array-constness",
     "\t\tProportion of array refs which are references to const arrays\n",
     1.),

    array_index_const
    ("--array-index-const",
     "\t\tProportion of array references which have a constant index\n",
     1.),

    array_in_for_use
    ("--array-in-for",
     "\t\tProportion of array references which use a loop index, lhs only.\n",
     0.),

    if_elses 
    ("--if-elses",
     "\t\tProportion of ifs which have elses.\n",
     0.30),

    loop_breaks 
    ("--loop-breaks",
     "\t\tProportion of loops which have breaks.\n",
     0.80),

    multiple_stmt 
    ("--multiple-stmt",
     "\t\tRatio of single statements to blocks.\n",
     0.30),

    constant_use 
    ("--constant-use",
     "\t\tRatio of constants to variables in expressions.\n",
     0.30),

    constant_small 
    ("--constant-small",
     "\t\tChance a constant will be between 0 and 1.\n",
     0.90),

//   random_names 
//   ("--random-names",
//    "\t\tShould names be random or easy on the eyes\n",
//    false),

    int_variables 
    ("--int-variables",
     "\t\tDeclare integer variables and use them in expressions\n",
     false),

    allow_two_negs 
    ("--allow-two-negs",
     "\t\tAllow two negs in an expression\n",
     false),
    first (link)
{
    link = 0;
}

RangeKnob::RangeKnob (const char *_name,
                      const char *_help,
                      int _from, int _to)
    :Knob (_name, _help),
     from (_from), to (_to)
{
}

IntKnob::IntKnob (const char *_name,
                  const char *_help,
                  int _val)
    :Knob (_name, _help), val (_val)
{
}

BoolKnob::BoolKnob (const char *_name,
                    const char *_help,
                    bool _x)
    :Knob (_name, _help), val (_x)
{
}

ProbKnob::ProbKnob (const char *_name,
                    const char *_help,
                    double _x)
    :Knob (_name, _help), prob (_x)
{
}

void RangeKnob::set (int a, int b)
{
    from = a;
    to = b;
}

// link together the members so we can iterate simply through the struct
// elements.


Knob::~Knob ()
{
}

Knob::Knob (const char *_name, const char *_help)
    :prev (link),
     name (_name), help (_help)
{
    link = this;
}


void RangeKnob::to_stream (std::ostream & out) const
{
    out << name << "=" << from;
    if (from != to)
        out << "," << to;
}

void IntKnob::to_stream (std::ostream & out) const
{
    out << name << "=" << val;
}

void BoolKnob::to_stream (std::ostream & out) const
{
    out << name << "=" << (val ? "t" : "f") ;
}

void BoolKnob::usage (std::ostream & out) const
{
    out << (name) << "=" << "[t|f] ";
    out << (val ? "{t}" : "{f}") << "\n" << help;
}

void ProbKnob::usage (std::ostream & out) const
{
    char t[1000];
    sprintf (t, "%s = <0.0 <= x <= 1.0> {%g}\n%s", name, prob, help);
    out << t;
}

void IntKnob::usage (std::ostream & out) const
{
    out << (name) << "=" << "<int>";
    out << "  {" << val << "}\n" << help;
}

void RangeKnob::usage (std::ostream & out) const
{
    out << (name) << "=" << "<int>,<int>";
    out << "  {" << from;
    if (from != to)
        out << "," << to;
    out << "}\n" << help;
}

bool BoolKnob::set_from_argument (const char *arg)
{
    if (arg[0] == 't' || arg[0] == '1')
    {
        val = true;
        return true;
    }
    if (arg[0] == 'f' || arg[0] == '0')
    {
        val = false;
        return true;
    }
    return false;
}

bool ProbKnob::set_from_argument (const char *arg)
{
    char *finish;
    prob = strtod (arg, &finish);
    return (arg != finish);
}

bool IntKnob::set_from_argument (const char *arg)
{
    char *finish;
    val = strtol (arg, &finish, 0);
    return (arg != finish);
}

int IntKnob::get () const
{
    return val;
}

unsigned IntKnob::uget () const
{
    return val;
}

bool RangeKnob::set_from_argument (const char *val)
{
    long int a, b;
    char *next;
    a = strtol (val, &next, 0);

    if (next == val)
    {
        return false;
    }

    if (*next == 0)
    {
        from = a;
        to = a;
        return true;
    }

    if (*next != ',')
    {
        return false;
    }

    char *next_2;
    next++;
    b = strtol (next, &next_2, 0);
    if (next_2 == next || *next_2 != 0)
        return false;

    from = a;
    to = b;
    return true;
}

bool Knobs::parse_argument (const char *arg)
{
    for (Knob * l = first; l; l = l->prev)
    {
        if (strncmp (l->name, arg, strlen (l->name)) == 0)
        {
            const char *val = strchr (arg, '=');
            if (val)
                return l->set_from_argument (val + 1);
        }
    }
    return false;
}

std::ostream & operator << (std::ostream &out, const Knobs& x)
{
    unsigned j = 0;
    for (Knob * l = x.first; l; l = l->prev)
    {
        std::ostringstream tmp;
        l->to_stream (tmp);
        out << std::setw(30) << tmp.str();
        j++;
        if (j == 3)
        {
            out << "\n";
            j = 0;
        }
    }

    return out;
}

std::string Knobs::usage ()
{
    std::ostringstream tmp;
    Knobs dummy = Knobs ();

    for (Knob * l = dummy.first; l; l = l->prev)
    {
        l->usage (tmp);
        tmp << "\n";
    }

    return tmp.str ();
}

bool BoolKnob::operator () () const
{
    return val;
}

void BoolKnob::set (bool x)
{
    val = x;
}

bool ProbKnob::operator () (Rand * r) const
{
    double rnd = r->rnd_flt ();
    return prob > rnd;
}

void ProbKnob::to_stream (std::ostream & out) const
{
    char t[1000];
    sprintf (t, "%s=%g", name, prob);
    out << t;
}

double ProbKnob::get () const
{
    return prob;
}

int RangeKnob::operator () (Rand * r) const 
{
    return r->range (from, to);
}

unsigned RangeKnob::random_uint (Rand * r) const
{
    return r->range (from, to);
}

//////////////////////////////////////////////////////////////////////
//
// Coverage 
// Used for random selections and maintaining stats.

// pointer to head of choice list, so we can look through choices later and
// generate stats.
Coverage * Coverage::head = 0;

Coverage::Coverage (std::string _title)
    :title (_title),
     prev (head)
{
    head = this;
}

Coverage:: ~ Coverage () 
{
}

void Coverage::increment (size_t idx)
{
    if (idx >= count.size ())
    {
        count.resize (idx + 1);

    }
    count[idx]++;
}

std::ostream &operator<<(std::ostream &out, const Coverage &thing)
{
    thing.output_worker (out);
    return out;
}

StrCoverage:: StrCoverage (const char *_title,
                           const char **_selections):Coverage (_title)
{
    while (*_selections)
    {
        width = std::max (width, strlen (*_selections));
        selections.push_back (*_selections);
        count.push_back (0);
        _selections++;
    }
// round to tab size
    width = (width + 7) & -8;
}

std::string StrCoverage::choose (class Rand * r)
{
    size_t idx = r->srange (0, count.size ());
    increment (idx);
    return selections[idx];
}

void StrCoverage::output_worker (std::ostream & out) const
{
    unsigned items = 0;
    out << title << "\n";
    size_t online = 0;
    for (unsigned i = 0; i < count.size (); i++)
    {
        out << " |";
        out.fill (' ');
        out.width (static_cast<int>(width));
        out << selections[i];
        out.fill (' ');
        out.width (6);
        out << count[i];
        out.width (0);
        online += width + 6;
        items++;
        if (items > 4 || online > 60)
        {
            out << " |\n";
            items = 0;
            online = 0;
        }
    }
    if (online)
    {
        out << " |\n";
    }
    if (prev) 
    {
        prev->output_worker (out);
    }
}

IntCoverage::IntCoverage (const char *_title):Coverage (_title)
{
}

void IntCoverage::output_worker (std::ostream & out) const
{
    unsigned items = 0;
    out << title << "\n";

    for (unsigned i = 0; i < count.size (); i++)
    {
        out << "|";
        out << std::setw(4) << i ;

        if (count[i]) 
        {
            out << std::setw(6) << count[i];
        }
        else 
        {
            out << std::setw(6) << "****";
        }
        items++;
        if (items > 4)
        {
            out << "\n";
            items = 0;
        }
    }

    if (items)
    {
        out << "\n";
    }

    if (prev)
    {
        prev->output_worker (out);
    }
}


}
}
