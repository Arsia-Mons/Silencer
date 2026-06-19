#pragma once

// Pipeline smoke test for the cppx transpile -> compile -> link flow.
// Self-contained (defines its own tiny element model) so it compiles
// independently of the retained runtime. DELETE this pair once real
// `.cppx` components exercise the pipeline end-to-end.

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

Node sample_tree();

}  // namespace silencer::cppx_smoke
