#ifndef TIGER_LEX_SCANNER_H_
#define TIGER_LEX_SCANNER_H_

#include <algorithm>
#include <string>

#include "scannerbase.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/parse/parserbase.h"

extern EM::ErrorMsg errormsg;

class Scanner : public ScannerBase {
 public:
  explicit Scanner(std::istream &in = std::cin, std::ostream &out = std::cout);

  Scanner(std::string const &infile, std::string const &outfile);

  int lex();

 private:
  int lex_();
  int executeAction_(size_t ruleNr);

  void print();
  void preCode();
  void postCode(PostEnum_ type);
  void adjust();
  void adjustStr();

  int commentLevel_;
  std::string stringBuf_;
  int parsedStringLength_;
  int charPos_;
};

inline Scanner::Scanner(std::istream &in, std::ostream &out)
    : ScannerBase(in, out), charPos_(1) {}

inline Scanner::Scanner(std::string const &infile, std::string const &outfile)
    : ScannerBase(infile, outfile), charPos_(1) {}

inline int Scanner::lex() { return lex_(); }

inline void Scanner::preCode() {
  // optionally replace by your own code
}

inline void Scanner::postCode(PostEnum_ type) {
  // optionally replace by your own code
}

inline void Scanner::print() { print_(); }

inline void Scanner::adjust() {
  errormsg.tokPos = charPos_;
  charPos_ += length();
}

inline void Scanner::adjustStr() { charPos_ += length(); parsedStringLength_ += length(); }

#endif  // TIGER_LEX_SCANNER_H_

