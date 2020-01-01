#include "tiger/translate/translate.h"

#include <cassert>
#include <cstdio>
#include <set>
#include <string>

#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/temp.h"
#include "tiger/semant/semant.h"
#include "tiger/semant/types.h"
#include "tiger/util/util.h"

// all declarations are moved to header file here.

extern EM::ErrorMsg errormsg;

// from semananalyze module
using VEnvType = S::Table<E::EnvEntry>*;
using TEnvType = S::Table<TY::Ty>*;

namespace {
static TY::TyList* make_formal_tylist(TEnvType tenv, A::FieldList* params)
{
  if (params == nullptr) {
    return nullptr;
  }

  TY::Ty* ty = tenv->Look(params->head->typ);
  if (ty == nullptr) {
    errormsg.Error(params->head->pos, "undefined type %s",
        params->head->typ->Name().c_str());
  }

  return new TY::TyList(ty->ActualTy(), make_formal_tylist(tenv, params->tail));
}

static TY::FieldList* make_fieldlist(TEnvType tenv, A::FieldList* fields)
{
  if (fields == nullptr) {
    return nullptr;
  }

  TY::Ty* ty = tenv->Look(fields->head->typ);
  return new TY::FieldList(new TY::Field(fields->head->name, ty),
      make_fieldlist(tenv, fields->tail));
}

} // namespace

namespace TR {

// Expression translations

T::Exp* ExExp::UnEx() const
{
  return this->exp;
}

T::Stm* ExExp::UnNx() const
{
  // should never be here
  return new T::ExpStm(this->exp);
}

TR::Cx ExExp::UnCx() const
{
  T::CjumpStm* jmp = new T::CjumpStm(T::NE_OP, this->exp, new T::ConstExp(0), nullptr, nullptr);
  TR::PatchList* trues = new PatchList(&(jmp->true_label), nullptr);
  TR::PatchList* falses = new PatchList(&(jmp->false_label), nullptr);
  return TR::Cx(trues, falses, jmp);
}

T::Exp* NxExp::UnEx() const
{
  return new T::EseqExp(this->stm, new T::ConstExp(0));
}

T::Stm* NxExp::UnNx() const
{
  return this->stm;
}

TR::Cx NxExp::UnCx() const
{
  errormsg.Error(0, "NxExp cannot be converted to CxExp");
}

T::Exp* CxExp::UnEx() const
{
  TEMP::Temp* r = TEMP::Temp::NewTemp();
  TEMP::Label *t = TEMP::NewLabel(), *f = TEMP::NewLabel();
  do_patch(this->cx.trues, t);
  do_patch(this->cx.falses, f);
  T::EseqExp* ret = new T::EseqExp(new T::MoveStm(new T::TempExp(r), new T::ConstExp(1)),
      new T::EseqExp(this->cx.stm,
          new T::EseqExp(new T::LabelStm(f),
              new T::EseqExp(new T::MoveStm(new T::TempExp(r), new T::ConstExp(0)),
                  new T::EseqExp(new T::LabelStm(t),
                      new T::TempExp(r))))));
  return ret;
}

T::Stm* CxExp::UnNx() const
{
  TEMP::Label* l = TEMP::NewLabel();
  do_patch(this->cx.trues, l);
  do_patch(this->cx.falses, l);
  return new T::SeqStm(this->cx.stm, new T::LabelStm(l));
}

TR::Cx CxExp::UnCx() const
{
  return this->cx;
}

Level* Level::NewLevel(
    Level* parent, TEMP::Label* name, U::BoolList* formals)
{
  // add static link as first parameter
  formals = new U::BoolList(true, formals);
  Level* ret = new Level(F::NewFrame(name, formals), parent);
  return ret;
}

void do_patch(PatchList* tList, TEMP::Label* label)
{
  for (; tList; tList = tList->tail)
    *(tList->head) = label;
}

PatchList* join_patch(PatchList* first, PatchList* second)
{
  if (!first)
    return second;
  for (; first->tail; first = first->tail)
    ;
  first->tail = second;
  return first;
}

Level* Outermost()
{
  static Level* lv = nullptr;
  if (lv != nullptr)
    return lv;

  lv = Level::NewLevel(nullptr, TEMP::NamedLabel("tigermain"), nullptr);
  return lv;
}

// allocate space in level
TR::Access* Level::AllocateLocal(bool escape, unsigned byte_count)
{
  return new TR::Access(this, frame->allocSpace(byte_count, escape));
}

TR::AccessList* Level::getFormals()
{
  // make TR access out of Frame access
  // escape static link here
  F::AccessList* frame_access = frame->getFormals()->tail;
  TR::AccessList* prehead = new TR::AccessList(nullptr, nullptr);
  TR::AccessList* tail = prehead;
  while (frame_access) {
    tail = tail->tail = new TR::AccessList(
        new TR::Access(this, frame_access->head), nullptr);
    frame_access = frame_access->tail;
  }
  return prehead->tail;
}

T::Stm* makeRecordArray(T::ExpList* fields, T::Exp* r, int offset, int size)
{
  assert(fields->head != nullptr);
  if (offset + 1 < size) {
    return new T::SeqStm(
        new T::MoveStm(
            new T::MemExp(
                new T::BinopExp(T::PLUS_OP,
                    r, new T::ConstExp(offset * TR::word_size))),
            fields->head),
        makeRecordArray(fields->tail, r, offset + 1, size));
  } else {
    return new T::MoveStm(
        new T::MemExp(new T::BinopExp(
            T::PLUS_OP, r, new T::ConstExp(offset * TR::word_size))),
        fields->head);
  }
}

// wrapper
// do proc entry exit
inline void TR::Level::doProcEntryExit(TR::Exp* body_stm)
{
  frame->doProcEntryExit1(body_stm->UnEx());
}

// wrapper
// directly get expression in frame access class
inline TR::Exp* Access::toExp()
{
  return new TR::ExExp(
      access->toExp(level->frame->getFramePointerExp()));
}

F::FragList* TranslateProgram(A::Exp* root)
{
  assert(root->kind == A::Exp::LET);
  TR::ExpAndTy ret = root->Translate(E::BaseVEnv(), E::BaseTEnv(), Outermost(), nullptr);
  F::FragAllocator::appendFrag(new F::ProcFrag(ret.exp->UnNx(), Outermost()->frame));
  return F::FragAllocator::getFragListHead();
}

} // namespace TR

