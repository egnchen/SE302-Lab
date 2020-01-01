#include "tiger/escape/escape.h"
#include <iostream>

/*
 * Escape analysis
 * Tranverse the tree, find escaped variables
 * and modify "escape" entry in VarDec
 */

namespace ESC {

void FindEscape(A::Exp* exp)
{
  EEnv* env = new EEnv();
  traverseExp(env, 1, exp);
}

void traverseExp(EEnv* env, int depth, A::Exp* e)
{
  switch (e->kind) {
  case A::Exp::VAR:
    traverseVar(env, depth, ((A::VarExp*)e)->var);
    break;
  case A::Exp::CALL: {
    A::ExpList* exp_list = ((A::CallExp*)e)->args;
    for(; exp_list; exp_list = exp_list->tail)
      traverseExp(env, depth, exp_list->head);
    break;
  }
  case A::Exp::OP:
    traverseExp(env, depth, ((A::OpExp*)e)->left);
    traverseExp(env, depth, ((A::OpExp*)e)->right);
    break;
  case A::Exp::RECORD: {
    A::EFieldList* field_list = ((A::RecordExp*)e)->fields;
    for (; field_list; field_list = field_list->tail)
      traverseExp(env, depth, field_list->head->exp);
    break;
  }
  case A::Exp::SEQ: {
    A::ExpList* exp_list = ((A::SeqExp*)e)->seq;
    for (; exp_list; exp_list = exp_list->tail)
      traverseExp(env, depth, exp_list->head);
    break;
  }
  case A::Exp::ASSIGN:
    traverseVar(env, depth, ((A::AssignExp*)e)->var);
    traverseExp(env, depth, ((A::AssignExp*)e)->exp);
    break;
  case A::Exp::IF:
    traverseExp(env, depth, ((A::IfExp*)e)->test);
    traverseExp(env, depth, ((A::IfExp*)e)->then);
    if (((A::IfExp*)e)->elsee)
      traverseExp(env, depth, ((A::IfExp*)e)->elsee);
    break;
  case A::Exp::WHILE:
    traverseExp(env, depth, ((A::WhileExp*)e)->test);
    traverseExp(env, depth, ((A::WhileExp*)e)->body);
    break;
  case A::Exp::FOR:
    // need escape analysis here.
    ((A::ForExp*)e)->escape = false;
    env->Enter(((A::ForExp*)e)->var,
        new EscapeEntry(depth, &((A::ForExp*)e)->escape));
    traverseExp(env, depth, ((A::ForExp*)e)->lo);
    traverseExp(env, depth, ((A::ForExp*)e)->hi);
    traverseExp(env, depth, ((A::ForExp*)e)->body);
    break;
  case A::Exp::LET: {
    A::DecList* dec_list = ((A::LetExp*)e)->decs;
    for (; dec_list; dec_list = dec_list->tail)
      traverseDec(env, depth, dec_list->head);
    traverseExp(env, depth, ((A::LetExp*)e)->body);
    break;
  }
  case A::Exp::ARRAY:
    traverseExp(env, depth, ((A::ArrayExp*)e)->size);
    traverseExp(env, depth, ((A::ArrayExp*)e)->init);
    break;
  case A::Exp::NIL:
  case A::Exp::INT:
  case A::Exp::STRING:
  case A::Exp::BREAK:
  case A::Exp::VOID:
    // do nothing
    break;
  default:
    assert(0);
  }
}

void traverseDec(EEnv* env, int depth, A::Dec* d)
{
  switch (d->kind) {
  case A::Dec::VAR:
    ((A::VarDec*)d)->escape = false;
    env->Enter(((A::VarDec*)d)->var,
        new EscapeEntry(depth, &((A::VarDec*)d)->escape));
    traverseExp(env, depth, ((A::VarDec*)d)->init);
    break;
  case A::Dec::FUNCTION: {
    A::FunDecList* fundec_list = ((A::FunctionDec*)d)->functions;
    for (; fundec_list; fundec_list = fundec_list->tail) {
      // add new environments...
      env->BeginScope();
      A::FunDec* dec = fundec_list->head;
      A::FieldList* field_list = dec->params;
      for (; field_list; field_list = field_list->tail)
        env->Enter(field_list->head->name,
          new EscapeEntry(depth + 1, &field_list->head->escape));
      traverseExp(env, depth + 1, dec->body);
      env->EndScope();
    }
    break;
  }
  case A::Dec::TYPE:
    // none of our business here
    break;
  default:
    assert(0);
  }
}

void traverseVar(EEnv* env, int depth, A::Var* v)
{
  switch(v->kind)
  {
    case A::Var::SIMPLE: {
      EscapeEntry *ee = env->Look(((A::SimpleVar *)v)->sym);
      if(ee->depth < depth) {
        std::cout << "Escaping ";
        v->Print(stdout, depth);
        std::cout << std::endl;
        *ee->escape = true;
      }
      break;
    }
    case A::Var::FIELD:
      traverseVar(env, depth, ((A::FieldVar *)v)->var);
      break;
    case A::Var::SUBSCRIPT:
      traverseVar(env, depth, ((A::SubscriptVar *)v)->var);
      break;
    default:
      assert(0);
  }
}

} // namespace ESC