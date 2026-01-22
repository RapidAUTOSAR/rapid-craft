#include "extractor_clang.h"
#include <clang-c/Index.h>
#include <unordered_map>
#include <iostream>

static std::string toStd(CXString s) {
  std::string out = clang_getCString(s) ? clang_getCString(s) : "";
  clang_disposeString(s);
  return out;
}

static std::string getFilePath(CXSourceLocation loc) {
  CXFile file;
  unsigned line, col, off;
  clang_getSpellingLocation(loc, &file, &line, &col, &off);
  if (!file) return "";
  return toStd(clang_getFileName(file));
}

struct VisitorCtx {
  IRTranslationUnit* ir = nullptr;

  // map cursor USR -> function name (to fill calls even if not recorded yet)
  std::unordered_map<std::string, std::string> usrToName;

  // current function USR while visiting its body
  std::string currentFuncUSR;
  std::string currentFuncName;
};

static bool isFromMainFile(CXCursor c) {
  auto loc = clang_getCursorLocation(c);
  return clang_Location_isFromMainFile(loc) != 0;
}

static bool isFunctionDecl(CXCursor c) {
  auto k = clang_getCursorKind(c);
  return k == CXCursor_FunctionDecl || k == CXCursor_CXXMethod ||
         k == CXCursor_Constructor || k == CXCursor_Destructor;
}

static CXChildVisitResult visitor(CXCursor c, CXCursor parent, CXClientData client_data) {
  auto* ctx = reinterpret_cast<VisitorCtx*>(client_data);

  // Function declaration
  if (isFunctionDecl(c) && isFromMainFile(c)) {
    IRFunction fn;
    fn.usr = toStd(clang_getCursorUSR(c));
    fn.name = toStd(clang_getCursorSpelling(c));
    fn.qualname = fn.name; // Phase1: best-effort
    fn.returnType = toStd(clang_getTypeSpelling(clang_getCursorResultType(c)));

    // location
    CXSourceLocation locStart = clang_getCursorLocation(c);
    fn.filePath = getFilePath(locStart);

    CXSourceRange range = clang_getCursorExtent(c);
    CXSourceLocation loc1 = clang_getRangeStart(range);
    CXSourceLocation loc2 = clang_getRangeEnd(range);

    unsigned line1, col1, off1;
    unsigned line2, col2, off2;
    CXFile f1, f2;
    clang_getSpellingLocation(loc1, &f1, &line1, &col1, &off1);
    clang_getSpellingLocation(loc2, &f2, &line2, &col2, &off2);

    fn.startLine = (int)line1;
    fn.endLine = (int)line2;

    // static?
    fn.isStatic = (clang_getCursorLinkage(c) == CXLinkage_Internal);

    ctx->ir->functions.push_back(fn);
    ctx->usrToName[fn.usr] = fn.name;

    // traverse its children with current function context
    auto prevUSR = ctx->currentFuncUSR;
    auto prevName = ctx->currentFuncName;
    ctx->currentFuncUSR = fn.usr;
    ctx->currentFuncName = fn.name;

    clang_visitChildren(c, visitor, client_data);

    ctx->currentFuncUSR = prevUSR;
    ctx->currentFuncName = prevName;

    return CXChildVisit_Continue;
  }

  // Call expression (inside a function body)
  if (clang_getCursorKind(c) == CXCursor_CallExpr && !ctx->currentFuncUSR.empty()) {
    // Try resolve direct callee
    CXCursor callee = clang_getCursorReferenced(c);
    if (!clang_Cursor_isNull(callee) && isFunctionDecl(callee)) {
      IRCall call;
      call.callerUSR = ctx->currentFuncUSR;
      call.callerName = ctx->currentFuncName;

      call.calleeUSR = toStd(clang_getCursorUSR(callee));
      call.calleeName = toStd(clang_getCursorSpelling(callee));
      if (call.calleeName.empty()) call.calleeName = "(unknown)";

      // call location
      CXSourceLocation loc = clang_getCursorLocation(c);
      call.filePath = getFilePath(loc);
      unsigned line, col, off;
      CXFile file;
      clang_getSpellingLocation(loc, &file, &line, &col, &off);
      call.line = (int)line;

      ctx->ir->calls.push_back(call);

      // record name map
      if (!call.calleeUSR.empty() && !call.calleeName.empty())
        ctx->usrToName[call.calleeUSR] = call.calleeName;
    }
  }

  return CXChildVisit_Recurse;
}

IRTranslationUnit ClangExtractor::parse(const ClangTUInput& in) {
  IRTranslationUnit out;

  CXIndex index = clang_createIndex(/*excludeDeclsFromPCH*/0, /*displayDiagnostics*/0);

  std::vector<const char*> cargs;
  cargs.reserve(in.args.size());
  for (auto& a : in.args) cargs.push_back(a.c_str());

  CXTranslationUnit tu = clang_parseTranslationUnit(
    index,
    in.sourcePath.c_str(),
    cargs.data(),
    (int)cargs.size(),
    nullptr,
    0,
    CXTranslationUnit_SkipFunctionBodies  // ⚠️ Phase1에서는 CallExpr를 얻기 위해 바디 필요!
  );

  // NOTE: SkipFunctionBodies면 CallExpr 못 얻는다. 그래서 Phase1은 bodies 포함.
  if (!tu) {
    clang_disposeIndex(index);
    throw std::runtime_error("Failed to parse TU: " + in.sourcePath);
  }

  // Re-parse with bodies enabled (safe)
  clang_disposeTranslationUnit(tu);
  tu = clang_parseTranslationUnit(
    index,
    in.sourcePath.c_str(),
    cargs.data(),
    (int)cargs.size(),
    nullptr,
    0,
    CXTranslationUnit_None
  );
  if (!tu) {
    clang_disposeIndex(index);
    throw std::runtime_error("Failed to parse TU(with bodies): " + in.sourcePath);
  }

  CXCursor root = clang_getTranslationUnitCursor(tu);
  VisitorCtx ctx;
  ctx.ir = &out;
  clang_visitChildren(root, visitor, &ctx);

  clang_disposeTranslationUnit(tu);
  clang_disposeIndex(index);

  return out;
}