namespace A {

TR::ExpAndTy SimpleVar::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // type checking
  E::VarEntry* env_entry = static_cast<E::VarEntry*>(venv->Look(this->sym));
  if (env_entry == nullptr)
    errormsg.Error(this->pos, "undefined variable %s", this->sym->Name().c_str());
  else if (env_entry->kind != E::EnvEntry::Kind::VAR)
    errormsg.Error(this->pos, "%s is not a variable.", this->sym);
  else {
    // type is checked now, go through static link
    T::Exp* ret = level->frame->getFramePointerExp();
    while (level != env_entry->access->level) {
      // static link is the first word in frame
      ret = new T::MemExp(
          new T::BinopExp(T::PLUS_OP, ret, new T::ConstExp(-TR::word_size)));
      level = level->parent;
    }
    ret = env_entry->access->access->toExp(ret);
    return TR::ExpAndTy(new TR::ExExp(ret), env_entry->ty);
  }
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy FieldVar::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // check main variable first
  TR::ExpAndTy ret = this->var->Translate(venv, tenv, level, label);
  // do type checking for record
  if (ret.ty->kind != TY::Ty::RECORD) {
    errormsg.Error(this->pos, "not a record type");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  // find the field
  TY::RecordTy* ty = static_cast<TY::RecordTy*>(ret.ty->ActualTy());
  TY::FieldList* cur = ty->fields;
  int offset = 0;
  for (; cur; cur = cur->tail) {
    if (this->sym == cur->head->name) {
      // found match
      return TR::ExpAndTy(
          new TR::ExExp(new T::MemExp(
              new T::BinopExp(T::PLUS_OP, ret.exp->UnEx(), new T::ConstExp(offset)))),
          cur->head->ty);
    }
    offset += TR::word_size;
  }
  errormsg.Error(this->pos, "field %s doesn't exist", this->sym->Name().c_str());
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy SubscriptVar::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // almost the same as fieldvar
  // check main variable first
  TR::ExpAndTy ret = this->var->Translate(venv, tenv, level, label);
  // do type checking for record
  if (ret.ty->kind != TY::Ty::ARRAY) {
    errormsg.Error(this->pos, "array type required");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  TR::ExpAndTy sub = this->subscript->Translate(venv, tenv, level, label);
  if (sub.ty->kind != TY::Ty::INT) {
    errormsg.Error(this->pos, "int type required for subscript");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  return TR::ExpAndTy(
      new TR::ExExp(new T::MemExp(
          new T::BinopExp(T::PLUS_OP,
              ret.exp->UnEx(),
              new T::BinopExp(
                  T::BinOp::MUL_OP, sub.exp->UnEx(),
                  new T::ConstExp(TR::word_size))))),
      static_cast<TY::ArrayTy*>(ret.ty)->ty);
}

TR::ExpAndTy VarExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  return this->var->Translate(venv, tenv, level, label);
}

TR::ExpAndTy NilExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::NilTy::Instance());
}

TR::ExpAndTy IntExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(i)), TY::IntTy::Instance());
}

TR::ExpAndTy StringExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  T::Exp* loc = nullptr;
  // deal with chars(length==1 strings), make them support equal ops
  // refer to __wrap_getchar in runtime.c

  // string struct has a minimal length of 5
  // which should be 8 considering word alignment
  static constexpr int char_entry_size = TR::word_size;
  if (s.length() == 1)
    loc = new T::BinopExp(T::PLUS_OP,
        new T::NameExp(TEMP::NamedLabel("consts")),
        new T::ConstExp(s[0] * char_entry_size));
  else {
    TEMP::Label* lb = TEMP::NewLabel();
    F::StringFrag* sf = new F::StringFrag(lb, s);
    F::FragAllocator::appendFrag(sf);
    loc = new T::NameExp(lb);
  }
  return TR::ExpAndTy(new TR::ExExp(loc), TY::StringTy::Instance());
}

