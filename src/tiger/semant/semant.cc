#include "tiger/semant/semant.h"
#include "tiger/errormsg/errormsg.h"
#include <cstring>
#include <set>
extern EM::ErrorMsg errormsg;

using VEnvType = S::Table<E::EnvEntry> *;
using TEnvType = S::Table<TY::Ty> *;

namespace {
static TY::TyList *make_formal_tylist(TEnvType tenv, A::FieldList *params) {
  if (params == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(params->head->typ);
  if (ty == nullptr) {
    errormsg.Error(params->head->pos, "undefined type %s",
                   params->head->typ->Name().c_str());
  }

  return new TY::TyList(ty->ActualTy(), make_formal_tylist(tenv, params->tail));
}

static TY::FieldList *make_fieldlist(TEnvType tenv, A::FieldList *fields) {
  if (fields == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(fields->head->typ);
  return new TY::FieldList(new TY::Field(fields->head->name, ty),
                           make_fieldlist(tenv, fields->tail));
}

}  // namespace

namespace A {

TY::Ty *SimpleVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  E::VarEntry *env_entry = static_cast<E::VarEntry *>(venv->Look(this->sym));
  if(env_entry == nullptr)
    errormsg.Error(this->pos, "undefined variable %s", this->sym->Name().c_str());
  else if(env_entry->kind != E::EnvEntry::Kind::VAR)
    errormsg.Error(this->pos, "%s is not a variable.", this->sym);
  else
    return env_entry->ty;
  return TY::VoidTy::Instance();
}

TY::Ty *FieldVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  TY::RecordTy *parent_type = static_cast<TY::RecordTy *>(
    this->var->SemAnalyze(venv, tenv, labelcount)->ActualTy());
  // check symbol
  if(parent_type->kind != TY::Ty::RECORD)
    errormsg.Error(this->pos, "not a record type");
  else {
    TY::FieldList *cur = parent_type->fields;
    for(; cur; cur = cur->tail) {
      if(this->sym == cur->head->name)
        // found match
        return cur->head->ty;
    }
    errormsg.Error(this->pos, "field %s doesn't exist", this->sym->Name().c_str());
  }
  return TY::VoidTy::Instance();
}
 
TY::Ty *SubscriptVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                                 int labelcount) const {
  TY::ArrayTy *parent_type = static_cast<TY::ArrayTy *>(
    this->var->SemAnalyze(venv, tenv, labelcount)->ActualTy());
  
  // check symbol
  if(parent_type->kind != TY::Ty::ARRAY)
    errormsg.Error(this->pos, "array type required");
  return parent_type->ty;
}

TY::Ty *VarExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  return this->var->SemAnalyze(venv, tenv, labelcount);
}

TY::Ty *NilExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  return TY::NilTy::Instance();
}

TY::Ty *IntExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  return TY::IntTy::Instance();
}

TY::Ty *StringExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  return TY::StringTy::Instance();
}

TY::Ty *CallExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                            int labelcount) const {
  // check function variable
  E::FunEntry *fun = static_cast<E::FunEntry *>(venv->Look(this->func));
  
  if(fun == nullptr)
    errormsg.Error(this->pos, "undefined function %s", this->func->Name().c_str());
  else if(fun->kind != E::EnvEntry::FUN)
    errormsg.Error(this->pos, "%s is not a function.", this->func);
  else {
    // check params
    TY::TyList *formals = fun->formals;
    A::ExpList *actuals = this->args;
    int i = 0;
    while(formals && actuals) {
      TY::Ty *now = actuals->head->SemAnalyze(venv, tenv, labelcount);
      if(!formals->head->ActualTy()->IsSameType(now)) {
        // doesn't match
        errormsg.Error(actuals->head->pos, "para type mismatch");
      }
      i++;
      formals = formals->tail;
      actuals = actuals->tail;
    }
    if(formals != nullptr)
      errormsg.Error(this->pos, "too few params in function %s", this->func->Name().c_str());
    else if(actuals != nullptr)
      errormsg.Error(this->pos, "too many params in function %s", this->func->Name().c_str());
    else
      return fun->result;
  }
  return TY::VoidTy::Instance();
}

