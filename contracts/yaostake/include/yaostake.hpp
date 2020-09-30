#include <utils.hpp>
#include <safemath.hpp>

CONTRACT yaostake : public contract
{
public:
  using contract::contract;
  static constexpr uint8_t SYSTEM_STATUS_OFF = 0;
  static constexpr uint8_t SYSTEM_STATUS_ON = 1;
  static constexpr uint8_t POOL_TYPE_STAKE = 1;
  static constexpr uint8_t POOL_TYPE_TF = 2;
  static constexpr uint64_t MAX_SUPPLY = 52'5000'0000'0000LL; // 25%
  static constexpr symbol_code MINED_SYMBOL_CODE = symbol_code("YAO");
  static constexpr symbol MINED_SYMBOL = symbol("YAO", 4);

  yaostake(name receiver, name code, datastream<const char *> ds)
      : contract(receiver, code, ds),
        _pools(_self, _self.value),
        _global(_self, _self.value) {}

  ACTION init(const name &admin, const name &token);
  ACTION create(name contract, symbol sym, asset reward, uint64_t epoch_time, uint64_t duration, asset min_staked, uint8_t type);
  ACTION setstatus(const uint8_t status);
  ACTION claim(name owner, uint64_t pool_id);
  ACTION harvest(uint64_t pool_id, uint64_t nonce);

  static name get_issuer(const name &token_contract_account, const symbol_code &sym_code)
  {
    stats statstable(token_contract_account, sym_code.raw());
    const auto &st = statstable.get(sym_code.raw());
    return st.issuer;
  }

  void handle_transfer(name from, name to, asset quantity, string memo, name code);

private:
  TABLE global
  {
    name admin;
    name token;
    uint8_t status;
  };

  TABLE pool
  {
    uint64_t id;
    uint8_t type; // 1=> Pure Stake; 2=> Transfer and Return Back
    name contract;
    symbol sym;
    asset total_staked;
    asset total_reward;
    asset released_reward;
    uint64_t epoch_time;
    uint64_t duration;
    asset min_staked;
    uint64_t last_harvest_time;
    uint64_t primary_key() const { return id; }
  };

  TABLE miner
  {
    name owner;
    name invitor;
    asset staked;
    asset claimed;
    asset unclaimed;
    asset refersum;
    uint64_t primary_key() const { return owner.value; }
  };

  typedef eosio::singleton<"global"_n, global> global_singleton;
  typedef eosio::multi_index<"pools"_n, pool> pools;
  typedef eosio::multi_index<"miners"_n, miner> miners;

  global_singleton _global;
  pools _pools;
  void check_system_on();
};
