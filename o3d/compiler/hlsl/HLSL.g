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


// This file contains the ANTLR grammar for parsing HLSL into an Abstract
// Syntax Tree (AST).

grammar HLSL;

options {
    language = Java;
 }

@header
{
}

translation_unit
  : ( global_declaration )* (technique)* EOF
  ;

global_declaration
  : (function_storage_class? function_type_specifier ID LPAREN) => function_declaration
  | sampler_declaration
  | texture_declaration
  | struct_definition
  | typedef_definition
  | var_declaration
  ;

// variables -------------------------------------------

var_declaration
  : var_storage_class*
    var_type_modifier?
    var_datatype id_declaration
    semantic?
    annotation_list?
    ('=' initializer)?
    var_packoffset?
    var_register_bind?
    SEMI
  ;

var_storage_class
  : 'extern'
  | 'nointerpolation'
  | 'shared'
  | 'static'
  | 'uniform'
  | 'volatile'
  ;

var_type_modifier
  : ('const'|'row_major'|'column_major');

var_datatype
  : buffer_type_specifier
  | scalar_type_or_string_specifier
  | vector_type_specifier
  | matrix_type_specifier
  | struct_type_specifier
  ;

var_packoffset
  : 'packoffset' LPAREN register_name (DOT vector_subcomponent)? RPAREN
  ;

var_register_bind
  : COLON register_name
  ;

id_declaration
  : ID ( LBRACKET constant_expression RBRACKET )?
  ;

// function --------------------------------------------

function_declaration
  : function_storage_class?
    function_type_specifier ID LPAREN argument_list RPAREN semantic?
    function_body (SEMI)?
  ;

function_storage_class
  : 'inline'   // ignoring platform target
  ;

function_type_specifier
  : scalar_type_specifier
  | vector_type_specifier
  | matrix_type_specifier
  | struct_type_specifier
  | 'void'
  ;

semantic
  :    COLON semantic_specifier ;

param_type_specifier
  : scalar_type_specifier
  | vector_type_specifier
  | matrix_type_specifier
  | struct_type_specifier
  | string_type_specifier
  ;

// typedef ---------------------------------------

typedef_definition
  : 'typedef'
  ;

// basic datatypes -------------------------------

buffer_type_specifier
  : ('buffer' '<' var_datatype '>' ID)
  ;

scalar_type_specifier
  : 'bool'
  | 'int'
  | 'uint'
  | 'half'
  | 'float'
  | 'double'
  ;

scalar_type_or_string_specifier
  : scalar_type_specifier
  | string_type_specifier
  ;

vector_type_specifier
  : 'bool1'
  | 'bool2'
  | 'bool3'
  | 'bool4'
  | 'int1'
  | 'int2'
  | 'int3'
  | 'int4'
  | 'uint1'
  | 'uint2'
  | 'uint3'
  | 'uint4'
  | 'half1'
  | 'half2'
  | 'half3'
  | 'half4'
  | 'float1'
  | 'float2'
  | 'float3'
  | 'float4'
  | 'double1'
  | 'double2'
  | 'double3'
  | 'double4'
  | 'vector' '<' scalar_type_specifier ',' ('1'|'2'|'3'|'4') '>'
  ;

matrix_type_specifier
  : 'float1x1'
  | 'float1x2'
  | 'float1x3'
  | 'float1x4'
  | 'float2x1'
  | 'float2x2'
  | 'float2x3'
  | 'float2x4'
  | 'float3x1'
  | 'float3x2'
  | 'float3x3'
  | 'float3x4'
  | 'float4x1'
  | 'float4x2'
  | 'float4x3'
  | 'float4x4'
  | 'matrix' '<' scalar_type_specifier ',' ('1'|'2'|'3'|'4') ',' ('1'|'2'|'3'|'4') '>'
  ;

string_type_specifier
  : 'string'
  ;

// Sampler declarations ----------------------------

sampler_declaration
  : SAMPLER ID '=' sampler_type_specifier '{' sampler_state_declaration+ '}' SEMI
  | SAMPLER ID SEMI // GLSL format
  | sampler_type_specifier id_declaration '=' '{' sampler_state_declaration_simple ']' SEMI  // DX10 style
  ;