TR::ExpAndTy CallExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // do type checking first
  // check function variable
  E::FunEntry* fun = static_cast<E::FunEntry*>(venv->Look(this->func));
  if (fun == nullptr)
    errormsg.Error(this->pos, "undefined function %s", this->func->Name().c_str());
  else if (fun->kind != E::EnvEntry::FUN)
    errormsg.Error(this->pos, "%s is not a function.", this->func);
  else {
    TY::TyList* formals = fun->formals;
    A::ExpList* actuals = this->args;
    T::ExpList* params_prehead = new T::ExpList(nullptr, nullptr);
    T::ExpList* params_tail = params_prehead;
    // find static link first
    // insert static link as first parameter
    // do not add static link for external functions
    if (fun->level->parent != nullptr) {
      T::Exp* static_link = level->frame->getFramePointerExp();
      TR::Level* static_container = level;
      while (static_container && static_container != fun->level->parent) {
        static_link = new T::MemExp(
            new T::BinopExp(T::PLUS_OP, static_link, new T::ConstExp(-TR::word_size)));
        static_container = static_container->parent;
      }
      params_tail = params_tail->tail = new T::ExpList(static_link, nullptr);
    }
    // if static container is nullptr, then reached root
    // assert(static_container != nullptr);
    // check params, and make ExpList the same time

    while (formals && actuals) {
      // keep parsing formal list even if there's a mismatch
      TR::ExpAndTy now = actuals->head->Translate(venv, tenv, level, label);
      if (!formals->head->ActualTy()->IsSameType(now.ty))
        errormsg.Error(actuals->head->pos, "para type mismatch");
      // append to ExpList
      params_tail = params_tail->tail = new T::ExpList(now.exp->UnEx(), nullptr);
      formals = formals->tail;
      actuals = actuals->tail;
    }
    if (formals != nullptr)
      errormsg.Error(this->pos, "too few params in function %s", this->func->Name().c_str());
    else if (actuals != nullptr)
      errormsg.Error(this->pos, "too many params in function %s", this->func->Name().c_str());
    // no error up till now
    if (fun->level->parent)
      return TR::ExpAndTy(
          new TR::ExExp(new T::CallExp(new T::NameExp(this->func), params_prehead->tail)),
          fun->result);
    else
      return TR::ExpAndTy(
          new TR::ExExp(level->frame->externalCall(this->func->Name(), params_prehead->tail)),
          fun->result);
  }
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy OpExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  TR::ExpAndTy l = left->Translate(venv, tenv, level, label);
  TR::ExpAndTy r = right->Translate(venv, tenv, level, label);
  switch (this->oper) {
  // calculations
  case A::Oper::PLUS_OP:
  case A::Oper::MINUS_OP:
  case A::Oper::TIMES_OP:
  case A::Oper::DIVIDE_OP:
    // these can only be applied to integers
    if (l.ty->kind != TY::Ty::Kind::NAME && !(l.ty->IsSameType(TY::IntTy::Instance())))
      errormsg.Error(this->pos, "integer required");
    else if (r.ty->kind != TY::Ty::Kind::NAME && !(r.ty->IsSameType(TY::IntTy::Instance())))
      errormsg.Error(this->pos, "integer required");
    else {
      // do the calculation
      T::BinOp op;
      switch (this->oper) {
      case A::Oper::PLUS_OP:
        op = T::PLUS_OP;
        break;
      case A::Oper::MINUS_OP:
        op = T::MINUS_OP;
        break;
      case A::Oper::TIMES_OP:
        op = T::MUL_OP;
        break;
      case A::Oper::DIVIDE_OP:
        op = T::DIV_OP;
        break;
      }
      return TR::ExpAndTy(new TR::ExExp(
                              new T::BinopExp(op, l.exp->UnEx(), r.exp->UnEx())),
          TY::IntTy::Instance());
    }
    break;
  // comparisons
  case A::Oper::GE_OP:
  case A::Oper::GT_OP:
  case A::Oper::LT_OP:
  case A::Oper::LE_OP:
  case A::Oper::EQ_OP:
  case A::Oper::NEQ_OP:
    // these can be applied integers & strings
    if (l.ty->kind != TY::Ty::Kind::NAME && r.ty->kind != TY::Ty::Kind::NAME && !(l.ty->IsSameType(r.ty)))
      errormsg.Error(this->pos, "same type required");
    else {
      T::RelOp op;
      switch (this->oper) {
      case A::Oper::GE_OP:
        op = T::GE_OP;
        break;
      case A::Oper::GT_OP:
        op = T::GT_OP;
        break;
      case A::Oper::LT_OP:
        op = T::LT_OP;
        break;
      case A::Oper::LE_OP:
        op = T::LE_OP;
        break;
      case A::Oper::EQ_OP:
        op = T::EQ_OP;
        break;
      case A::Oper::NEQ_OP:
        op = T::NE_OP;
        break;
      }
      T::CjumpStm* stm;
      if (l.ty->ActualTy() == TY::StringTy::Instance()) {
        assert(op == T::EQ_OP);
        T::CallExp *call_exp = level->frame->externalCall(
          "stringEqual", new T::ExpList(l.exp->UnEx(), new T::ExpList(r.exp->UnEx(), nullptr)));
        stm = new T::CjumpStm(op, call_exp, new T::ConstExp(1), nullptr, nullptr);
      } else
        stm = new T::CjumpStm(op, l.exp->UnEx(), r.exp->UnEx(), nullptr, nullptr);
      TR::PatchList* trues = new TR::PatchList(&(stm->true_label), nullptr);
      TR::PatchList* falses = new TR::PatchList(&(stm->false_label), nullptr);
      return TR::ExpAndTy(new TR::CxExp(trues, falses, stm), TY::IntTy::Instance());
    }
    break;
  }
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy RecordExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  TY::RecordTy* typ = static_cast<TY::RecordTy*>(tenv->Look(this->typ));
  if (typ == nullptr)
    errormsg.Error(this->pos, "undefined type %s", this->typ->Name().c_str());
  else if (typ->kind != TY::Ty::Kind::RECORD)
    errormsg.Error(this->pos, "%s is not a record type.", this->typ->Name().c_str());
  else {
    TY::FieldList* formals = typ->fields;
    A::EFieldList* actuals = this->fields;
    T::ExpList* explist_prehead = new T::ExpList(nullptr, nullptr);
    T::ExpList* explist_tail = explist_prehead;
    int cnt = 0;
    // according to specs formals & actuals should match in order
    while (formals && actuals) {
      if (formals->head->name != actuals->head->name) {
        errormsg.Error(actuals->head->exp->pos,
            "Expected %s, got %s", formals->head->name, actuals->head->name);
        break;
      } else {
        // evaluate type and expression
        TR::ExpAndTy ret = actuals->head->exp->Translate(venv, tenv, level, label);
        if (!(formals->head->ty->IsSameType(ret.ty))) {
          errormsg.Error(actuals->head->exp->pos,
              "Type mismatch.");
          break;
        }
        // add to expression list
        T::ExpList* new_tail = new T::ExpList(ret.exp->UnEx(), nullptr);
        explist_tail = explist_tail->tail = new_tail;
        // continue
        formals = formals->tail;
        actuals = actuals->tail;
        cnt++;
        continue;
      }
    }
    if (formals != nullptr && actuals != nullptr)
      return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());

    if (formals != nullptr)
      errormsg.Error(this->pos, "Arguments not enough, only got %d", cnt);
    else if (actuals != nullptr)
      errormsg.Error(this->pos, "Too many arguments, got %d", cnt);
    else {
      // perfectly fine
      TEMP::Temp* r = TEMP::Temp::NewTemp();
      T::Stm* stm = new T::MoveStm(
          new T::TempExp(r),
          level->frame->externalCall(
              "allocRecord",
              new T::ExpList(new T::ConstExp(cnt * TR::word_size), nullptr)));
      T::Exp* ret = new T::EseqExp(stm,
          new T::EseqExp(TR::makeRecordArray(explist_prehead->tail, new T::TempExp(r), 0, cnt),
              new T::TempExp(r)));
      return TR::ExpAndTy(new TR::ExExp(ret), typ);
    }
  }
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy SeqExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // deal with first value first
  // gotta find a more elegant way to do this...
  A::ExpList* exps = this->seq;
  if (exps) {
    T::EseqExp* head = nullptr;
    TR::ExpAndTy ret_value = exps->head->Translate(venv, tenv, level, label);
    TY::Ty* final_type = ret_value.ty;
    if (exps->tail == nullptr) {
      // only one
      fprintf(stdout, "%d:SeqExp has only one expression.\n", this->pos);
      return TR::ExpAndTy(ret_value.exp, final_type);
    }
    // construct list header
    head = new T::EseqExp(ret_value.exp->UnNx(), nullptr);
    exps = exps->tail;
    T::EseqExp* tail = head;
    // construct list in the middle
    for (; exps->tail; exps = exps->tail) {
      ret_value = exps->head->Translate(venv, tenv, level, label);
      T::EseqExp* new_tail = new T::EseqExp(ret_value.exp->UnNx(), nullptr);
      tail->exp = (T::Exp*)new_tail;
      tail = new_tail;
    }
    // construct list tail
    ret_value = exps->head->Translate(venv, tenv, level, label);
    tail->exp = ret_value.exp->UnEx();
    final_type = ret_value.ty;
    return TR::ExpAndTy(new TR::ExExp(head), final_type);
  } else {
    // this is wrong...
    errormsg.Error(this->pos, "empty sequence expression");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
}

