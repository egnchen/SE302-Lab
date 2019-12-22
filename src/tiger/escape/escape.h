#ifndef TIGER_ESCAPE_ESCAPE_H_
#define TIGER_ESCAPE_ESCAPE_H_

#include "tiger/absyn/absyn.h"

namespace ESC {

class EscapeEntry {
 public:
  int depth;
  bool* escape;

  EscapeEntry(int depth, bool* escape) : depth(depth), escape(escape) {}
};

using EEnv = S::Table<ESC::EscapeEntry>;

void FindEscape(A::Exp* exp);

void traverseExp(EEnv *env, int depth, A::Exp *e);
void traverseDec(EEnv *env, int depth, A::Dec *d);
void traverseVar(EEnv *env, int depth, A::Var *v);

}  // namespace ESC

#endif