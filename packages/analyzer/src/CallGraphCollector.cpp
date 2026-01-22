#include "CallGraphCollector.h"

#include <algorithm>

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

// ------------------------------
// C++17 helpers
// ------------------------------
static bool endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

static bool startsWith(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size()) return false;
    return std::equal(prefix.begin(), prefix.end(), str.begin());
}

// ------------------------------
// CallGraphCollector
// ------------------------------
CallGraphCollector::CallGraphCollector(ASTContext& context, const RcAnalyzerOptions& options)
    : Context(context), SM(context.getSourceManager()), Opts(options) {}

void CallGraphCollector::ensureNode(const std::string& name) {
    Nodes.insert(name);
    CallGraph.try_emplace(name);
    CallOrder.try_emplace(name);
}

bool CallGraphCollector::VisitFunctionDecl(FunctionDecl* FD) {
    if (!FD || !FD->hasBody()) return true;
    if (!isUserFunction(FD)) return true;

    CurrentFunction = FD;

    std::string caller = FD->getNameAsString();
    ensureNode(caller);
    return true;
}

std::string CallGraphCollector::getIndirectLabel(const CallExpr* CE) const {
    if (!Opts.indirectLabelVar) return "(indirect)";

    // Try to extract something meaningful from callee expression:
    // fp()  -> DeclRefExpr "fp"
    // (*fp)() -> UnaryOperator with subexpr DeclRefExpr "fp"
    const Expr* callee = CE ? CE->getCallee() : nullptr;
    if (!callee) return "(indirect)";

    callee = callee->IgnoreImpCasts();

    std::string hint;

    if (auto* DRE = dyn_cast<DeclRefExpr>(callee)) {
        hint = DRE->getNameInfo().getAsString();
    } else if (auto* UO = dyn_cast<UnaryOperator>(callee)) {
        const Expr* sub = UO->getSubExpr();
        if (sub) {
            sub = sub->IgnoreImpCasts();
            if (auto* subDRE = dyn_cast<DeclRefExpr>(sub)) {
                hint = subDRE->getNameInfo().getAsString();
            }
        }
    } else {
        // fallback: short textual hint (avoid huge)
        hint = "expr";
    }

    if (hint.empty()) return "(indirect)";
    return "(indirect:" + hint + ")";
}

bool CallGraphCollector::VisitCallExpr(CallExpr* CE) {
    if (!CurrentFunction) return true;

    std::string callerName = CurrentFunction->getNameAsString();
    ensureNode(callerName);

    // direct callee
    const FunctionDecl* Callee = CE ? CE->getDirectCallee() : nullptr;
    if (Callee) {
        std::string calleeName = Callee->getNameAsString();

        // stdlib/system handling
        if (isSystemFunctionName(calleeName)) {
            if (Opts.stdlibLeaf) {
                ensureNode(calleeName);                 // leaf node visible
                CallGraph[callerName].insert(calleeName);
                CallOrder[callerName].push_back(calleeName);
            }
            return true; // never traverse further; we don't collect callee bodies anyway
        }

        ensureNode(calleeName);
        CallGraph[callerName].insert(calleeName);
        CallOrder[callerName].push_back(calleeName);
        return true;
    }

    // indirect call (function pointer, virtual call without resolvable target, etc.)
    const std::string indirect = getIndirectLabel(CE);
    ensureNode(indirect);
    CallGraph[callerName].insert(indirect);
    CallOrder[callerName].push_back(indirect);
    return true;
}

bool CallGraphCollector::isUserFunction(const FunctionDecl* FD) const {
    if (!FD) return false;

    SourceLocation loc = FD->getLocation();
    if (!loc.isValid()) return false;

    std::string file = SM.getFilename(loc).str();

    // Basic user-code heuristic: physical source files.
    // (You can extend to include .hpp/.h etc if you want bodies there.)
    return endsWith(file, ".c") || endsWith(file, ".cpp") || endsWith(file, ".cc");
}

bool CallGraphCollector::isSystemFunctionName(const std::string& name) const {
    // very conservative: anything starting with these is almost certainly toolchain/runtime/builtin
    static const std::vector<std::string> prefixes = {
        "__", "_mingw", "__builtin", "__imp_", "_chkstk", "__security", "__acrt"
    };
    for (const auto& p : prefixes) {
        if (startsWith(name, p)) return true;
    }

    // Treat common C stdlib as "system" for leaf toggle purposes.
    // This is what you want for design-level graphs.
    static const std::set<std::string> stdlibNames = {
        "printf", "fprintf", "sprintf", "snprintf", "puts", "putchar",
        "malloc", "calloc", "realloc", "free",
        "memcpy", "memset", "memcmp", "strlen", "strcpy", "strncpy",
        "strcmp", "strncmp", "strcat", "strncat",
        "fopen", "fclose", "fread", "fwrite", "fflush",
        "exit", "abort", "assert"
    };
    if (stdlibNames.count(name)) return true;

    return false;
}

// ------------------------------
// JSON output
// ------------------------------
void CallGraphCollector::dumpAsJson() const {
    llvm::outs() << "{\n  \"callGraph\": {\n";

    bool firstCaller = true;
    for (const auto& kv : CallGraph) {
        const auto& caller = kv.first;
        const auto& callees = kv.second;

        if (!firstCaller) llvm::outs() << ",\n";
        firstCaller = false;

        llvm::outs() << "    \"" << caller << "\": [";

        bool firstCallee = true;
        for (const auto& callee : callees) {
            if (!firstCallee) llvm::outs() << ", ";
            firstCallee = false;
            llvm::outs() << "\"" << callee << "\"";
        }
        llvm::outs() << "]";
    }

    llvm::outs() << "\n  }\n}\n";
}