sampler_state_declaration
  : ( ('Texture'|'texture') '=' '<' ID '>' SEMI )  // DX9 must have one of these
  | sampler_state_declaration_simple
  ;

sampler_state_declaration_simple
  : ( sampler_state_specifier '=' initializer SEMI )  // DX10 style
  ;

sampler_type_specifier
  : 'sampler' | 'Sampler'
  | 'sampler1D' | 'Sampler1D' | 'sampler1d'
  | 'sampler2D' | 'Sampler2D' | 'sampler2d'
  | 'sampler3D' | 'Sampler3D' | 'sampler3d'
  | 'samplerCUBE' | 'SamplerCUBE' | 'samplercube'
  | 'sampler_state' | 'samplerstate' | 'SamplerState'
  | 'SamplerComparisonState' | 'samplercomparisonstate'  // DX10 only
  ;

sampler_state_specifier
  : 'AddressU' | 'addressu'
  | 'AddressV' | 'addressv'
  | 'AddressW' | 'addressw'
  | 'BorderColor' | 'bordercolor'
  | 'ComparisonFilter' | 'comparisonfilter'  // SamplerComparisonState only
  | 'ComparisonFunc' | 'comparisonfunc'   // SamplerComparisonState only
  | 'MagFilter' | 'magfilter'
  | 'MaxAnisotropy' | 'maxanisotropy'
  | 'MinFilter' | 'minfilter'
  | 'MipFilter' | 'mipfilter'
  | 'MipMapLODBias' | 'MipMapLodBias' | 'mipmapLODbias' | 'mipmaplodbias'
  ;

 // texture declaration ----------------------------

texture_declaration
  : texture_type_specifier ID annotation_list? SEMI
  | texture_type_specifier '<'
      (scalar_type_specifier | vector_type_specifier ) '>' ID SEMI  // GLSL syntax.
  ;

texture_type_specifier
   : 'texture'
   | 'texture1D' | 'Texture1D'
   | 'texture2D' | 'Texture2D'
   | 'texture3D' | 'Texture3D'
   | 'textureCUBE' | 'TextureCUBE'
   ;

 // struct declaration -----------------------------

struct_type_specifier
  : ID
  | ( STRUCT ( ID )? LCURLY ) => struct_definition
  | STRUCT ID
  ;

annotation_list
  : '<'  (annotation)* '>';

annotation
  : scalar_type_or_string_specifier ID '=' constant_expression SEMI ;

initializer
  : constant_expression
  | '{' constant_expression ( ',' constant_expression )* '}'
  ;

register_name
  // registers for VS_3_0
  :    'register' '(' input_register_name | output_register_name ')';

input_register_name
  : (('v'|'r'|'c'|'b'|'i'|'s'|'o') DECIMAL_LITERAL)
  | ('a0'|'aL'|'p0')
  ;

output_register_name
  : 'oD0'|'oD1'|'oFog'|'oPos'|'oPts'
  | 'oT0'|'oT1'|'oT2'|'oT3'|'oT4'|'oT5'|'oT6'|'oT7'
  ;

pack_offset
  : .+ ;   // no idea what this field looks like.

argument_list
  : ( param_declaration ( COMMA param_declaration )* )?
  ;

param_declaration
  : param_direction? param_variability? param_type_specifier id_declaration semantic?
  | SAMPLER ID
//    | FUNCTION type_specifier ID
  ;

param_variability
  : CONST
  | UNIFORM
  ;

param_direction
  : IN
  | OUT
  | INOUT
  ;

function_body
  : LCURLY ( decl_or_statement )* RCURLY
  ;

decl_or_statement
  // We copied the following sub-rule here to expedite the parsing time
  // as this is a much more common case than the "Id init_declarator_list"
  // case which would normally spend a lot of time in exception handling.
  : (lvalue_expression assignment_operator ) => assignment_statement
  | ( ( CONST )? vector_type_specifier ) => ( CONST )? vector_type_specifier init_declarator_list SEMI
  | ( ( CONST )? scalar_type_specifier ) => ( CONST )? scalar_type_specifier init_declarator_list SEMI
  | ( STRUCT ( ID )? LCURLY ) => struct_definition ( init_declarator_list )? SEMI
  | STRUCT ID init_declarator_list SEMI
  | ( ID init_declarator_list ) => ID init_declarator_list SEMI
  | statement
  ;

