#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "CallGraphCollector.h"

using namespace clang;
using namespace clang::tooling;

class CallGraphActionFactory
    : public clang::tooling::FrontendActionFactory {
public:
    explicit CallGraphActionFactory(const RcAnalyzerOptions& options)
        : Opts(options) {}

    std::unique_ptr<clang::FrontendAction> create() override {
        return std::make_unique<CallGraphFrontendAction>(Opts);
    }

private:
    RcAnalyzerOptions Opts;
};

// ------------------------------
// CLI options
// ------------------------------
static llvm::cl::OptionCategory RapidCraftCategory("rapid-craft-analyzer options");

static llvm::cl::opt<std::string> OptEmit(
    "emit",
    llvm::cl::desc("Output format: json | puml | both"),
    llvm::cl::init("json"),
    llvm::cl::cat(RapidCraftCategory));

static llvm::cl::opt<std::string> OptStdlibLeaf(
    "stdlib-leaf",
    llvm::cl::desc("Stdlib leaf handling: on | off (on: include stdlib as leaf nodes, off: skip stdlib edges)"),
    llvm::cl::init("off"),
    llvm::cl::cat(RapidCraftCategory));

static llvm::cl::opt<std::string> OptIndirectLabel(
    "indirect-label",
    llvm::cl::desc("Indirect call label: plain | var (plain: '(indirect)', var: '(indirect:<expr>)')"),
    llvm::cl::init("plain"),
    llvm::cl::cat(RapidCraftCategory));

static llvm::cl::opt<int> OptSeqDepth(
    "seq-depth",
    llvm::cl::desc("Max expansion depth for sequence diagram (>=1)"),
    llvm::cl::init(5),
    llvm::cl::cat(RapidCraftCategory));

static llvm::cl::opt<std::string> OptSeqRoot(
    "seq-root",
    llvm::cl::desc("Sequence diagram root function name (empty = auto: 'main' if present, else first user function)"),
    llvm::cl::init(""),
    llvm::cl::cat(RapidCraftCategory));

static llvm::cl::opt<bool> OptNoCompileDbWarn(
    "no-compile-db-warning",
    llvm::cl::desc("Suppress compilation database warning"),
    llvm::cl::init(false),
    llvm::cl::cat(RapidCraftCategory));

int main(int argc, const char **argv)
{
    std::vector<const char*> OptArgv;
    OptArgv.push_back(argv[0]); // program name

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // clang-tooling 규칙:
        // 옵션은 '-'로 시작
        if (!arg.empty() && arg[0] == '-') {
            OptArgv.push_back(argv[i]);
        }
    }

    // 반드시 null-terminated처럼 argc 전달
    int OptArgc = static_cast<int>(OptArgv.size());

    llvm::cl::ParseCommandLineOptions(
        OptArgc, OptArgv.data(),
        "rapid-craft-analyzer\n"
    );

    RcAnalyzerOptions opts;
    opts.emit = OptEmit;
    opts.stdlibLeaf = (OptStdlibLeaf == "on");
    opts.indirectLabelVar = (OptIndirectLabel == "var");
    opts.sequenceMaxDepth = (OptSeqDepth < 1 ? 1 : OptSeqDepth);
    opts.sequenceRoot = OptSeqRoot;

    // 1) Try normal CommonOptionsParser (compile_commands.json)
    auto ExpectedParser =
        CommonOptionsParser::create(argc, argv, RapidCraftCategory);

    if (ExpectedParser) {
        CommonOptionsParser &OptionsParser = ExpectedParser.get();
        ClangTool Tool(
            OptionsParser.getCompilations(),
            OptionsParser.getSourcePathList()
        );

        auto Factory =
            std::make_unique<CallGraphActionFactory>(opts);

        return Tool.run(Factory.get());
    }

    // 2) Fallback: no compilation database
    if (!OptNoCompileDbWarn) {
        llvm::errs()
            << "[info] No compilation database found. "
            << "Using default compile flags: -std=c11\n";
    }

    // Collect source files manually
    std::vector<std::string> Sources;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!arg.empty() && arg[0] != '-') {
            Sources.push_back(arg);
        }
    }

    if (Sources.empty()) {
        llvm::errs() << "error: no input files\n";
        return 1;
    }

    // 3) FixedCompilationDatabase with default flags
    clang::tooling::FixedCompilationDatabase Compilations(
        ".", std::vector<std::string>{"-std=c11"}
    );

    ClangTool Tool(Compilations, Sources);

    auto Factory =
        std::make_unique<CallGraphActionFactory>(opts);

    return Tool.run(Factory.get());
}