// ------------------------------
// Sequence Diagram Rules (PlantUML)
// ------------------------------
//
// 규칙 정의(설계용 최소 규칙):
// 1) Root 함수에서 시작하여 "CallOrder(순서)" 기반으로 메시지 출력.
// 2) Direct call: caller -> callee : call
// 3) stdlib: --stdlib-leaf=on 일 때만 메시지로 포함(leaf로만 표시).
// 4) indirect call: caller -> (indirect) 또는 (indirect:fp)
// 5) expansion: user 함수에 대해서만 DFS 확장. depth 제한(seq-depth).
//    - stdlib/indirect는 확장하지 않음.
// 6) 순환 방지: 현재 call stack에 이미 있으면 더 확장하지 않음.
//
// 한계(의도적):
// - 분기/루프/조건문은 현재 단계에서는 모델링하지 않는다.
// - "관계"가 아니라 "관측된 호출 순서" 기반의 간단 시퀀스만 생성.
//

std::string CallGraphCollector::pickSequenceRoot() const {
    if (!Opts.sequenceRoot.empty()) return Opts.sequenceRoot;
    if (Nodes.count("main")) return "main";
    if (!Nodes.empty()) return *Nodes.begin();
    return "main";
}

void CallGraphCollector::emitSeqParticipants(llvm::raw_ostream& os) const {
    // participants: only "meaningful" names. We'll include all Nodes that look like functions/labels.
    for (const auto& n : Nodes) {
        // PlantUML participant name quoting
        os << "participant \"" << n << "\" as " << "P" << std::hash<std::string>{}(n) << "\n";
    }
}

static bool isIndirectNode(const std::string& name) {
    return name.rfind("(indirect", 0) == 0;
}

static std::string sanitizePumlId(const std::string& name) {
    std::string id;
    id.reserve(name.size());

    for (char c : name) {
        if (isalnum(static_cast<unsigned char>(c))) {
            id.push_back(c);
        } else {
            id.push_back('_');
        }
    }

    // PUML id must not start with digit
    if (!id.empty() && isdigit(static_cast<unsigned char>(id[0]))) {
        id = "_" + id;
    }

    return id;
}

static std::string pumlId(const std::string& name) {
    static std::unordered_map<std::string, int> used;
    std::string base = sanitizePumlId(name);

    int& idx = used[base];
    if (idx == 0) {
        idx = 1;
        return base;
    }

    return base + "_" + std::to_string(idx++);
}

void CallGraphCollector::emitSeqFrom(llvm::raw_ostream& os,
                                    const std::string& caller,
                                    int depth,
                                    std::set<std::string>& stack) const {
    if (depth <= 0) return;
    if (stack.count(caller)) return;

    stack.insert(caller);

    auto it = CallOrder.find(caller);
    if (it == CallOrder.end()) {
        stack.erase(caller);
        return;
    }

    for (const auto& callee : it->second) {
        // message
        if (isIndirectNode(callee)) {
            os << pumlId(caller) << " ..> " << pumlId(callee)
            << " : indirect call\n";
        } else {
            os << pumlId(caller) << " -> " << pumlId(callee)
            << " : call\n";
        }

        // expand only if callee is a user function node we have an order list for,
        // and is not stdlib/indirect label
        bool isIndirect = startsWith(callee, "(indirect");
        bool isSys = isSystemFunctionName(callee);

        if (!isIndirect && !isSys && CallOrder.count(callee)) {
            os << "activate " << pumlId(callee) << "\n";
            emitSeqFrom(os, callee, depth - 1, stack);
            os << "deactivate " << pumlId(callee) << "\n";
        }
    }

    stack.erase(caller);
}

void CallGraphCollector::dumpSequenceAsPlantUml() const {
    llvm::raw_ostream& os = llvm::outs();
    const std::string root = pickSequenceRoot();

    os << "@startuml\n";
    os << "hide footbox\n";
    os << "skinparam sequenceMessageAlign center\n";
    os << "title rapid-craft sequence (root: " << root << ", depth: " << Opts.sequenceMaxDepth << ")\n\n";

    emitSeqParticipants(os);
    os << "\n";

    if (!Nodes.count(root)) {
        os << "' root not found: " << root << "\n";
        os << "@enduml\n";
        return;
    }

    os << "activate " << pumlId(root) << "\n";
    std::set<std::string> stack;
    emitSeqFrom(os, root, Opts.sequenceMaxDepth, stack);
    os << "deactivate " << pumlId(root) << "\n";

    os << "@enduml\n";
}

// ------------------------------
// AST Consumer / Frontend Action
// ------------------------------
CallGraphASTConsumer::CallGraphASTConsumer(ASTContext& context, const RcAnalyzerOptions& options)
    : Opts(options), Collector(context, options) {}

void CallGraphASTConsumer::HandleTranslationUnit(ASTContext& context) {
    Collector.TraverseDecl(context.getTranslationUnitDecl());

    if (Opts.emit == "json") {
        Collector.dumpAsJson();
    }
    else if (Opts.emit == "puml") {
        Collector.dumpSequenceAsPlantUml();
    }
    else if (Opts.emit == "both") {
        Collector.dumpAsJson();
        llvm::outs() << "\n";
        Collector.dumpSequenceAsPlantUml();
    }
    else{
        Collector.dumpAsJson();
    }
}

CallGraphFrontendAction::CallGraphFrontendAction(const RcAnalyzerOptions& options)
    : Opts(options) {}

std::unique_ptr<ASTConsumer>
CallGraphFrontendAction::CreateASTConsumer(CompilerInstance& CI, StringRef) {
    return std::make_unique<CallGraphASTConsumer>(CI.getASTContext(), Opts);
}
