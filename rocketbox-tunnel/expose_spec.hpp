#pragma once

#include <stdexcept>
#include <string>
#include <vector>

/** One published host port; bare CLI port becomes tcp+udp. */
struct ExposeRule {
  int port = 0;
  bool tcp = true;
  bool udp = true;
};

/** Parse "445", "tcp:445", "udp:137". Throws on invalid. */
inline ExposeRule parse_expose_token(const std::string& tok) {
  ExposeRule r;
  if (tok.compare(0, 4, "tcp:") == 0) {
    r.port = std::stoi(tok.substr(4));
    r.tcp = true;
    r.udp = false;
  } else if (tok.compare(0, 4, "udp:") == 0) {
    r.port = std::stoi(tok.substr(4));
    r.tcp = false;
    r.udp = true;
  } else {
    r.port = std::stoi(tok);
    r.tcp = true;
    r.udp = true;
  }
  if (r.port <= 0 || r.port > 65535) {
    throw std::runtime_error("invalid expose port: " + tok);
  }
  return r;
}

inline void parse_expose_list(const std::string& s, std::vector<ExposeRule>& out) {
  std::string part;
  for (size_t i = 0; i <= s.size(); ++i) {
    if (i == s.size() || s[i] == ',') {
      if (!part.empty()) {
        out.push_back(parse_expose_token(part));
        part.clear();
      }
    } else {
      part.push_back(s[i]);
    }
  }
}
