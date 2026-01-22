#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/CompilerInstance.h"

// ------------------------------
// Options shared across action/collector
// ------------------------------
struct RcAnalyzerOptions {
    std::string emit = "both";     // json | puml | both
    bool stdlibLeaf = false;       // include stdlib calls as leaf edges
    bool indirectLabelVar = false; // (indirect:<expr>) vs (indirect)
    int sequenceMaxDepth = 5;      // depth for sequence expansion
    std::string sequenceRoot;      // optional root name
};

class CallGraphCollector : public clang::RecursiveASTVisitor<CallGraphCollector> {
public:
    CallGraphCollector(clang::ASTContext& context, const RcAnalyzerOptions& options);

    bool VisitFunctionDecl(clang::FunctionDecl* FD);
    bool VisitCallExpr(clang::CallExpr* CE);

    // outputs
    void dumpAsJson() const;
    void dumpSequenceAsPlantUml() const;

private:
    clang::ASTContext& Context;
    const clang::SourceManager& SM;
    const RcAnalyzerOptions& Opts;

    const clang::FunctionDecl* CurrentFunction = nullptr;

    // callGraph relationship (set)
    std::map<std::string, std::set<std::string>> CallGraph;

    // call order per function (vector) for sequence generation
    std::map<std::string, std::vector<std::string>> CallOrder;

    // track all nodes we care about
    std::set<std::string> Nodes;

    // helpers
    bool isUserFunction(const clang::FunctionDecl* FD) const;
    bool isSystemFunctionName(const std::string& name) const;
    std::string getIndirectLabel(const clang::CallExpr* CE) const;

    void ensureNode(const std::string& name);

    // sequence helpers
    std::string pickSequenceRoot() const;
    void emitSeqParticipants(llvm::raw_ostream& os) const;
    void emitSeqFrom(llvm::raw_ostream& os,
                     const std::string& caller,
                     int depth,
                     std::set<std::string>& stack) const;
};

// ------------------------------
// AST Consumer / FrontendAction
// ------------------------------
class CallGraphASTConsumer : public clang::ASTConsumer {
public:
    CallGraphASTConsumer(clang::ASTContext& context, const RcAnalyzerOptions& options);
    void HandleTranslationUnit(clang::ASTContext& context) override;

private:
    const RcAnalyzerOptions& Opts;
    CallGraphCollector Collector;
};

class CallGraphFrontendAction : public clang::ASTFrontendAction {
public:
    explicit CallGraphFrontendAction(const RcAnalyzerOptions& options);

    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance& CI, clang::StringRef) override;

private:
    RcAnalyzerOptions Opts; // stored copy (factory passes by value)
};
