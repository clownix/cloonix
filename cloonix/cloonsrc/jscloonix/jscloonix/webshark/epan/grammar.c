#include "config.h"
#define WS_LOG_DOMAIN LOG_DOMAIN_DFILTER
#include <assert.h>
#include "dfilter/dfilter-int.h"
#include "dfilter/syntax-tree.h"
#include "dfilter/sttype-field.h"
#include "dfilter/sttype-slice.h"
#include "dfilter/sttype-op.h"
#include "dfilter/sttype-function.h"
#include "dfilter/sttype-set.h"
#include "dfilter/drange.h"
#include <epan/strutil.h>

#include "grammar.h"

#ifdef _MSC_VER
#pragma warning(disable:4671)
#endif

static stnode_t *
new_reference(dfsyntax_t *dfs, stnode_t *node);

static stnode_t *
new_function(dfsyntax_t *dfs, stnode_t *node);

static stnode_t *
resolve_unparsed(dfsyntax_t *dfs, stnode_t *node);

#define FAIL(dfs, node, ...) \
    do { \
        ws_noisy("Parsing failed here."); \
        dfilter_fail(dfs, DF_ERROR_GENERIC, stnode_location(node), __VA_ARGS__); \
    } while (0)

DIAG_OFF_LEMON()
/**************** End of %include directives **********************************/
/* These constants specify the various numeric values for terminal symbols.
***************** Begin token definitions *************************************/
#ifndef TOKEN_TEST_OR
#define TOKEN_TEST_OR                         1
#define TOKEN_TEST_XOR                        2
#define TOKEN_TEST_AND                        3
#define TOKEN_TEST_NOT                        4
#define TOKEN_TEST_ALL_EQ                     5
#define TOKEN_TEST_ANY_EQ                     6
#define TOKEN_TEST_ALL_NE                     7
#define TOKEN_TEST_ANY_NE                     8
#define TOKEN_TEST_LT                         9
#define TOKEN_TEST_LE                        10
#define TOKEN_TEST_GT                        11
#define TOKEN_TEST_GE                        12
#define TOKEN_TEST_CONTAINS                  13
#define TOKEN_TEST_MATCHES                   14
#define TOKEN_BITWISE_AND                    15
#define TOKEN_PLUS                           16
#define TOKEN_MINUS                          17
#define TOKEN_STAR                           18
#define TOKEN_RSLASH                         19
#define TOKEN_PERCENT                        20
#define TOKEN_UNARY_PLUS                     21
#define TOKEN_UNARY_MINUS                    22
#define TOKEN_LPAREN                         23
#define TOKEN_RPAREN                         24
#define TOKEN_STRING                         25
#define TOKEN_CHARCONST                      26
#define TOKEN_LITERAL                        27
#define TOKEN_NUMBER                         28
#define TOKEN_FIELD                          29
#define TOKEN_IDENTIFIER                     30
#define TOKEN_UNPARSED                       31
#define TOKEN_HASH                           32
#define TOKEN_LBRACKET                       33
#define TOKEN_RBRACKET                       34
#define TOKEN_INDEX                          35
#define TOKEN_ATSIGN                         36
#define TOKEN_DOLLAR                         37
#define TOKEN_LBRACE                         38
#define TOKEN_RBRACE                         39
#define TOKEN_TEST_IN                        40
#define TOKEN_ANY                            41
#define TOKEN_ALL                            42
#define TOKEN_DOTDOT                         43
#define TOKEN_COMMA                          44
#define TOKEN_RANGE_NODE                     45
#endif
/**************** End token definitions ***************************************/

