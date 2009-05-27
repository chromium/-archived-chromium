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



#ifndef PURITAN_KNOBS_H
#define PURITAN_KNOBS_H

#include <string> 
#include <list>
#include <vector>

namespace Salem 
{ 
namespace Puritan
{

class OutputInfo
{
public:
    typedef enum 
    {
        Float1,
        Float2,
        Float4
    } ArgSize;

    unsigned n_samplers;
    std::list <std::pair <ArgSize, std::string> > uniforms;
    std::list <ArgSize> returns;
};

class Knob 
{
    class Knob *prev;
    const char *name;
    const char *help;

    virtual bool set_from_argument (const char *val) = 0;
    bool parse_argument (const char *val);
    virtual void usage (std::ostream &out) const= 0;

    friend class Knobs;
    friend class IntKnob;
    friend class RangeKnob;
    friend class BoolKnob;
    friend class ProbKnob;
    friend std::ostream & operator << (std::ostream &out, const class Knobs& x);

public:
    Knob(const char *name, const char *desc);
    virtual ~Knob();

    virtual void to_stream (std::ostream &) const = 0;

};

class RangeKnob : public Knob 
{
    int from;
    int to;

    bool set_from_argument (const char *val);
    void usage (std::ostream &) const;

public:
    RangeKnob (const char *,  const char *,int f, int t);

    void set(int, int);
    int operator () (class Rand *) const;
    unsigned int random_uint (class Rand *) const ;
    void to_stream (std::ostream &) const;

};

class IntKnob : public Knob 
{
    int val;
 
    bool set_from_argument (const char *val);
    void usage (std::ostream &) const;

public:
    IntKnob (const char *, const char *,int);

    int get() const;
    unsigned uget() const;
    void set(int x) { val = x;}
    void to_stream (std::ostream &) const;

};

class BoolKnob : public Knob 
{
    bool val;

    bool set_from_argument (const char *val);
    void usage (std::ostream &) const;
public:
    BoolKnob (const char *, const char *,bool f);
    void to_stream (std::ostream &) const;
    bool operator () () const;
    void set(bool x);
};

class ProbKnob : public Knob 
{
    double prob;

    bool set_from_argument (const char *val);
    void usage (std::ostream &) const;
public:
    ProbKnob (const char *, const char *,double d);
    void set (double x) { prob = x; } 
    double get() const;
    void to_stream (std::ostream &) const;
    bool operator () (Rand *r) const ;
};

class Knobs 
{
public:
    RangeKnob top_level_statements;
    RangeKnob for_count;
    RangeKnob while_count;
    RangeKnob do_count;
    RangeKnob if_count;
    RangeKnob block_count;
    IntKnob code_limit;
    IntKnob exp_limit;
    RangeKnob exp_count;
    RangeKnob func_count;
    ProbKnob func_trim;
    ProbKnob arg_in_chance;
    ProbKnob arg_out_chance;
    RangeKnob for_nesting;
    RangeKnob while_nesting;
    RangeKnob block_nesting;
    RangeKnob do_nesting;
    RangeKnob if_nesting;
    RangeKnob block_length;
    RangeKnob expression_depth;
    ProbKnob special_chance;
    ProbKnob float4_chance;
    ProbKnob float4_struct_member_chance;
    ProbKnob float2_chance;
    ProbKnob func_chance;
    ProbKnob term_chance;
    ProbKnob unary_chance;
    ProbKnob swizzle_chance;
    ProbKnob fcall_chance;
    ProbKnob copy_swizzle_chance;
    ProbKnob noinline_chance;
    ProbKnob lhs_swizzle_chance;
    ProbKnob assop_chance;
    ProbKnob selfmod_chance;
    ProbKnob trifunc_chance;
    ProbKnob binfunc_chance;
    ProbKnob relop_chance;
    ProbKnob relop_cond_chance;
    ProbKnob type_change_chance;
    ProbKnob type_float4_chance;
    ProbKnob type_float2_chance;
    RangeKnob sampler_count;
    ProbKnob sampler_chance;
    RangeKnob uniform_count;
    ProbKnob uniform_chance;
    RangeKnob arg_count;
    RangeKnob static_initializer_depth;
    BoolKnob standalone;
    IntKnob seed;
    ProbKnob variable_reuse;
    ProbKnob array_use;
    ProbKnob array_reuse;
    ProbKnob array_constness;
    ProbKnob array_index_const;
    ProbKnob array_in_for_use;
    ProbKnob if_elses;
    ProbKnob loop_breaks;
    ProbKnob multiple_stmt;
    ProbKnob constant_use;
    ProbKnob constant_small;
    BoolKnob int_variables;
    BoolKnob allow_two_negs;

    Knob *first;
    Knobs(void);
    void to_stream (std::ostream &);
    bool parse_argument (const char *arg);
    static std::string usage ();
};

class Coverage
{
private:
    // Next in chain

    std::string title;
    std::vector <int> count;
    Coverage *prev;

    friend class StrCoverage;
    friend class IntCoverage;

public:
    static Coverage *head;
    virtual void output_worker (std::ostream & out) const = 0;
    Coverage (std::string _title) ;
    virtual ~ Coverage () ;

    void increment (size_t idx);
};

class StrCoverage:Coverage
{
private:
    std::vector <std::string> selections;
    size_t width;
    void output_worker (std::ostream & out) const;

public:
    StrCoverage (const char *_title, const char **_selections);
    std::string choose (class Rand * r);
};

class IntCoverage:public Coverage
{
private:
    void output_worker (std::ostream & out) const;
public:
    IntCoverage (const char *_title);
};


std::ostream & operator << (std::ostream &out, const class Salem::Puritan::Coverage &x);
std::ostream & operator << (std::ostream &out, const class Salem::Puritan::Knobs &x);
}
}

#endif