TY::Ty *OpExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // analyze operations
  // make this clearer
  TY::Ty *left_type = this->left->SemAnalyze(venv, tenv, labelcount);
  TY::Ty *right_type = this->right->SemAnalyze(venv, tenv, labelcount);
  switch(this->oper) {
    // calculations
    case A::Oper::PLUS_OP:
    case A::Oper::MINUS_OP:
    case A::Oper::TIMES_OP:
    case A::Oper::DIVIDE_OP:
    // these can be applied to only integers
      if(left_type->kind != TY::Ty::Kind::NAME
        && !(left_type->IsSameType(TY::IntTy::Instance())))
        errormsg.Error(this->pos, "integer required");
      else if(right_type->kind != TY::Ty::Kind::NAME
        && !(right_type->IsSameType(TY::IntTy::Instance())))
        errormsg.Error(this->pos, "integer required");
      else
        return TY::IntTy::Instance();
      break;
    // comparisons
    case A::Oper::GE_OP:
    case A::Oper::GT_OP:
    case A::Oper::LT_OP:
    case A::Oper::LE_OP:
    case A::Oper::EQ_OP:
    case A::Oper::NEQ_OP:
      // these can be applied integers & strings
      if(left_type->kind != TY::Ty::Kind::NAME
        && right_type->kind != TY::Ty::Kind::NAME
        && !(left_type->IsSameType(right_type)))
        errormsg.Error(this->pos, "same type required");
      else
        return TY::IntTy::Instance();
  }
}

TY::Ty *RecordExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  TY::RecordTy *typ = static_cast<TY::RecordTy *>(tenv->Look(this->typ));
  if(typ == nullptr)
    errormsg.Error(this->pos, "undefined type %s", this->typ->Name().c_str());
  else if(typ->kind != TY::Ty::Kind::RECORD)
    errormsg.Error(this->pos, "%s is not a record type.", this->typ->Name().c_str());
  else {
    TY::FieldList *formals = typ->fields;
    A::EFieldList *actuals = this->fields;
    int i = 0;
    // according to specs formals & actuals should match in order
    while(formals && actuals) {
      if(formals->head->name != actuals->head->name)
        errormsg.Error(actuals->head->exp->pos, "Expected %s, got %s",
          formals->head->name, actuals->head->name);
      else if(!(typ->fields->head->ty->IsSameType(
        this->fields->head->exp->SemAnalyze(venv, tenv, labelcount))))
        errormsg.Error(actuals->head->exp->pos, "Type mismatch.");
      else {
        // can continue
        formals = formals->tail;
        actuals = actuals->tail;
        i++;
        continue;
      }
      break;
    }
    if(formals != nullptr)
      errormsg.Error(this->pos, "Arguments not enough, only got %d", i);
    else if(actuals != nullptr)
      errormsg.Error(this->pos, "Too many arguments, got %d", i);
    else
      return typ;
  }
  return TY::VoidTy::Instance();
}

TY::Ty *SeqExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // sequentially type check everything
  TY::Ty *ret;
  for(A::ExpList *exps = this->seq; exps; exps = exps->tail) {
    ret = exps->head->SemAnalyze(venv, tenv, labelcount);
  }
  return ret;
}

TY::Ty *AssignExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  // calc lvalue first, then exp
  // doesn't have a return value according to specs
  TY::Ty *var_type = this->var->SemAnalyze(venv, tenv, labelcount);
  TY::Ty *exp_type = this->exp->SemAnalyze(venv, tenv, labelcount);
  if(!(var_type->IsSameType(exp_type)))
    errormsg.Error(this->pos, "unmatched assign exp");
  // check if it's a loop variable
  if(this->var->kind == A::Var::Kind::SIMPLE) {
    E::VarEntry *var_entry = 
      static_cast<E::VarEntry *>(venv->Look((static_cast<A::SimpleVar *>(this->var))->sym));
    if(var_entry->kind != E::EnvEntry::Kind::VAR)
      errormsg.Error(this->pos, "inconsistency occured, cannot determine whether is loop var or not");
    else if(var_entry->readonly)
      errormsg.Error(this->pos, "loop variable can't be assigned");
  }
  return TY::VoidTy::Instance();
}

TY::Ty *IfExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // check the condition first
  if(!(this->test->SemAnalyze(venv, tenv, labelcount)->
    IsSameType(TY::IntTy::Instance())))
    errormsg.Error(this->test->pos, "Test expression doesn't have an integer type.");
  else {
    // check then-else
    // according to spec exp1 and exp2 must have same type
    TY::Ty *then_type = this->then->SemAnalyze(venv, tenv, labelcount),
      *else_type = nullptr;
    if(this->elsee != nullptr) {
      // if-then-else: then & else have same type
      else_type = this->elsee->SemAnalyze(venv, tenv, labelcount);
      if(!(then_type->IsSameType(else_type)))
        errormsg.Error(this->pos, "then exp and else exp type mismatch");
    } else {
      // if-then: no return value
      if(then_type->kind != TY::Ty::Kind::VOID)
        errormsg.Error(this->then->pos, "if-then exp's body must produce no value");
    }
    return then_type;
  }
}