/* The next sections is a series of control #defines.
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used to store the integer codes
**                       that represent terminal and non-terminal symbols.
**                       "unsigned char" is used if there are fewer than
**                       256 symbols.  Larger types otherwise.
**    YYNOCODE           is a number of type YYCODETYPE that is not used for
**                       any terminal or nonterminal symbol.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       (also known as: "terminal symbols") have fall-back
**                       values which should be used if the original symbol
**                       would not parse.  This permits keywords to sometimes
**                       be used as identifiers, for example.
**    YYACTIONTYPE       is the data type used for "action codes" - numbers
**                       that indicate what to do in response to the next
**                       token.
**    DfilterTOKENTYPE     is the data type used for minor type for terminal
**                       symbols.  Background: A "minor type" is a semantic
**                       value associated with a terminal or non-terminal
**                       symbols.  For example, for an "ID" terminal symbol,
**                       the minor type might be the name of the identifier.
**                       Each non-terminal can have a different minor type.
**                       Terminal symbols all have the same minor type, though.
**                       This macros defines the minor type for terminal
**                       symbols.
**    YYMINORTYPE        is the data type used for all minor types.
**                       This is typically a union of many types, one of
**                       which is DfilterTOKENTYPE.  The entry in the union
**                       for terminal symbols is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    DfilterARG_SDECL     A static variable declaration for the %extra_argument
**    DfilterARG_PDECL     A parameter declaration for the %extra_argument
**    DfilterARG_PARAM     Code to pass %extra_argument as a subroutine parameter
**    DfilterARG_STORE     Code to store %extra_argument into yypParser
**    DfilterARG_FETCH     Code to extract %extra_argument from yypParser
**    DfilterCTX_*         As DfilterARG_ except for %extra_context
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYNTOKEN           Number of terminal symbols
**    YY_MAX_SHIFT       Maximum value for shift actions
**    YY_MIN_SHIFTREDUCE Minimum value for shift-reduce actions
**    YY_MAX_SHIFTREDUCE Maximum value for shift-reduce actions
**    YY_ERROR_ACTION    The yy_action[] code for syntax error
**    YY_ACCEPT_ACTION   The yy_action[] code for accept
**    YY_NO_ACTION       The yy_action[] code for no-op
**    YY_MIN_REDUCE      Minimum value for reduce actions
**    YY_MAX_REDUCE      Maximum value for reduce actions
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/************* Begin control #defines *****************************************/
#define YYCODETYPE unsigned char
#define YYNOCODE 66
#define YYACTIONTYPE unsigned char
#define DfilterTOKENTYPE stnode_t*
typedef union {
  int yyinit;
  DfilterTOKENTYPE yy0;
  GSList* yy69;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 100
#endif
#define DfilterARG_SDECL dfsyntax_t *dfs;
#define DfilterARG_PDECL ,dfsyntax_t *dfs
#define DfilterARG_PARAM ,dfs
#define DfilterARG_FETCH dfsyntax_t *dfs=yypParser->dfs;
#define DfilterARG_STORE yypParser->dfs=dfs;
#define DfilterCTX_SDECL
#define DfilterCTX_PDECL
#define DfilterCTX_PARAM
#define DfilterCTX_FETCH
#define DfilterCTX_STORE
#define YYNSTATE             61
#define YYNRULE              68
#define YYNRULE_WITH_ACTION  68
#define YYNTOKEN             46
#define YY_MAX_SHIFT         60
#define YY_MIN_SHIFTREDUCE   112
#define YY_MAX_SHIFTREDUCE   179
#define YY_ERROR_ACTION      180
#define YY_ACCEPT_ACTION     181
#define YY_NO_ACTION         182
#define YY_MIN_REDUCE        183
#define YY_MAX_REDUCE        250
/************* End control #defines *******************************************/
#define YY_NLOOKAHEAD ((int)(sizeof(yy_lookahead)/sizeof(yy_lookahead[0])))

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N <= YY_MAX_SHIFT             Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   N between YY_MIN_SHIFTREDUCE       Shift to an arbitrary state then
**     and YY_MAX_SHIFTREDUCE           reduce by rule N-YY_MIN_SHIFTREDUCE.
**
**   N == YY_ERROR_ACTION               A syntax error has occurred.
**
**   N == YY_ACCEPT_ACTION              The parser accepts its input.
**
**   N == YY_NO_ACTION                  No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
**   N between YY_MIN_REDUCE            Reduce by rule N-YY_MIN_REDUCE
**     and YY_MAX_REDUCE
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as either:
**
**    (A)   N = yy_action[ yy_shift_ofst[S] + X ]
**    (B)   N = yy_default[S]
**
** The (A) formula is preferred.  The B formula is used instead if
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X.
**
** The formulas above are for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
**
*********** Begin parsing tables **********************************************/
#define YY_ACTTAB_COUNT (470)
static const YYACTIONTYPE yy_action[] = {
 /*     0 */   184,   46,  241,   11,    2,  235,   35,  206,   59,  202,
 /*    10 */   209,  210,   56,  207,  208,  234,   21,   19,   20,   18,
 /*    20 */    17,   16,   15,    1,    5,  121,  122,  123,  124,  125,
 /*    30 */    60,  127,    2,   59,  202,  205,   27,   25,   14,   59,
 /*    40 */   203,    7,    6,  176,   21,   19,  183,    4,    3,    5,
 /*    50 */    34,    1,  130,  121,  122,  123,  124,  125,   60,  127,
 /*    60 */   125,  126,  127,   13,   27,   25,   14,   27,    8,    7,
 /*    70 */     6,  125,  126,  127,  181,   43,  185,   30,  206,   59,
 /*    80 */   202,  209,  210,   40,  207,  208,  174,  231,  236,  150,
 /*    90 */   151,  152,  153,  156,  157,  154,  155,    3,    5,   22,
 /*   100 */    20,   18,   17,   16,   15,   42,  185,   30,  206,   59,
 /*   110 */   202,  209,  210,   40,  207,  208,   31,  231,  236,   49,
 /*   120 */    50,  190,  185,   30,  206,   59,  202,  209,  210,   40,
 /*   130 */   207,  208,   33,  231,  236,  133,   55,  185,   30,  206,
 /*   140 */    59,  202,  209,  210,   40,  207,  208,  175,  231,  236,
 /*   150 */    51,   47,  185,   30,  206,   59,  202,  209,  210,   40,
 /*   160 */   207,  208,    9,  231,  236,  187,  185,   30,  206,   59,
 /*   170 */   202,  209,  210,   40,  207,  208,  173,  231,  236,  182,
 /*   180 */    21,   19,   59,  202,   57,  182,   58,  182,  177,  121,
 /*   190 */   122,  123,  124,  125,   60,  127,  182,   21,   19,  182,
 /*   200 */    27,   25,   14,   17,   16,   15,  121,  122,  123,  124,
 /*   210 */   125,   60,  127,  182,  182,  182,  182,   27,   25,   14,
 /*   220 */    28,  206,   59,  202,  209,  210,   40,  207,  208,  182,
 /*   230 */   231,  238,   28,  206,   59,  202,  209,  210,   40,  207,
 /*   240 */   208,   48,  231,  237,  182,  182,  182,   39,  206,   59,
 /*   250 */   202,  209,  210,   56,  207,  208,  242,    4,    3,    5,
 /*   260 */    35,  206,   59,  202,  209,  210,   56,  207,  208,  182,
 /*   270 */   182,   29,  206,   59,  202,  209,  210,   56,  207,  208,
 /*   280 */   120,  230,   37,  206,   59,  202,  209,  210,   56,  207,
 /*   290 */   208,   38,  206,   59,  202,  209,  210,   56,  207,  208,
 /*   300 */   182,   36,  206,   59,  202,  209,  210,   56,  207,  208,
 /*   310 */   219,  206,   59,  202,  209,  210,   56,  207,  208,  218,
 /*   320 */   206,   59,  202,  209,  210,   56,  207,  208,  217,  206,
 /*   330 */    59,  202,  209,  210,   56,  207,  208,   45,  206,   59,
 /*   340 */   202,  209,  210,   56,  207,  208,  213,  206,   59,  202,
 /*   350 */   209,  210,   56,  207,  208,   44,  206,   59,  202,  209,
 /*   360 */   210,   56,  207,  208,  212,  206,   59,  202,  209,  210,
 /*   370 */    56,  207,  208,   41,  206,   59,  202,  209,  210,   56,
 /*   380 */   207,  208,  121,  122,  123,  124,  125,   60,  127,  182,
 /*   390 */   182,  182,  182,   27,   25,  206,   59,  202,  209,  210,
 /*   400 */    53,  207,  208,  206,   59,  202,  209,  210,   54,  207,
 /*   410 */   208,   22,   20,   18,   17,   16,   15,   22,   20,   18,
 /*   420 */    17,   16,   15,   22,   20,   18,   17,   16,   15,  125,
 /*   430 */   126,  127,  182,   52,  182,  182,   27,  129,   26,   12,
 /*   440 */   172,  149,   24,   23,  182,   10,  182,   58,  182,  182,
 /*   450 */   182,  182,  182,  182,  182,  182,  182,  182,  182,  182,
 /*   460 */   182,  182,   33,  182,  182,  182,  182,  182,  182,   32,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */     0,   48,   49,   62,    4,   65,   53,   54,   55,   56,
 /*    10 */    57,   58,   59,   60,   61,   65,   16,   17,   16,   17,
 /*    20 */    18,   19,   20,   23,    3,   25,   26,   27,   28,   29,
 /*    30 */    30,   31,    4,   55,   56,   57,   36,   37,   38,   55,
 /*    40 */    56,   41,   42,   24,   16,   17,    0,    1,    2,    3,
 /*    50 */    33,   23,   35,   25,   26,   27,   28,   29,   30,   31,
 /*    60 */    29,   30,   31,   44,   36,   37,   38,   36,   38,   41,
 /*    70 */    42,   29,   30,   31,   50,   51,   52,   53,   54,   55,
 /*    80 */    56,   57,   58,   59,   60,   61,   45,   63,   64,    5,
 /*    90 */     6,    7,    8,    9,   10,   11,   12,    2,    3,   15,
 /*   100 */    16,   17,   18,   19,   20,   51,   52,   53,   54,   55,
 /*   110 */    56,   57,   58,   59,   60,   61,   40,   63,   64,   46,
 /*   120 */    46,   51,   52,   53,   54,   55,   56,   57,   58,   59,
 /*   130 */    60,   61,   33,   63,   64,   39,   51,   52,   53,   54,
 /*   140 */    55,   56,   57,   58,   59,   60,   61,   45,   63,   64,
 /*   150 */    32,   51,   52,   53,   54,   55,   56,   57,   58,   59,
 /*   160 */    60,   61,   23,   63,   64,   51,   52,   53,   54,   55,
 /*   170 */    56,   57,   58,   59,   60,   61,   34,   63,   64,   66,
 /*   180 */    16,   17,   55,   56,   57,   66,   44,   66,   24,   25,
 /*   190 */    26,   27,   28,   29,   30,   31,   66,   16,   17,   66,
 /*   200 */    36,   37,   38,   18,   19,   20,   25,   26,   27,   28,
 /*   210 */    29,   30,   31,   66,   66,   66,   66,   36,   37,   38,
 /*   220 */    53,   54,   55,   56,   57,   58,   59,   60,   61,   66,
 /*   230 */    63,   64,   53,   54,   55,   56,   57,   58,   59,   60,
 /*   240 */    61,   47,   63,   64,   66,   66,   66,   53,   54,   55,
 /*   250 */    56,   57,   58,   59,   60,   61,   49,    1,    2,    3,
 /*   260 */    53,   54,   55,   56,   57,   58,   59,   60,   61,   66,
 /*   270 */    66,   53,   54,   55,   56,   57,   58,   59,   60,   61,
 /*   280 */    24,   63,   53,   54,   55,   56,   57,   58,   59,   60,
 /*   290 */    61,   53,   54,   55,   56,   57,   58,   59,   60,   61,
 /*   300 */    66,   53,   54,   55,   56,   57,   58,   59,   60,   61,
 /*   310 */    53,   54,   55,   56,   57,   58,   59,   60,   61,   53,
 /*   320 */    54,   55,   56,   57,   58,   59,   60,   61,   53,   54,
 /*   330 */    55,   56,   57,   58,   59,   60,   61,   53,   54,   55,
 /*   340 */    56,   57,   58,   59,   60,   61,   53,   54,   55,   56,
 /*   350 */    57,   58,   59,   60,   61,   53,   54,   55,   56,   57,
 /*   360 */    58,   59,   60,   61,   53,   54,   55,   56,   57,   58,
 /*   370 */    59,   60,   61,   53,   54,   55,   56,   57,   58,   59,
 /*   380 */    60,   61,   25,   26,   27,   28,   29,   30,   31,   66,
 /*   390 */    66,   66,   66,   36,   37,   54,   55,   56,   57,   58,
 /*   400 */    59,   60,   61,   54,   55,   56,   57,   58,   59,   60,
 /*   410 */    61,   15,   16,   17,   18,   19,   20,   15,   16,   17,
 /*   420 */    18,   19,   20,   15,   16,   17,   18,   19,   20,   29,
 /*   430 */    30,   31,   66,    4,   66,   66,   36,   34,   38,   43,
 /*   440 */    39,   39,   13,   14,   66,   44,   66,   44,   66,   66,
 /*   450 */    66,   66,   66,   66,   66,   66,   66,   66,   66,   66,
 /*   460 */    66,   66,   33,   66,   66,   66,   66,   66,   66,   40,
 /*   470 */    66,   66,   66,   66,   66,   66,   66,   66,   66,   66,
 /*   480 */    66,   66,   66,   66,   66,   66,   66,   46,   46,   46,
 /*   490 */    46,   46,   46,   46,   46,   46,   46,   46,   46,   46,
 /*   500 */    46,   46,   46,   46,   46,   46,   46,   46,   46,   46,
 /*   510 */    46,   46,   46,   46,   46,   46,
};
#define YY_SHIFT_COUNT    (60)
#define YY_SHIFT_MIN      (0)
#define YY_SHIFT_MAX      (429)
static const unsigned short int yy_shift_ofst[] = {
 /*     0 */     0,   28,   28,   28,   28,   28,  181,  181,  181,  164,
 /*    10 */   181,  181,  181,  181,  181,  181,  181,  181,  181,  181,
 /*    20 */   181,  181,  181,  357,  357,  400,   31,   42,   84,   84,
 /*    30 */    84,   30,   30,   41,   41,  396,  402,  408,  408,  408,
 /*    40 */   429,    2,  256,   46,  185,  185,  401,   95,   19,  142,
 /*    50 */   403,   17,   76,   99,   99,   21,   99,   96,  102,  118,
 /*    60 */   139,
};
#define YY_REDUCE_COUNT (34)
#define YY_REDUCE_MIN   (-60)
#define YY_REDUCE_MAX   (349)
static const short yy_reduce_ofst[] = {
 /*     0 */    24,   54,   70,   85,  100,  114,  167,  179,  -47,  194,
 /*    10 */   207,  218,  229,  238,  248,  257,  266,  275,  284,  293,
 /*    20 */   302,  311,  320,  341,  349,  -22,  127,  -16,  -59,  -59,
 /*    30 */   -59,  -60,  -50,   73,   74,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */   180,  180,  180,  180,  180,  180,  180,  180,  180,  180,
 /*    10 */   180,  180,  180,  180,  180,  180,  180,  180,  180,  180,
 /*    20 */   180,  180,  180,  180,  180,  180,  180,  180,  180,  229,
 /*    30 */   186,  180,  180,  180,  180,  239,  180,  240,  250,  249,
 /*    40 */   211,  214,  180,  180,  215,  216,  180,  188,  180,  180,
 /*    50 */   180,  180,  180,  233,  232,  189,  211,  180,  180,  199,
 /*    60 */   197,
};
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
**
** After the "shift" half of a SHIFTREDUCE action, the stateno field
** actually contains the reduce action for the second half of the
** SHIFTREDUCE.
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number, or reduce action in SHIFTREDUCE */
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level */
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  yyStackEntry *yytos;          /* Pointer to top element of the stack */
#ifdef YYTRACKMAXSTACKDEPTH
  int yyhwm;                    /* High-water mark of the stack */
#endif
#ifndef YYNOERRORRECOVERY
  int yyerrcnt;                 /* Shifts left before out of the error */
#endif
  DfilterARG_SDECL                /* A place to hold %extra_argument */
  DfilterCTX_SDECL                /* A place to hold %extra_context */
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
  yyStackEntry yystk0;          /* First stack entry */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
  yyStackEntry *yystackEnd;            /* Last entry in the stack */
#endif
};
typedef struct yyParser yyParser;