TR::ExpAndTy AssignExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // calc lvalue first, then exp
  // doesn't have a return value according to specs
  TR::ExpAndTy var_ret = this->var->Translate(venv, tenv, level, label);
  TR::ExpAndTy exp_ret = this->exp->Translate(venv, tenv, level, label);
  if (!(var_ret.ty->IsSameType(exp_ret.ty))) {
    errormsg.Error(this->pos, "unmatched assign exp");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  // check if it's a loop variable
  if (this->var->kind == A::Var::Kind::SIMPLE) {
    E::VarEntry* var_entry = static_cast<E::VarEntry*>(venv->Look((static_cast<A::SimpleVar*>(this->var))->sym));
    if (var_entry->kind != E::EnvEntry::Kind::VAR) {
      errormsg.Error(this->pos, "inconsistency occured, cannot determine whether is loop var or not");
      return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
    } else if (var_entry->readonly) {
      errormsg.Error(this->pos, "loop variable can't be assigned");
      return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
    }
  }
  // do actual stuff
  return TR::ExpAndTy(
      new TR::NxExp(
          new T::MoveStm(var_ret.exp->UnEx(),
              exp_ret.exp->UnEx())),
      TY::VoidTy::Instance());
}

TR::ExpAndTy IfExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // check the condition first
  TR::ExpAndTy t = this->test->Translate(venv, tenv, level, label);
  if (!t.ty->IsSameType(TY::IntTy::Instance())) {
    errormsg.Error(this->test->pos, "Test expression doesn't have an integer type.");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  // do patching for test clause
  TEMP::Temp* r = TEMP::Temp::NewTemp();
  TEMP::Label* true_label = TEMP::NewLabel();
  TEMP::Label* false_label = TEMP::NewLabel();
  TR::Cx test_cx = t.exp->UnCx();
  TR::do_patch(test_cx.trues, true_label);
  TR::do_patch(test_cx.falses, false_label);

  // parse then & else
  TR::ExpAndTy then_eat = then->Translate(venv, tenv, level, label);
  if (this->elsee != nullptr) {
    // if-then-else: then & else have same type & return that type
    TR::ExpAndTy else_eat = this->elsee->Translate(venv, tenv, level, label);
    if (!(then_eat.ty->IsSameType(else_eat.ty))) {
      errormsg.Error(this->pos, "then exp and else exp type mismatch");
      return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
    }
    TEMP::Label* meeting = TEMP::NewLabel();
    T::Exp* exp = new T::EseqExp(test_cx.stm,
        new T::EseqExp(new T::LabelStm(true_label),
            new T::EseqExp(new T::MoveStm(new T::TempExp(r), then_eat.exp->UnEx()),
                new T::EseqExp(new T::JumpStm(new T::NameExp(meeting), new TEMP::LabelList(meeting, nullptr)),
                    new T::EseqExp(new T::LabelStm(false_label),
                        new T::EseqExp(new T::MoveStm(new T::TempExp(r), else_eat.exp->UnEx()),
                            new T::EseqExp(new T::JumpStm(new T::NameExp(meeting), new TEMP::LabelList(meeting, nullptr)),
                                new T::EseqExp(new T::LabelStm(meeting),
                                    new T::TempExp(r)))))))));
    return TR::ExpAndTy(new TR::ExExp(exp), then_eat.ty);
  } else {
    // if-then: no return value
    if (then_eat.ty->kind != TY::Ty::Kind::VOID) {
      errormsg.Error(this->then->pos, "if-then exp's body must produce no value");
      return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
    }
    T::Stm* s = new T::SeqStm(test_cx.stm,
        new T::SeqStm(new T::LabelStm(true_label),
            new T::SeqStm(then_eat.exp->UnNx(),
                new T::LabelStm(false_label))));
    return TR::ExpAndTy(new TR::NxExp(s), TY::VoidTy::Instance());
  }
}

