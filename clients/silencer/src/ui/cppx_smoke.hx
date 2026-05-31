#pragma once

// SIL-7 pipeline smoke test for the cppx transpile -> compile -> link flow.
//
// Self-contained on purpose: it defines a tiny element model so the
// generated translation unit compiles before the retained runtime
// (react/element/tree) is vendored in SIL-9. It exercises the host-tag
// lowering path (`detail.Host`), text children, designated-init props,
// and `children({...})`. DELETE this pair once real `.cppx` components
// (SIL-16+) exercise the pipeline end-to-end.

#include <string>
#include <vector>

namespace silencer::cppx_smoke {

struct Node {
  std::string label;
  std::vector<Node> children;
};

inline Node text(std::string value) {
  return Node{std::move(value), {}};
}

inline std::vector<Node> children(std::vector<Node> items) {
  return items;
}

namespace detail {

struct HostProps {
  std::string id;
  std::vector<Node> children;
};

inline Node Host(HostProps props) {
  return Node{std::move(props.id), std::move(props.children)};
}

}  // namespace detail

// Built by the generated cppx_smoke.cpp; proves the generated TU links.
Node sample_tree();

}  // namespace silencer::cppx_smoke