#include <assert.h>
#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#if defined(YYCOVERAGE) || !defined(NDEBUG)
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = {
  /*    0 */ "$",
  /*    1 */ "TEST_OR",
  /*    2 */ "TEST_XOR",
  /*    3 */ "TEST_AND",
  /*    4 */ "TEST_NOT",
  /*    5 */ "TEST_ALL_EQ",
  /*    6 */ "TEST_ANY_EQ",
  /*    7 */ "TEST_ALL_NE",
  /*    8 */ "TEST_ANY_NE",
  /*    9 */ "TEST_LT",
  /*   10 */ "TEST_LE",
  /*   11 */ "TEST_GT",
  /*   12 */ "TEST_GE",
  /*   13 */ "TEST_CONTAINS",
  /*   14 */ "TEST_MATCHES",
  /*   15 */ "BITWISE_AND",
  /*   16 */ "PLUS",
  /*   17 */ "MINUS",
  /*   18 */ "STAR",
  /*   19 */ "RSLASH",
  /*   20 */ "PERCENT",
  /*   21 */ "UNARY_PLUS",
  /*   22 */ "UNARY_MINUS",
  /*   23 */ "LPAREN",
  /*   24 */ "RPAREN",
  /*   25 */ "STRING",
  /*   26 */ "CHARCONST",
  /*   27 */ "LITERAL",
  /*   28 */ "NUMBER",
  /*   29 */ "FIELD",
  /*   30 */ "IDENTIFIER",
  /*   31 */ "UNPARSED",
  /*   32 */ "HASH",
  /*   33 */ "LBRACKET",
  /*   34 */ "RBRACKET",
  /*   35 */ "INDEX",
  /*   36 */ "ATSIGN",
  /*   37 */ "DOLLAR",
  /*   38 */ "LBRACE",
  /*   39 */ "RBRACE",
  /*   40 */ "TEST_IN",
  /*   41 */ "ANY",
  /*   42 */ "ALL",
  /*   43 */ "DOTDOT",
  /*   44 */ "COMMA",
  /*   45 */ "RANGE_NODE",
  /*   46 */ "range_node_list",
  /*   47 */ "func_params_list",
  /*   48 */ "set_list",
  /*   49 */ "set_element_list",
  /*   50 */ "sentence",
  /*   51 */ "expr",
  /*   52 */ "relation",
  /*   53 */ "arithmetic_expr",
  /*   54 */ "atom",
  /*   55 */ "field",
  /*   56 */ "layered_field",
  /*   57 */ "rawable_field",
  /*   58 */ "reference",
  /*   59 */ "entity",
  /*   60 */ "slice",
  /*   61 */ "function",
  /*   62 */ "cmp_op",
  /*   63 */ "comparison_test",
  /*   64 */ "relation_test",
  /*   65 */ "set",
};
#endif /* defined(YYCOVERAGE) || !defined(NDEBUG) */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "sentence ::= expr",
 /*   1 */ "sentence ::=",
 /*   2 */ "expr ::= relation",
 /*   3 */ "expr ::= arithmetic_expr",
 /*   4 */ "expr ::= expr TEST_AND expr",
 /*   5 */ "expr ::= expr TEST_OR expr",
 /*   6 */ "expr ::= expr TEST_XOR expr",
 /*   7 */ "expr ::= TEST_NOT expr",
 /*   8 */ "expr ::= LPAREN expr RPAREN",
 /*   9 */ "atom ::= STRING",
 /*  10 */ "atom ::= CHARCONST",
 /*  11 */ "atom ::= LITERAL",
 /*  12 */ "atom ::= NUMBER",
 /*  13 */ "field ::= FIELD",
 /*  14 */ "field ::= IDENTIFIER",
 /*  15 */ "field ::= UNPARSED",
 /*  16 */ "layered_field ::= field",
 /*  17 */ "layered_field ::= field HASH LBRACKET range_node_list RBRACKET",
 /*  18 */ "layered_field ::= field HASH INDEX",
 /*  19 */ "rawable_field ::= layered_field",
 /*  20 */ "rawable_field ::= ATSIGN layered_field",
 /*  21 */ "reference ::= DOLLAR LBRACE rawable_field RBRACE",
 /*  22 */ "reference ::= DOLLAR rawable_field",
 /*  23 */ "entity ::= atom",
 /*  24 */ "entity ::= slice",
 /*  25 */ "entity ::= function",
 /*  26 */ "entity ::= rawable_field",
 /*  27 */ "entity ::= reference",
 /*  28 */ "arithmetic_expr ::= entity",
 /*  29 */ "arithmetic_expr ::= PLUS arithmetic_expr",
 /*  30 */ "arithmetic_expr ::= MINUS arithmetic_expr",
 /*  31 */ "arithmetic_expr ::= arithmetic_expr BITWISE_AND arithmetic_expr",
 /*  32 */ "arithmetic_expr ::= arithmetic_expr PLUS arithmetic_expr",
 /*  33 */ "arithmetic_expr ::= arithmetic_expr MINUS arithmetic_expr",
 /*  34 */ "arithmetic_expr ::= arithmetic_expr STAR arithmetic_expr",
 /*  35 */ "arithmetic_expr ::= arithmetic_expr RSLASH arithmetic_expr",
 /*  36 */ "arithmetic_expr ::= arithmetic_expr PERCENT arithmetic_expr",
 /*  37 */ "arithmetic_expr ::= LBRACE arithmetic_expr RBRACE",
 /*  38 */ "cmp_op ::= TEST_ALL_EQ",
 /*  39 */ "cmp_op ::= TEST_ANY_EQ",
 /*  40 */ "cmp_op ::= TEST_ALL_NE",
 /*  41 */ "cmp_op ::= TEST_ANY_NE",
 /*  42 */ "cmp_op ::= TEST_GT",
 /*  43 */ "cmp_op ::= TEST_GE",
 /*  44 */ "cmp_op ::= TEST_LT",
 /*  45 */ "cmp_op ::= TEST_LE",
 /*  46 */ "comparison_test ::= arithmetic_expr cmp_op arithmetic_expr",
 /*  47 */ "comparison_test ::= arithmetic_expr cmp_op comparison_test",
 /*  48 */ "relation_test ::= comparison_test",
 /*  49 */ "relation_test ::= entity TEST_CONTAINS entity",
 /*  50 */ "relation_test ::= entity TEST_MATCHES entity",
 /*  51 */ "relation_test ::= entity TEST_IN set",
 /*  52 */ "relation_test ::= entity TEST_NOT TEST_IN set",
 /*  53 */ "relation ::= relation_test",
 /*  54 */ "relation ::= ANY relation_test",
 /*  55 */ "relation ::= ALL relation_test",
 /*  56 */ "set_element_list ::= arithmetic_expr",
 /*  57 */ "set_element_list ::= arithmetic_expr DOTDOT arithmetic_expr",
 /*  58 */ "set_list ::= set_element_list",
 /*  59 */ "set_list ::= set_list COMMA set_element_list",
 /*  60 */ "set ::= LBRACE set_list RBRACE",
 /*  61 */ "slice ::= entity LBRACKET range_node_list RBRACKET",
 /*  62 */ "range_node_list ::= RANGE_NODE",
 /*  63 */ "range_node_list ::= range_node_list COMMA RANGE_NODE",
 /*  64 */ "function ::= IDENTIFIER LPAREN func_params_list RPAREN",
 /*  65 */ "function ::= IDENTIFIER LPAREN RPAREN",
 /*  66 */ "func_params_list ::= arithmetic_expr",
 /*  67 */ "func_params_list ::= func_params_list COMMA arithmetic_expr",
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.  Return the number
** of errors.  Return 0 on success.
*/
static int yyGrowStack(yyParser *p){
  int newSize;
  int idx;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 100;
  idx = p->yytos ? (int)(p->yytos - p->yystack) : 0;
  if( p->yystack==&p->yystk0 ){
    pNew = malloc(newSize*sizeof(pNew[0]));
    if( pNew ) pNew[0] = p->yystk0;
  }else{
    pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  }
  if( pNew ){
    p->yystack = pNew;
    p->yytos = &p->yystack[idx];
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows from %d to %d entries.\n",
              yyTracePrompt, p->yystksz, newSize);
    }
#endif
    p->yystksz = newSize;
  }
  return pNew==0;
}
#endif

/* Datatype of the argument to the memory allocated passed as the
** second argument to DfilterAlloc() below.  This can be changed by
** putting an appropriate #define in the %include section of the input
** grammar.
*/
#ifndef YYMALLOCARGTYPE
# define YYMALLOCARGTYPE size_t
#endif