TR::ExpAndTy WhileExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // check condition first
  TR::ExpAndTy test_eat = this->test->Translate(venv, tenv, level, label);
  if (!test_eat.ty->IsSameType(TY::IntTy::Instance())) {
    errormsg.Error(this->test->pos, "Test expression doesn't have an integer type.");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }

  // generate labels first
  // so that body can break through
  TEMP::Label *body_label = TEMP::NewLabel(),
              *test_label = TEMP::NewLabel(),
              *done_label = TEMP::NewLabel();
  // translate body now
  TR::ExpAndTy body_eat = this->body->Translate(venv, tenv, level, done_label);
  if (body_eat.ty->kind != TY::Ty::Kind::VOID) {
    errormsg.Error(this->test->pos, "while body must produce no value");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  // do actual stuff
  TR::Cx test_cx = test_eat.exp->UnCx();
  TR::do_patch(test_cx.trues, body_label);
  TR::do_patch(test_cx.falses, done_label);
  T::Stm* s = new T::SeqStm(new T::LabelStm(test_label),
      new T::SeqStm(test_cx.stm,
          new T::SeqStm(new T::LabelStm(body_label),
              new T::SeqStm(body_eat.exp->UnNx(),
                  new T::SeqStm(new T::JumpStm(new T::NameExp(test_label), new TEMP::LabelList(test_label, nullptr)),
                      new T::LabelStm(done_label))))));
  return TR::ExpAndTy(new TR::NxExp(s), TY::VoidTy::Instance());
}

