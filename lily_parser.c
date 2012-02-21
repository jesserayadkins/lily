#include <string.h>

#include "lily_ast.h"
#include "lily_debug.h"
#include "lily_emitter.h"
#include "lily_impl.h"
#include "lily_lexer.h"
#include "lily_parser.h"
#include "lily_msgbuf.h"
#include "lily_symtab.h"
#include "lily_vm.h"

static void parse_expr_value(lily_parse_state *parser)
{
    lily_lex_state *lex = parser->lex;
    lily_symtab *symtab = parser->symtab;

    while (1) {
        if (lex->token == tk_word) {
            lily_var *var = lily_var_by_name(symtab, lex->label);

            if (var == NULL)
                lily_raise(parser->error, "Variable '%s' is undefined.\n",
                           lex->label);

            if (isafunc(var)) {
                /* New trees will get saved to the args section of this tree
                    when they are done. */
                lily_ast_enter_func(parser->ast_pool, var);

                lily_lexer(parser->lex);
                if (lex->token != tk_left_parenth)
                    lily_raise(parser->error,
                        "Expected '(' after function name.\n");

                lily_lexer(lex);
                /* Get the first value of the function. */
                continue;
            }
            else {
                lily_ast_push_sym(parser->ast_pool, (lily_sym *)var);

                lily_lexer(lex);
            }
        }
        else {
            lily_literal *lit;
            lily_class *cls;

            if (lex->token == tk_double_quote)
                cls = lily_class_by_id(symtab, SYM_CLASS_STR);
            else if (lex->token == tk_integer)
                cls = lily_class_by_id(symtab, SYM_CLASS_INTEGER);
            else if (lex->token == tk_number)
                cls = lily_class_by_id(symtab, SYM_CLASS_NUMBER);
            else
                break;

            lit = lily_new_literal(symtab, cls);
            lit->value = lex->value;

            lily_ast_push_sym(parser->ast_pool, (lily_sym *)lit);

            lily_lexer(lex);
            break;
        }
    }
}

/* Expressions are divided into two states:
 * Value: A value is needed. ( is handled, because parenth expressions always
   yield a single value).
 * Op: The expression has enough values. Getting an op means another value will
   be necessary. If a word is found, it is the first word of the next
   expression (so no semicolons). ) is handled, because the expression is done
   and can properly be closed. */

static void parse_expr_top(lily_parse_state *parser)
{
    lily_lex_state *lex = parser->lex;

    while (1) {
        if (lex->token == tk_word) {
            if (parser->ast_pool->save_index != 0)
                lily_raise(parser->error,
                           "Expected ')' or a binary op, not a label.\n");

            break;
        }
        else if (lex->token == tk_right_parenth) {
            lily_ast_pop_tree(parser->ast_pool);

            lily_lexer(lex);
            /* If no functions, and a word, then the word is the start of the
               next statement. */
            if (lex->token == tk_word && parser->ast_pool->save_index == 0)
                break;
        }
        else {
            lily_expr_op op;
            if (lex->token == tk_equal)
                op = expr_assign;
            else if (lex->token == tk_plus)
                op = expr_plus;
            else if (lex->token == tk_minus)
                op = expr_minus;
            else if (lex->token == tk_end_tag || lex->token == tk_eof)
                break;
            else {
                lily_raise(parser->error,
                    "parse_expr_top: Unexpected token value %s.\n",
                    tokname(lex->token));
            }
            lily_ast_push_binary_op(parser->ast_pool, op);

            lily_lexer(lex);
        }
        parse_expr_value(parser);
    }

    lily_emit_ast(parser->emit, parser->ast_pool->root);
    lily_ast_reset_pool(parser->ast_pool);
}