/* Initialize a new parser that has already been allocated.
*/
void DfilterInit(void *yypRawParser DfilterCTX_PDECL){
  yyParser *yypParser = (yyParser*)yypRawParser;
  DfilterCTX_STORE
#ifdef YYTRACKMAXSTACKDEPTH
  yypParser->yyhwm = 0;
#endif
#if YYSTACKDEPTH<=0
  yypParser->yytos = NULL;
  yypParser->yystack = NULL;
  yypParser->yystksz = 0;
  if( yyGrowStack(yypParser) ){
    yypParser->yystack = &yypParser->yystk0;
    yypParser->yystksz = 1;
  }
#endif
#ifndef YYNOERRORRECOVERY
  yypParser->yyerrcnt = -1;
#endif
  yypParser->yytos = yypParser->yystack;
  yypParser->yystack[0].stateno = 0;
  yypParser->yystack[0].major = 0;
#if YYSTACKDEPTH>0
  yypParser->yystackEnd = &yypParser->yystack[YYSTACKDEPTH-1];
#endif
}

#ifndef Dfilter_ENGINEALWAYSONSTACK
/*
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to Dfilter and DfilterFree.
*/
void *DfilterAlloc(void *(*mallocProc)(YYMALLOCARGTYPE) DfilterCTX_PDECL){
  yyParser *yypParser;
  yypParser = (yyParser*)(*mallocProc)( (YYMALLOCARGTYPE)sizeof(yyParser) );
  if( yypParser ){
    DfilterCTX_STORE
    DfilterInit(yypParser DfilterCTX_PARAM);
  }
  return (void*)yypParser;
}
#endif /* Dfilter_ENGINEALWAYSONSTACK */


/* The following function deletes the "minor type" or semantic value
** associated with a symbol.  The symbol can be either a terminal
** or nonterminal. "yymajor" is the symbol code, and "yypminor" is
** a pointer to the value to be deleted.  The code used to do the
** deletions is derived from the %destructor and/or %token_destructor
** directives of the input grammar.
*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  DfilterARG_FETCH
  DfilterCTX_FETCH
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are *not* used
    ** inside the C code.
    */
/********* Begin destructor definitions ***************************************/
      /* TERMINAL Destructor */
    case 1: /* TEST_OR */
    case 2: /* TEST_XOR */
    case 3: /* TEST_AND */
    case 4: /* TEST_NOT */
    case 5: /* TEST_ALL_EQ */
    case 6: /* TEST_ANY_EQ */
    case 7: /* TEST_ALL_NE */
    case 8: /* TEST_ANY_NE */
    case 9: /* TEST_LT */
    case 10: /* TEST_LE */
    case 11: /* TEST_GT */
    case 12: /* TEST_GE */
    case 13: /* TEST_CONTAINS */
    case 14: /* TEST_MATCHES */
    case 15: /* BITWISE_AND */
    case 16: /* PLUS */
    case 17: /* MINUS */
    case 18: /* STAR */
    case 19: /* RSLASH */
    case 20: /* PERCENT */
    case 21: /* UNARY_PLUS */
    case 22: /* UNARY_MINUS */
    case 23: /* LPAREN */
    case 24: /* RPAREN */
    case 25: /* STRING */
    case 26: /* CHARCONST */
    case 27: /* LITERAL */
    case 28: /* NUMBER */
    case 29: /* FIELD */
    case 30: /* IDENTIFIER */
    case 31: /* UNPARSED */
    case 32: /* HASH */
    case 33: /* LBRACKET */
    case 34: /* RBRACKET */
    case 35: /* INDEX */
    case 36: /* ATSIGN */
    case 37: /* DOLLAR */
    case 38: /* LBRACE */
    case 39: /* RBRACE */
    case 40: /* TEST_IN */
    case 41: /* ANY */
    case 42: /* ALL */
    case 43: /* DOTDOT */
    case 44: /* COMMA */
    case 45: /* RANGE_NODE */
{

    (void)dfs;
    stnode_free((yypminor->yy0));

}
      break;
      /* Default NON-TERMINAL Destructor */
    case 50: /* sentence */
    case 51: /* expr */
    case 52: /* relation */
    case 53: /* arithmetic_expr */
    case 54: /* atom */
    case 55: /* field */
    case 56: /* layered_field */
    case 57: /* rawable_field */
    case 58: /* reference */
    case 59: /* entity */
    case 60: /* slice */
    case 61: /* function */
    case 62: /* cmp_op */
    case 63: /* comparison_test */
    case 64: /* relation_test */
    case 65: /* set */
{
stnode_free((yypminor->yy0));
}
      break;
    case 46: /* range_node_list */
{
drange_node_free_list((yypminor->yy69));
}
      break;
    case 47: /* func_params_list */
{
st_funcparams_free((yypminor->yy69));
}
      break;
    case 48: /* set_list */
    case 49: /* set_element_list */
{
set_nodelist_free((yypminor->yy69));
}
      break;
/********* End destructor definitions *****************************************/
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
*/
static void yy_pop_parser_stack(yyParser *pParser){
  yyStackEntry *yytos;
  assert( pParser->yytos!=0 );
  assert( pParser->yytos > pParser->yystack );
  yytos = pParser->yytos--;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yy_destructor(pParser, yytos->major, &yytos->minor);
}

/*
** Clear all secondary memory allocations from the parser
*/
void DfilterFinalize(void *p){
  yyParser *pParser = (yyParser*)p;
  while( pParser->yytos>pParser->yystack ) yy_pop_parser_stack(pParser);
#if YYSTACKDEPTH<=0
  if( pParser->yystack!=&pParser->yystk0 ) free(pParser->yystack);
#endif
}

#ifndef Dfilter_ENGINEALWAYSONSTACK
/*
** Deallocate and destroy a parser.  Destructors are called for
** all stack elements before shutting the parser down.
**
** If the YYPARSEFREENEVERNULL macro exists (for example because it
** is defined in a %include section of the input grammar) then it is
** assumed that the input pointer is never NULL.
*/
void DfilterFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
#ifndef YYPARSEFREENEVERNULL
  if( p==0 ) return;
#endif
  DfilterFinalize(p);
  (*freeProc)(p);
}
#endif /* Dfilter_ENGINEALWAYSONSTACK */

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
int DfilterStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyhwm;
}
#endif

/* This array of booleans keeps track of the parser statement
** coverage.  The element yycoverage[X][Y] is set when the parser
** is in state X and has a lookahead token Y.  In a well-tested
** systems, every element of this matrix should end up being set.
*/
#if defined(YYCOVERAGE)
static unsigned char yycoverage[YYNSTATE][YYNTOKEN];
#endif

/*
** Write into out a description of every state/lookahead combination that
**
**   (1)  has not been used by the parser, and
**   (2)  is not a syntax error.
**
** Return the number of missed state/lookahead combinations.
*/
#if defined(YYCOVERAGE)
int DfilterCoverage(FILE *out){
  int stateno, iLookAhead, i;
  int nMissed = 0;
  for(stateno=0; stateno<YYNSTATE; stateno++){
    i = yy_shift_ofst[stateno];
    for(iLookAhead=0; iLookAhead<YYNTOKEN; iLookAhead++){
      if( yy_lookahead[i+iLookAhead]!=iLookAhead ) continue;
      if( yycoverage[stateno][iLookAhead]==0 ) nMissed++;
      if( out ){
        fprintf(out,"State %d lookahead %s %s\n", stateno,
                yyTokenName[iLookAhead],
                yycoverage[stateno][iLookAhead] ? "ok" : "missed");
      }
    }
  }
  return nMissed;
}
#endif

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
*/
static YYACTIONTYPE yy_find_shift_action(
  YYCODETYPE iLookAhead,    /* The look-ahead token */
  YYACTIONTYPE stateno      /* Current state number */
){
  int i;

  if( stateno>YY_MAX_SHIFT ) return stateno;
  assert( stateno <= YY_SHIFT_COUNT );
#if defined(YYCOVERAGE)
  yycoverage[stateno][iLookAhead] = 1;
#endif
  do{
    i = yy_shift_ofst[stateno];
    assert( i>=0 );
    assert( i<=YY_ACTTAB_COUNT );
    assert( i+YYNTOKEN<=(int)YY_NLOOKAHEAD );
    assert( iLookAhead!=YYNOCODE );
    assert( iLookAhead < YYNTOKEN );
    i += iLookAhead;
    assert( i<(int)YY_NLOOKAHEAD );
    if( yy_lookahead[i]!=iLookAhead ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback;            /* Fallback token */
      assert( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0]) );
      iFallback = yyFallback[iLookAhead];
      if( iFallback!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        assert( yyFallback[iFallback]==0 ); /* Fallback loop must terminate */
        iLookAhead = iFallback;
        continue;
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        assert( j<(int)(sizeof(yy_lookahead)/sizeof(yy_lookahead[0])) );
        if( yy_lookahead[j]==YYWILDCARD && iLookAhead>0 ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead],
               yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
      return yy_default[stateno];
    }else{
      assert( i>=0 && i<(int)(sizeof(yy_action)/sizeof(yy_action[0])) );
      return yy_action[i];
    }
  }while(1);
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
*/
static YYACTIONTYPE yy_find_reduce_action(
  YYACTIONTYPE stateno,     /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_COUNT );
#endif
  i = yy_reduce_ofst[stateno];
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_ACTTAB_COUNT );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser){
   DfilterARG_FETCH
   DfilterCTX_FETCH
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
/******** Begin %stack_overflow code ******************************************/
/******** End %stack_overflow code ********************************************/
   DfilterARG_STORE /* Suppress warning about unused %extra_argument var */
   DfilterCTX_STORE
}