init_declarator_list
  : init_declarator ( COMMA init_declarator )* ;

init_declarator
  : declarator ( ASSIGN initializer )? ;

declarator
  : ID ( LBRACKET ( constant_expression )? RBRACKET )*;

struct_definition
  : STRUCT ( ID )? LCURLY struct_declaration_list RCURLY ID? SEMI;

struct_declaration_list
    // We currently don't support nested structs so the field type
    // can only be either a scalar or a vector.
  : ( struct_interpolation_modifier?
    (scalar_type_specifier|vector_type_specifier) ID
    (COLON semantic_specifier)? SEMI )+
  ;

struct_interpolation_modifier   // DX10 only
  : 'linear'
  | 'centroid'
  | 'nointerpolation'
  | 'noperspective'
  ;

semantic_specifier
  : ID ;

statement
  : ( lvalue_expression assignment_operator ) => assignment_statement
  | ( lvalue_expression self_modify_operator ) => post_modify_statement
  | pre_modify_statement
  | expression_statement
  | compound_statement
  | selection_statement
  | iteration_statement
  | jump_statement
  | SEMI
  ;

assignment_statement
  : lvalue_expression assignment_operator expression SEMI ;

pre_modify_statement
  : pre_modify_expression SEMI ;

pre_modify_expression
  : self_modify_operator lvalue_expression ;

post_modify_statement
  : post_modify_expression SEMI ;

post_modify_expression
  : lvalue_expression self_modify_operator ;

self_modify_operator
  : PLUSPLUS | MINUSMINUS ;

expression_statement
  : expression SEMI ;

compound_statement
  : LCURLY (
          ( ID init_declarator_list) => ID init_declarator_list SEMI
        | ( ( CONST )? vector_type_specifier ) => ( CONST )? vector_type_specifier init_declarator_list SEMI
        | ( ( CONST )? scalar_type_specifier ) => ( CONST )? scalar_type_specifier init_declarator_list SEMI
        | ( STRUCT ( ID )? LCURLY ) => struct_definition ( init_declarator_list )? SEMI
        | STRUCT ID init_declarator_list SEMI
        | statement
        )*
    RCURLY
  ;

selection_statement
  : IF LPAREN expression RPAREN statement ( ELSE statement )?
  ;

iteration_statement
  : WHILE LPAREN expression RPAREN statement
  | FOR LPAREN assignment_statement
      equality_expression SEMI modify_expression RPAREN statement
  | DO statement WHILE LPAREN expression RPAREN SEMI
  ;

modify_expression
  : (lvalue_expression assignment_operator ) =>
      lvalue_expression assignment_operator expression
  | pre_modify_expression
  | post_modify_expression
  ;

jump_statement
  : BREAK SEMI
  | CONTINUE
  | RETURN ( expression )? SEMI
  | DISCARD
  ;

expression
  : conditional_expression ;

assignment_operator
  : ASSIGN
  | MUL_ASSIGN
  | DIV_ASSIGN
  | ADD_ASSIGN
  | SUB_ASSIGN
  | BITWISE_AND_ASSIGN
  | BITWISE_OR_ASSIGN
  | BITWISE_XOR_ASSIGN
  | BITWISE_SHIFTL_ASSIGN
  | BITWISE_SHIFTR_ASSIGN
  ;

constant_expression
  : (ID) => variable_expression
  | literal_value ;

conditional_expression
  : logical_or_expression ( QUESTION expression COLON conditional_expression )?
  ;

logical_or_expression
  : exclusive_or_expression ( OR exclusive_or_expression )*
  ;

// We remove the NOT operator from the unary expression and stick it here
// so that it has a lower precedence than relational operations.
logical_and_expression
  : ( NOT )? inclusive_or_expression ( AND ( NOT )? inclusive_or_expression )*
  ;

