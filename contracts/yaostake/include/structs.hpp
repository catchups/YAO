#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/print.hpp>

using namespace eosio;
using namespace std;

struct transfer_args
{
  name from;
  name to;
  asset quantity;
  string memo;
};

struct account
{
  asset balance;

  uint64_t primary_key() const { return balance.symbol.code().raw(); }
};

struct currency_stats
{
  asset supply;
  asset max_supply;
  name issuer;
  uint64_t primary_key() const { return supply.symbol.code().raw(); }
};

typedef eosio::multi_index<"stat"_n, currency_stats> stats;
typedef eosio::multi_index<"accounts"_n, account> accounts;