/*
** Print tracing information for a SHIFT action
*/
#ifndef NDEBUG
static void yyTraceShift(yyParser *yypParser, int yyNewState, const char *zTag){
  if( yyTraceFILE ){
    if( yyNewState<YYNSTATE ){
      fprintf(yyTraceFILE,"%s%s '%s', go to state %d\n",
         yyTracePrompt, zTag, yyTokenName[yypParser->yytos->major],
         yyNewState);
    }else{
      fprintf(yyTraceFILE,"%s%s '%s', pending reduce %d\n",
         yyTracePrompt, zTag, yyTokenName[yypParser->yytos->major],
         yyNewState - YY_MIN_REDUCE);
    }
  }
}
#else
# define yyTraceShift(X,Y,Z)
#endif

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  YYACTIONTYPE yyNewState,      /* The new state to shift in */
  YYCODETYPE yyMajor,           /* The major token to shift in */
  DfilterTOKENTYPE yyMinor        /* The minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yytos++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
    yypParser->yyhwm++;
    assert( yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack) );
  }
#endif
#if YYSTACKDEPTH>0
  if( yypParser->yytos>yypParser->yystackEnd ){
    yypParser->yytos--;
    yyStackOverflow(yypParser);
    return;
  }
#else
  if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz] ){
    if( yyGrowStack(yypParser) ){
      yypParser->yytos--;
      yyStackOverflow(yypParser);
      return;
    }
  }
#endif
  if( yyNewState > YY_MAX_SHIFT ){
    yyNewState += YY_MIN_REDUCE - YY_MIN_SHIFTREDUCE;
  }
  yytos = yypParser->yytos;
  yytos->stateno = yyNewState;
  yytos->major = yyMajor;
  yytos->minor.yy0 = yyMinor;
  yyTraceShift(yypParser, yyNewState, "Shift");
}

/* For rule J, yyRuleInfoLhs[J] contains the symbol on the left-hand side
** of that rule */
static const YYCODETYPE yyRuleInfoLhs[] = {
    50,  /* (0) sentence ::= expr */
    50,  /* (1) sentence ::= */
    51,  /* (2) expr ::= relation */
    51,  /* (3) expr ::= arithmetic_expr */
    51,  /* (4) expr ::= expr TEST_AND expr */
    51,  /* (5) expr ::= expr TEST_OR expr */
    51,  /* (6) expr ::= expr TEST_XOR expr */
    51,  /* (7) expr ::= TEST_NOT expr */
    51,  /* (8) expr ::= LPAREN expr RPAREN */
    54,  /* (9) atom ::= STRING */
    54,  /* (10) atom ::= CHARCONST */
    54,  /* (11) atom ::= LITERAL */
    54,  /* (12) atom ::= NUMBER */
    55,  /* (13) field ::= FIELD */
    55,  /* (14) field ::= IDENTIFIER */
    55,  /* (15) field ::= UNPARSED */
    56,  /* (16) layered_field ::= field */
    56,  /* (17) layered_field ::= field HASH LBRACKET range_node_list RBRACKET */
    56,  /* (18) layered_field ::= field HASH INDEX */
    57,  /* (19) rawable_field ::= layered_field */
    57,  /* (20) rawable_field ::= ATSIGN layered_field */
    58,  /* (21) reference ::= DOLLAR LBRACE rawable_field RBRACE */
    58,  /* (22) reference ::= DOLLAR rawable_field */
    59,  /* (23) entity ::= atom */
    59,  /* (24) entity ::= slice */
    59,  /* (25) entity ::= function */
    59,  /* (26) entity ::= rawable_field */
    59,  /* (27) entity ::= reference */
    53,  /* (28) arithmetic_expr ::= entity */
    53,  /* (29) arithmetic_expr ::= PLUS arithmetic_expr */
    53,  /* (30) arithmetic_expr ::= MINUS arithmetic_expr */
    53,  /* (31) arithmetic_expr ::= arithmetic_expr BITWISE_AND arithmetic_expr */
    53,  /* (32) arithmetic_expr ::= arithmetic_expr PLUS arithmetic_expr */
    53,  /* (33) arithmetic_expr ::= arithmetic_expr MINUS arithmetic_expr */
    53,  /* (34) arithmetic_expr ::= arithmetic_expr STAR arithmetic_expr */
    53,  /* (35) arithmetic_expr ::= arithmetic_expr RSLASH arithmetic_expr */
    53,  /* (36) arithmetic_expr ::= arithmetic_expr PERCENT arithmetic_expr */
    53,  /* (37) arithmetic_expr ::= LBRACE arithmetic_expr RBRACE */
    62,  /* (38) cmp_op ::= TEST_ALL_EQ */
    62,  /* (39) cmp_op ::= TEST_ANY_EQ */
    62,  /* (40) cmp_op ::= TEST_ALL_NE */
    62,  /* (41) cmp_op ::= TEST_ANY_NE */
    62,  /* (42) cmp_op ::= TEST_GT */
    62,  /* (43) cmp_op ::= TEST_GE */
    62,  /* (44) cmp_op ::= TEST_LT */
    62,  /* (45) cmp_op ::= TEST_LE */
    63,  /* (46) comparison_test ::= arithmetic_expr cmp_op arithmetic_expr */
    63,  /* (47) comparison_test ::= arithmetic_expr cmp_op comparison_test */
    64,  /* (48) relation_test ::= comparison_test */
    64,  /* (49) relation_test ::= entity TEST_CONTAINS entity */
    64,  /* (50) relation_test ::= entity TEST_MATCHES entity */
    64,  /* (51) relation_test ::= entity TEST_IN set */
    64,  /* (52) relation_test ::= entity TEST_NOT TEST_IN set */
    52,  /* (53) relation ::= relation_test */
    52,  /* (54) relation ::= ANY relation_test */
    52,  /* (55) relation ::= ALL relation_test */
    49,  /* (56) set_element_list ::= arithmetic_expr */
    49,  /* (57) set_element_list ::= arithmetic_expr DOTDOT arithmetic_expr */
    48,  /* (58) set_list ::= set_element_list */
    48,  /* (59) set_list ::= set_list COMMA set_element_list */
    65,  /* (60) set ::= LBRACE set_list RBRACE */
    60,  /* (61) slice ::= entity LBRACKET range_node_list RBRACKET */
    46,  /* (62) range_node_list ::= RANGE_NODE */
    46,  /* (63) range_node_list ::= range_node_list COMMA RANGE_NODE */
    61,  /* (64) function ::= IDENTIFIER LPAREN func_params_list RPAREN */
    61,  /* (65) function ::= IDENTIFIER LPAREN RPAREN */
    47,  /* (66) func_params_list ::= arithmetic_expr */
    47,  /* (67) func_params_list ::= func_params_list COMMA arithmetic_expr */
};