inclusive_or_expression
  : exclusive_or_expression (BITWISE_OR exclusive_or_expression )*
  ;

exclusive_or_expression
  : and_expression ( BITWISE_XOR and_expression )*
  ;

and_expression
  : equality_expression ( BITWISE_AND equality_expression )*
  ;

equality_expression
  : relational_expression ( (EQUAL|NOT_EQUAL) relational_expression )*
  ;

relational_expression
  : shift_expression ( (LT|GT|LTE|GTE) shift_expression )*
  ;

shift_expression
  : additive_expression ( (BITWISE_SHIFTL|BITWISE_SHIFTR) additive_expression )*
  ;

additive_expression
  : multiplicative_expression ( (PLUS|MINUS) multiplicative_expression )*
  ;

multiplicative_expression
  : cast_expression ( (MUL|DIV|MOD) cast_expression )*
  ;

cast_expression
  : LBRACKET param_type_specifier RBRACKET cast_expression
  | unary_expression
  ;

unary_expression
  : (PLUS|MINUS) unary_expression
  | postfix_expression
  ;

postfix_expression
  : primary_expression ( postfix_suffix )? ;

lvalue_expression
  : variable_expression ( postfix_suffix )? ;

postfix_suffix
  // choosing between struct field access or vector swizzling is a semantic choice.
  : ( DOT swizzle )
  | ( DOT primary_expression )+ ;

primary_expression
//    : constructor
  : intrinsic_name LPAREN argument_expression_list RPAREN
  | variable_or_call_expression
  | literal_value
  | LPAREN expression RPAREN
  ;

variable_expression
  : ID ( LBRACKET expression RBRACKET )? ;

// Combine variable expression and call expression here to get rid of the
// syntactic predicate we used to use in the primary_expression rule. Using
// predicates results in the parser spending a lot of time in exception
// handling (when lookahead fails).
variable_or_call_expression
  : ID
    (
      ( ( LBRACKET expression RBRACKET )? )
      |
      ( LPAREN argument_expression_list RPAREN )
    )
  ;

constructor
  : vector_type_specifier LPAREN expression ( COMMA expression )* RPAREN ;

argument_expression_list
  : ( expression ( COMMA expression )* )? ;

intrinsic_name
  : ABS
  | ACOS
  | ALL
  | ANY
  | ASFLOAT
  | ASIN
  | ASINT
  | ASUINT
  | ATAN
  | ATAN2
  | CEIL
  | CLAMP
  | CLIP
  | COS
  | COSH
  | CROSS
  | DDX
  | DDY
  | DEGREES
  | DETERMINANT
  | DISTANCE
  | DOT
  | EXP
  | EXP2
  | FACEFORWARD
  | FLOOR
  | FMOD
  | FRAC
  | FREXP
  | FWIDTH
  | ISFINITE
  | ISINF
  | ISNAN
  | LDEXP
  | LENGTH
  | LERP
  | LIT
  | LOG
  | LOG10
  | LOG2
  | MAX
  | MIN
  | MODF
  | MUL
  | NOISE
  | NORMALIZE
  | POW
  | RADIANS
  | REFLECT
  | REFRACT
  | ROUND
  | RSQRT
  | SATURATE
  | SIGN
  | SIN
  | SINCOS
  | SINH
  | SMOOTHSTEP
  | SQRT
  | STEP
  | TAN
  | TANH
  | TEX1D
  | TEX1DBIAS
  | TEX1DGRAD
  | TEX1DLOD
  | TEX1DPROJ
  | TEX2D
  | TEX2DBIAS
  | TEX2DGRAD
  | TEX2DLOD
  | TEX2DPROJ
  | TEX3D
  | TEX3DBIAS
  | TEX3DGRAD
  | TEX3DLOD
  | TEX3DPROJ
  | TEXCUBE
  | TEXCUBEBIAS
  | TEXCUBEGRAD
  | TEXCUBELOD
  | TEXCUBEPROJ
  | TRANSPOSE
  | TRUNC
  ;

