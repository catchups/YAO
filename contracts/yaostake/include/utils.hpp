#include <structs.hpp>

namespace utils
{
  void inline_transfer(const name &contract, const name &from, const name &to, const asset &quantity, const string &memo)
  {
    if (quantity.amount <= 0)
      return;
    auto data = make_tuple(from, to, quantity, memo);
    action(permission_level{from, "active"_n}, contract, "transfer"_n, data).send();
  }

  void issue(const name &contract, const name &issuer, const asset &quantity)
  {
    auto data = make_tuple(issuer, quantity);
    action(permission_level{issuer, "active"_n}, contract, "issue"_n, data).send();
  }

  vector<string> split(const string &str, const string &delim)
  {
    vector<string> strs;
    size_t prev = 0, pos = 0;
    do
    {
      pos = str.find(delim, prev);
      if (pos == string::npos)
        pos = str.length();
      string s = str.substr(prev, pos - prev);
      if (!str.empty())
        strs.push_back(s);
      prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return strs;
  }
} // namespace utils