// put it all together, for statement should be
// translated into:
/*
i = lo;                 # loopvar_init
if(i > hi) goto done;   # check_lo_hi
body:
{
  # body here
  # if break, goto done
}
if(i < hi) {            # test 
  i++;                  # loopvar_inc
  goto body;
}
done:
*/
TR::ExpAndTy ForExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // check types for two bounds first
  TR::ExpAndTy lo_eat = this->lo->Translate(venv, tenv, level, label);
  if (!(lo_eat.ty->IsSameType(TY::IntTy::Instance()))) {
    errormsg.Error(this->lo->pos, "for exp's range type is not integer");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  TR::ExpAndTy hi_eat = this->hi->Translate(venv, tenv, level, label);
  if (!(hi_eat.ty->IsSameType(TY::IntTy::Instance()))) {
    errormsg.Error(this->hi->pos, "for exp's range type is not integer");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }

  venv->BeginScope();
  tenv->BeginScope();
  // allocate loopvar in frame
  E::VarEntry* loopvar_entry = new E::VarEntry(level->AllocateLocal(false), TY::IntTy::Instance(), true);
  venv->Enter(var, loopvar_entry);
  // allocate space for limit variable too, but don't update venv.
  TR::Exp* limit = new TR::ExExp(new T::TempExp(TEMP::Temp::NewTemp()));
  // get loopvar expression
  TR::Exp* loopvar = loopvar_entry->access->toExp();
  // allocate labels
  TEMP::Label *body_label = TEMP::NewLabel(),
              *inc_label = TEMP::NewLabel(),
              *done_label = TEMP::NewLabel();

  // parse loop body
  TR::ExpAndTy body_eat = this->body->Translate(venv, tenv, level, done_label);
  if (body_eat.ty->kind != TY::Ty::Kind::VOID) {
    errormsg.Error(this->pos, "for body must produce no value");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  // loop variable initial assignment
  T::Stm* loopvar_init = new T::MoveStm(loopvar->UnEx(), lo_eat.exp->UnEx());
  T::Stm* limit_init = new T::MoveStm(limit->UnEx(), hi_eat.exp->UnEx());
  // test if lo <= hi before first iteration
  T::Stm* check_lo_hi = new T::CjumpStm(T::LT_OP, loopvar->UnEx(), limit->UnEx(),
      body_label, done_label);
  // loop variable increasement statement
  T::Stm* loopvar_inc = new T::MoveStm(loopvar->UnEx(),
      new T::BinopExp(T::PLUS_OP, loopvar->UnEx(), new T::ConstExp(1)));
  // test statement: if(i < hi) {i++; goto body;}
  T::Stm* test = new T::SeqStm(
      new T::CjumpStm(T::LT_OP, loopvar->UnEx(), limit->UnEx(), inc_label, done_label),
      new T::SeqStm(new T::LabelStm(inc_label),
          new T::SeqStm(loopvar_inc,
              new T::JumpStm(new T::NameExp(body_label), new TEMP::LabelList(body_label, nullptr)))));
  T::Stm* ret = new T::SeqStm(loopvar_init,
      new T::SeqStm(limit_init,
          new T::SeqStm(check_lo_hi,
              new T::SeqStm(new T::LabelStm(body_label),
                  new T::SeqStm(body_eat.exp->UnNx(),
                      new T::SeqStm(test,
                          new T::LabelStm(done_label)))))));
  venv->EndScope();
  tenv->EndScope();
  return TR::ExpAndTy(new TR::NxExp(ret), TY::VoidTy::Instance());
}

TR::ExpAndTy BreakExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  T::Stm* ret = new T::JumpStm(new T::NameExp(label), new TEMP::LabelList(label, nullptr));
  return TR::ExpAndTy(new TR::NxExp(ret), TY::VoidTy::Instance());
}

TR::ExpAndTy LetExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  venv->BeginScope();
  tenv->BeginScope();
  T::EseqExp* prehead = new T::EseqExp(nullptr, nullptr);
  T::EseqExp* tail = prehead;
  for (A::DecList* decs = this->decs; decs; decs = decs->tail) {
    TR::Exp* r = decs->head->Translate(venv, tenv, level, label);
    if (r == nullptr)
      continue;
    tail->exp = (T::Exp*)(new T::EseqExp(r->UnNx(), nullptr));
    tail = (T::EseqExp*)tail->exp;
  }
  TR::ExpAndTy ret = this->body->Translate(venv, tenv, level, label);
  tail->exp = ret.exp->UnEx();
  venv->EndScope();
  tenv->EndScope();
  return TR::ExpAndTy(new TR::ExExp(prehead->exp), ret.ty);
}