int_literal
  : DECIMAL_LITERAL
  | OCT_LITERAL
  | HEX_LITERAL
  ;
literal_value
  : ('"') => string_literal
  | int_literal
  | float_literal
  ;


float_literal
  : FLOAT_LITERAL
  ;

string_literal
  : '"' STRING_LITERAL '"'
  ;

vector_subcomponent
  :    DOT ('x'|'y'|'z'|'w'|'r'|'g'|'b'|'a')
  ;

swizzle
  : ( s += ('x'|'y'|'z'|'w') )+ { $s.size() <= 4 }?
  | ( s += ('r'|'g'|'b'|'a') )+ { $s.size() <= 4 }?
  ;

// effects ---------------------------------------

technique
  : 'technique' ID? '{' (pass)+  '}' SEMI?
  ;

pass
  : 'pass' (ID)? '{' (state_assignment)*    '}' SEMI?
  ;

state_assignment
  : a=ID '=' state_assignment_value SEMI
         { System.out.println("Found stateassignment: " + $a.text); }
  ;

state_assignment_value
  : 'compile' p=ID q=variable_or_call_expression
         { System.out.println("Found compile: " + $p.text + " " + $q.text); }
  | a=expression
         { System.out.println("Found stateassignmentvalue: " + $a.text); }
  | ('true' | 'false')
  ;

// -----------------------------------------------------------------------------

NOT           :  '!' ;
NOT_EQUAL     :  '!=' ;
AND           :  '&&' ;
LPAREN        :  '(' ;
RPAREN        :  ')' ;
MUL           :  '*' ;
MUL_ASSIGN    :  '*=' ;
PLUS          :  '+' ;
PLUSPLUS      :  '++' ;
ADD_ASSIGN    :  '+=' ;
COMMA         :  ',' ;
MINUS         :  '-' ;
MINUSMINUS    :  '--' ;
SUB_ASSIGN    :  '-=' ;
DIV           :  '/' ;
DIV_ASSIGN    :  '/=' ;
MOD           :  '%';
MOD_ASSIGN    :     '%=';
COLON         :  ':' ;
SEMI          :  ';' ;
LT            :  '<' ;
LTE           :  '<=' ;
ASSIGN        :  '=' ;
EQUAL         :  '==' ;
GT            :  '>' ;
GTE           :  '>=' ;
QUESTION      :  '?' ;
LBRACKET      :  '[' ;
RBRACKET      :  ']' ;
LCURLY        :  '{' ;
OR            :  '||' ;
RCURLY        :  '}' ;
DOT           :  '.' ;
BITWISE_NOT    :  '~';
BITWISE_SHIFTL : '<<';
BITWISE_SHIFTR : '>>';
BITWISE_AND    : '&';
BITWISE_OR     : '|';
BITWISE_XOR    : '^';
BITWISE_SHIFTL_ASSIGN : '<<=';
BITWISE_SHIFTR_ASSIGN : '>>=';
BITWISE_AND_ASSIGN    :     '&=';
BITWISE_OR_ASSIGN     :     '|=';
BITWISE_XOR_ASSIGN    :     '^=';

// keywords ----------------------------

BREAK            : 'break';
BUFFER           : 'buffer';
CBUFFER          : 'cbuffer';
CONST            : 'const';
CONTINUE         : 'continue';
DISCARD          : 'discard';
DO               : 'do';
ELSE             : 'else';
EXTERN           : 'extern';
FALSE            : 'false';
FOR              : 'for';
IF               : 'if';
IN               : 'in';
INLINE           : 'inline';
INOUT            : 'inout';
MATRIX           : 'matrix';
NAMESPACE        : 'namespace';
NOINTERPOLATION  : 'nointerpolation';
OUT              : 'out';
RETURN           : 'return';
REGISTER         : 'register';
SHARED           : 'shared';
STATEBLOCK       : 'stateblock';
STATEBLOCK_STATE : 'stateblock_state';
STATIC           : 'static';
STRING           : 'string';
STRUCT           : 'struct';
SWITCH           : 'switch';
TBUFFER          : 'tbuffer';
TEXTURE          : 'texture';
TEXTURE1D        : 'texture1d';
TEXTURE1DARRAY   : 'texture1darray';
TEXTURE2D        : 'texture2d';
TEXTURE2DARRAY   : 'texture2darray';
TEXTURE2DMS      : 'texture2dms';
TEXTURE2DMSARRAY : 'texture2dmsarray';
TEXTURE3D        : 'texture3d';
TEXTURECUBE      : 'texturecube';
TEXTURECUBEARRAY : 'texturecubearray';
TRUE             : 'true';
TYPEDEF          : 'typedef';
UNIFORM          : 'uniform';
VOID             : 'void';
VOLATILE         : 'volatile';
WHILE            : 'while';

