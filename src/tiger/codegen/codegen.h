#ifndef TIGER_CODEGEN_CODEGEN_H_
#define TIGER_CODEGEN_CODEGEN_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/translate/tree.h"

// pre decl
namespace F {
class Frame;
}

namespace AS {
class InstrList;
class Instr;
}

namespace T {
class StmList;
}

namespace CG {

class ASManager
{
private:
  AS::InstrList *prehead;
  AS::InstrList *tail;
public:
  ASManager();
  void emit(AS::Instr *);
  AS::InstrList *getHead();
};

void munchStm(T::Stm *, ASManager &, const F::Frame *);
TEMP::Temp *munchExp(T::Exp *, ASManager &, const F::Frame *);
void munchArgs(T::ExpList *, ASManager &, const F::Frame *);

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList);
}
#endif