static void parse_declaration(lily_parse_state *parser, lily_class *cls)
{
    lily_lex_state *lex = parser->lex;
    lily_var *var;

    while (1) {
        /* This starts at the class name, or the comma. The label is next. */
        lily_lexer(lex);

        if (lex->token != tk_word)
            lily_raise(parser->error, "Expected a variable name, not %s.\n",
                       tokname(lex->token));

        var = lily_var_by_name(parser->symtab, lex->label);
        if (var != NULL)
            lily_raise(parser->error, "%s has already been declared.\n",
                       var->name);

        var = lily_new_var(parser->symtab, cls, lex->label);

        lily_lexer(parser->lex);
        /* Handle an initializing assignment, if there is one. */
        if (lex->token == tk_equal) {
            lily_ast_push_sym(parser->ast_pool, (lily_sym *)var);
            parse_expr_top(parser);
        }

        /* This is the start of the next statement. */
        if (lex->token == tk_word || lex->token == tk_end_tag ||
            lex->token == tk_eof)
            break;
        else if (lex->token != tk_comma) {
            lily_raise(parser->error, "Expected ',' or ')', not %s.\n",
                       tokname(lex->token));
        }
        /* else comma, so just jump back up. */
    }
}

static void parse_statement(lily_parse_state *parser)
{
    char *label = parser->lex->label;
    lily_class *lclass;

    lclass = lily_class_by_name(parser->symtab, label);

    if (lclass != NULL) {
        /* Do decl parsing, which will handle any assignments. */
        parse_declaration(parser, lclass);
    }
    else {
        /* statement like 'print(x)', or 'a = b'. Call unary to prep the tree,
           then binary handles the rest. */
        parse_expr_value(parser);
        parse_expr_top(parser);
    }
}

void lily_free_parse_state(lily_parse_state *parser)
{
    lily_ast_free_pool(parser->ast_pool);
    lily_free_symtab(parser->symtab);
    lily_free_lex_state(parser->lex);
    lily_free_emit_state(parser->emit);
    lily_free(parser);
}

lily_parse_state *lily_new_parse_state(lily_excep_data *excep)
{
    lily_parse_state *s = lily_malloc(sizeof(lily_parse_state));

    if (s == NULL)
        return NULL;

    s->ast_pool = lily_ast_init_pool(excep, 8);
    s->error = excep;

    s->symtab = lily_new_symtab(excep);
    s->emit = lily_new_emit_state(excep);
    s->lex = lily_new_lex_state(excep);

    if (s->lex == NULL || s->emit == NULL || s->symtab == NULL ||
        s->ast_pool == NULL) {
        if (s->symtab != NULL)
            lily_free_symtab(s->symtab);
        if (s->emit != NULL)
            lily_free_emit_state(s->emit);
        if (s->lex != NULL)
            lily_free_lex_state(s->lex);
        if (s->ast_pool != NULL)
            lily_ast_free_pool(s->ast_pool);
        lily_free(s);
        return NULL;
    }

    s->symtab->lex_linenum = &s->lex->line_num;
    s->ast_pool->lex_linenum = &s->lex->line_num;
    s->emit->symtab = s->symtab;
    lily_emit_set_target(s->emit, s->symtab->main);
    return s;
}

void lily_parser(lily_parse_state *parser)
{
    lily_lex_state *lex = parser->lex;
    lily_lexer(lex);

    while (1) {
        if (lex->token == tk_word)
            parse_statement(parser);
        else if (lex->token == tk_end_tag || lex->token == tk_eof) {
            lily_emit_vm_return(parser->emit);
            /* Show symtab until the bugs are gone. */
            lily_show_symtab(parser->symtab);

            lily_vm_execute(parser->error, parser->symtab->main);
            /* Show var values, to verify execution went as expected. */
            lily_show_var_values(parser->symtab);
            /* Clear the main func for reuse. */
            lily_reset_main(parser->symtab);

            if (lex->token == tk_end_tag) {
                lily_lexer_handle_page_data(parser->lex);
                if (lex->token == tk_eof)
                    break;
            }
            else
                break;
        }
    }
}