// fx keywords ---------------------

BLENDSTATE           : 'blendstate';
COMPILE            : 'compile';
DEPTHSTENCILSTATE  : 'depthstencilstate';
DEPTHSTENCILVIEW   : 'depthstencilview';
GEOMETRYSHADER       : 'geometryshader';
PASS               : 'pass';
PIXELSHADER           : 'pixelshader';
RASTERIZERSTATE       : 'rasterizerstate';
RENDERTARGETVIEW   : 'rendertargetview';
TECHNIQUE           : 'technique';
TECHNIQUE10           : 'technique10';
VERTEXSHADER       : 'vertexshader';


// intrinsic functions --------------------
ABS         : 'abs';
ACOS        : 'acos';
ALL            : 'all';
ANY            : 'any';
ASFLOAT     : 'asfloat';
ASIN        : 'asin';
ASINT        : 'asint';
ASUINT        : 'asuint';
ATAN        : 'atan';
ATAN2        : 'atan2';
CEIL        : 'ceil';
CLAMP        : 'clamp';
CLIP        : 'clip';
COS            : 'cos';
COSH        : 'cosh';
CROSS        : 'cross';
DDX            : 'ddx';
DDY            : 'ddy';
DEGREES     : 'degrees';
DETERMINANT : 'determinant';
DISTANCE    : 'distance';
DOT_PRODUCT    : 'dot';
EXP            : 'exp';
EXP2        : 'exp2';
FACEFORWARD : 'faceforward';
FLOOR        : 'floor';
FMOD        : 'fmod';
FRAC        : 'frac';
FREXP        : 'frexp';
FWIDTH        : 'fwidth';
ISFINITE    : 'isfinite';
ISINF        : 'isinf';
ISNAN        : 'isnan';
LDEXP        : 'ldexp';
LENGTH        : 'length';
LERP        : 'lerp';
LIT            : 'lit';
LOG            : 'log';
LOG10        : 'log10';
LOG2        : 'log2';
MAX            : 'max';
MIN            : 'min';
MODF        : 'modf';
MUL_FUNC    : 'mul';
NOISE        : 'noise';
NORMALIZE   : 'normalize';
POW            : 'pow';
RADIANS     : 'radians';
REFLECT     : 'reflect';
REFRACT     : 'refract';
ROUND       : 'round';
RSQRT       : 'rsqrt';
SATURATE    : 'saturate';
SIGN        : 'sign';
SIN         : 'sin';
SINCOS      : 'sincos';
SINH        : 'sinh';
SMOOTHSTEP  : 'smoothstep';
SQRT        : 'sqrt';
STEP        : 'step';
TAN         : 'tan';
TANH        : 'tanh';
TEX1D       : 'tex1D';
TEX1DBIAS   : 'tex1Dbias';
TEX1DGRAD   : 'tex1Dgrad';
TEX1DLOD    : 'tex1Dlod';
TEX1DPROJ   : 'tex1Dproj';
TEX2D       : 'tex2D';
TEX2DBIAS   : 'tex2Dbias';
TEX2DGRAD   : 'tex2Dgrad';
TEX2DLOD    : 'tex2Dlod';
TEX2DPROJ   : 'tex2Dproj';
TEX3D       : 'tex3D';
TEX3DBIAS   : 'tex3Dbias';
TEX3DGRAD   : 'tex3Dgrad';
TEX3DLOD    : 'tex3Dlod';
TEX3DPROJ   : 'tex3Dproj';
TEXCUBE     : 'texCUBE';
TEXCUBEBIAS : 'texCUBEbias';
TEXCUBEGRAD : 'texCUBEgrad';
TEXCUBELOD  : 'texCUBElod';
TEXCUBEPROJ : 'texCUBEproj';
TRANSPOSE   : 'transpose';
TRUNC       : 'trunc';



