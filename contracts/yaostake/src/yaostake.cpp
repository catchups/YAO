#include <yaostake.hpp>

extern "C"
{
  void apply(uint64_t receiver, uint64_t code, uint64_t action)
  {
    if (code == receiver)
    {
      switch (action)
      {
        EOSIO_DISPATCH_HELPER(yaostake, (init)(create)(setstatus)(claim)(harvest))
      }
    }
    else
    {
      if (action == name("transfer").value)
      {
        yaostake inst(name(receiver), name(code), datastream<const char *>(nullptr, 0));
        const auto t = unpack_action_data<transfer_args>();
        inst.handle_transfer(t.from, t.to, t.quantity, t.memo, name(code));
      }
    }
  }
}

// We keep this function to avoid extreme situations happen.
void yaostake::check_system_on()
{
  check(_global.exists(), "contract has not been initialized");
  auto glb = _global.get();
  check(glb.status == SYSTEM_STATUS_ON, "system is paused");
}

void yaostake::init(const name &admin, const name &token)
{
  require_auth(_self);
  check(!_global.exists(), "contract has already been initialized");

  check(is_account(admin), "bad admin");
  check(is_account(token), "bad token");

  _global.get_or_create(_self,
                        global{
                            .admin = admin,
                            .token = token,
                            .status = SYSTEM_STATUS_ON});
}

void yaostake::setstatus(const uint8_t status)
{
  check(status == SYSTEM_STATUS_OFF || status == SYSTEM_STATUS_ON, "invalid status");
  check(_global.exists(), "contract has not been initialized");
  auto glb = _global.get();
  require_auth(glb.admin);
  check(status != glb.status, "status can not be the same");
  glb.status = status;
  _global.set(glb, _self);
}

void yaostake::create(name contract, symbol sym, asset reward, uint64_t epoch_time, uint64_t duration, asset min_staked, uint8_t type)
{
  check_system_on();
  const auto glb = _global.get();
  require_auth(glb.admin);

  auto itr = _pools.begin();
  while (itr != _pools.end())
  {
    auto exists = itr->contract == contract && itr->sym == sym && itr->type == type;
    check(!exists, "token exists");
    itr++;
  }

  check(reward.symbol == MINED_SYMBOL, "invalid reward symbol");
  check(min_staked.symbol == sym, "invalid min_stake symbol");
  check(epoch_time > 0, "invalid epoch");
  check(duration > 0, "invalid duration");
  check(type == POOL_TYPE_STAKE || type == POOL_TYPE_TF, "invalid type");

  auto total = reward;
  itr = _pools.begin();
  while (itr != _pools.end())
  {
    total += itr->total_reward;
    itr++;
  }
  check(total.amount <= MAX_SUPPLY, "reach the maximal supply");
  auto pool_id = _pools.available_primary_key();
  if (pool_id == 0)
  {
    pool_id = 1;
  }

  _pools.emplace(_self, [&](auto &s) {
    s.id = pool_id;
    s.type = type;
    s.contract = contract;
    s.sym = sym;
    s.total_staked = asset(0, sym);
    s.total_reward = reward;
    s.released_reward = asset(0, reward.symbol);
    s.epoch_time = epoch_time;
    s.duration = duration;
    s.min_staked = min_staked;
    s.last_harvest_time = epoch_time;
  });
}

void yaostake::claim(name owner, uint64_t pool_id)
{
  check_system_on();
  require_auth(owner);
  auto itr_pool = _pools.require_find(pool_id, "pool does not exists");
  miners _miners(_self, pool_id);
  auto itr_mine = _miners.require_find(owner.value, "miner does not exists");
  check(itr_mine->unclaimed.amount > 0, "no rewards to claim");
  const auto glb = _global.get();
  auto quantity = itr_mine->unclaimed;
  _miners.modify(itr_mine, same_payer, [&](auto &s) {
    s.claimed += quantity;
    s.unclaimed = asset(0, quantity.symbol);
  });

  utils::inline_transfer(glb.token, _self, owner, quantity, string("claim rewards"));
}

