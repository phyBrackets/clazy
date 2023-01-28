#include "unused-result-check.h"

#include <clang/AST/AST.h>
#include <clang/AST/ExprCXX.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchersInternal.h>

using namespace clang::ast_matchers;
class ClazyContext;

using namespace clang;

class Caller : public ClazyAstMatcherCallback {
public:
  Caller(CheckBase *check) : ClazyAstMatcherCallback(check) {}
  virtual void run(const MatchFinder::MatchResult &result) {
    if (const CXXMemberCallExpr *callExpr =
            result.Nodes.getNodeAs<CXXMemberCallExpr>("callExpr")) {
      if (callExpr->getMethodDecl()->isConst() &&
          !callExpr->getMethodDecl()->getReturnType()->isVoidType()) {
        const auto &parents = result.Context->getParents(*callExpr);

        if (parents[0].get<Stmt>() != nullptr &&
            parents[0].get<Decl>() == nullptr) {
          if (!llvm::dyn_cast<Expr>(parents[0].get<Stmt>()) &&
              !llvm::dyn_cast<ReturnStmt>(parents[0].get<Stmt>()) &&
              !llvm::dyn_cast<IfStmt>(parents[0].get<Stmt>()) &&
              !llvm::dyn_cast<CXXThisExpr>(parents[0].get<Stmt>())) {
            m_check->emitWarning(
                callExpr->getExprLoc(),
                "Result of const member function is not used.");
          }
        }

        else if (parents[0].get<Decl>() != nullptr &&
                 parents[0].get<Stmt>() == nullptr) {
          if (!llvm::dyn_cast<VarDecl>(parents[0].get<Decl>()) &&
              !llvm::dyn_cast<CXXConstructorDecl>(parents[0].get<Decl>())) {
            m_check->emitWarning(
                callExpr->getExprLoc(),
                "Result of const member function is not used.");
          }
        }
      }
    }
  }
};

ResultNotUsedChecker::ResultNotUsedChecker(const std::string &name,
                                           ClazyContext *context)
    : CheckBase(name, context, Option_CanIgnoreIncludes),
      m_astMatcherCallBack(new Caller(this)) {}

ResultNotUsedChecker::~ResultNotUsedChecker() { delete m_astMatcherCallBack; }

void ResultNotUsedChecker::VisitStmt(Stmt *stmt) {
  auto call = dyn_cast<CXXMemberCallExpr>(stmt);
  if (!call || call->getNumArgs() != 1)
    return;
}

void ResultNotUsedChecker::registerASTMatchers(MatchFinder &finder) {
  finder.addMatcher(cxxMemberCallExpr().bind("callExpr"), m_astMatcherCallBack);
}