// fundamental tokens ---------------------

fragment HEXDIGIT
  : ('0'..'9'|'a'..'f'|'A'..'F')
  ;

fragment UNICODE_ESCAPE
  :   '\\' 'u' HEXDIGIT HEXDIGIT HEXDIGIT HEXDIGIT
  ;

fragment OCTAL_ESCAPE
  :   '\\' ('0'..'3') ('0'..'7') ('0'..'7')
  |   '\\' ('0'..'7') ('0'..'7')
  |   '\\' ('0'..'7')
  ;

fragment ESCAPE_SEQUENCE
  :   '\\' ('b'|'t'|'n'|'f'|'r'|'\"'|'\''|'\\')
  |   UNICODE_ESCAPE
  |   OCTAL_ESCAPE
  ;

fragment EXPONENT : ('e'|'E') (PLUS | MINUS)? ('0'..'9')+ ;

fragment FLOATSUFFIX : ('f'|'F'|'h'|'H') ;

ID
  : ('a'..'z'|'A'..'Z'|'_')('a'..'z'|'A'..'Z'|'_'|'0'..'9')*
  {
      // check the length of the identifier before accepting it.
      if (($ID.length() > 96)) {
          RecognitionException();
      }
  }
  ;

DECIMAL_LITERAL
  :     ('1'..'9')('0'..'9')+
  ;

OCT_LITERAL
  : ('0'..'3') ('0'..'7') ('0'..'7')
  | ('0'..'7') ('0'..'7')
  | ('0'..'7')
  ;

HEX_LITERAL
  : '0x' HEXDIGIT+
  ;

CHARACTER_LITERAL
  :   '\'' ( ESCAPE_SEQUENCE | ~('\''|'\\') ) '\''
  ;

STRING_LITERAL
  :  ( ESCAPE_SEQUENCE | ~('\\'|'"') )*
  ;

FLOAT_LITERAL
  : (PLUS | MINUS)?  ('0'..'9')+ '.' ('0'..'9')* (EXPONENT)? (FLOATSUFFIX)?
  | (PLUS | MINUS)?  '.' ('0'..'9')+ (EXPONENT)? (FLOATSUFFIX)?
  | (PLUS | MINUS)?  ('0'..'9')+ (EXPONENT)? (FLOATSUFFIX)?
  ;

// skipped elements -----------------

WHITESPACE
  : ( ' ' | '\t' | '\f' | '\r' )
  { $channel = HIDDEN; }
  ;

COMMENT
  : '//' (~('\n'|'\r'))*
  { $channel = HIDDEN; }
  ;

MULTILINE_COMMENT
  : '/*' ( options {greedy=false;} : . )* '*/'
  { $channel = HIDDEN; }
  ;

//RESERVED_WORDS
//    : 'auto'
//    | 'case'
//    | 'catch'
//    | 'char'
//    | 'class'
//    | 'const_cast'
//    | 'default'
//    | 'delete'
//    | 'dynamic_cast'
//    | 'enum'
//    | 'explicit'
//    | 'friend'
//    | 'goto'
//    | 'long'
//    | 'mutable'
//    | 'new'
//    | 'operator'
//    | 'private'
//    | 'protected'
//    | 'public'
//    | 'reinterpret_cast'
//    | 'short'
//    | 'signed'
//    | 'sizeof'
//    | 'snorm'
//    | 'static_cast'
//    | 'template'
//    | 'this'
//    | 'throw'
//    | 'try'
//    | 'typename'
//    | 'union'
//    | 'unorm'
//    | 'unsigned'
//    | 'using'
//    | 'varying'
//    | 'virtual'
//    { $channel = HIDDEN; }
//    ;
