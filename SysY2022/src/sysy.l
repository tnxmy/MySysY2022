%{
#include "sysy.tab.hpp"
#include <stdlib.h>
#include <string.h>
%}

%option noyywrap
%option yylineno

digit       [0-9]
nondigit    [_a-zA-Z]
identifier  {nondigit}({nondigit}|{digit})*

decimal     [1-9]{digit}*
octal       0[0-7]*
hex         0[xX][0-9a-fA-F]+

whitespace  [ \t\r\n]+

line_comment    "//".*
block_comment   "/*"([^*]|\*+[^*/])*\*+"/"

%%

{whitespace}    { /* 跳过 */ }
{line_comment}  { /* 跳过单行注释 */ }
{block_comment} { /* 跳过多行注释 */ }

"int"       { return INT; }
"float"     { return FLOAT; }
"void"      { return VOID; }
"const"     { return CONST; }
"if"        { return IF; }
"else"      { return ELSE; }
"while"     { return WHILE; }
"break"     { return BREAK; }
"continue"  { return CONTINUE; }
"return"    { return RETURN; }

{identifier}    { yylval.str = strdup(yytext); return IDENT; }

{decimal}       { yylval.num = strtol(yytext, NULL, 10); return INT_CONST; }
{octal}         { yylval.num = strtol(yytext, NULL, 8);  return INT_CONST; }
{hex}           { yylval.num = strtol(yytext, NULL, 16); return INT_CONST; }

0[xX][0-9a-fA-F]+\.[0-9a-fA-F]*[pP][+-]?[0-9]+       { yylval.fnum = strtof(yytext, NULL); return FLOAT_CONST; }
0[xX][0-9a-fA-F]*\.[0-9a-fA-F]+[pP][+-]?[0-9]+       { yylval.fnum = strtof(yytext, NULL); return FLOAT_CONST; }
{digit}+"."{digit}*([eE][+-]?{digit}+)?       { yylval.fnum = strtof(yytext, NULL); return FLOAT_CONST; }
{digit}*"."{digit}+([eE][+-]?{digit}+)?       { yylval.fnum = strtof(yytext, NULL); return FLOAT_CONST; }
{digit}+[eE][+-]?{digit}+                     { yylval.fnum = strtof(yytext, NULL); return FLOAT_CONST; }

\"([^\"\\\n]|\\.)*\"                          { yylval.str = strdup(yytext); return STRING_CONST; }

"+"     { return PLUS; }
"-"     { return MINUS; }
"*"     { return MUL; }
"/"     { return DIV; }
"%"     { return MOD; }

"="     { return ASSIGN; }
"=="    { return EQ; }
"!="    { return NEQ; }
"<"     { return LT; }
">"     { return GT; }
"<="    { return LE; }
">="    { return GE; }

"&&"    { return AND; }
"||"    { return OR; }
"!"     { return NOT; }

";"     { return SEMICOLON; }
","     { return COMMA; }
"("     { return LPAREN; }
")"     { return RPAREN; }
"["     { return LBRACKET; }
"]"     { return RBRACKET; }
"{"     { return LBRACE; }
"}"     { return RBRACE; }

.       { fprintf(stderr, "Error at line %d: unrecognized character '%s'\n", yylineno, yytext); return ERROR; }

%%
