#include <string>
#include <vector>

#include "slot_machine.hpp"

#define EOS_SYMBOL S(4, EOS)
#define LKT_SYMBOL S(4, LKT)

#define TOKEN_CONTRACT N(eosio.token)
#define LKT_CONTRACT N(chyyshayysha)
#define BANCOR_CONTRACT N(bkyyshayysha)

void slot_machine::init(const checksum256& hash) {
    require_auth(_self);
    auto g = global.find(0);
    if (g == global.end()) {
      global.emplace(_self, [&](auto& g){
        g.id = 0;
        g.hash = hash;
        g.trade_pos = 0;
        g.queue_size = 0;
        g.status = 0;
        g.lkt_bonus_rate = 0;
      });
    } else {
      global.modify(g, 0, [&](auto& g) {
        g.hash = hash;
      });
    }
  }

uint64_t slot_machine::merge_seed(const checksum256& s1, const checksum256& s2) {
  uint64_t hash = 0, x;
  for (int i = 0; i < 32; ++i) {
      hash ^= (s1.hash[i] ^ s2.hash[i]) << ((i & 7) << 3);
  }
  return hash;
}

uint64_t slot_machine::get_bonus(uint64_t seed, uint64_t amount, uint64_t bet_type) {
  seed %= 216;
  uint64_t i = 0;
  while (seed >= tps[i]) {
      seed -= tps[i];
      ++i;
  }
  if (bet_type == i) {
      return (uint64_t)(pl[i]*amount);
  } else {
      return 0;
  }
}

uint8_t slot_machine::char2int(char input)
{

    if(input >= '0' && input <= '9') {
        return input - '0';
    }
    if(input >= 'A' && input <= 'F') {
        return input - 'A' + 10;
    }
    if(input >= 'a' && input <= 'f') {
        return input - 'a' + 10;
    }
    eosio_assert(false, "invalid checksum" );
    return 0;
}

void slot_machine::string2seed(const string& str, checksum256& seed) {
    for (uint8_t i = 0; i < 32; i ++) {
        seed.hash[i] = char2int(str[i*2])*16 + char2int(str[i*2+1]);
    }
}

void slot_machine::transfer(account_name from, account_name to, asset quantity, std::string memo) {
    if (from == _self || to != _self) {
      return;
    }

    if (memo == "flow into slot machine") {
        return;
    }

    eosio_assert(quantity.is_valid(), "Invalid token transfer");
    eosio_assert(quantity.amount > 0, "Quantity must be positive");
    eosio_assert(quantity.amount >= 10000, "Bet must large than 1 EOS");

    std::vector<std::string> inputs;
	
    if (quantity.symbol == EOS_SYMBOL) {
		split(memo, " ", inputs);

        eosio_assert(inputs.size() == 2, "invalid input" );
        eosio_assert(inputs[1].size() == 64, "invalid checksum" );

        uint64_t type = stoi(inputs[0]);
        eosio_assert( type <= 3, "type invalid" );

        asset pool_balance = token(TOKEN_CONTRACT).get_balance(_self, EOS_SYMBOL);

        eosio_assert( quantity.amount < pool_balance.amount, "can not large than pool");
        eosio_assert( quantity.amount * pl[type] * 30 <  pool_balance.amount, "can not too large");

        checksum256 seed;
        string2seed(inputs[1], seed);

        bet(from, type, quantity, seed);
    }
}

void slot_machine::bet(account_name account, uint64_t type, asset quantity, const checksum256& seed ) {

    require_auth(account);

    eosio_assert(quantity.amount > 0, "must purchase a positive amount");
    eosio_assert(quantity.symbol == EOS_SYMBOL, "only EOS allowed" );

    auto p = queues.find(account);

    eosio_assert( p == queues.end(), "The last game have not finished yet." );

    p = queues.emplace(_self, [&](auto& item){
        item.account = account;
    });

    auto global_itr = global.find(0);
    eosio_assert(global_itr->queue_size <= 20, "too many bet in queue" );

    global.modify(global_itr, 0, [&](auto &g) {
      g.queue_size = g.queue_size + 1;
    });

    eosio_assert(global_itr->status == 1, "slot machine not start yet." );

    player_index playertable(_self, account);
    auto player_itr = playertable.begin();
    if (player_itr == playertable.end()) {
        playertable.emplace(account, [&](auto& plyer) {
            plyer.status = 1;
            plyer.seed = seed;
            plyer.bet_type = type;
            plyer.bet_amount = quantity;
        });
    } else {
        playertable.modify(*player_itr, 0, [&](auto& plyer) {
            plyer.status = 1;
            plyer.seed = seed;
            plyer.bet_type = type;
            plyer.bet_amount = quantity;
        });
    }

    asset lkt_balance = token(LKT_CONTRACT).get_balance(_self, LKT_SYMBOL);


    uint64_t lkt_back = quantity.amount * (global_itr->lkt_bonus_rate) / 100;
    if (lkt_back > 0 && lkt_balance.amount >= lkt_back) {
        action(
            permission_level{_self, N(active)},
            LKT_CONTRACT, N(transfer),
            make_tuple(_self, account, asset(lkt_back, LKT_SYMBOL), std::string("Play slot machine get lkt, have fun!")))
            .send();
    }

}

