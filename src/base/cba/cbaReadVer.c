/**CFile****************************************************************

  FileName    [cbaReadVer.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Hierarchical word-level netlist.]

  Synopsis    [BLIF writer.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - November 29, 2014.]

  Revision    [$Id: cbaReadVer.c,v 1.00 2014/11/29 00:00:00 alanmi Exp $]

***********************************************************************/

#include "cba.h"
#include "cbaPrs.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

// Verilog keywords
typedef enum { 
    PRS_VER_NONE = 0,  // 0:  unused
    PRS_VER_INPUT,     // 1:  input
    PRS_VER_OUTPUT,    // 2:  output
    PRS_VER_INOUT,     // 3:  inout
    PRS_VER_WIRE,      // 4:  wire
    PRS_VER_MODULE,    // 5:  module
    PRS_VER_ASSIGN,    // 6:  assign
    PRS_VER_REG,       // 7:  reg
    PRS_VER_ALWAYS,    // 8:  always
    PRS_VER_DEFPARAM,  // 9:  always
    PRS_VER_BEGIN,     // 10: begin
    PRS_VER_END,       // 11: end
    PRS_VER_ENDMODULE, // 12: endmodule
    PRS_VER_UNKNOWN    // 13: unknown
} Cba_VerType_t;

static const char * s_VerTypes[PRS_VER_UNKNOWN+1] = {
    NULL,              // 0:  unused
    "input",           // 1:  input
    "output",          // 2:  output
    "inout",           // 3:  inout
    "wire",            // 4:  wire
    "module",          // 5:  module
    "assign",          // 6:  assign
    "reg",             // 7:  reg
    "always",          // 8:  always
    "defparam",        // 9:  defparam
    "begin",           // 10: begin
    "end",             // 11: end
    "endmodule",       // 12: endmodule
    NULL               // 13: unknown 
};

static inline void Prs_NtkAddVerilogDirectives( Prs_Man_t * p )
{
    int i;
    for ( i = 1; s_VerTypes[i]; i++ )
        Abc_NamStrFindOrAdd( p->pStrs, (char *)s_VerTypes[i],   NULL );
    assert( Abc_NamObjNumMax(p->pStrs) == i );
}


