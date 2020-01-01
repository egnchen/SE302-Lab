#include "tiger/codegen/codegen.h"
#include "tiger/frame/x64frame.h"

#include <sstream>

namespace F {
class X64Frame;
}

namespace CG {

const int DEFAULT_STR_SIZE = 32;
typedef TEMP::TempList TL;

std::string get_framesize(const F::Frame *f) {
  return f->label->Name() + "_fs";
}

std::string get_x8664_pushq() { return "pushq `s0"; }
std::string get_x8664_movq_mem() { return "movq `s0, (`s1)"; }
std::string get_x8664_movq_temp() { return "movq `s0, `d0"; }
std::string get_x8664_movq_imm_temp(int imm)
{
  std::ostringstream ss;
  ss << "movq $" << imm << ", `d0";
  return ss.str();
}
std::string get_x8664_movq_imm_temp(std::string imm)
{
  return "leaq " + imm + "(%rip), `d0";
}
std::string get_x8664_movq_imm_mem(int imm)
{
  std::ostringstream ss;
  ss << "movq $" << imm << ", (`s0)";
  return ss.str();
}
std::string get_x8664_movq_mem_const_offset(const int offset)
{
  std::ostringstream ss;
  ss << "movq `s0, " << offset << "(`s1)";
  return ss.str();
}
std::string get_x8664_movq_mem_offset_fp(const int offset, const F::Frame *f)
{
  std::ostringstream ss;
  ss << "movq `s0, (" << offset << '+' << get_framesize(f) << ")(`s1)";
  return ss.str();
}

std::string get_x8664_movq_temp_mem() { return "movq (`s0), `d0"; }
std::string get_x8664_movq_temp_const_offset(const int offset)
{
  std::ostringstream ss;
  ss << "movq " << offset << "(`s0), `d0";
  return ss.str();
}
std::string get_x8664_movq_temp_offset() { return "movq (`s0, `s1), `d0"; }
std::string get_x8664_movq_temp_offset_fp(const int offset, const F::Frame *f)
{
  std::ostringstream ss;
  ss << "movq (" << offset << '+' << get_framesize(f) << ")(`s0), `d0";
  return ss.str();
}
std::string get_x8664_cmp() { return "cmpq `s0, `s1"; }
std::string get_x8664_fp(const F::Frame *f) { return "leaq " + get_framesize(f) + "(`s0), `d0"; }
std::string get_x8664_cjump(T::RelOp oper)
{
  std::string op;
  switch(oper)
  {
    case T::EQ_OP: op = "je"; break;
    case T::NE_OP: op = "jne"; break;
    case T::LT_OP: op = "jl"; break;
    case T::GT_OP: op = "jg"; break;
    case T::LE_OP: op = "jle"; break;
    case T::GE_OP: op = "jge"; break;
    default: fprintf(stdout, "warning: not supported op in cjump");
  }
  return op + " `j0";
}
std::string get_x8664_jump() { return "jmp `j0"; }
std::string get_x8664_addq() { return "addq `s0, `d0"; }
std::string get_x8664_subq() { return "subq `s0, `d0"; }
std::string get_x8664_imulq() { return "imulq `s0, `d0"; }
std::string get_x8664_idivq() { return "idivq `s0"; }
std::string get_x8664_leaq(int offset)
{
  std::ostringstream ss;
  ss << "leaq " << offset << "(`s0), `d0";
  return ss.str();
}
std::string get_x8664_callq(std::string label) { return "callq " + label; }

ASManager::ASManager() {
  prehead = new AS::InstrList(nullptr, nullptr);
  tail = prehead;
}

void ASManager::emit(AS::Instr *instr) {
  tail = tail->tail = new AS::InstrList(instr, nullptr);
}

inline AS::InstrList *ASManager::getHead() { return prehead->tail; }

void munchStm(T::Stm *stm, ASManager &a, const F::Frame *f)
{
  switch(stm->kind)
  {
    case T::Stm::MOVE:
    {
      T::MoveStm *move_stm = static_cast<T::MoveStm *>(stm);
      T::Exp *dst = move_stm->dst;
      T::Exp *src = move_stm->src;
      if(dst->kind == T::Exp::MEM) {
        T::MemExp *mem_dst = (T::MemExp *)dst;
        T::Exp *addr = mem_dst->exp;
        // deal with binop here
        if(addr->kind == T::Exp::Kind::BINOP
          && ((T::BinopExp *)addr)->op == T::BinOp::PLUS_OP
          && (((T::BinopExp *)addr)->left->kind == T::Exp::CONST
            || ((T::BinopExp *)addr)->right->kind == T::Exp::CONST)) {
          T::BinopExp *baddr = (T::BinopExp *)addr;
          bool is_left_const = baddr->left->kind == T::Exp::CONST;
          T::ConstExp *c = (T::ConstExp *)(is_left_const ? baddr->left : baddr->right);
          T::Exp *other = is_left_const ? baddr->right : baddr->left;
          if(other->kind == T::Exp::TEMP && ((T::TempExp *)other)->temp == f->getFramePointer())
          {
            // deal with frame pointer
            // movq s0, (offset+framesize)(%rsp)
            a.emit(
              new AS::OperInstr(get_x8664_movq_mem_offset_fp(c->consti, f),
                nullptr,
                new TL(munchExp(src, a, f),
                  new TL(f->getStackPointer(), nullptr)),
                nullptr));
          } else {
            // regular stuff
            // movq s0, offset(s1)
            a.emit(new AS::OperInstr(
              get_x8664_movq_mem_const_offset(c->consti),
              nullptr,
              new TL(munchExp(src, a, f),
                new TL(munchExp(other, a, f), nullptr)),
              nullptr));
          }
        } else if(src->kind == T::Exp::CONST) {
          // movq $xxx, (s0)
          a.emit(new AS::OperInstr(
            get_x8664_movq_imm_mem(((T::ConstExp *)src)->consti),
            nullptr, new TL(munchExp(mem_dst->exp, a, f), nullptr), nullptr));
        }
        else
        {
          // movq s0, (s1) or whatever
          // since fp needs special care, just throw in a temp register
          a.emit(new AS::OperInstr(
            get_x8664_movq_mem(),
            nullptr,
            new TL(munchExp(src, a, f),
              new TL(munchExp(mem_dst->exp, a, f), nullptr)),
            nullptr
          ));
        }
      }
      else
      {
        // movq s0, d0
        assert(dst->kind == T::Exp::TEMP);
        T::TempExp *temp_dst = (T::TempExp *)dst;
        a.emit(new AS::MoveInstr(
          get_x8664_movq_temp(),
          new TL(temp_dst->temp, nullptr),
          new TL(munchExp(src, a, f), nullptr)
        ));
      }
      break;
    }
    case T::Stm::LABEL:
    {
      T::LabelStm *label_stm = (T::LabelStm *)stm;
      TEMP::Label *label = label_stm->label;
      a.emit(new AS::LabelInstr(label->Name().c_str(), label));
      break;
    }
    case T::Stm::JUMP:
    {
      T::JumpStm *jump_stm = (T::JumpStm *)stm;
      // others not supported or we'll see...
      assert(jump_stm->exp->kind == T::Exp::NAME);
      T::NameExp *dst = (T::NameExp *)(jump_stm->exp);
      a.emit(new AS::OperInstr(
        get_x8664_jump(),
        nullptr, nullptr,
        new AS::Targets(jump_stm->jumps)));
      break;
    }
    case T::Stm::CJUMP:
    {
      T::CjumpStm *cjump_stm = (T::CjumpStm *)stm;
      TEMP::Temp *left = munchExp(cjump_stm->left, a, f),
        *right = munchExp(cjump_stm->right, a, f);
      a.emit(new AS::OperInstr(get_x8664_cmp(),
        // watch out for the order here, bro
        nullptr, new TL(right, new TL(left, nullptr)), nullptr));
      a.emit(new AS::OperInstr(
        get_x8664_cjump(cjump_stm->op),
        nullptr, nullptr,
        new AS::Targets(new TEMP::LabelList(cjump_stm->true_label, nullptr))
      ));
      break;
    }
    case T::Stm::EXP:
    {
      // generate code and discard result
      munchExp(((T::ExpStm *)stm)->exp, a, f);
      break;
    }
    case T::Stm::SEQ:
      fputs("SeqExp should be eliminated after canonicalization.\n", stdout);
      break;
    default:
      fputs("Reached invalid part of program\n", stdout);
      assert(0);
  }
}

TEMP::Temp *munchExp(T::Exp *exp, ASManager &a, const F::Frame *f)
{
  TEMP::Temp *r = TEMP::Temp::NewTemp();
  switch(exp->kind)
  {
    case T::Exp::MEM:
    {
      T::MemExp *mem_exp = (T::MemExp *)exp;
      T::Exp *addr = mem_exp->exp;
      if(addr->kind == T::Exp::BINOP)
      {
        T::BinopExp *baddr = (T::BinopExp *)addr;
        T::Exp *left = baddr->left, *right = baddr->right;
        // deal with frame pointer, make fp=sp+framesize
        // we assume that fp can only appear on lhs of BinopExp
        // and fp can only appear with a ConstExp in BinopExp
        assert(!(baddr->right->kind == T::Exp::TEMP
          && (((T::TempExp *)baddr->right)->temp == f->getFramePointer())));
        
        if((left->kind == T::Exp::TEMP &&
          ((T::TempExp *)left)->temp == f->getFramePointer()))
        {
          // movq $xxx(%rbp), %rt
          // evaluate as movq ($xxx+framesize)(%rsp), rt
          T::TempExp *fp_exp = (T::TempExp *)left;
          T::ConstExp *const_exp = (T::ConstExp *)right;
          assert(const_exp->kind == T::Exp::CONST);
          a.emit(new AS::OperInstr(
            get_x8664_movq_temp_offset_fp(const_exp->consti, f),
            new TL(r, nullptr),
            new TL(f->getStackPointer(), nullptr),
            nullptr
          ));
        }
        else if(left->kind == T::Exp::CONST || right->kind == T::Exp::CONST)
        {
          // movq xxx(%s0), %rt
          bool is_left_const = left->kind == T::Exp::CONST;
          T::ConstExp *const_exp = (T::ConstExp *)( is_left_const ? left : right);
          T::TempExp *temp_exp = (T::TempExp *)(is_left_const ? right : left);
          a.emit(new AS::OperInstr(
            get_x8664_movq_temp_const_offset(const_exp->consti),
            new TL(r, nullptr),
            new TL(munchExp(temp_exp, a, f), nullptr),
            nullptr
          ));
        }
        else
        {
          // movq %s0(%s1), %rt
          // munch them all
          TEMP::Temp *addr_base_temp = munchExp(baddr->left, a, f);
          TEMP::Temp *addr_offset_temp = munchExp(baddr->right, a, f);
          a.emit(new AS::OperInstr(
            get_x8664_movq_temp_offset(),
            new TL(r, nullptr),
            new TL(addr_base_temp, new TL(addr_offset_temp, nullptr)),
            nullptr
          ));
        }
      }
      else
      {
        // addr.kind != BINOP
        // movq (%s0), %rt
        TEMP::Temp *addr_temp = munchExp(addr, a, f);
        a.emit(new AS::OperInstr(
          get_x8664_movq_temp_mem(),
          new TL(r, nullptr),
          new TL(addr_temp, nullptr),
          nullptr
        ));
      }
      break;
    }
    case T::Exp::BINOP:
    {
      T::BinopExp *bin_exp = (T::BinopExp *)exp;
      // deal with fp
      assert(!(bin_exp->right->kind == T::Exp::TEMP
        && ((T::TempExp *)bin_exp)->temp == f->getFramePointer()));
      
      if((bin_exp->op == T::BinOp::PLUS_OP || bin_exp->op == T::BinOp::MINUS_OP)
        && (bin_exp->right->kind == T::Exp::CONST || bin_exp->left->kind == T::Exp::CONST))
      {
        // leaq offset(s0), rt
        bool is_left_const = bin_exp->left->kind == T::Exp::CONST;
        T::ConstExp *const_exp = (T::ConstExp *)(is_left_const ? bin_exp->left : bin_exp->right);
        T::Exp *base_exp = (T::TempExp *)(is_left_const ? bin_exp->right : bin_exp->left);
        int offset = const_exp->consti;
        if(bin_exp->op == T::BinOp::MINUS_OP) offset = -offset;
        a.emit(new AS::OperInstr(
          get_x8664_leaq(offset),
          new TL(r, nullptr),
          new TL(munchExp(base_exp, a, f), nullptr),
          nullptr
        ));
      }
      else
      {
        // movq s0, rt
        // addq/subq/imulq/idivq s1, rt
        TEMP::Temp *left_temp = munchExp(bin_exp->left, a, f);
        TEMP::Temp *right_temp = munchExp(bin_exp->right, a, f);
        
        std::string assem;
        switch(bin_exp->op)
        {
          case T::BinOp::PLUS_OP:   assem = get_x8664_addq(); break;
          case T::BinOp::MINUS_OP:  assem = get_x8664_subq(); break;
          case T::BinOp::MUL_OP:    assem = get_x8664_imulq(); break;
          case T::BinOp::DIV_OP:    assem = get_x8664_idivq(); break;
          default: fputs("Operation not supported.", stdout); 
        }
        // division should be treated differently in x86-64
        if(bin_exp->op != T::BinOp::DIV_OP)
        {
          a.emit(new AS::MoveInstr(get_x8664_movq_temp(),
            new TL(r, nullptr), new TL(left_temp, nullptr)));
          a.emit(new AS::OperInstr(assem,
            new TL(r, nullptr), new TL(right_temp, nullptr), nullptr));
        }
        else
        {
          // division
          // movq %s0 %rax
          // cltd
          // idivq %s1
          // movq %rax %rt
          F::X64Frame *fr = (F::X64Frame *)f;
          a.emit(new AS::MoveInstr(get_x8664_movq_temp(),
            new TL(fr->rax, nullptr), new TL(left_temp, nullptr)));
          a.emit(new AS::OperInstr("cltd",
            new TL(fr->rax, new TL(fr->rdx, nullptr)),
            new TL(fr->rax, nullptr), nullptr));
          a.emit(new AS::OperInstr(get_x8664_idivq(),
            new TL(fr->rax, new TL(fr->rdx, nullptr)),
            new TL(right_temp, new TL(fr->rax, new TL(fr->rdx, nullptr))),
            nullptr));
          a.emit(new AS::MoveInstr(get_x8664_movq_temp(),
            new TL(r, nullptr), new TL(fr->rax, nullptr)));
        }
      }
      break;
    }
    case T::Exp::TEMP:
    {
      // only fp needs our attention.
      T::TempExp *temp_exp = (T::TempExp *)exp;
      if(temp_exp->temp == f->getFramePointer())
      {
        a.emit(new AS::OperInstr(get_x8664_fp(f),
          new TL(r, nullptr), new TL(f->getStackPointer(), nullptr), nullptr));
      }
      else r = temp_exp->temp;
      break;
    }
    case T::Exp::CONST:
    {
      a.emit(new AS::OperInstr(
        get_x8664_movq_imm_temp(((T::ConstExp *)exp)->consti),
        new TL(r, nullptr), nullptr, nullptr));
      break;
    }
    case T::Exp::NAME:
    {
      // movq NAME %rt
      a.emit(new AS::OperInstr(
        get_x8664_movq_imm_temp(((T::NameExp *)exp)->name->Name()),
        new TL(r, nullptr), nullptr, nullptr));
      break;
    }
    case T::Exp::CALL:
    {
      T::CallExp *call_exp = (T::CallExp *)exp;
      assert(call_exp->fun->kind == T::Exp::NAME);
      T::NameExp *fun_exp = (T::NameExp *)(call_exp->fun);
      TEMP::TempList *args = munchArgs(call_exp->args, a, f);
      a.emit(new AS::OperInstr(get_x8664_callq(fun_exp->name->Name()),
        ((F::X64Frame *)f)->caller_saved, nullptr, nullptr));
      unMunchArgs(call_exp->args, a, f);
      a.emit(new AS::MoveInstr(get_x8664_movq_temp(),
        new TL(r, nullptr), new TL(((F::X64Frame *)f)->rax, nullptr)));
      break;
    }

    case T::Exp::ESEQ:
    default:
      fputs("Unsupported exp type", stdout);
      assert(0);
  }
  return r;
}

TEMP::TempList *munchArgs(T::ExpList *args, ASManager &a, const F::Frame *f)
{
  int i = 0;
  F::X64Frame *fr = (F::X64Frame *)f;
  TEMP::TempList *prehead = new TEMP::TempList(nullptr, nullptr);
  TEMP::TempList *tail = prehead;
  while(args)
  {
    TEMP::Temp *arg = munchExp(args->head, a, f);
    if(i < fr->param_reg_count) {
      a.emit(new AS::MoveInstr(get_x8664_movq_temp(),
        new TL(fr->param_regs[i], nullptr), new TL(arg, nullptr)));
      tail = tail->tail = new TEMP::TempList(fr->param_regs[i], nullptr);
    }
    else
      a.emit(new AS::OperInstr(get_x8664_pushq(),
        nullptr, new TL(arg, nullptr), nullptr));
    i++;
    args = args->tail;
  }
  return prehead->tail;
}

void unMunchArgs(T::ExpList *args, ASManager &a, const F::Frame *f)
{
  int i = 0;
  F::X64Frame *fr = (F::X64Frame *)f;
  for(; args; i++, args = args->tail);
  if(i > fr->param_reg_count) {
    i -= fr->param_reg_count;
    std::ostringstream ss;
    ss << "subq $" << (i * TR::word_size) << " $rsp";
    a.emit(new AS::OperInstr(ss.str(), nullptr, nullptr, nullptr));
  }
}

AS::InstrList *Codegen(F::Frame* f, T::StmList* stmList) {
  ASManager a;
  f->onEnter(a);
  for(; stmList; stmList = stmList->tail)
    munchStm(stmList->head, a, f);
  f->onReturn(a);
  return a.getHead();
}

}  // namespace CG