#include "straightline/slp.h"
#include <iostream>

namespace A {
int CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return max(stm1->MaxArgs(), stm2->MaxArgs());
}

Table *CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  return stm2->Interp(stm1->Interp(t));
}

int AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exp->MaxArgs();
}

Table *AssignStm::Interp(Table *t) const {
  IntAndTable it = exp->Interp(t);
  return it.t->Update(id, it.i);
}

int PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exps->MaxArgs();
}

Table *PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  Table *ret = exps->Interp(t, true);
  std::cout << std::endl;
  return ret;
}

int IdExp::MaxArgs() const { return 0; }

IntAndTable IdExp::Interp(Table *t) const { return IntAndTable(t->Lookup(id), t); }

int NumExp::MaxArgs() const { return 0; }

IntAndTable NumExp::Interp(Table *t) const { return IntAndTable(num, t); }

int OpExp::MaxArgs() const { return max(left->MaxArgs(), right->MaxArgs()); }

IntAndTable OpExp::Interp(Table *t) const {
  int lhs, rhs;
  IntAndTable it = left->Interp(t);
  lhs = it.i;
  it = right->Interp(it.t);
  rhs = it.i;
  switch (oper)
  {
  case PLUS:  return IntAndTable(lhs + rhs, it.t);
  case MINUS: return IntAndTable(lhs - rhs, it.t);
  case TIMES: return IntAndTable(lhs * rhs, it.t);
  case DIV:   return IntAndTable(lhs / rhs, it.t);
  default:
    assert(false);  // should never reach here
  }
}

int EseqExp::MaxArgs() const {
  return max(stm->MaxArgs(), exp->MaxArgs());
}

IntAndTable EseqExp::Interp(Table *t) const {
  t = stm->Interp(t);
  return exp->Interp(t);
}


int PairExpList::MaxArgs() const{
  return max(NumExps(), max(head->MaxArgs(), tail->MaxArgs()));
}

int PairExpList::NumExps() const{
  return 1 + tail->NumExps();
}

Table *PairExpList::Interp(Table *t, bool ifPrint) const {
  IntAndTable it = head->Interp(t);
  if(ifPrint)
    std::cout << it.i << ' ';
  return tail->Interp(it.t, ifPrint);
}

int LastExpList::MaxArgs() const{
  return last->MaxArgs();
}

int LastExpList::NumExps() const{
  return 1;
}

Table *LastExpList::Interp(Table *t, bool ifPrint) const {
  IntAndTable it = last->Interp(t);
  if(ifPrint) std::cout << it.i;
  return it.t;
}

int Table::Lookup(std::string key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(std::string key, int value) const {
  return new Table(key, value, this);
}
}  // namespace A