// character recognition 
static inline int Prs_CharIsSpace( char c )   { return (c == ' ' || c == '\t' || c == '\r' || c == '\n');                           }
static inline int Prs_CharIsDigit( char c )   { return (c >= '0' && c <= '9');                                                      }
static inline int Prs_CharIsDigitB( char c )  { return (c == '0' || c == '1'  || c == 'x' || c == 'z');                             }
static inline int Prs_CharIsDigitH( char c )  { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');  }
static inline int Prs_CharIsChar( char c )    { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');                            }
static inline int Prs_CharIsSymb1( char c )   { return Prs_CharIsChar(c) || c == '_';                                               }
static inline int Prs_CharIsSymb2( char c )   { return Prs_CharIsSymb1(c) || Prs_CharIsDigit(c) || c == '$';                        }

static inline int Prs_ManIsChar( Prs_Man_t * p, char c )    { return p->pCur[0] == c;                        }
static inline int Prs_ManIsChar1( Prs_Man_t * p, char c )   { return p->pCur[1] == c;                        }
static inline int Prs_ManIsDigit( Prs_Man_t * p )           { return Prs_CharIsDigit(*p->pCur);              }

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

// predefined primitives
typedef struct Prs_VerPrim_t_ Prs_VerPrim_t;
struct Prs_VerPrim_t_
{
    int    Type;
    char * pName;
};
static const Prs_VerPrim_t s_VerilogPrims[16] = 
{
    {CBA_BOX_BUF,  "buf"   },   
    {CBA_BOX_INV,  "not"   },   
    {CBA_BOX_AND,  "and"   },   
    {CBA_BOX_NAND, "nand"  },  
    {CBA_BOX_OR,   "or"    },    
    {CBA_BOX_NOR,  "nor"   },   
    {CBA_BOX_XOR,  "xor"   },   
    {CBA_BOX_XNOR, "xnor"  },  
    {CBA_BOX_TRI,  "bufif1"},  
    {0}
};

// predefined operator names
static const char * s_VerNames[100] = 
{
    NULL,
    "VERIFIC_",
    "add_",                  
    "mult_",                 
    "div_",                  
    "mod_",                  
    "rem_",                  
    "shift_left_",           
    "shift_right_",          
    "rotate_left_",          
    "rotate_right_",         
    "reduce_and_",           
    "reduce_or_",            
    "reduce_xor_",           
    "reduce_nand_",          
    "reduce_nor_",           
    "reduce_xnor_",          
    "LessThan_",             
    "Mux_",                  
    "Select_",               
    "Decoder_",              
    "EnabledDecoder_",       
    "PrioSelect_",           
    "DualPortRam_",          
    "ReadPort_",             
    "WritePort_",            
    "ClockedWritePort_",     
    "lut",                   
    "and_",                  
    "or_",                   
    "xor_",                  
    "nand_",                 
    "nor_",                  
    "xnor_",                 
    "buf_",                  
    "inv_",                  
    "tri_",                  
    "sub_",                  
    "unary_minus_",          
    "equal_",                
    "not_equal_",            
    "mux_",                  
    "wide_mux_",             
    "wide_select_",          
    "wide_dff_",             
    "wide_dlatch_",          
    "wide_dffrs_",           
    "wide_dlatchrs_",        
    "wide_prio_select_",     
    "pow_",                  
    "PrioEncoder_",          
    "abs",                   
    NULL
};

typedef struct Prs_VerInfo_t_ Prs_VerInfo_t;
struct Prs_VerInfo_t_
{
    int    Type;
    int    nInputs;
    char * pTypeName;
    char * pSigNames[6];
};
static const Prs_VerInfo_t s_VerInfo[100] = 
{
    {-1,              0, NULL,                     /* "PRIM_NONE"               */ {NULL}},
    {CBA_BOX_CT,      0, "VERIFIC_PWR",            /* "PRIM_PWR"                */ {"o"}},
    {CBA_BOX_CF,      0, "VERIFIC_GND",            /* "PRIM_GND"                */ {"o"}},
    {CBA_BOX_CX,      0, "VERIFIC_X",              /* "PRIM_X"                  */ {"o"}},
    {CBA_BOX_CZ,      0, "VERIFIC_Z",              /* "PRIM_Z"                  */ {"o"}},
    {CBA_BOX_INV,     1, "VERIFIC_INV",            /* "PRIM_INV"                */ {"i","o"}},
    {CBA_BOX_BUF,     1, "VERIFIC_BUF",            /* "PRIM_BUF"                */ {"i","o"}},
    {CBA_BOX_AND,     1, "VERIFIC_AND",            /* "PRIM_AND"                */ {"a0","a1","o"}},
    {CBA_BOX_NAND,    2, "VERIFIC_NAND",           /* "PRIM_NAND"               */ {"a0","a1","o"}},
    {CBA_BOX_OR,      2, "VERIFIC_OR",             /* "PRIM_OR"                 */ {"a0","a1","o"}},
    {CBA_BOX_NOR,     2, "VERIFIC_NOR",            /* "PRIM_NOR"                */ {"a0","a1","o"}},
    {CBA_BOX_XOR,     2, "VERIFIC_XOR",            /* "PRIM_XOR"                */ {"a0","a1","o"}},
    {CBA_BOX_XNOR,    2, "VERIFIC_XNOR",           /* "PRIM_XNOR"               */ {"a0","a1","o"}},
    {CBA_BOX_MUX,     3, "VERIFIC_MUX",            /* "PRIM_MUX"                */ {"c","a1","a0","o"}}, // changed order
    {-1,              0, "VERIFIC_PULLUP",         /* "PRIM_PULLUP"             */ {"o"}},
    {-1,              0, "VERIFIC_PULLDOWN",       /* "PRIM_PULLDOWN"           */ {"o"}},
    {CBA_BOX_TRI,     3, "VERIFIC_TRI",            /* "PRIM_TRI"                */ {"i","c","o"}},
    {CBA_BOX_LATCH,   4, "VERIFIC_DLATCH",         /* "PRIM_DLATCH"             */ {"d","async_val","async_cond","gate","q"}}, // changed order
    {CBA_BOX_LATCHRS, 4, "VERIFIC_DLATCHRS",       /* "PRIM_DLATCHRS"           */ {"d","s","r","gate","q"}},                  // changed order
    {CBA_BOX_DFF,     4, "VERIFIC_DFF",            /* "PRIM_DFF"                */ {"d","async_val","async_cond","clk","q"}},  // changed order
    {CBA_BOX_DFFRS,   4, "VERIFIC_DFFRS",          /* "PRIM_DFFRS"              */ {"d","s","r","clk","q"}},                   // changed order
    {-1,              2, "VERIFIC_NMOS",           /* "PRIM_NMOS"               */ {"c","d","o"}},
    {-1,              2, "VERIFIC_PMOS",           /* "PRIM_PMOS"               */ {"c","d","o"}},
    {-1,              3, "VERIFIC_CMOS",           /* "PRIM_CMOS"               */ {"d","nc","pc","o"}},
    {-1,              2, "VERIFIC_TRAN",           /* "PRIM_TRAN"               */ {"inout1","inout2","control"}},
    {CBA_BOX_ADD,     3, "VERIFIC_FADD",           /* "PRIM_FADD"               */ {"cin","a","b","o","cout"}},
    {-1,              3, "VERIFIC_RCMOS",          /* "PRIM_RCMOS"              */ {"d","nc","pc","o"}},
    {-1,              2, "VERIFIC_RNMOS",          /* "PRIM_RNMOS"              */ {"c","d","o"}},
    {-1,              2, "VERIFIC_RPMOS",          /* "PRIM_RPMOS"              */ {"c","d","o"}},
    {-1,              2, "VERIFIC_RTRAN",          /* "PRIM_RTRAN"              */ {"inout1","inout2","control"}},
    {-1,              0, "VERIFIC_HDL_ASSERTION",  /* "PRIM_HDL_ASSERTION"      */ {"condition"}},
    {CBA_BOX_ADD,     3, "add_",                   /* "OPER_ADDER"              */ {"cin","a","b","o","cout"}},
    {CBA_BOX_MUL,     2, "mult_",                  /* "OPER_MULTIPLIER"         */ {"a","b","o"}},
    {CBA_BOX_DIV,     2, "div_",                   /* "OPER_DIVIDER"            */ {"a","b","o"}},
    {CBA_BOX_MOD,     2, "mod_",                   /* "OPER_MODULO"             */ {"a","b","o"}},
    {CBA_BOX_REM,     2, "rem_",                   /* "OPER_REMAINDER"          */ {"a","b","o"}},
    {CBA_BOX_SHIL,    3, "shift_left_",            /* "OPER_SHIFT_LEFT"         */ {"cin","a","amount","o"}},
    {CBA_BOX_SHIR,    3, "shift_right_",           /* "OPER_SHIFT_RIGHT"        */ {"cin","a","amount","o"}},
    {CBA_BOX_ROTL,    2, "rotate_left_",           /* "OPER_ROTATE_LEFT"        */ {"a","amount","o"}},
    {CBA_BOX_ROTR,    2, "rotate_right_",          /* "OPER_ROTATE_RIGHT"       */ {"a","amount","o"}},
    {CBA_BOX_RAND,    1, "reduce_and_",            /* "OPER_REDUCE_AND"         */ {"a","o"}},
    {CBA_BOX_ROR,     1, "reduce_or_",             /* "OPER_REDUCE_OR"          */ {"a","o"}},
    {CBA_BOX_RXOR,    1, "reduce_xor_",            /* "OPER_REDUCE_XOR"         */ {"a","o"}},
    {CBA_BOX_RNAND,   1, "reduce_nand_",           /* "OPER_REDUCE_NAND"        */ {"a","o"}},
    {CBA_BOX_RNOR,    1, "reduce_nor_",            /* "OPER_REDUCE_NOR"         */ {"a","o"}},
    {CBA_BOX_RXNOR,   1, "reduce_xnor_",           /* "OPER_REDUCE_XNOR"        */ {"a","o"}},
    {CBA_BOX_LTHAN,   3, "LessThan_",              /* "OPER_LESSTHAN"           */ {"cin","a","b","o"}},
    {CBA_BOX_NMUX,    2, "Mux_",                   /* "OPER_NTO1MUX"            */ {"sel","data","o"}},
    {CBA_BOX_SEL,     2, "Select_",                /* "OPER_SELECTOR"           */ {"sel","data","o"}},
    {CBA_BOX_DEC,     1, "Decoder_",               /* "OPER_DECODER"            */ {"a","o"}},
    {CBA_BOX_EDEC,    2, "EnabledDecoder_",        /* "OPER_ENABLED_DECODER"    */ {"en","i","o"}},
    {CBA_BOX_PSEL,    3, "PrioSelect_",            /* "OPER_PRIO_SELECTOR"      */ {"cin","sel","data","o"}},
    {CBA_BOX_RAM,     4, "DualPortRam_",           /* "OPER_DUAL_PORT_RAM"      */ {"write_enable","write_address","write_data","read_address","read_data"}},
    {CBA_BOX_RAMR,    3, "ReadPort_",              /* "OPER_READ_PORT"          */ {"read_enable", "read_address", "RAM", "read_data" }},
    {CBA_BOX_RAMW,    3, "WritePort_",             /* "OPER_WRITE_PORT"         */ {"write_enable","write_address","write_data", "RAM"}},
    {CBA_BOX_RAMWC,   4, "ClockedWritePort_",      /* "OPER_CLOCKED_WRITE_PORT" */ {"clk","write_enable","write_address","write_data", "RAM"}},
    {CBA_BOX_LUT,     1, "lut",                    /* "OPER_LUT"                */ {"i","o"}},
    {CBA_BOX_AND,     2, "and_",                   /* "OPER_WIDE_AND"           */ {"a","b","o"}},
    {CBA_BOX_OR,      2, "or_",                    /* "OPER_WIDE_OR"            */ {"a","b","o"}},
    {CBA_BOX_XOR,     2, "xor_",                   /* "OPER_WIDE_XOR"           */ {"a","b","o"}},
    {CBA_BOX_NAND,    2, "nand_",                  /* "OPER_WIDE_NAND"          */ {"a","b","o"}},
    {CBA_BOX_NOR,     2, "nor_",                   /* "OPER_WIDE_NOR"           */ {"a","b","o"}},
    {CBA_BOX_XNOR,    2, "xnor_",                  /* "OPER_WIDE_XNOR"          */ {"a","b","o"}},
    {CBA_BOX_BUF,     1, "buf_",                   /* "OPER_WIDE_BUF"           */ {"i","o"}},
    {CBA_BOX_INV,     1, "inv_",                   /* "OPER_WIDE_INV"           */ {"i","o"}},
    {CBA_BOX_TRI,     2, "tri_",                   /* "OPER_WIDE_TRI"           */ {"i","c","o"}},
    {CBA_BOX_SUB,     2, "sub_",                   /* "OPER_MINUS"              */ {"a","b","o"}},
    {CBA_BOX_MIN,     1, "unary_minus_",           /* "OPER_UMINUS"             */ {"i","o"}},
    {CBA_BOX_EQU,     2, "equal_",                 /* "OPER_EQUAL"              */ {"a","b","o"}},
    {CBA_BOX_NEQU,    2, "not_equal_",             /* "OPER_NEQUAL"             */ {"a","b","o"}},
    {CBA_BOX_MUX,     3, "mux_",                   /* "OPER_WIDE_MUX"           */ {"cond","d1","d0","o"}}, // changed order
    {CBA_BOX_NMUX,    2, "wide_mux_",              /* "OPER_WIDE_NTO1MUX"       */ {"sel","data","o"}},
    {CBA_BOX_SEL,     2, "wide_select_",           /* "OPER_WIDE_SELECTOR"      */ {"sel","data","o"}},
    {CBA_BOX_DFF,     4, "wide_dff_",              /* "OPER_WIDE_DFF"           */ {"d","async_val","async_cond","clock","q"}},
    {CBA_BOX_DFFRS,   4, "wide_dffrs_",            /* "OPER_WIDE_DFFRS"         */ {"d","set","reset","clock","q"}},
    {CBA_BOX_LATCHRS, 4, "wide_dlatchrs_",         /* "OPER_WIDE_DLATCHRS"      */ {"d","set","reset","clock","q"}},
    {CBA_BOX_LATCH,   4, "wide_dlatch_",           /* "OPER_WIDE_DLATCH"        */ {"d","async_val","async_cond","clock","q"}},
    {CBA_BOX_PSEL,    3, "wide_prio_select_",      /* "OPER_WIDE_PRIO_SELECTOR" */ {"sel","data","carry_in","o"}},
    {CBA_BOX_POW,     2, "pow_",                   /* "OPER_POW"                */ {"a","b","o"}},
    {CBA_BOX_PENC,    1, "PrioEncoder_",           /* "OPER_PRIO_ENCODER"       */ {"sel","o"}},
    {CBA_BOX_ABS,     1, "abs",                    /* "OPER_ABS"                */ {"i","o"}},
    {-1,              0, NULL,                     /* "PRIM_END"                */ {NULL}}
}; 


// check if it is a Verilog predefined module
static inline int Prs_ManIsVerilogPrim( char * pName )
{
    int i;
    for ( i = 0; s_VerilogPrims[i].pName; i++ )
        if ( !strcmp(pName, s_VerilogPrims[i].pName) )
            return s_VerilogPrims[i].Type;
    return 0;
}
// check if it is a known module
static inline int Prs_ManIsKnownModule( char * pName )
{
    int i;
    for ( i = 1; s_VerNames[i]; i++ )
        if ( !strncmp(pName, s_VerNames[i], strlen(s_VerNames[i])) )
            return i;
    return 0;
}
// check if it is a known module
static inline int Prs_ManFindType( char * pName, int * pInputs, int fOut, char *** ppNames )
{
    int i;
    *pInputs = -1;
    for ( i = 1; s_VerInfo[i].pTypeName; i++ )
        if ( !strncmp(pName, s_VerInfo[i].pTypeName, strlen(s_VerInfo[i].pTypeName)) )
        {
            *pInputs = s_VerInfo[i].nInputs;
            *ppNames = (char **)s_VerInfo[i].pSigNames + (fOut ? s_VerInfo[i].nInputs : 0);
            return s_VerInfo[i].Type;
        }
    return CBA_OBJ_BOX;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

// skips Verilog comments (returns 1 if some comments were skipped)
static inline int Prs_ManUtilSkipComments( Prs_Man_t * p )
{
    if ( !Prs_ManIsChar(p, '/') )
        return 0;
    if ( Prs_ManIsChar1(p, '/') )
    {
        for ( p->pCur += 2; p->pCur < p->pLimit; p->pCur++ )
            if ( Prs_ManIsChar(p, '\n') )
                { p->pCur++; return 1; }
    }
    else if ( Prs_ManIsChar1(p, '*') )
    {
        for ( p->pCur += 2; p->pCur < p->pLimit; p->pCur++ )
            if ( Prs_ManIsChar(p, '*') && Prs_ManIsChar1(p, '/') )
                { p->pCur++; p->pCur++; return 1; }
    }
    return 0;
}
static inline int Prs_ManUtilSkipName( Prs_Man_t * p )
{
    if ( !Prs_ManIsChar(p, '\\') )
        return 0;
    for ( p->pCur++; p->pCur < p->pLimit; p->pCur++ )
        if ( Prs_ManIsChar(p, ' ') )
            { p->pCur++; return 1; }
    return 0;
}

// skip any number of spaces and comments
static inline int Prs_ManUtilSkipSpaces( Prs_Man_t * p )
{
    while ( p->pCur < p->pLimit )
    {
        while ( Prs_CharIsSpace(*p->pCur) ) 
            p->pCur++;
        if ( !*p->pCur )
            return Prs_ManErrorSet(p, "Unexpectedly reached end-of-file.", 1);
        if ( !Prs_ManUtilSkipComments(p) )
            return 0;
    }
    return Prs_ManErrorSet(p, "Unexpectedly reached end-of-file.", 1);
}
// skip everything including comments until the given char
static inline int Prs_ManUtilSkipUntil( Prs_Man_t * p, char c )
{
    while ( p->pCur < p->pLimit )
    {
        if ( Prs_ManIsChar(p, c) )
            return 1;
        if ( Prs_ManUtilSkipComments(p) )
            continue;
        if ( Prs_ManUtilSkipName(p) )
            continue;
        p->pCur++;
    }
    return 0;
}
// skip everything including comments until the given word
static inline int Prs_ManUtilSkipUntilWord( Prs_Man_t * p, char * pWord )
{
    char * pPlace = strstr( p->pCur, pWord );
    if ( pPlace == NULL )  return 1;
    p->pCur = pPlace + strlen(pWord);
    return 0;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int Prs_ManReadName( Prs_Man_t * p )
{
    char * pStart = p->pCur;
    if ( Prs_ManIsChar(p, '\\') ) // escaped name
    {
        pStart = ++p->pCur;
        while ( !Prs_ManIsChar(p, ' ') ) 
            p->pCur++;
    }    
    else if ( Prs_CharIsSymb1(*p->pCur) ) // simple name
    {
        p->pCur++;
        while ( Prs_CharIsSymb2(*p->pCur) ) 
            p->pCur++;
    }
    else 
        return 0;
    return Abc_NamStrFindOrAddLim( p->pStrs, pStart, p->pCur, NULL );
}
static inline int Prs_ManReadNameList( Prs_Man_t * p, Vec_Int_t * vTemp, char LastSymb )
{
    Vec_IntClear( vTemp );
    while ( 1 )
    {
        int Item = Prs_ManReadName(p);
        if ( Item == 0 )                    return Prs_ManErrorSet(p, "Cannot read name in the list.", 0);
        if ( Prs_ManUtilSkipSpaces(p) )     return Prs_ManErrorSet(p, "Error number 1.", 0);
        if ( Item == PRS_VER_WIRE  )
            continue;
        Vec_IntPush( vTemp, Item );
        if ( Prs_ManIsChar(p, LastSymb) )   break;
        if ( !Prs_ManIsChar(p, ',') )       return Prs_ManErrorSet(p, "Expecting comma in the list.", 0);
        p->pCur++;
        if ( Prs_ManUtilSkipSpaces(p) )     return Prs_ManErrorSet(p, "Error number 2.", 0);
    }
    return 1;
}
static inline int Prs_ManReadConstant( Prs_Man_t * p )
{
    char * pStart = p->pCur;
    assert( Prs_ManIsDigit(p) );
    while ( Prs_ManIsDigit(p) ) 
        p->pCur++;
    if ( !Prs_ManIsChar(p, '\'') )          return Prs_ManErrorSet(p, "Cannot read constant.", 0);
    p->pCur++;
    if ( Prs_ManIsChar(p, 'b') )
    {
        p->pCur++;
        while ( Prs_CharIsDigitB(*p->pCur) ) 
        {
            if ( *p->pCur == '0' )
                p->pNtk->fHasC0s = 1;
            else if ( *p->pCur == '1' )
                p->pNtk->fHasC1s = 1;
            else if ( *p->pCur == 'x' )
                p->pNtk->fHasCXs = 1;
            else if ( *p->pCur == 'z' )
                p->pNtk->fHasCZs = 1;
            p->pCur++;
        }
    }
    else if ( Prs_ManIsChar(p, 'h') )
    {
        p->pCur++;
        p->pNtk->fHasC0s = 1;
        while ( Prs_CharIsDigitH(*p->pCur) ) 
        {
            if ( *p->pCur != '0' )
                p->pNtk->fHasC1s = 1;
            p->pCur++;
        }
    }
    else if ( Prs_ManIsChar(p, 'd') )
    {
        p->pCur++;
        p->pNtk->fHasC0s = 1;
        while ( Prs_ManIsDigit(p) ) 
        {
            if ( *p->pCur != '0' )
                p->pNtk->fHasC1s = 1;
            p->pCur++;
        }
    }
    else                                    return Prs_ManErrorSet(p, "Cannot read radix of constant.", 0);
    return Abc_NamStrFindOrAddLim( p->pFuns, pStart, p->pCur, NULL );
}
static inline int Prs_ManReadRange( Prs_Man_t * p )
{
    assert( Prs_ManIsChar(p, '[') );
    Vec_StrClear( &p->vCover );
    Vec_StrPush( &p->vCover, *p->pCur++ );
    if ( Prs_ManUtilSkipSpaces(p) )         return Prs_ManErrorSet(p, "Error number 3.", 0);
    if ( !Prs_ManIsDigit(p) )               return Prs_ManErrorSet(p, "Cannot read digit in range specification.", 0);
    while ( Prs_ManIsDigit(p) )
        Vec_StrPush( &p->vCover, *p->pCur++ );
    if ( Prs_ManUtilSkipSpaces(p) )         return Prs_ManErrorSet(p, "Error number 4.", 0);
    if ( Prs_ManIsChar(p, ':') )
    {
        Vec_StrPush( &p->vCover, *p->pCur++ );
        if ( Prs_ManUtilSkipSpaces(p) )     return Prs_ManErrorSet(p, "Error number 5.", 0);
        if ( !Prs_ManIsDigit(p) )           return Prs_ManErrorSet(p, "Cannot read digit in range specification.", 0);
        while ( Prs_ManIsDigit(p) )
            Vec_StrPush( &p->vCover, *p->pCur++ );
        if ( Prs_ManUtilSkipSpaces(p) )     return Prs_ManErrorSet(p, "Error number 6.", 0);
    }
    if ( !Prs_ManIsChar(p, ']') )           return Prs_ManErrorSet(p, "Cannot read closing brace in range specification.", 0);
    Vec_StrPush( &p->vCover, *p->pCur++ );
    Vec_StrPush( &p->vCover, '\0' );
    return Abc_NamStrFindOrAdd( p->pStrs, Vec_StrArray(&p->vCover), NULL );
}
static inline int Prs_ManReadConcat( Prs_Man_t * p, Vec_Int_t * vTemp2 )
{
    extern int Prs_ManReadSignalList( Prs_Man_t * p, Vec_Int_t * vTemp, char LastSymb, int fAddForm );
    assert( Prs_ManIsChar(p, '{') );
    p->pCur++;
    if ( !Prs_ManReadSignalList( p, vTemp2, '}', 0 ) ) return Prs_ManErrorSet(p, "Error number 7.", 0);
    // check final
    assert( Prs_ManIsChar(p, '}') );
    p->pCur++;
    // return special case
    assert( Vec_IntSize(vTemp2) > 0 );
    if ( Vec_IntSize(vTemp2) == 1 )
        return Vec_IntEntry(vTemp2, 0);
    return Abc_Var2Lit2( Prs_NtkAddConcat(p->pNtk, vTemp2), CBA_PRS_CONCAT );
}
static inline int Prs_ManReadSignal( Prs_Man_t * p )
{
    int Item;
    if ( Prs_ManUtilSkipSpaces(p) )         return Prs_ManErrorSet(p, "Error number 8.", 0);
    if ( Prs_ManIsDigit(p) )
    {
        Item = Prs_ManReadConstant(p);
        if ( Item == 0 )                    return Prs_ManErrorSet(p, "Error number 9.", 0);
        if ( Prs_ManUtilSkipSpaces(p) )     return Prs_ManErrorSet(p, "Error number 10.", 0);
        return Abc_Var2Lit2( Item, CBA_PRS_CONST );
    }
    if ( Prs_ManIsChar(p, '{') )
    {
        if ( p->fUsingTemp2 )               return Prs_ManErrorSet(p, "Cannot read nested concatenations.", 0);
        p->fUsingTemp2 = 1;
        Item = Prs_ManReadConcat(p, &p->vTemp2);
        p->fUsingTemp2 = 0;
        if ( Item == 0 )                    return Prs_ManErrorSet(p, "Error number 11.", 0);
        if ( Prs_ManUtilSkipSpaces(p) )     return Prs_ManErrorSet(p, "Error number 12.", 0);
        return Item;
    }
    else
    {
        Item = Prs_ManReadName( p );
        if ( Item == 0 )                    return Prs_ManErrorSet(p, "Error number 13.", 0);    // was        return 1;                
        if ( Prs_ManUtilSkipSpaces(p) )     return Prs_ManErrorSet(p, "Error number 14.", 0);
        if ( Prs_ManIsChar(p, '[') )
        {
            int Range = Prs_ManReadRange(p);
            if ( Range == 0 )               return Prs_ManErrorSet(p, "Error number 15.", 0);
            if ( Prs_ManUtilSkipSpaces(p) ) return Prs_ManErrorSet(p, "Error number 16.", 0);
            return Abc_Var2Lit2( Prs_NtkAddSlice(p->pNtk, Item, Range), CBA_PRS_SLICE );
        }
        return Abc_Var2Lit2( Item, CBA_PRS_NAME );
    }
}
int Prs_ManReadSignalList( Prs_Man_t * p, Vec_Int_t * vTemp, char LastSymb, int fAddForm )
{
    Vec_IntClear( vTemp );
    while ( 1 )
    {
        int Item = Prs_ManReadSignal(p);
        if ( Item == 0 )                    return Prs_ManErrorSet(p, "Cannot read signal in the list.", 0);
        if ( fAddForm )
            Vec_IntPush( vTemp, 0 );
        Vec_IntPush( vTemp, Item );
        if ( Prs_ManIsChar(p, LastSymb) )   break;
        if ( !Prs_ManIsChar(p, ',') )       return Prs_ManErrorSet(p, "Expecting comma in the list.", 0);
        p->pCur++;
    }
    return 1;
}
static inline int Prs_ManReadSignalList2( Prs_Man_t * p, Vec_Int_t * vTemp )
{
    int FormId, ActItem;
    Vec_IntClear( vTemp );
    assert( Prs_ManIsChar(p, '.') );
    while ( Prs_ManIsChar(p, '.') )
    {
        p->pCur++;
        FormId = Prs_ManReadName( p );
        if ( FormId == 0 )                  return Prs_ManErrorSet(p, "Cannot read formal name of the instance.", 0);
        if ( !Prs_ManIsChar(p, '(') )       return Prs_ManErrorSet(p, "Cannot read \"(\" in the instance.", 0);
        p->pCur++;
        if ( Prs_ManUtilSkipSpaces(p) )     return Prs_ManErrorSet(p, "Error number 17.", 0);
        ActItem = Prs_ManReadSignal( p );
        if ( ActItem == 0 )                 return Prs_ManErrorSet(p, "Cannot read actual name of the instance.", 0);
        if ( !Prs_ManIsChar(p, ')') )       return Prs_ManErrorSet(p, "Cannot read \")\" in the instance.", 0);
        p->pCur++;
        Vec_IntPushTwo( vTemp, FormId, ActItem );
        if ( Prs_ManUtilSkipSpaces(p) )     return Prs_ManErrorSet(p, "Error number 18.", 0);
        if ( Prs_ManIsChar(p, ')') )        break;
        if ( !Prs_ManIsChar(p, ',') )       return Prs_ManErrorSet(p, "Expecting comma in the instance.", 0);
        p->pCur++;
        if ( Prs_ManUtilSkipSpaces(p) )     return Prs_ManErrorSet(p, "Error number 19.", 0);
    }
    assert( Vec_IntSize(vTemp) > 0 );
    assert( Vec_IntSize(vTemp) % 2 == 0 );
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int Prs_ManReadDeclaration( Prs_Man_t * p, int Type )
{
    int i, NameId, RangeId = 0;
    Vec_Int_t * vNames[4]  = { &p->pNtk->vInputs,  &p->pNtk->vOutputs,  &p->pNtk->vInouts,  &p->pNtk->vWires };
    Vec_Int_t * vNamesR[4] = { &p->pNtk->vInputsR, &p->pNtk->vOutputsR, &p->pNtk->vInoutsR, &p->pNtk->vWiresR };
    assert( Type >= PRS_VER_INPUT && Type <= PRS_VER_WIRE );
    if ( Prs_ManUtilSkipSpaces(p) )                                   return Prs_ManErrorSet(p, "Error number 20.", 0);
    if ( Prs_ManIsChar(p, '[') && !(RangeId = Prs_ManReadRange(p)) )  return Prs_ManErrorSet(p, "Error number 21.", 0);
    if ( !Prs_ManReadNameList( p, &p->vTemp, ';' ) )                  return Prs_ManErrorSet(p, "Error number 22.", 0);
    Vec_IntForEachEntry( &p->vTemp, NameId, i )
    {
        Vec_IntPush( vNames[Type - PRS_VER_INPUT], NameId );
        Vec_IntPush( vNamesR[Type - PRS_VER_INPUT], RangeId );
        if ( Type < PRS_VER_WIRE )
            Vec_IntPush( &p->pNtk->vOrder, Abc_Var2Lit2(NameId, Type) );
    }
    return 1;
}
static inline int Prs_ManReadAssign( Prs_Man_t * p )
{
    int OutItem, InItem, fCompl = 0, fCompl2 = 0, Oper = 0;
    // read output name
    OutItem = Prs_ManReadSignal( p );
    if ( OutItem == 0 )                     return Prs_ManErrorSet(p, "Cannot read output in assign-statement.", 0);
    if ( !Prs_ManIsChar(p, '=') )           return Prs_ManErrorSet(p, "Expecting \"=\" in assign-statement.", 0);
    p->pCur++;
    if ( Prs_ManUtilSkipSpaces(p) )         return Prs_ManErrorSet(p, "Error number 23.", 0);
    if ( Prs_ManIsChar(p, '~') ) 
    { 
        fCompl = 1; 
        p->pCur++; 
    }
    // write output name
    Vec_IntClear( &p->vTemp );
    Vec_IntPush( &p->vTemp, 0 );
    Vec_IntPush( &p->vTemp, OutItem );
    // read first name
    InItem = Prs_ManReadSignal( p );
    if ( InItem == 0 )                      return Prs_ManErrorSet(p, "Cannot read first input name in the assign-statement.", 0);
    Vec_IntPush( &p->vTemp, 0 );
    Vec_IntPush( &p->vTemp, InItem );
    // check unary operator
    if ( Prs_ManIsChar(p, ';') )
    {
        Vec_IntPush( &p->vTemp, 0 );
        Vec_IntPush( &p->vTemp, OutItem );
        Oper = fCompl ? CBA_BOX_INV : CBA_BOX_BUF;
        Prs_NtkAddBox( p->pNtk, Oper, 0, &p->vTemp );
        return 1;
    }
    if ( Prs_ManIsChar(p, '&') ) 
        Oper = CBA_BOX_AND;
    else if ( Prs_ManIsChar(p, '|') ) 
        Oper = CBA_BOX_OR;
    else if ( Prs_ManIsChar(p, '^') ) 
        Oper = CBA_BOX_XOR;
    else if ( Prs_ManIsChar(p, '?') ) 
        Oper = CBA_BOX_MUX;
    else                                    return Prs_ManErrorSet(p, "Unrecognized operator in the assign-statement.", 0);
    p->pCur++; 
    if ( Prs_ManUtilSkipSpaces(p) )         return Prs_ManErrorSet(p, "Error number 24.", 0);
    if ( Prs_ManIsChar(p, '~') ) 
    { 
        fCompl2 = 1; 
        p->pCur++; 
    }
    // read second name
    InItem = Prs_ManReadSignal( p );
    if ( InItem == 0 )                      return Prs_ManErrorSet(p, "Cannot read second input name in the assign-statement.", 0);
    Vec_IntPush( &p->vTemp, 0 );
    Vec_IntPush( &p->vTemp, InItem );
    // read third argument
    if ( Oper == CBA_BOX_MUX )
    {
        assert( fCompl == 0 ); 
        if ( !Prs_ManIsChar(p, ':') )       return Prs_ManErrorSet(p, "Expected colon in the MUX assignment.", 0);
        p->pCur++; 
        // read third name
        InItem = Prs_ManReadSignal( p );
        if ( InItem == 0 )                  return Prs_ManErrorSet(p, "Cannot read third input name in the assign-statement.", 0);
        Vec_IntPush( &p->vTemp, 0 );
        Vec_IntPush( &p->vTemp, InItem );
        if ( !Prs_ManIsChar(p, ';') )       return Prs_ManErrorSet(p, "Expected semicolon at the end of the assign-statement.", 0);
    }
    else
    {
        // figure out operator
        if ( Oper == CBA_BOX_AND )
        {
            if ( fCompl && !fCompl2 )
                Oper = CBA_BOX_SHARPL;
            else if ( !fCompl && fCompl2 )
                Oper = CBA_BOX_SHARP;
            else if ( fCompl && fCompl2 )
                Oper = CBA_BOX_NOR;
        }
        else if ( Oper == CBA_BOX_OR )
        {
            if ( fCompl && fCompl2 )
                Oper = CBA_BOX_NAND;
            else assert( !fCompl && !fCompl2 );
        }
        else if ( Oper == CBA_BOX_XOR )
        {
            if ( fCompl && !fCompl2 )
                Oper = CBA_BOX_XNOR;
            else assert( !fCompl && !fCompl2 );
        }
    }
    // save binary operator
    Prs_NtkAddBox( p->pNtk, Oper, 0, &p->vTemp );
    return 1;
}
static inline int Prs_ManReadInstance( Prs_Man_t * p, int Func )
{
    int InstId, Status;
/*
    static Counter = 0;
    if ( ++Counter == 7 )
    {
        int s=0;
    }
*/
    if ( Prs_ManUtilSkipSpaces(p) )         return Prs_ManErrorSet(p, "Error number 25.", 0);
    if ( (InstId = Prs_ManReadName(p)) )
        if (Prs_ManUtilSkipSpaces(p))       return Prs_ManErrorSet(p, "Error number 26.", 0);
    if ( !Prs_ManIsChar(p, '(') )           return Prs_ManErrorSet(p, "Expecting \"(\" in module instantiation.", 0);
    p->pCur++;
    if ( Prs_ManUtilSkipSpaces(p) )         return Prs_ManErrorSet(p, "Error number 27.", 0);
    if ( Prs_ManIsChar(p, '.') ) // box
        Status = Prs_ManReadSignalList2(p, &p->vTemp);
    else  // node
    {
        //char * s = Abc_NamStr(p->pStrs, Func);
        // translate elementary gate
        int iFuncNew = Prs_ManIsVerilogPrim(Abc_NamStr(p->pStrs, Func));
        if ( iFuncNew == 0 )                return Prs_ManErrorSet(p, "Cannot find elementary gate.", 0);
        Func = iFuncNew;
        Status = Prs_ManReadSignalList( p, &p->vTemp, ')', 1 );
    }
    if ( Status == 0 )                      return Prs_ManErrorSet(p, "Error number 28.", 0);
    assert( Prs_ManIsChar(p, ')') );
    p->pCur++;
    if ( Prs_ManUtilSkipSpaces(p) )         return Prs_ManErrorSet(p, "Error number 29.", 0);
    if ( !Prs_ManIsChar(p, ';') )           return Prs_ManErrorSet(p, "Expecting semicolon in the instance.", 0);
    // add box 
    Prs_NtkAddBox( p->pNtk, Func, InstId, &p->vTemp );
    return 1;
}
static inline int Prs_ManReadArguments( Prs_Man_t * p )
{
    int iRange = 0, iType = -1;
    Vec_Int_t * vSigs[3]  = { &p->pNtk->vInputs,  &p->pNtk->vOutputs,  &p->pNtk->vInouts  };
    Vec_Int_t * vSigsR[3] = { &p->pNtk->vInputsR, &p->pNtk->vOutputsR, &p->pNtk->vInoutsR };
    assert( Prs_ManIsChar(p, '(') );
    p->pCur++;
    if ( Prs_ManUtilSkipSpaces(p) )             return Prs_ManErrorSet(p, "Error number 30.", 0);
    while ( 1 )
    {
        int iName = Prs_ManReadName( p );
        if ( iName == 0 )                       return Prs_ManErrorSet(p, "Error number 31.", 0);
        if ( Prs_ManUtilSkipSpaces(p) )         return Prs_ManErrorSet(p, "Error number 32.", 0);
        if ( iName >= PRS_VER_INPUT && iName <= PRS_VER_INOUT ) // declaration
        {
            iType = iName;
            if ( Prs_ManIsChar(p, '[') )
            {
                iRange = Prs_ManReadRange(p);
                if ( iRange == 0 )              return Prs_ManErrorSet(p, "Error number 33.", 0);
                if ( Prs_ManUtilSkipSpaces(p) ) return Prs_ManErrorSet(p, "Error number 34.", 0);
            }
            iName = Prs_ManReadName( p );
            if ( iName == 0 )                   return Prs_ManErrorSet(p, "Error number 35.", 0);
        }
        if ( iType > 0 )
        {
            Vec_IntPush( vSigs[iType - PRS_VER_INPUT], iName );
            Vec_IntPush( vSigsR[iType - PRS_VER_INPUT], iRange );
            Vec_IntPush( &p->pNtk->vOrder, Abc_Var2Lit2(iName, iType) );
        }
        if ( Prs_ManIsChar(p, ')') )
            break;
        if ( !Prs_ManIsChar(p, ',') )           return Prs_ManErrorSet(p, "Expecting comma in the instance.", 0);
        p->pCur++;
        if ( Prs_ManUtilSkipSpaces(p) )         return Prs_ManErrorSet(p, "Error number 36.", 0);
    }
    // check final
    assert( Prs_ManIsChar(p, ')') );
    return 1;
}
// this procedure can return:
// 0 = reached end-of-file; 1 = successfully parsed; 2 = recognized as primitive; 3 = failed and skipped; 4 = error (failed and could not skip)
static inline int Prs_ManReadModule( Prs_Man_t * p )
{
    int iToken, Status;
    if ( p->pNtk != NULL )                  return Prs_ManErrorSet(p, "Parsing previous module is unfinished.", 4);
    if ( Prs_ManUtilSkipSpaces(p) )
    { 
        Prs_ManErrorClear( p );       
        return 0; 
    }
    // read keyword
    iToken = Prs_ManReadName( p );
    if ( iToken != PRS_VER_MODULE )         return Prs_ManErrorSet(p, "Cannot read \"module\" keyword.", 4);
    if ( Prs_ManUtilSkipSpaces(p) )         return 4;
    // read module name
    iToken = Prs_ManReadName( p );
    if ( iToken == 0 )                      return Prs_ManErrorSet(p, "Cannot read module name.", 4);
    if ( Prs_ManIsKnownModule(Abc_NamStr(p->pStrs, iToken)) )
    {
        if ( Prs_ManUtilSkipUntilWord( p, "endmodule" ) ) return Prs_ManErrorSet(p, "Cannot find \"endmodule\" keyword.", 4);
        //printf( "Warning! Skipped known module \"%s\".\n", Abc_NamStr(p->pStrs, iToken) );
        Vec_IntPush( &p->vKnown, iToken );
        return 2;
    }
    Prs_ManInitializeNtk( p, iToken, 1 );
    // skip arguments
    if ( Prs_ManUtilSkipSpaces(p) )         return 4;
    if ( !Prs_ManIsChar(p, '(') )           return Prs_ManErrorSet(p, "Cannot find \"(\" in the argument declaration.", 4);
    if ( !Prs_ManReadArguments(p) )         return 4;
    assert( *p->pCur == ')' );
    p->pCur++;
    if ( Prs_ManUtilSkipSpaces(p) )         return 4;
    // read declarations and instances
    while ( Prs_ManIsChar(p, ';') )
    {
        p->pCur++;
        if ( Prs_ManUtilSkipSpaces(p) )     return 4;
        iToken = Prs_ManReadName( p );
        if ( iToken == PRS_VER_ENDMODULE )
        {
            Vec_IntPush( &p->vSucceeded, p->pNtk->iModuleName );
            Prs_ManFinalizeNtk( p );
            return 1;
        }
        if ( iToken >= PRS_VER_INPUT && iToken <= PRS_VER_WIRE ) // declaration
            Status = Prs_ManReadDeclaration( p, iToken );
        else if ( iToken == PRS_VER_REG || iToken == PRS_VER_DEFPARAM ) // unsupported keywords
            Status = Prs_ManUtilSkipUntil( p, ';' );
        else // read instance
        {
            if ( iToken == PRS_VER_ASSIGN )
                Status = Prs_ManReadAssign( p );
            else
                Status = Prs_ManReadInstance( p, iToken );
            if ( Status == 0 )
            {
                if ( Prs_ManUtilSkipUntilWord( p, "endmodule" ) ) return Prs_ManErrorSet(p, "Cannot find \"endmodule\" keyword.", 4);
                //printf( "Warning! Failed to parse \"%s\". Adding module \"%s\" as blackbox.\n", 
                //    Abc_NamStr(p->pStrs, iToken), Abc_NamStr(p->pStrs, p->pNtk->iModuleName) );
                Vec_IntPush( &p->vFailed, p->pNtk->iModuleName );
                // cleanup
                Vec_IntErase( &p->pNtk->vWires );
                Vec_IntErase( &p->pNtk->vWiresR );
                Vec_IntErase( &p->pNtk->vSlices );
                Vec_IntErase( &p->pNtk->vConcats );
                Vec_IntErase( &p->pNtk->vBoxes );
                Vec_IntErase( &p->pNtk->vObjs );
                p->fUsingTemp2 = 0;
                // add
                Prs_ManFinalizeNtk( p );
                Prs_ManErrorClear( p );
                return 3;
            }
        }
        if ( !Status )                      return 4;
        if ( Prs_ManUtilSkipSpaces(p) )     return 4;
    }
    return Prs_ManErrorSet(p, "Cannot find \";\" in the module definition.", 4);
}
static inline int Prs_ManReadDesign( Prs_Man_t * p )
{
    while ( 1 )
    {
        int RetValue = Prs_ManReadModule( p );
        if ( RetValue == 0 ) // end of file
            break;
        if ( RetValue == 1 ) // successfully parsed
            continue;
        if ( RetValue == 2 ) // recognized as primitive
            continue;
        if ( RetValue == 3 ) // failed and skipped
            continue;
        if ( RetValue == 4 ) // error
            return 0;
        assert( 0 );
    }
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Prs_ManPrintModules( Prs_Man_t * p )
{
    char * pName; int i; 
    printf( "Succeeded parsing %d models:\n", Vec_IntSize(&p->vSucceeded) );
    Prs_ManForEachNameVec( &p->vSucceeded, p, pName, i )
        printf( " %s", pName );
    printf( "\n" );
    printf( "Skipped %d known models:\n", Vec_IntSize(&p->vKnown) );
    Prs_ManForEachNameVec( &p->vKnown, p, pName, i )
        printf( " %s", pName );
    printf( "\n" );
    printf( "Skipped %d failed models:\n", Vec_IntSize(&p->vFailed) );
    Prs_ManForEachNameVec( &p->vFailed, p, pName, i )
        printf( " %s", pName );
    printf( "\n" );
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Prs_ManReadVerilog( char * pFileName )
{
    Vec_Ptr_t * vPrs = NULL;
    Prs_Man_t * p = Prs_ManAlloc( pFileName );
    if ( p == NULL )
        return NULL;
    Abc_NamStrFindOrAdd( p->pFuns, "1\'b0", NULL );
    Abc_NamStrFindOrAdd( p->pFuns, "1\'b1", NULL );
    Abc_NamStrFindOrAdd( p->pFuns, "1\'bx", NULL );
    Abc_NamStrFindOrAdd( p->pFuns, "1\'bz", NULL );
    Prs_NtkAddVerilogDirectives( p );
    Prs_ManReadDesign( p );   
    Prs_ManPrintModules( p );
    if ( Prs_ManErrorPrint(p) )
        ABC_SWAP( Vec_Ptr_t *, vPrs, p->vNtks );
    Prs_ManFree( p );
    return vPrs;
}

void Prs_ManReadVerilogTest( char * pFileName )
{
    abctime clk = Abc_Clock();
    Vec_Ptr_t * vPrs = Prs_ManReadVerilog( pFileName );
    if ( !vPrs ) return;
    printf( "Finished reading %d networks. ", Vec_PtrSize(vPrs) );
    printf( "NameIDs = %d. ", Abc_NamObjNumMax(Prs_ManNameMan(vPrs)) );
    printf( "Memory = %.2f MB. ", 1.0*Prs_ManMemory(vPrs)/(1<<20) );
    Abc_PrintTime( 1, "Time", Abc_Clock() - clk );
    Prs_ManWriteVerilog( Extra_FileNameGenericAppend(pFileName, "_out.v"), vPrs );
    Prs_ManVecFree( vPrs );
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Prs_CreateVerilogFindFon( Cba_Ntk_t * p, int NameId )
{
    int iFon = Cba_NtkGetMap( p, NameId );
    if ( iFon )
        return iFon;
    printf( "Network \"%s\": Signal \"%s\" is not driven.\n", Cba_NtkName(p), Cba_NtkStr(p, NameId) );
    return 0;
}
int Prs_CreateSlice( Cba_Ntk_t * p, int iFon, Prs_Ntk_t * pNtk, int Left, int Right )
{
    char Buffer[1000];
    int iObj, iFonNew, NameId;
    if ( Left != Right )
        sprintf( Buffer, "%s[%d:%d]", Cba_FonNameStr(p, iFon), Left, Right );
    else
        sprintf( Buffer, "%s[%d]", Cba_FonNameStr(p, iFon), Right );
    // check existing slice
    NameId = Cba_NtkNewStrId( p, Buffer );
    iFonNew = Cba_NtkGetMap( p, NameId );
    if ( iFonNew )
        return iFonNew;
    // create slice
    iObj = Cba_ObjAlloc( p, CBA_BOX_SLICE, 1, 1 );
    Cba_ObjSetName( p, iObj, NameId );
    Cba_ObjSetFinFon( p, iObj, 0, iFon );
    iFonNew = Cba_ObjFon0(p, iObj);
    Cba_FonSetLeft( p, iFonNew, Abc_AbsInt(Left-Right) );
    Cba_FonSetRight( p, iFonNew, 0 );
    Cba_FonSetName( p, iFonNew, NameId );
    Cba_NtkSetMap( p, NameId, iFonNew );
    return iFonNew;        
}
int Prs_CreateCatIn( Cba_Ntk_t * p, Prs_Ntk_t * pNtk, int Con )
{
    extern int Prs_CreateSignalIn( Cba_Ntk_t * p, Prs_Ntk_t * pNtk, int Sig );
    char Buffer[100]; int i, Sig, iObj, iFon, NameId, nBits = 0;
    Vec_Int_t * vSigs = Prs_CatSignals(pNtk, Con);
    // create input concatenation
    iObj = Cba_ObjAlloc( p, CBA_BOX_CATIN, Vec_IntSize(vSigs), 1 );
    iFon = Cba_ObjFon0(p, iObj);
    sprintf( Buffer, "_icc%d_", iObj );
    NameId = Cba_NtkNewStrId( p, Buffer );
    Cba_FonSetName( p, iFon, NameId );
    Cba_NtkSetMap( p, NameId, iFon );
    // set inputs
    Vec_IntForEachEntry( vSigs, Sig, i )
    {
        iFon = Prs_CreateSignalIn( p, pNtk, Sig );
        if ( iFon )
            Cba_ObjSetFinFon( p, iObj, i, iFon );
        if ( iFon )
            nBits += Cba_FonRange( p, iFon );
    }
    iFon = Cba_ObjFon0(p, iObj);
    Cba_FonSetLeft( p, iFon, nBits-1 );
    Cba_FonSetRight( p, iFon, 0 );
    return Cba_ObjFon0(p, iObj);
}
int Prs_CreateSignalIn( Cba_Ntk_t * p, Prs_Ntk_t * pNtk, int Sig )
{
    int Left, Right, iFon, Value = Abc_Lit2Var2( Sig );
    Prs_ManType_t Type = (Prs_ManType_t)Abc_Lit2Att2( Sig );
    if ( !Sig ) return 0;
    if ( Type == CBA_PRS_NAME )
        return Prs_CreateVerilogFindFon( p, Cba_NtkNewStrId(p, Prs_NtkStr(pNtk, Value)) );
    if ( Type == CBA_PRS_CONST )
        return Cba_FonFromConst( Value );
    if ( Type == CBA_PRS_SLICE )
    {
        iFon =  Prs_CreateVerilogFindFon( p, Cba_NtkNewStrId(p, Prs_NtkStr(pNtk, Prs_SliceName(pNtk, Value))) );
        if ( !iFon )
            return 0;
        Prs_NtkParseRange( pNtk, Prs_SliceRange(pNtk, Value), &Left, &Right );
        return Prs_CreateSlice( p, iFon, pNtk, Left, Right );
    }
    assert( Type == CBA_PRS_CONCAT );
    return Prs_CreateCatIn( p, pNtk, Value );
}

int Prs_CreateCatOut( Cba_Ntk_t * p, int iFon, Prs_Ntk_t * pNtk, int Con )
{
    int i, Sig, iObj, iFonNew, NameId;
    Vec_Int_t * vSigs = Prs_CatSignals(pNtk, Con);
    char FonName[100]; sprintf( FonName, "_occ%d_", iFon );
    NameId = Cba_NtkNewStrId( p, FonName ); 
    Cba_FonSetName( p, iFon, NameId );
    Cba_NtkSetMap( p, NameId, iFon );
    // range of iFon is not ready and will be set later
    // create output concatenation
    iObj = Cba_ObjAlloc( p, CBA_BOX_CATOUT, 1, Vec_IntSize(vSigs) );
    Cba_ObjSetFinFon( p, iObj, 0, iFon );
    // set outputs
    Vec_IntForEachEntry( vSigs, Sig, i )
    {
        Prs_ManType_t Type = (Prs_ManType_t)Abc_Lit2Att2( Sig );
        assert( Type == CBA_PRS_NAME );
        NameId = Cba_NtkNewStrId( p, Prs_NtkStr(pNtk, Abc_Lit2Var2(Sig)) ); 
        iFonNew = Cba_ObjFon( p, iObj, Vec_IntSize(vSigs)-1-i );
        Cba_FonSetName( p, iFonNew, NameId );
        Cba_NtkSetMap( p, NameId, iFonNew );
    }
    return iObj;
}
void Prs_CreateSignalOut( Cba_Ntk_t * p, int iFon, Prs_Ntk_t * pNtk, int Sig )
{
    int Value = Abc_Lit2Var2( Sig );
    Prs_ManType_t Type = (Prs_ManType_t)Abc_Lit2Att2( Sig );
    if ( !Sig ) return;
    if ( Type == CBA_PRS_NAME )
    {
        int NameId = Cba_NtkNewStrId(p, Prs_NtkStr(pNtk, Value));
        Cba_FonSetName( p, iFon, NameId );
        Cba_NtkSetMap( p, NameId, iFon );
        return;
    }
    assert( Type == CBA_PRS_SLICE );
    Prs_CreateCatOut( p, iFon, pNtk, Value );
}
// looks at multi-bit signal; if one bit is repeated, returns this bit; otherwise, returns -1
int Prs_CreateBitSignal( Prs_Ntk_t * pNtk, int Sig )
{
    Vec_Int_t * vSigs;
    int i, SigTemp, SigOne = -1, Value = Abc_Lit2Var2( Sig );
    Prs_ManType_t Type = (Prs_ManType_t)Abc_Lit2Att2( Sig );
    if ( Type == CBA_PRS_NAME || Type == CBA_PRS_SLICE )
        return -1;
    if ( Type == CBA_PRS_CONST )
    {
        int fOnly0 = 1, fOnly1 = 1;
        char * pConst = Prs_NtkConst(pNtk, Value);
        pConst = strchr( pConst, '\'' ) + 1;
        assert( *pConst == 'b' );
        while ( *++pConst )
            if ( *pConst == '0' )
                fOnly1 = 0;
            else if ( *pConst == '1' )
                fOnly0 = 0;
        if ( fOnly0 )
            return Abc_Var2Lit2( 1, CBA_PRS_CONST ); // const0
        if ( fOnly1 )
            return Abc_Var2Lit2( 2, CBA_PRS_CONST ); // const1
        return -1;
    }
    assert( Type == CBA_PRS_CONCAT );
    vSigs = Prs_CatSignals( pNtk, Value );
    Vec_IntForEachEntry( vSigs, SigTemp, i )
    {
        Value = Abc_Lit2Var2( SigTemp );
        Type = (Prs_ManType_t)Abc_Lit2Att2( SigTemp );
        if ( Type != CBA_PRS_NAME )
            return -1;
        if ( SigOne == -1 )
            SigOne = Value;
        else if ( SigOne != Value )
            return -1;
    }
    assert( SigOne >= 0 );
    return Abc_Var2Lit2( SigOne, CBA_PRS_NAME );
}
void Prs_CreateVerilogPio( Cba_Ntk_t * p, Prs_Ntk_t * pNtk )
{
    int i, NameId, RangeId, Left, Right, iObj, iFon;
    Cba_NtkCleanObjFuncs( p );
    Cba_NtkCleanObjNames( p );
    Cba_NtkCleanFonNames( p );
    Cba_NtkCleanFonLefts( p );
    Cba_NtkCleanFonRights( p );
    // create inputs
    Cba_NtkCleanMap( p );
    assert( Vec_IntSize(&pNtk->vInouts) == 0 );
    Vec_IntForEachEntryTwo( &pNtk->vInputs, &pNtk->vInputsR, NameId, RangeId, i )
    {
        iObj = Cba_ObjAlloc( p, CBA_OBJ_PI, 0, 1 );
        Cba_ObjSetName( p, iObj, NameId ); // direct name
        iFon = Cba_ObjFon0(p, iObj);
        if ( RangeId )
        {
            Prs_NtkParseRange( pNtk, RangeId, &Left, &Right );
            Cba_FonSetLeft( p, iFon, Left );
            Cba_FonSetRight( p, iFon, Right );
        }
        Cba_FonSetName( p, iFon, NameId );
        Cba_NtkSetMap( p, NameId, iObj );
    }
    // create outputs
    Vec_IntForEachEntryTwo( &pNtk->vOutputs, &pNtk->vOutputsR, NameId, RangeId, i )
    {
        iObj = Cba_ObjAlloc( p, CBA_OBJ_PO, 1, 0 );
        Cba_ObjSetName( p, iObj, NameId ); // direct name
        Cba_NtkSetMap( p, NameId, iObj );
    }
    // create order
    Vec_IntForEachEntry( &pNtk->vOrder, NameId, i )
    {
        iObj = Prs_CreateVerilogFindFon( p, Abc_Lit2Var2(NameId) ); // labeled name
        if ( iObj )
            Vec_IntPush( &p->vOrder, iObj );
    }
}
int Prs_CreateVerilogNtk( Cba_Ntk_t * p, Prs_Ntk_t * pNtk )
{
    Vec_Int_t * vBox;
    int i, k, iObj, iTerm, iFon, FormId, ActId;
    int NameId, RangeId, Left, Right;
    // map inputs
    Cba_NtkCleanMap( p );
    Cba_NtkForEachPi( p, iObj, i )
        Cba_NtkSetMap( p, Cba_ObjName(p, iObj), Cba_ObjFon0(p, iObj) );
    // create objects
    Prs_NtkForEachBox( pNtk, vBox, i )
    {
        if ( Prs_BoxIsNode(pNtk, i) ) // node
        {
            int nSigs = Prs_BoxIONum( pNtk, i );
            iObj = Cba_ObjAlloc( p, Prs_BoxNtk(pNtk, i), nSigs-1, 1 );
            Prs_CreateSignalOut( p, Cba_ObjFon0(p, iObj), pNtk, Vec_IntEntry(vBox, 1) ); // node output
            //Cba_ObjSetFunc( p, iObj, FuncId );
        }
        else // box
        {
            Cba_Ntk_t * pBox = NULL; int nInputs, nOutputs = 1;
            char ** pOutNames, * pNtkName = Prs_NtkStr(pNtk, Prs_BoxNtk(pNtk, i));
            Cba_ObjType_t Type = Prs_ManFindType( pNtkName, &nInputs, 1, &pOutNames );
            if ( Type == CBA_OBJ_BOX )
            {
                pBox = Cba_ManNtkFind( p->pDesign, pNtkName );
                if ( pBox == NULL )
                {
                    printf( "Fatal error: Cannot find module \"%s\".\n", pNtkName );
                    continue;
                }
                nInputs = Cba_NtkPiNum(pBox);
                nOutputs = Cba_NtkPoNum(pBox);
            }
            else if ( Type == CBA_BOX_ADD )
                nOutputs = 2;
            else if ( Type == CBA_BOX_NMUX )
                nInputs = 1 + (1 << atoi(pNtkName+strlen("wide_mux_")));
            else if ( Type == CBA_BOX_SEL )
                nInputs = 1 + atoi(pNtkName+strlen("wide_select_"));
            // create object
            iObj = Cba_ObjAlloc( p, Type, nInputs, nOutputs );
            if ( pBox ) Cba_ObjSetFunc( p, iObj, Cba_NtkId(pBox) );
            // mark PO objects
            Cba_NtkCleanMap2( p );
            if ( pBox )
                Cba_NtkForEachPo( pBox, iTerm, k )
                    Cba_NtkSetMap2( p, Cba_ObjName(pBox, iTerm), k+1 );
            else
                for ( k = 0; k < nOutputs; k++ )
                    Cba_NtkSetMap2( p, Cba_NtkStrId(p, pOutNames[k]), k+1 );
            // map box fons
            Vec_IntForEachEntryDouble( vBox, FormId, ActId, k )
                if ( Cba_NtkGetMap2(p, FormId) )
                {
                    iFon = Cba_ObjFon(p, iObj, Cba_NtkGetMap2(p, FormId)-1);
                    Prs_CreateSignalOut( p, iFon, pNtk, ActId );
                }
        }
        if ( Prs_BoxName(pNtk, i) )
            Cba_ObjSetName( p, iObj, Prs_BoxName(pNtk, i) );
    }

    // add wire ranges 
    Vec_IntForEachEntryTwo( &pNtk->vWires, &pNtk->vWiresR, NameId, RangeId, i )
    {
        iFon = Prs_CreateVerilogFindFon( p, NameId ); // direct name
        if ( iFon == 0 || RangeId == 0 ) // unused wire or 1-bit wire (default)
            continue;
        Prs_NtkParseRange( pNtk, RangeId, &Left, &Right );
        Cba_FonSetLeft( p, iFon, Left );
        Cba_FonSetRight( p, iFon, Right );
    }
    Vec_IntForEachEntryTwo( &pNtk->vOutputs, &pNtk->vOutputsR, NameId, RangeId, i )
    {
        iFon = Prs_CreateVerilogFindFon( p, NameId ); // direct name
        if ( iFon == 0 || RangeId == 0 ) // unused wire or 1-bit wire (default)
            continue;
        Prs_NtkParseRange( pNtk, RangeId, &Left, &Right );
        Cba_FonSetLeft( p, iFon, Left );
        Cba_FonSetRight( p, iFon, Right );
    }

    // set cat-out ranges
    Cba_NtkForEachObj( p, iObj )
        if ( Cba_ObjType(p, iObj) == CBA_BOX_CATOUT )
        {
            int nBits = 0;
            Cba_ObjForEachFon( p, iObj, iFon, k )
                nBits += Cba_FonRange(p, iFon);
            iFon = Cba_ObjFinFon( p, iObj, 0 );
            Cba_FonSetLeft ( p, iFon, nBits-1 );
            Cba_FonSetRight( p, iFon, 0 );
        }

    // connect objects
    Prs_NtkForEachBox( pNtk, vBox, i )
    {
        iObj = Cba_NtkPiNum(p) + Cba_NtkPoNum(p) + i + 1;
        if ( Prs_BoxIsNode(pNtk, i) ) // node
        {
            Vec_IntForEachEntryDoubleStart( vBox, FormId, ActId, k, 2 )
            {
                iFon = Prs_CreateSignalIn( p, pNtk, ActId );
                if ( iFon )
                    Cba_ObjSetFinFon( p, iObj, k/2-1, iFon ); 
            }
        }
        else // box
        {
            int nInputs = -1;
            char ** pInNames, * pNtkName = Prs_NtkStr(pNtk, Prs_BoxNtk(pNtk, i));
            Cba_ObjType_t Type = Prs_ManFindType( pNtkName, &nInputs, 0, &pInNames );
            assert( Type == Cba_ObjType(p, iObj) );
            // mark PI objects
            Cba_NtkCleanMap2( p );
            if ( Type == CBA_OBJ_BOX )
            {
                Cba_Ntk_t * pBox = Cba_ObjNtk(p, iObj);
                assert( Cba_NtkPiNum(pBox) == Cba_ObjFinNum(p, iObj) );
                assert( Cba_NtkPoNum(pBox) == Cba_ObjFonNum(p, iObj) );
                Cba_NtkForEachPi( pBox, iTerm, k )
                    Cba_NtkSetMap2( p, Cba_ObjName(pBox, iTerm), k+1 );
            }
            else
            {
                assert( nInputs >= 0 );
                for ( k = 0; k < nInputs; k++ )
                    Cba_NtkSetMap2( p, Cba_NtkStrId(p, pInNames[k]), k+1 );
            }
            if ( !strncmp(pNtkName, "wide_dffrs_", strlen("wide_dffrs_")) || !strncmp(pNtkName, "wide_dlatchrs_", strlen("wide_dlatchrs_")) )
            {
                int iSigSet = -1, iSigRst = -1;
                int IndexSet = -1, IndexRst = -1;
                int iBitSet, iBitRst;
                assert( Type == CBA_BOX_DFFRS || Type == CBA_BOX_LATCHRS );
                Vec_IntForEachEntryDouble( vBox, FormId, ActId, k )
                    if ( Cba_NtkGetMap2(p, FormId) == 2 ) // plus 1
                        iSigSet = ActId, IndexSet = k+1;
                    else if ( Cba_NtkGetMap2(p, FormId) == 3 ) // plus 1
                        iSigRst = ActId, IndexRst = k+1;
                assert( iSigSet >= 0 && iSigRst >= 0 );
                iBitSet = Prs_CreateBitSignal( pNtk, iSigSet );
                iBitRst = Prs_CreateBitSignal( pNtk, iSigRst );
                if ( iBitSet == -1 || iBitSet == -1 )
                {
                    // perform blasting of the flop/latch
                    assert( 0 );
                    continue;
                }
                // update box
                Vec_IntWriteEntry( vBox, IndexSet, iBitSet );
                Vec_IntWriteEntry( vBox, IndexRst, iBitRst );
            }
            // connect box fins
            Vec_IntForEachEntryDouble( vBox, FormId, ActId, k )
                if ( Cba_NtkGetMap2(p, FormId) )
                {
                    iFon = Prs_CreateSignalIn( p, pNtk, ActId );
                    if ( iFon )
                        Cba_ObjSetFinFon( p, iObj, Cba_NtkGetMap2(p, FormId)-1, iFon );
                }
            // special cases
            if ( Type == CBA_BOX_NMUX || Type == CBA_BOX_SEL )
            {
                int FonCat = Cba_ObjFinFon( p, iObj, 1 );
                int nBits  = Cba_FonRange( p, FonCat );
                int nParts = Cba_ObjFinNum(p, iObj) - 1;
                int Slice  = nBits / nParts;
                assert( Cba_ObjFinNum(p, iObj) > 2 );
                assert( Slice * nParts == nBits );
                Cba_ObjCleanFinFon( p, iObj, 1 );
                for ( k = 0; k < nParts; k++ )
                {
//                    iFon = Prs_CreateSlice( p, FonCat, pNtk, (nParts-1-k)*Slice+Slice-1, (nParts-1-k)*Slice );
                    iFon = Prs_CreateSlice( p, FonCat, pNtk, k*Slice+Slice-1, k*Slice );
                    Cba_ObjSetFinFon( p, iObj, k+1, iFon );
                }
            }
        }
    }
    // connect outputs
    Vec_IntForEachEntryTwo( &pNtk->vOutputs, &pNtk->vOutputsR, NameId, RangeId, i )
    {
        iObj = Cba_NtkPo( p, i );
        assert( NameId == Cba_ObjName(p, iObj) ); // direct name
        iFon = Prs_CreateVerilogFindFon( p, NameId );
        if ( !iFon ) 
            continue;
        Cba_ObjSetFinFon( p, iObj, 0, iFon );
        if ( RangeId )
        {
            Prs_NtkParseRange( pNtk, RangeId, &Left, &Right );
            assert( Left  == Cba_FonLeft(p, iFon) );
            assert( Right == Cba_FonRight(p, iFon) );
        }
    }
    return 0;
}
Cba_Man_t * Prs_ManBuildCbaVerilog( char * pFileName, Vec_Ptr_t * vDes )
{
    Prs_Ntk_t * pPrsNtk; int i, fError = 0;
    Prs_Ntk_t * pPrsRoot = Prs_ManRoot(vDes);
    // start the manager
    Abc_Nam_t * pStrs = Abc_NamRef(pPrsRoot->pStrs);
    Abc_Nam_t * pFuns = Abc_NamRef(pPrsRoot->pFuns);
    Abc_Nam_t * pMods = Abc_NamStart( 100, 24 );
    Cba_Man_t * p = Cba_ManAlloc( pFileName, Vec_PtrSize(vDes), pStrs, pFuns, pMods );
    // initialize networks
    Vec_PtrForEachEntry( Prs_Ntk_t *, vDes, pPrsNtk, i )
    {
        Cba_Ntk_t * pNtk = Cba_NtkAlloc( p, Prs_NtkId(pPrsNtk), Prs_NtkPiNum(pPrsNtk), Prs_NtkPoNum(pPrsNtk), Prs_NtkObjNum(pPrsNtk), 100, 100 );
        Prs_CreateVerilogPio( pNtk, pPrsNtk );
        Cba_NtkAdd( p, pNtk );
    }
    // create networks
    Vec_PtrForEachEntry( Prs_Ntk_t *, vDes, pPrsNtk, i )
    {
        printf( "Elaboration module \"%s\"...\n", Prs_NtkName(pPrsNtk) );
        fError = Prs_CreateVerilogNtk( Cba_ManNtk(p, i+1), pPrsNtk );
        if ( fError )
            break;
    }
    if ( fError )
        printf( "Quitting because of errors.\n" );
    else
        Cba_ManPrepareSeq( p );
    return p;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Cba_Man_t * Cba_ManReadVerilog( char * pFileName )
{
    Cba_Man_t * p = NULL;
    Vec_Ptr_t * vDes = Prs_ManReadVerilog( pFileName );
    if ( vDes && Vec_PtrSize(vDes) )
        p = Prs_ManBuildCbaVerilog( pFileName, vDes );
    if ( vDes )
        Prs_ManVecFree( vDes );
    return p;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END

