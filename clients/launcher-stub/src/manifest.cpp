#include "stub.h"

// First string value for "key" in a small flat JSON object. Not a JSON
// parser: the manifests are our own, tiny and flat, and any confusion here
// just reads as "no update", which is the safe outcome.
std::string stub::json_str_field(const std::string &j, const std::string &key) {
  const std::string pat = "\"" + key + "\"";
  size_t p = j.find(pat);
  if (p == std::string::npos)
    return "";
  p += pat.size();
  while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == '\r' || j[p] == '\n'))
    p++;
  if (p >= j.size() || j[p] != ':')
    return "";
  p++;
  while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == '\r' || j[p] == '\n'))
    p++;
  if (p >= j.size() || j[p] != '"')
    return "";
  p++;
  std::string out;
  while (p < j.size() && j[p] != '"') {
    char c = j[p];
    if (c == '\\' && p + 1 < j.size()) {
      p++;
      c = j[p];
      if (c == 'n')
        c = '\n';
      else if (c == 't')
        c = '\t';
      // \" \\ \/ fall through as the escaped character itself
    }
    out += c;
    p++;
  }
  return p < j.size() ? out : "";
}