TY::Ty *WhileExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  // check condition first
  if(!(this->test->SemAnalyze(venv, tenv, labelcount)->
    IsSameType(TY::IntTy::Instance())))
    errormsg.Error(this->test->pos, "Test expression doesn't have an integer type.");
  
  TY::Ty *ret = this->body->SemAnalyze(venv, tenv, labelcount);
  if(ret->kind != TY::Ty::Kind::VOID) {
    errormsg.Error(this->test->pos, "while body must produce no value");
    return TY::VoidTy::Instance();
  }
  return ret;
}

TY::Ty *ForExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // check two bounds first
  if(!(this->lo->SemAnalyze(venv, tenv, labelcount)->
    IsSameType(TY::IntTy::Instance())))
    errormsg.Error(this->lo->pos, "for exp's range type is not integer");
  else if(!(this->hi->SemAnalyze(venv, tenv, labelcount)->
    IsSameType(TY::IntTy::Instance())))
    errormsg.Error(this->hi->pos, "for exp's range type is not integer");
  // continue 
  // modify environment
  venv->BeginScope(); tenv->BeginScope();
  venv->Enter(this->var, new E::VarEntry(TY::IntTy::Instance(), true));
  auto exp_type = this->body->SemAnalyze(venv, tenv, labelcount);
  if(exp_type->kind != TY::Ty::Kind::VOID)
    errormsg.Error(this->pos, "for body must produce no value");
  venv->EndScope(); tenv->EndScope();
  return TY::VoidTy::Instance();
}

TY::Ty *BreakExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  // Jesus, what should I do?
  return TY::VoidTy::Instance();
}

TY::Ty *LetExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // here we go...
  venv->BeginScope(); tenv->BeginScope();
  for(A::DecList *decs = this->decs; decs; decs = decs->tail)
    decs->head->SemAnalyze(venv, tenv, labelcount);
  TY::Ty *ret = this->body->SemAnalyze(venv, tenv, labelcount);
  venv->EndScope(); tenv->EndScope();
  return ret;
}

TY::Ty *ArrayExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  // check type first
  TY::ArrayTy *array_type = static_cast<TY::ArrayTy *>(tenv->Look(this->typ)->ActualTy());
  if(array_type->kind != TY::Ty::Kind::ARRAY)
    errormsg.Error(this->pos, "%s is not a valid array type.", this->typ);
  else if(!(this->size->SemAnalyze(venv, tenv, labelcount)->IsSameType(TY::IntTy::Instance())))
    errormsg.Error(this->pos, "Length is not an integer expression.");
  else if(!(this->init->SemAnalyze(venv, tenv, labelcount)->IsSameType(array_type->ty)))
    errormsg.Error(this->pos, "type mismatch");
  else
    return array_type;
  return TY::VoidTy::Instance();
}

TY::Ty *VoidExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                            int labelcount) const {
  // WTF?
  return TY::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  // function declaration first
  std::set<S::Symbol *> funcnames;
  for(auto funcs = this->functions; funcs; funcs = funcs->tail) {
    auto func = funcs->head;
    if(funcnames.find(func->name) != funcnames.end()) {
      errormsg.Error(this->pos, "two functions have the same name");
      return;
    }
    funcnames.insert(func->name);
    // optional result type
    TY::Ty *result_type;
    TY::TyList *formal_types = make_formal_tylist(tenv, func->params);
    if(func->result == nullptr)
      result_type = TY::VoidTy::Instance();
    else
      result_type = tenv->Look(func->result);
    venv->Enter(func->name, new E::FunEntry(formal_types, result_type));
  }
  // parse function
  for(auto funcs = this->functions; funcs; funcs = funcs->tail) {
    auto func = funcs->head;
    // begin function body parsing
    venv->BeginScope(); tenv->BeginScope();
    // make param list & add them to scope
    auto field_type_list = make_fieldlist(tenv, func->params);
    for(auto field_entry = field_type_list; field_entry; field_entry = field_entry->tail)
      venv->Enter(field_entry->head->name, new E::VarEntry(field_entry->head->ty));
    // analyze the function body
    TY::Ty *exp_type = func->body->SemAnalyze(venv, tenv, labelcount);
    // ok, ok.
    venv->EndScope(); tenv->EndScope();
    // check body type and declared type
    TY::Ty *decl_type = (static_cast<E::FunEntry *>(venv->Look(func->name)))->result;
    if(exp_type != decl_type) {
      if(decl_type->kind == TY::Ty::Kind::VOID)
        errormsg.Error(this->pos, "procedure returns value");
      else
        errormsg.Error(this->pos, "function return value doesn't match");
    }
  }
}

void VarDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // variable declaration
  // type field is optional
  TY::Ty *exp_type = this->init->SemAnalyze(venv, tenv, labelcount);
  if(this->typ != nullptr) {
    // check type
    TY::Ty *desired_type = tenv->Look(this->typ);
    if(!(exp_type->IsSameType(desired_type))) {
      errormsg.Error(this->pos, "type mismatch");
      return;
    }
  } else if(exp_type->kind == TY::Ty::Kind::NIL) {
    errormsg.Error(this->pos, "init should not be nil without type specified");
    return;
  }
  // TODO just insert it for gods sake
  venv->Enter(this->var, new E::VarEntry(exp_type));
}

void TypeDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // deal with recursive definition, set type to null first
  // and find dup type names, of course
  std::set<S::Symbol *> typenames;
  for(auto decs = this->types; decs; decs = decs->tail) {
    auto dec = decs->head;
    if(typenames.find(dec->name) != typenames.end()) {
      errormsg.Error(this->pos, "two types have the same name");
      return;
    }
    typenames.insert(dec->name);
    TY::Ty *t = new TY::NameTy(dec->name, nullptr);
    // TODO make this more elegant
    tenv->Enter(dec->name, t);
  }
  // do the actual parsing
  for(auto decs = this->types; decs; decs = decs->tail) {
    auto dec = decs->head;
    auto new_ty = dec->ty->SemAnalyze(tenv);
    TY::NameTy *old_ty = static_cast<TY::NameTy *>(tenv->Look(dec->name));
    if(old_ty->kind != TY::Ty::Kind::NAME)
      errormsg.Error(this->pos, "inconsistency, check the semantic checking routine.");
    old_ty->ty = new_ty;
  }
  // check illegal type cycles
  for(auto decs = this->types; decs; decs = decs->tail) {
    auto dec = decs->head;
    TY::NameTy *orig = static_cast<TY::NameTy *>(tenv->Look(dec->name));
    TY::NameTy *cur = static_cast<TY::NameTy *>(orig->ty);
    while(cur->kind == TY::Ty::Kind::NAME && cur != orig)
      cur = static_cast<TY::NameTy *>(cur->ty);
    if(cur == orig) {
      errormsg.Error(this->pos, "illegal type cycle");
      break;
    }
  }
  // assign real values
  for(auto decs = this->types; decs; decs = decs->tail)
    tenv->Set(decs->head->name,
      (static_cast<TY::NameTy *>(tenv->Look(decs->head->name)))->ty);
}

TY::Ty *NameTy::SemAnalyze(TEnvType tenv) const {
  TY::Ty *type = tenv->Look(this->name);
  if(type == nullptr) {
    errormsg.Error(this->pos, "Invalid type definition, cannot find name.");
    type = TY::VoidTy::Instance();
  } 
  // else if(type->kind == TY::Ty::Kind::NAME
  //   && (static_cast<TY::NameTy *>(type))->ty == nullptr) {
  //   // recursive definition
  //   errormsg.Error(this->pos, "Invalid type definition: recursive definition.");
  //   type = TY::VoidTy::Instance();   
  // }
  return new TY::NameTy(this->name, type);
}

TY::Ty *RecordTy::SemAnalyze(TEnvType tenv) const {
  // deal with recursive declaration here
  auto field_list = make_fieldlist(tenv, this->record);
  
  auto field_entry = field_list;
  auto record_entry = this->record;
  while(field_entry && record_entry) {
    auto field = field_entry->head;
    auto record = record_entry->head;
    if(field->ty == nullptr)
      errormsg.Error(this->pos, "undefined type %s", record->typ->Name().c_str());
    else if(field->ty->kind == TY::Ty::Kind::NAME
      && (static_cast<TY::NameTy *>(field_entry->head->ty))->sym == nullptr) {
      errormsg.Error(this->pos, "Invalid type definition: cyclic definition.");
      return TY::VoidTy::Instance();
    }
    field_entry = field_entry->tail;
    record_entry = record_entry->tail;
  }
  return new TY::RecordTy(field_list);
}

TY::Ty *ArrayTy::SemAnalyze(TEnvType tenv) const {
  // look up the symbol first
  TY::Ty *type = tenv->Look(this->array);
  if(type == nullptr) {
    errormsg.Error(this->pos, "Invalid array, cannot find %s.", this->array);
    return TY::VoidTy::Instance();
  }
  return new TY::ArrayTy(type);
}

}  // namespace A

namespace SEM {
void SemAnalyze(A::Exp *root) {
  if (root) root->SemAnalyze(E::BaseVEnv(), E::BaseTEnv(), 0);
}

}  // namespace SEM