/* For rule J, yyRuleInfoNRhs[J] contains the negative of the number
** of symbols on the right-hand side of that rule. */
static const signed char yyRuleInfoNRhs[] = {
   -1,  /* (0) sentence ::= expr */
    0,  /* (1) sentence ::= */
   -1,  /* (2) expr ::= relation */
   -1,  /* (3) expr ::= arithmetic_expr */
   -3,  /* (4) expr ::= expr TEST_AND expr */
   -3,  /* (5) expr ::= expr TEST_OR expr */
   -3,  /* (6) expr ::= expr TEST_XOR expr */
   -2,  /* (7) expr ::= TEST_NOT expr */
   -3,  /* (8) expr ::= LPAREN expr RPAREN */
   -1,  /* (9) atom ::= STRING */
   -1,  /* (10) atom ::= CHARCONST */
   -1,  /* (11) atom ::= LITERAL */
   -1,  /* (12) atom ::= NUMBER */
   -1,  /* (13) field ::= FIELD */
   -1,  /* (14) field ::= IDENTIFIER */
   -1,  /* (15) field ::= UNPARSED */
   -1,  /* (16) layered_field ::= field */
   -5,  /* (17) layered_field ::= field HASH LBRACKET range_node_list RBRACKET */
   -3,  /* (18) layered_field ::= field HASH INDEX */
   -1,  /* (19) rawable_field ::= layered_field */
   -2,  /* (20) rawable_field ::= ATSIGN layered_field */
   -4,  /* (21) reference ::= DOLLAR LBRACE rawable_field RBRACE */
   -2,  /* (22) reference ::= DOLLAR rawable_field */
   -1,  /* (23) entity ::= atom */
   -1,  /* (24) entity ::= slice */
   -1,  /* (25) entity ::= function */
   -1,  /* (26) entity ::= rawable_field */
   -1,  /* (27) entity ::= reference */
   -1,  /* (28) arithmetic_expr ::= entity */
   -2,  /* (29) arithmetic_expr ::= PLUS arithmetic_expr */
   -2,  /* (30) arithmetic_expr ::= MINUS arithmetic_expr */
   -3,  /* (31) arithmetic_expr ::= arithmetic_expr BITWISE_AND arithmetic_expr */
   -3,  /* (32) arithmetic_expr ::= arithmetic_expr PLUS arithmetic_expr */
   -3,  /* (33) arithmetic_expr ::= arithmetic_expr MINUS arithmetic_expr */
   -3,  /* (34) arithmetic_expr ::= arithmetic_expr STAR arithmetic_expr */
   -3,  /* (35) arithmetic_expr ::= arithmetic_expr RSLASH arithmetic_expr */
   -3,  /* (36) arithmetic_expr ::= arithmetic_expr PERCENT arithmetic_expr */
   -3,  /* (37) arithmetic_expr ::= LBRACE arithmetic_expr RBRACE */
   -1,  /* (38) cmp_op ::= TEST_ALL_EQ */
   -1,  /* (39) cmp_op ::= TEST_ANY_EQ */
   -1,  /* (40) cmp_op ::= TEST_ALL_NE */
   -1,  /* (41) cmp_op ::= TEST_ANY_NE */
   -1,  /* (42) cmp_op ::= TEST_GT */
   -1,  /* (43) cmp_op ::= TEST_GE */
   -1,  /* (44) cmp_op ::= TEST_LT */
   -1,  /* (45) cmp_op ::= TEST_LE */
   -3,  /* (46) comparison_test ::= arithmetic_expr cmp_op arithmetic_expr */
   -3,  /* (47) comparison_test ::= arithmetic_expr cmp_op comparison_test */
   -1,  /* (48) relation_test ::= comparison_test */
   -3,  /* (49) relation_test ::= entity TEST_CONTAINS entity */
   -3,  /* (50) relation_test ::= entity TEST_MATCHES entity */
   -3,  /* (51) relation_test ::= entity TEST_IN set */
   -4,  /* (52) relation_test ::= entity TEST_NOT TEST_IN set */
   -1,  /* (53) relation ::= relation_test */
   -2,  /* (54) relation ::= ANY relation_test */
   -2,  /* (55) relation ::= ALL relation_test */
   -1,  /* (56) set_element_list ::= arithmetic_expr */
   -3,  /* (57) set_element_list ::= arithmetic_expr DOTDOT arithmetic_expr */
   -1,  /* (58) set_list ::= set_element_list */
   -3,  /* (59) set_list ::= set_list COMMA set_element_list */
   -3,  /* (60) set ::= LBRACE set_list RBRACE */
   -4,  /* (61) slice ::= entity LBRACKET range_node_list RBRACKET */
   -1,  /* (62) range_node_list ::= RANGE_NODE */
   -3,  /* (63) range_node_list ::= range_node_list COMMA RANGE_NODE */
   -4,  /* (64) function ::= IDENTIFIER LPAREN func_params_list RPAREN */
   -3,  /* (65) function ::= IDENTIFIER LPAREN RPAREN */
   -1,  /* (66) func_params_list ::= arithmetic_expr */
   -3,  /* (67) func_params_list ::= func_params_list COMMA arithmetic_expr */
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
**
** The yyLookahead and yyLookaheadToken parameters provide reduce actions
** access to the lookahead token (if any).  The yyLookahead will be YYNOCODE
** if the lookahead token has already been consumed.  As this procedure is
** only called from one place, optimizing compilers will in-line it, which
** means that the extra parameters have no performance impact.
*/
static YYACTIONTYPE yy_reduce(
  yyParser *yypParser,         /* The parser */
  unsigned int yyruleno,       /* Number of the rule by which to reduce */
  int yyLookahead,             /* Lookahead token, or YYNOCODE if none */
  DfilterTOKENTYPE yyLookaheadToken  /* Value of the lookahead token */
  DfilterCTX_PDECL                   /* %extra_context */
){
  int yygoto;                     /* The next state */
  YYACTIONTYPE yyact;             /* The next action */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  DfilterARG_FETCH
  (void)yyLookahead;
  (void)yyLookaheadToken;
  yymsp = yypParser->yytos;

  switch( yyruleno ){
        YYMINORTYPE yylhsminor;
      case 0: /* sentence ::= expr */
{ dfs->st_root = yymsp[0].minor.yy0; }
        break;
      case 1: /* sentence ::= */
{ dfs->st_root = NULL; }
        break;
      case 2: /* expr ::= relation */
      case 3: /* expr ::= arithmetic_expr */ yytestcase(yyruleno==3);
      case 9: /* atom ::= STRING */ yytestcase(yyruleno==9);
      case 10: /* atom ::= CHARCONST */ yytestcase(yyruleno==10);
      case 11: /* atom ::= LITERAL */ yytestcase(yyruleno==11);
      case 12: /* atom ::= NUMBER */ yytestcase(yyruleno==12);
      case 13: /* field ::= FIELD */ yytestcase(yyruleno==13);
      case 14: /* field ::= IDENTIFIER */ yytestcase(yyruleno==14);
      case 15: /* field ::= UNPARSED */ yytestcase(yyruleno==15);
      case 23: /* entity ::= atom */ yytestcase(yyruleno==23);
      case 24: /* entity ::= slice */ yytestcase(yyruleno==24);
      case 25: /* entity ::= function */ yytestcase(yyruleno==25);
      case 26: /* entity ::= rawable_field */ yytestcase(yyruleno==26);
      case 27: /* entity ::= reference */ yytestcase(yyruleno==27);
      case 48: /* relation_test ::= comparison_test */ yytestcase(yyruleno==48);
      case 53: /* relation ::= relation_test */ yytestcase(yyruleno==53);
{ yylhsminor.yy0 = yymsp[0].minor.yy0; }
  yymsp[0].minor.yy0 = yylhsminor.yy0;
        break;
      case 4: /* expr ::= expr TEST_AND expr */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_AND, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 5: /* expr ::= expr TEST_OR expr */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_OR, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 6: /* expr ::= expr TEST_XOR expr */
{
    stnode_t *A = stnode_new(STTYPE_TEST, NULL, NULL, DFILTER_LOC_EMPTY);
    sttype_oper_set2(A, STNODE_OP_OR, stnode_dup(yymsp[-2].minor.yy0), stnode_dup(yymsp[0].minor.yy0));

    stnode_t *B = stnode_new(STTYPE_TEST, NULL, NULL, DFILTER_LOC_EMPTY);
    sttype_oper_set2(B, STNODE_OP_AND, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);

    stnode_t *C = stnode_new(STTYPE_TEST, NULL, NULL, DFILTER_LOC_EMPTY);
    sttype_oper_set1(C, STNODE_OP_NOT, B);

    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_AND, A, C);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 7: /* expr ::= TEST_NOT expr */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set1(yylhsminor.yy0, STNODE_OP_NOT, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-1].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-1].minor.yy0 = yylhsminor.yy0;
        break;
      case 8: /* expr ::= LPAREN expr RPAREN */
      case 37: /* arithmetic_expr ::= LBRACE arithmetic_expr RBRACE */ yytestcase(yyruleno==37);
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[-2].minor.yy0);
    stnode_free(yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 16: /* layered_field ::= field */
      case 19: /* rawable_field ::= layered_field */ yytestcase(yyruleno==19);
      case 28: /* arithmetic_expr ::= entity */ yytestcase(yyruleno==28);
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
}
  yymsp[0].minor.yy0 = yylhsminor.yy0;
        break;
      case 17: /* layered_field ::= field HASH LBRACKET range_node_list RBRACKET */
{
    yylhsminor.yy0 = resolve_unparsed(dfs, yymsp[-4].minor.yy0);
    sttype_field_set_range(yylhsminor.yy0, yymsp[-1].minor.yy69);
    g_slist_free(yymsp[-1].minor.yy69);
    stnode_merge_location(yylhsminor.yy0, yymsp[-4].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[0].minor.yy0);
}
  yy_destructor(yypParser,32,&yymsp[-3].minor);
  yy_destructor(yypParser,33,&yymsp[-2].minor);
  yymsp[-4].minor.yy0 = yylhsminor.yy0;
        break;
      case 18: /* layered_field ::= field HASH INDEX */
{
    yylhsminor.yy0 = resolve_unparsed(dfs, yymsp[-2].minor.yy0);
    char *err_msg = NULL;
    drange_node *range = drange_node_from_str(stnode_token(yymsp[0].minor.yy0), &err_msg);
    if (err_msg != NULL) {
        FAIL(dfs, yymsp[0].minor.yy0, "%s", err_msg);
        g_free(err_msg);
    }
    sttype_field_set_range1(yylhsminor.yy0, range);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[0].minor.yy0);
}
  yy_destructor(yypParser,32,&yymsp[-1].minor);
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 20: /* rawable_field ::= ATSIGN layered_field */
{
    yylhsminor.yy0 = resolve_unparsed(dfs, yymsp[0].minor.yy0);
    sttype_field_set_raw(yylhsminor.yy0, true);
    stnode_merge_location(yylhsminor.yy0, yymsp[-1].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[-1].minor.yy0);
}
  yymsp[-1].minor.yy0 = yylhsminor.yy0;
        break;
      case 21: /* reference ::= DOLLAR LBRACE rawable_field RBRACE */
{
    yymsp[-1].minor.yy0 = resolve_unparsed(dfs, yymsp[-1].minor.yy0);
    yylhsminor.yy0 = new_reference(dfs, yymsp[-1].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-3].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[-1].minor.yy0);
    stnode_free(yymsp[-3].minor.yy0);
    stnode_free(yymsp[0].minor.yy0);
}
  yy_destructor(yypParser,38,&yymsp[-2].minor);
  yymsp[-3].minor.yy0 = yylhsminor.yy0;
        break;
      case 22: /* reference ::= DOLLAR rawable_field */
{
    yymsp[0].minor.yy0 = resolve_unparsed(dfs, yymsp[0].minor.yy0);
    yylhsminor.yy0 = new_reference(dfs, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-1].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[0].minor.yy0);
    stnode_free(yymsp[-1].minor.yy0);
}
  yymsp[-1].minor.yy0 = yylhsminor.yy0;
        break;
      case 29: /* arithmetic_expr ::= PLUS arithmetic_expr */
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
    stnode_merge_location(yylhsminor.yy0, yymsp[-1].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[-1].minor.yy0);
}
  yymsp[-1].minor.yy0 = yylhsminor.yy0;
        break;
      case 30: /* arithmetic_expr ::= MINUS arithmetic_expr */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set1(yylhsminor.yy0, STNODE_OP_UNARY_MINUS, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-1].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-1].minor.yy0 = yylhsminor.yy0;
        break;
      case 31: /* arithmetic_expr ::= arithmetic_expr BITWISE_AND arithmetic_expr */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_BITWISE_AND, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 32: /* arithmetic_expr ::= arithmetic_expr PLUS arithmetic_expr */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_ADD, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 33: /* arithmetic_expr ::= arithmetic_expr MINUS arithmetic_expr */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_SUBTRACT, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 34: /* arithmetic_expr ::= arithmetic_expr STAR arithmetic_expr */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_MULTIPLY, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 35: /* arithmetic_expr ::= arithmetic_expr RSLASH arithmetic_expr */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_DIVIDE, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 36: /* arithmetic_expr ::= arithmetic_expr PERCENT arithmetic_expr */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_MODULO, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 38: /* cmp_op ::= TEST_ALL_EQ */
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
    sttype_oper_set_op(yylhsminor.yy0, STNODE_OP_ALL_EQ);
}
  yymsp[0].minor.yy0 = yylhsminor.yy0;
        break;
      case 39: /* cmp_op ::= TEST_ANY_EQ */
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
    sttype_oper_set_op(yylhsminor.yy0, STNODE_OP_ANY_EQ);
}
  yymsp[0].minor.yy0 = yylhsminor.yy0;
        break;
      case 40: /* cmp_op ::= TEST_ALL_NE */
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
    sttype_oper_set_op(yylhsminor.yy0, STNODE_OP_ALL_NE);
}
  yymsp[0].minor.yy0 = yylhsminor.yy0;
        break;
      case 41: /* cmp_op ::= TEST_ANY_NE */
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
    sttype_oper_set_op(yylhsminor.yy0, STNODE_OP_ANY_NE);
}
  yymsp[0].minor.yy0 = yylhsminor.yy0;
        break;
      case 42: /* cmp_op ::= TEST_GT */
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
    sttype_oper_set_op(yylhsminor.yy0, STNODE_OP_GT);
}
  yymsp[0].minor.yy0 = yylhsminor.yy0;
        break;
      case 43: /* cmp_op ::= TEST_GE */
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
    sttype_oper_set_op(yylhsminor.yy0, STNODE_OP_GE);
}
  yymsp[0].minor.yy0 = yylhsminor.yy0;
        break;
      case 44: /* cmp_op ::= TEST_LT */
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
    sttype_oper_set_op(yylhsminor.yy0, STNODE_OP_LT);
}
  yymsp[0].minor.yy0 = yylhsminor.yy0;
        break;
      case 45: /* cmp_op ::= TEST_LE */
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
    sttype_oper_set_op(yylhsminor.yy0, STNODE_OP_LE);
}
  yymsp[0].minor.yy0 = yylhsminor.yy0;
        break;
      case 46: /* comparison_test ::= arithmetic_expr cmp_op arithmetic_expr */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2_args(yymsp[-1].minor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 47: /* comparison_test ::= arithmetic_expr cmp_op comparison_test */
{
    stnode_t *L, *F;

    F = yymsp[0].minor.yy0;
    while (stnode_type_id(F) == STTYPE_TEST) {
        sttype_oper_get(F, NULL, &F, NULL);
    }

    L = yymsp[-1].minor.yy0;
    sttype_oper_set2_args(L, yymsp[-2].minor.yy0, stnode_dup(F));

    yylhsminor.yy0 = stnode_new_empty(STTYPE_TEST);
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_AND, L, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 49: /* relation_test ::= entity TEST_CONTAINS entity */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_CONTAINS, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 50: /* relation_test ::= entity TEST_MATCHES entity */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_MATCHES, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 51: /* relation_test ::= entity TEST_IN set */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yylhsminor.yy0, STNODE_OP_IN, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 52: /* relation_test ::= entity TEST_NOT TEST_IN set */
{
    yylhsminor.yy0 = yymsp[-1].minor.yy0;
    sttype_oper_set2(yymsp[-1].minor.yy0, STNODE_OP_NOT_IN, yymsp[-3].minor.yy0, yymsp[0].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-3].minor.yy0, yymsp[0].minor.yy0);
}
  yy_destructor(yypParser,4,&yymsp[-2].minor);
  yymsp[-3].minor.yy0 = yylhsminor.yy0;
        break;
      case 54: /* relation ::= ANY relation_test */
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
    sttype_test_set_match(yylhsminor.yy0, STNODE_MATCH_ANY);
    stnode_merge_location(yylhsminor.yy0, yymsp[-1].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[-1].minor.yy0);
}
  yymsp[-1].minor.yy0 = yylhsminor.yy0;
        break;
      case 55: /* relation ::= ALL relation_test */
{
    yylhsminor.yy0 = yymsp[0].minor.yy0;
    sttype_test_set_match(yylhsminor.yy0, STNODE_MATCH_ALL);
    stnode_merge_location(yylhsminor.yy0, yymsp[-1].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[-1].minor.yy0);
}
  yymsp[-1].minor.yy0 = yylhsminor.yy0;
        break;
      case 56: /* set_element_list ::= arithmetic_expr */
{
    yylhsminor.yy69 = g_slist_append(NULL, yymsp[0].minor.yy0);
    yylhsminor.yy69 = g_slist_append(yylhsminor.yy69, NULL);
}
  yymsp[0].minor.yy69 = yylhsminor.yy69;
        break;
      case 57: /* set_element_list ::= arithmetic_expr DOTDOT arithmetic_expr */
{
    yylhsminor.yy69 = g_slist_append(NULL, yymsp[-2].minor.yy0);
    yylhsminor.yy69 = g_slist_append(yylhsminor.yy69, yymsp[0].minor.yy0);
}
  yy_destructor(yypParser,43,&yymsp[-1].minor);
  yymsp[-2].minor.yy69 = yylhsminor.yy69;
        break;
      case 58: /* set_list ::= set_element_list */
{
    yylhsminor.yy69 = g_slist_concat(NULL, yymsp[0].minor.yy69);
}
  yymsp[0].minor.yy69 = yylhsminor.yy69;
        break;
      case 59: /* set_list ::= set_list COMMA set_element_list */
{
    yylhsminor.yy69 = g_slist_concat(yymsp[-2].minor.yy69, yymsp[0].minor.yy69);
}
  yy_destructor(yypParser,44,&yymsp[-1].minor);
  yymsp[-2].minor.yy69 = yylhsminor.yy69;
        break;
      case 60: /* set ::= LBRACE set_list RBRACE */
{
    yylhsminor.yy0 = stnode_new(STTYPE_SET, yymsp[-1].minor.yy69, NULL, DFILTER_LOC_EMPTY);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[-2].minor.yy0);
    stnode_free(yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 61: /* slice ::= entity LBRACKET range_node_list RBRACKET */
{
    yylhsminor.yy0 = stnode_new(STTYPE_SLICE, NULL, NULL, DFILTER_LOC_EMPTY);
    sttype_slice_set(yylhsminor.yy0, yymsp[-3].minor.yy0, yymsp[-1].minor.yy69);

    /* Delete the list, but not the drange_nodes that
     * the list contains. */
    g_slist_free(yymsp[-1].minor.yy69);
}
  yy_destructor(yypParser,33,&yymsp[-2].minor);
  yy_destructor(yypParser,34,&yymsp[0].minor);
  yymsp[-3].minor.yy0 = yylhsminor.yy0;
        break;
      case 62: /* range_node_list ::= RANGE_NODE */
{
    char *err_msg = NULL;
    drange_node *rn = drange_node_from_str(stnode_token(yymsp[0].minor.yy0), &err_msg);
    if (err_msg != NULL) {
        FAIL(dfs, yymsp[0].minor.yy0, "%s", err_msg);
        g_free(err_msg);
    }
    yylhsminor.yy69 = g_slist_append(NULL, rn);
    stnode_free(yymsp[0].minor.yy0);
}
  yymsp[0].minor.yy69 = yylhsminor.yy69;
        break;
      case 63: /* range_node_list ::= range_node_list COMMA RANGE_NODE */
{
    char *err_msg = NULL;
    drange_node *rn = drange_node_from_str(stnode_token(yymsp[0].minor.yy0), &err_msg);
    if (err_msg != NULL) {
        FAIL(dfs, yymsp[0].minor.yy0, "%s", err_msg);
        g_free(err_msg);
    }
    yylhsminor.yy69 = g_slist_append(yymsp[-2].minor.yy69, rn);
    stnode_free(yymsp[0].minor.yy0);
}
  yy_destructor(yypParser,44,&yymsp[-1].minor);
  yymsp[-2].minor.yy69 = yylhsminor.yy69;
        break;
      case 64: /* function ::= IDENTIFIER LPAREN func_params_list RPAREN */
{
    yylhsminor.yy0 = new_function(dfs, yymsp[-3].minor.yy0);
    sttype_function_set_params(yylhsminor.yy0, yymsp[-1].minor.yy69);
    stnode_merge_location(yylhsminor.yy0, yymsp[-3].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[0].minor.yy0);
}
  yy_destructor(yypParser,23,&yymsp[-2].minor);
  yymsp[-3].minor.yy0 = yylhsminor.yy0;
        break;
      case 65: /* function ::= IDENTIFIER LPAREN RPAREN */
{
    yylhsminor.yy0 = new_function(dfs, yymsp[-2].minor.yy0);
    stnode_merge_location(yylhsminor.yy0, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
    stnode_free(yymsp[0].minor.yy0);
}
  yy_destructor(yypParser,23,&yymsp[-1].minor);
  yymsp[-2].minor.yy0 = yylhsminor.yy0;
        break;
      case 66: /* func_params_list ::= arithmetic_expr */
{
    yylhsminor.yy69 = g_slist_append(NULL, yymsp[0].minor.yy0);
}
  yymsp[0].minor.yy69 = yylhsminor.yy69;
        break;
      case 67: /* func_params_list ::= func_params_list COMMA arithmetic_expr */
{
    yylhsminor.yy69 = g_slist_append(yymsp[-2].minor.yy69, yymsp[0].minor.yy0);
}
  yy_destructor(yypParser,44,&yymsp[-1].minor);
  yymsp[-2].minor.yy69 = yylhsminor.yy69;
        break;
      default:
        break;
/********** End reduce actions ************************************************/
  };
  assert( yyruleno<sizeof(yyRuleInfoLhs)/sizeof(yyRuleInfoLhs[0]) );
  yygoto = yyRuleInfoLhs[yyruleno];
  yysize = yyRuleInfoNRhs[yyruleno];
  yyact = yy_find_reduce_action(yymsp[yysize].stateno,(YYCODETYPE)yygoto);

  /* There are no SHIFTREDUCE actions on nonterminals because the table
  ** generator has simplified them to pure REDUCE actions. */
  assert( !(yyact>YY_MAX_SHIFT && yyact<=YY_MAX_SHIFTREDUCE) );

  /* It is not possible for a REDUCE to be followed by an error */
  assert( yyact!=YY_ERROR_ACTION );

  yymsp += yysize+1;
  yypParser->yytos = yymsp;
  yymsp->stateno = (YYACTIONTYPE)yyact;
  yymsp->major = (YYCODETYPE)yygoto;
  yyTraceShift(yypParser, yyact, "... then shift");
  return yyact;
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  DfilterARG_FETCH
  DfilterCTX_FETCH
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
/************ Begin %parse_failure code ***************************************/
/************ End %parse_failure code *****************************************/
  DfilterARG_STORE /* Suppress warning about unused %extra_argument variable */
  DfilterCTX_STORE
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  DfilterTOKENTYPE yyminor         /* The minor type of the error token */
){
  DfilterARG_FETCH
  DfilterCTX_FETCH
#define TOKEN yyminor
/************ Begin %syntax_error code ****************************************/

    if (!TOKEN) {
        dfilter_fail(dfs, DF_ERROR_UNEXPECTED_END, DFILTER_LOC_EMPTY, "Unexpected end of filter expression.");
        return;
    }
    FAIL(dfs, TOKEN, "\"%s\" was unexpected in this context.", stnode_token(TOKEN));
/************ End %syntax_error code ******************************************/
  DfilterARG_STORE /* Suppress warning about unused %extra_argument variable */
  DfilterCTX_STORE
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  DfilterARG_FETCH
  DfilterCTX_FETCH
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
#ifndef YYNOERRORRECOVERY
  yypParser->yyerrcnt = -1;
#endif
  assert( yypParser->yytos==yypParser->yystack );
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
/*********** Begin %parse_accept code *****************************************/
/*********** End %parse_accept code *******************************************/
  DfilterARG_STORE /* Suppress warning about unused %extra_argument variable */
  DfilterCTX_STORE
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "DfilterAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void Dfilter(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  DfilterTOKENTYPE yyminor       /* The value for the token */
  DfilterARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  YYACTIONTYPE yyact;   /* The parser action. */
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  int yyendofinput;     /* True if we are at the end of input */
#endif
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser = (yyParser*)yyp;  /* The parser */
  DfilterCTX_FETCH
  DfilterARG_STORE

  assert( yypParser->yytos!=0 );
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  yyendofinput = (yymajor==0);
#endif

  yyact = yypParser->yytos->stateno;
#ifndef NDEBUG
  if( yyTraceFILE ){
    if( yyact < YY_MIN_REDUCE ){
      fprintf(yyTraceFILE,"%sInput '%s' in state %d\n",
              yyTracePrompt,yyTokenName[yymajor],yyact);
    }else{
      fprintf(yyTraceFILE,"%sInput '%s' with pending reduce %d\n",
              yyTracePrompt,yyTokenName[yymajor],yyact-YY_MIN_REDUCE);
    }
  }
#endif

  while(1){ /* Exit by "break" */
    assert( yypParser->yytos>=yypParser->yystack );
    assert( yyact==yypParser->yytos->stateno );
    yyact = yy_find_shift_action((YYCODETYPE)yymajor,yyact);
    if( yyact >= YY_MIN_REDUCE ){
      unsigned int yyruleno = yyact - YY_MIN_REDUCE; /* Reduce by this rule */
#ifndef NDEBUG
      assert( yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0])) );
      if( yyTraceFILE ){
        int yysize = yyRuleInfoNRhs[yyruleno];
        if( yysize ){
          fprintf(yyTraceFILE, "%sReduce %d [%s]%s, pop back to state %d.\n",
            yyTracePrompt,
            yyruleno, yyRuleName[yyruleno],
            yyruleno<YYNRULE_WITH_ACTION ? "" : " without external action",
            yypParser->yytos[yysize].stateno);
        }else{
          fprintf(yyTraceFILE, "%sReduce %d [%s]%s.\n",
            yyTracePrompt, yyruleno, yyRuleName[yyruleno],
            yyruleno<YYNRULE_WITH_ACTION ? "" : " without external action");
        }
      }