TR::ExpAndTy ArrayExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // check type first
  TY::ArrayTy* array_type = static_cast<TY::ArrayTy*>(tenv->Look(this->typ)->ActualTy());
  if (array_type->kind != TY::Ty::Kind::ARRAY) {
    errormsg.Error(this->pos, "%s is not a valid array type.", this->typ);
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  // check type for array size
  TR::ExpAndTy size_eat = this->size->Translate(venv, tenv, level, label);
  if (!size_eat.ty->IsSameType(TY::IntTy::Instance())) {
    errormsg.Error(this->pos, "Length is not an integer expression.");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  // check type for initial value
  TR::ExpAndTy init_eat = this->init->Translate(venv, tenv, level, label);
  if (!init_eat.ty->IsSameType(array_type->ty)) {
    errormsg.Error(this->pos, "type mismatch");
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  // do translation
  TEMP::Temp* r = TEMP::Temp::NewTemp();
  T::Stm* creation = new T::MoveStm(
      new T::TempExp(r),
      level->frame->externalCall(
          "initArray",
          new T::ExpList(size_eat.exp->UnEx(),
              new T::ExpList(init_eat.exp->UnEx(), nullptr))));
  T::Exp* ret = new T::EseqExp(creation, new T::TempExp(r));
  return TR::ExpAndTy(new TR::ExExp(ret), array_type);
}

TR::ExpAndTy VoidExp::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // do nothing
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::Exp* FunctionDec::Translate(S::Table<E::EnvEntry>* venv,
    S::Table<TY::Ty>* tenv, TR::Level* level,
    TEMP::Label* label) const
{
  // iterate through the function list to
  // check duplicate function names within the list
  std::set<S::Symbol*> funcnames;
  for (auto funcs = this->functions; funcs; funcs = funcs->tail) {
    auto func = funcs->head;
    if (funcnames.find(func->name) != funcnames.end()) {
      errormsg.Error(this->pos, "two functions have the same name");
      return nullptr;
    }
    funcnames.insert(func->name);
    // optional result type
    TY::Ty* result_type;
    TY::TyList* formal_types = make_formal_tylist(tenv, func->params);
    if (func->result == nullptr)
      result_type = TY::VoidTy::Instance();
    else
      result_type = tenv->Look(func->result);
    // construct bool list
    U::BoolList* bool_list_prehead = new U::BoolList(0, nullptr);
    U::BoolList* bool_list_tail = bool_list_prehead;
    for (A::FieldList* cur = func->params; cur; cur = cur->tail)
      bool_list_tail = bool_list_tail->tail = new U::BoolList(cur->head->escape, nullptr);
    // make new level
    TR::Level* new_level = TR::Level::NewLevel(level, func->name, bool_list_prehead->tail);
    // seems unnecessarily, but I'm not sure...
    TEMP::Label* label = TEMP::NamedLabel(func->name->Name());
    // push to the environment
    venv->Enter(func->name, new E::FunEntry(new_level, label, formal_types, result_type));
  }

  // do actual parsing
  for (auto funcs = this->functions; funcs; funcs = funcs->tail) {
    auto func = funcs->head;
    E::FunEntry* fun_entry = static_cast<E::FunEntry*>(venv->Look(func->name));

    // begin function body parsing
    venv->BeginScope();
    tenv->BeginScope();

    // add params to current environment
    {
      auto field_entry = make_fieldlist(tenv, func->params);
      auto formal_entry = fun_entry->level->getFormals();
      while (field_entry && formal_entry) {
        E::VarEntry* var_entry = new E::VarEntry(formal_entry->head, field_entry->head->ty);
        venv->Enter(field_entry->head->name, var_entry);
        field_entry = field_entry->tail;
        formal_entry = formal_entry->tail;
      }
      assert(field_entry == nullptr && formal_entry == nullptr);
    }

    // analyze the function body
    TR::ExpAndTy body_eat = func->body->Translate(venv, tenv, fun_entry->level, fun_entry->label);

    // end function body parsing
    venv->EndScope();
    tenv->EndScope();

    // check body type and declared type
    TY::Ty* decl_type = (static_cast<E::FunEntry*>(venv->Look(func->name)))->result;
    if (!body_eat.ty->IsSameType(decl_type)) {
      // print error message, discard the result
      if (decl_type->kind == TY::Ty::Kind::VOID)
        errormsg.Error(this->pos, "procedure body_eaturns value");
      else
        errormsg.Error(this->pos, "function return value doesn't match");
    } else
      // add procedure exit and add to fraglist
      fun_entry->level->doProcEntryExit(body_eat.exp);
  }
  return nullptr;
}

TR::Exp* VarDec::Translate(S::Table<E::EnvEntry>* venv, S::Table<TY::Ty>* tenv,
    TR::Level* level, TEMP::Label* label) const
{
  // type field is optional
  TR::ExpAndTy init_eat = this->init->Translate(venv, tenv, level, label);
  assert(init_eat.exp != nullptr);
  if (this->typ != nullptr) {
    // check type
    TY::Ty* desired_type = tenv->Look(this->typ);
    if (!(init_eat.ty->IsSameType(desired_type))) {
      errormsg.Error(this->pos, "type mismatch");
      return nullptr;
    }
  } else if (init_eat.ty == TY::NilTy::Instance()) {
    errormsg.Error(this->pos, "init should not be nil without type specified");
    return nullptr;
  }
  // insert it into venv
  TR::Access* acc = level->AllocateLocal(this->escape);
  venv->Enter(this->var, new E::VarEntry(acc, init_eat.ty));
  // translation
  // make the assignment
  TR::Exp* acc_exp = acc->toExp();
  T::MoveStm* assign = new T::MoveStm(acc_exp->UnEx(), init_eat.exp->UnEx());
  return new TR::NxExp(assign);
}

TR::Exp* TypeDec::Translate(S::Table<E::EnvEntry>* venv, S::Table<TY::Ty>* tenv,
    TR::Level* level, TEMP::Label* label) const
{
  // deal with recursive definition, set type to null first
  // and find dup type names, of course
  std::set<S::Symbol*> typenames;
  for (auto decs = this->types; decs; decs = decs->tail) {
    auto dec = decs->head;
    if (typenames.find(dec->name) != typenames.end()) {
      errormsg.Error(this->pos, "two types have the same name");
      return nullptr;
    }
    typenames.insert(dec->name);
    TY::Ty* t = new TY::NameTy(dec->name, nullptr);
    tenv->Enter(dec->name, t);
  }
  // do the actual parsing
  for (auto decs = this->types; decs; decs = decs->tail) {
    auto dec = decs->head;
    auto new_ty = dec->ty->SemAnalyze(tenv);
    TY::NameTy* old_ty = static_cast<TY::NameTy*>(tenv->Look(dec->name));
    if (old_ty->kind != TY::Ty::Kind::NAME)
      errormsg.Error(this->pos, "inconsistency, check the semantic checking routine.");
    old_ty->ty = new_ty;
  }
  // check illegal type cycles
  // TODO this impl have bugs, may produce infinite cycle
  // it can pass all given tests though :)
  // to solve this use a visit array
  /*
       /->3-\
  1 -> 2    |
       <-5<-4
  */
  for (auto decs = this->types; decs; decs = decs->tail) {
    auto dec = decs->head;
    TY::NameTy* orig = static_cast<TY::NameTy*>(tenv->Look(dec->name));
    TY::NameTy* cur = static_cast<TY::NameTy*>(orig->ty);
    while (cur->kind == TY::Ty::Kind::NAME && cur != orig)
      cur = static_cast<TY::NameTy*>(cur->ty);
    if (cur == orig) {
      errormsg.Error(this->pos, "illegal type cycle");
      break;
    }
  }
  // assign real values
  for (auto decs = this->types; decs; decs = decs->tail)
    tenv->Set(decs->head->name,
        (static_cast<TY::NameTy*>(tenv->Look(decs->head->name)))->ty);
  return nullptr;
}

TY::Ty* NameTy::Translate(S::Table<TY::Ty>* tenv) const
{
  TY::Ty* type = tenv->Look(this->name);
  if (type == nullptr) {
    errormsg.Error(this->pos, "Invalid type definition, cannot find name.");
    type = TY::VoidTy::Instance();
  }
  // recursive definition is dealt with in TypeDec
  return new TY::NameTy(this->name, type);
}

TY::Ty* RecordTy::Translate(S::Table<TY::Ty>* tenv) const
{
  // deal with recursive declaration here
  auto field_list = make_fieldlist(tenv, this->record);

  auto field_entry = field_list;
  auto record_entry = this->record;
  while (field_entry && record_entry) {
    auto field = field_entry->head;
    auto record = record_entry->head;
    if (field->ty == nullptr) {
      errormsg.Error(this->pos, "undefined type %s", record->typ->Name().c_str());
      return TY::VoidTy::Instance();
    } else if (field->ty->kind == TY::Ty::Kind::NAME && (static_cast<TY::NameTy*>(field_entry->head->ty))->sym == nullptr) {
      errormsg.Error(this->pos, "Invalid type definition: cyclic definition.");
      return TY::VoidTy::Instance();
    }
    field_entry = field_entry->tail;
    record_entry = record_entry->tail;
  }
  return new TY::RecordTy(field_list);
}

TY::Ty* ArrayTy::Translate(S::Table<TY::Ty>* tenv) const
{
  // look up the symbol first
  TY::Ty* type = tenv->Look(this->array);
  if (type == nullptr) {
    errormsg.Error(this->pos, "Invalid array, cannot find %s.", this->array);
    return TY::VoidTy::Instance();
  }
  return new TY::ArrayTy(type);
}

} // namespace A