void yaostake::harvest(uint64_t pool_id, uint64_t nonce)
{
  check_system_on();
  const auto glb = _global.get();
  require_auth(glb.admin);

  auto itr_pool = _pools.require_find(pool_id, "pool does not exists");
  auto now_time = current_time_point().sec_since_epoch();
  check(now_time >= itr_pool->epoch_time, "mining hasn't started yet");
  check(now_time <= itr_pool->epoch_time + itr_pool->duration, "mining is over");
  check(itr_pool->total_staked.amount > 0, "no staked tokens");

  auto supply_per_second = safemath::div(itr_pool->total_reward.amount, uint64_t(itr_pool->duration));
  auto time_elapsed = now_time - itr_pool->last_harvest_time;
  auto token_issued = asset(safemath::mul(uint64_t(time_elapsed), supply_per_second), itr_pool->released_reward.symbol);
  check(token_issued.amount > 0, "invalid token_issued");
  _pools.modify(itr_pool, same_payer, [&](auto &s) {
    s.released_reward += token_issued;
    s.last_harvest_time = now_time;
  });

  // issue
  const auto issuer = get_issuer(glb.token, itr_pool->total_reward.symbol.code());
  utils::issue(issuer, _self, token_issued);

  // update every miner
  miners _miners(_self, itr_pool->id);
  auto itr_mine = _miners.begin();
  check(itr_mine != _miners.end(), "no miners");
  while (itr_mine != _miners.end())
  {
    double ratio = (double)(itr_mine->staked.amount) / itr_pool->total_staked.amount;
    uint64_t amount = (uint64_t)(token_issued.amount * ratio);
    check(amount >= 0, "invalid amount");
    asset mined = asset(amount, token_issued.symbol);
    asset to_invitor = mined / 20;
    asset to_user = mined - to_invitor;
    if (to_invitor.amount >= 1) {
      if (itr_mine->invitor != _self) {
        // invitor
        auto itr_invitor = _miners.require_find(itr_mine->invitor.value, "not this invitor");
        _miners.modify(itr_invitor, same_payer, [&](auto &s) {
          s.unclaimed += to_invitor;
          s.refersum += to_invitor;
        });
      } else {
        // to blackhole
        utils::inline_transfer(glb.token, _self, "eosio.null"_n, to_invitor, string("burn"));
      }
    }

    if (to_user.amount >= 1) {
      _miners.modify(itr_mine, same_payer, [&](auto &s) {
        s.unclaimed += to_user;
      });
    }
    itr_mine++;
  }
}

void yaostake::handle_transfer(name from, name to, asset quantity, string memo, name code)
{
  if (from == _self || to != _self)
  {
    return;
  }
  check_system_on();
  const auto glb = _global.get();
  require_auth(from);
  vector<string> strs = utils::split(memo, ":");
  if (strs.size() == 0) {
    return;
  }
  check(strs.size() == 1 || strs.size() == 2, "invalid memo length");
  string act = strs[0];
  check(act == "stake" || act == "tf", "invalid memo action");
  name invitor = _self;
  bool has_invitor = strs.size() == 2;
  if (has_invitor) {
    invitor = name(strs[1]);
  }
  check(is_account(invitor), "invalid invitor");
  check(invitor != from, "invitor can not be self");
  uint8_t type = POOL_TYPE_STAKE;
  if (act == "tf") type = POOL_TYPE_TF;

  auto sym = quantity.symbol;
  pools _pools(_self, _self.value);
  auto itr = _pools.begin();
  while (itr != _pools.end())
  {
    if (itr->contract == code && itr->sym == sym && itr->type == type)
    {
      break;
    }
    itr++;
  }
  check(itr != _pools.end(), "pool not found");
  check(itr->contract == code && itr->sym == sym, "error token");
  check(quantity >= itr->min_staked, "the amount of staked is too small");
  auto now_time = current_time_point().sec_since_epoch();
  check(now_time <= itr->epoch_time + itr->duration, "mining is over");
  auto to_stake = quantity;
  if (itr->type == POOL_TYPE_TF)
  {
    const auto to_dev = asset(safemath::div(quantity.amount, uint64_t(1000)), quantity.symbol); // 0.1%  To Dev
    to_stake = quantity - to_dev;                                                               // 99.9% To Pool
    utils::inline_transfer(code, _self, from, to_stake, string("refund"));
    utils::inline_transfer(code, _self, glb.admin, to_dev, string("dev rewards"));
  }
  _pools.modify(itr, same_payer, [&](auto &s) {
    s.total_staked += to_stake;
  });

  auto zero = asset(0, MINED_SYMBOL);
  miners _miners(_self, itr->id);
  if (invitor != _self) {
    _miners.require_find(invitor.value, "invitor must stake first");
  }
  auto itr_mine = _miners.find(from.value);
  if (itr_mine == _miners.end())
  {
    _miners.emplace(_self, [&](auto &s) {
      s.owner = from;
      s.invitor = invitor;
      s.staked = to_stake;
      s.claimed = zero;
      s.unclaimed = zero;
      s.refersum = zero;
    });
  }
  else
  {
    if (invitor != _self) {
      check(itr_mine->invitor == invitor, "can not change invitor");
    }
    _miners.modify(itr_mine, same_payer, [&](auto &s) {
      s.staked += to_stake;
    });
  }
}