void slot_machine::reveal(checksum256& seed, checksum256& hash) {

    require_auth(_self);

    assert_sha256( (char *)&seed, sizeof(seed), (const checksum256 *)& global.begin()->hash );

    uint64_t cnt = 0;
    for (; queues.begin() != queues.end() ;) {
      rock(*queues.begin(), seed);
      queues.erase(queues.begin());
      cnt ++;
      if (cnt == 5) break;
    }

    auto itr = global.find(0);
    global.modify(itr, 0, [&](auto &g) {
      g.hash = hash;
      eosio_assert(g.queue_size >= cnt, "serious error" );
      g.queue_size = g.queue_size - cnt;
    });
}

void slot_machine::rock(const queueitem&  item, const checksum256& seed) {
    player_index playertable(_self, item.account);
    auto player_itr = playertable.begin();
    uint64_t result_seed = merge_seed(seed, player_itr->seed);
    uint64_t bonus = get_bonus(result_seed, player_itr->bet_amount.amount, player_itr->bet_type);
    uint64_t slot_pos = result_seed % 216;
    if (bonus > 0) {
        auto itr = global.find(0);
        global.modify(itr, 0, [&](auto &g) {
            g.add_trade(global::trade_info{item.account, asset(bonus, EOS_SYMBOL), player_itr->bet_amount, player_itr->bet_type, current_time()});
        });
        action(
            permission_level{_self, N(active)},
            _self, N(result),
            make_tuple(std::string("win"), item.account, asset(bonus, EOS_SYMBOL), player_itr->bet_type, player_itr->seed))
        .send();

        // earn 
        action(
            permission_level{_self, N(active)},
            TOKEN_CONTRACT, N(transfer),
            make_tuple(_self, item.account, asset(bonus, EOS_SYMBOL), std::string("Congratulations!")))
            .send();

    } else {
        action(
            permission_level{_self, N(active)},
            _self, N(result),
            make_tuple(std::string("lost"), item.account, asset(0, EOS_SYMBOL), player_itr->bet_type,  player_itr->seed))
        .send();
    }

    playertable.modify(*player_itr, 0, [&](auto & plyer) {
        plyer.status = 0;
        plyer.win_amount = asset(bonus, EOS_SYMBOL);
        plyer.slot_pos = slot_pos;
    });
}

// just for log
void slot_machine::result( std::string result, account_name account, asset win_amount,  uint64_t bet_type,  checksum256 player_seed ) 
{
    require_auth(_self);
}

void slot_machine::update(uint64_t status, uint64_t lkt_bonus_rate) {
    auto itr = global.find(0);
    global.modify(itr, 0, [&](auto& g) {
        g.status = status;
        g.lkt_bonus_rate = lkt_bonus_rate;
    });
}

void slot_machine::flowtobancor() {

    require_auth(_self);

    asset pool_balance = token(TOKEN_CONTRACT).get_balance(_self, EOS_SYMBOL);
    eosio_assert(pool_balance.amount > 50000000, "the pool must be large than init" );

    // every time only flow 5%
    uint64_t flow_amount = pool_balance.amount * 5 / 100;


    action(
        permission_level{_self, N(active)},
        BANCOR_CONTRACT, N(flowtoquote),
        make_tuple( _self, asset(flow_amount, EOS_SYMBOL), std::string("slot machine earn!") ))
    .send();
}

void slot_machine::erase()
{
    require_auth(_self);
    global.erase(global.find(0));

    for (; queues.begin() != queues.end() ;) {
      queues.erase(queues.begin());
    }

}

#undef EOSIO_ABI
#define EOSIO_ABI(TYPE, MEMBERS)                                                                                                              \
  extern "C" {                                                                                                                                    \
  void apply(uint64_t receiver, uint64_t code, uint64_t action)                                                                                   \
  {                                                                                                                                               \
    auto self = receiver;                                                                                                                         \
    if (action == N(onerror))                                                                                                                     \
    {                                                                                                                                             \
      eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account");                                        \
    }                                                                                                                                             \
    if ((code == TOKEN_CONTRACT && action == N(transfer)) || (code == self && (action != N(transfer) ))) \
    {                                                                                                                                             \
      TYPE thiscontract(self);                                                                                                                    \
      switch (action)                                                                                                                             \
      {                                                                                                                                           \
        EOSIO_API(TYPE, MEMBERS)                                                                                                                  \
      }                                                                                                                                           \
    }                                                                                                                                             \
  }                                                                                                                                               \
}

// generate .wasm and .wast file
EOSIO_ABI(slot_machine, (transfer)(init)(reveal)(result)(update)(flowtobancor)(erase))
