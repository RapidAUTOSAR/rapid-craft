#pragma once

#include <string>
#include <vector>

struct IRFile {
  std::string path;
  std::string hash;
};

struct IRFunction {
  std::string usr;
  std::string name;
  std::string qualname;
  std::string filePath;
  int startLine = 0;
  int endLine = 0;
  bool isStatic = false;
  std::string returnType;
};

struct IRCall {
  std::string callerUSR;
  std::string calleeUSR;
  std::string callerName;
  std::string calleeName;
  std::string filePath;
  int line = 0;
  std::string callType;
};

struct IRTranslationUnit {
  std::vector<IRFunction> functions;
  std::vector<IRCall> calls;
};