#endif /* NDEBUG */

      /* Check that the stack is large enough to grow by a single entry
      ** if the RHS of the rule is empty.  This ensures that there is room
      ** enough on the stack to push the LHS value */
      if( yyRuleInfoNRhs[yyruleno]==0 ){
#ifdef YYTRACKMAXSTACKDEPTH
        if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
          yypParser->yyhwm++;
          assert( yypParser->yyhwm ==
                  (int)(yypParser->yytos - yypParser->yystack));
        }
#endif
#if YYSTACKDEPTH>0
        if( yypParser->yytos>=yypParser->yystackEnd ){
          yyStackOverflow(yypParser);
          break;
        }
#else
        if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz-1] ){
          if( yyGrowStack(yypParser) ){
            yyStackOverflow(yypParser);
            break;
          }
        }
#endif
      }
      yyact = yy_reduce(yypParser,yyruleno,yymajor,yyminor DfilterCTX_PARAM);
    }else if( yyact <= YY_MAX_SHIFTREDUCE ){
      yy_shift(yypParser,yyact,(YYCODETYPE)yymajor,yyminor);
#ifndef YYNOERRORRECOVERY
      yypParser->yyerrcnt--;
#endif
      break;
    }else if( yyact==YY_ACCEPT_ACTION ){
      yypParser->yytos--;
      yy_accept(yypParser);
      return;
    }else{
      assert( yyact == YY_ERROR_ACTION );
      yyminorunion.yy0 = yyminor;
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminor);
      }
      yymx = yypParser->yytos->major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor, &yyminorunion);
        yymajor = YYNOCODE;
      }else{
        while( yypParser->yytos > yypParser->yystack ){
          yyact = yy_find_reduce_action(yypParser->yytos->stateno,
                                        YYERRORSYMBOL);
          if( yyact<=YY_MAX_SHIFTREDUCE ) break;
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yytos <= yypParser->yystack || yymajor==0 ){
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
          yypParser->yyerrcnt = -1;
#endif
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          yy_shift(yypParser,yyact,YYERRORSYMBOL,yyminor);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
      if( yymajor==YYNOCODE ) break;
      yyact = yypParser->yytos->stateno;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor, yyminor);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      break;
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor, yyminor);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
        yypParser->yyerrcnt = -1;
