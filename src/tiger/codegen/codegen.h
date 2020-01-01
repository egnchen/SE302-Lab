#ifndef TIGER_CODEGEN_CODEGEN_H_
#define TIGER_CODEGEN_CODEGEN_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/translate/tree.h"
#include <sstream>

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

std::string get_framesize(const F::Frame *f);
std::string get_x8664_pushq();
std::string get_x8664_movq_mem();
std::string get_x8664_movq_temp();
std::string get_x8664_movq_imm_temp(int imm);
std::string get_x8664_movq_imm_temp(std::string imm);
std::string get_x8664_movq_imm_mem(int imm);
std::string get_x8664_movq_mem_const_offset(const int offset);
std::string get_x8664_movq_mem_offset_fp(const int offset, const F::Frame *f);
std::string get_x8664_movq_temp_const_offset(const int offset);
std::string get_x8664_movq_temp_offset();
std::string get_x8664_movq_temp_offset_fp(const int offset, const F::Frame *f);
std::string get_x8664_cmp();
std::string get_x8664_fp(const F::Frame *f);
std::string get_x8664_cjump(T::RelOp oper);
std::string get_x8664_jump();
std::string get_x8664_addq();
std::string get_x8664_subq();
std::string get_x8664_imulq();
std::string get_x8664_idivq();
std::string get_x8664_leaq(int offset);
std::string get_x8664_callq(std::string label);

void munchStm(T::Stm *, ASManager &, const F::Frame *);
TEMP::Temp *munchExp(T::Exp *, ASManager &, const F::Frame *);
TEMP::TempList *munchArgs(T::ExpList *, ASManager &, const F::Frame *);

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList);
}
#endif