#endif
      }
      break;
#endif
    }
  }
#ifndef NDEBUG
  if( yyTraceFILE ){
    yyStackEntry *i;
    char cDiv = '[';
    fprintf(yyTraceFILE,"%sReturn. Stack=",yyTracePrompt);
    for(i=&yypParser->yystack[1]; i<=yypParser->yytos; i++){
      fprintf(yyTraceFILE,"%c%s", cDiv, yyTokenName[i->major]);
      cDiv = ' ';
    }
    fprintf(yyTraceFILE,"]\n");
  }
#endif
  return;
}

/*
** Return the fallback token corresponding to canonical token iToken, or
** 0 if iToken has no fallback.
*/
int DfilterFallback(int iToken){
#ifdef YYFALLBACK
  assert( iToken<(int)(sizeof(yyFallback)/sizeof(yyFallback[0])) );
  return yyFallback[iToken];
#else
  (void)iToken;
  return 0;
#endif
}

DIAG_ON_LEMON()

    static stnode_t *
    resolve_unparsed(dfsyntax_t *dfs, stnode_t *node)
    {
        if (stnode_type_id(node) != STTYPE_UNPARSED) {
            ws_assert(stnode_type_id(node) == STTYPE_FIELD);
            return node;
        }
        const char *name = stnode_token(node);
        header_field_info *hfinfo = dfilter_resolve_unparsed(name, dfs->deprecated);
        if (hfinfo == NULL) {
            FAIL(dfs, node, "\"%s\" is not a valid protocol or protocol field.", name);
        }
        stnode_replace(node, STTYPE_FIELD, hfinfo); // NULL is OK here, we will fail later
        return node;
    }

    static stnode_t *
    new_reference(dfsyntax_t *dfs , stnode_t *node)
    {
        /* convert field to reference */

        stnode_t *ref = stnode_new(STTYPE_REFERENCE, sttype_field_hfinfo(node), g_strdup(stnode_token(node)), stnode_location(node));
        sttype_field_set_drange(ref, sttype_field_drange_steal(node));
        sttype_field_set_raw(ref, sttype_field_raw(node));
        return ref;
    }

    static stnode_t *
    new_function(dfsyntax_t *dfs, stnode_t *node)
    {
        const char *name = stnode_token(node);

        df_func_def_t *def = df_func_lookup(name);
        if (!def) {
            FAIL(dfs, node, "Function '%s' does not exist", name);
        }
        stnode_replace(node, STTYPE_FUNCTION, def);
        return node;
    }
