#include <string>
#include <vector>
#include <eosiolib/transaction.hpp>

#include "slot_machine.hpp"

#define EOS_SYMBOL S(4, EOS)
#define LKT_SYMBOL S(4, LKT)

#define TOKEN_CONTRACT N(eosio.token)
#define LKT_CONTRACT N(chyyshayysha)
#define BANCOR_CONTRACT N(bkyyshayysha)
#define LOG_CONTRACT N(luckyslotlog)

void slot_machine::init() {
    require_auth(_self);
    auto g = global.begin();
    if (g == global.end()) {
      global.emplace(_self, [&](auto& g){
        g.id = 0;
        g.current_id = 0;
        g.queue_size = 0;
        g.status = 0;
        g.safe_balance = asset(70000000, EOS_SYMBOL);
        g.lkt_bonus_rate = 0;
      });
    }
  }

void slot_machine::addhash(checksum256 hash) {
    require_auth(_self);
    hashlist.emplace(_self, [&](auto& g){
        g.hash = hash;
        g.id = hashlist.available_primary_key();
    });

}

uint64_t slot_machine::merge_seed(const checksum256& s1, const checksum256& s2) {
  uint64_t hash = 0, x;
  for (int i = 0; i < 32; ++i) {
      hash ^= (s1.hash[i] ^ s2.hash[i]) << ((i & 7) << 3);
  }
  return hash;
}

void slot_machine::get_bonus(uint64_t seed, uint64_t amount, uint64_t bet_type, uint64_t& bonus, uint64_t& reveal_type, uint64_t& reveal_pos ) {
  seed %= 216;
  reveal_pos = seed;
  reveal_type = 0;
  while (seed >= tps[reveal_type]) {
      seed -= tps[reveal_type];
      ++reveal_type;
  }
  if (bet_type == reveal_type) {
      bonus = (uint64_t)(pl[reveal_type]*amount);
  } else {
      bonus = 0;
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
    eosio_assert(quantity.amount >= 10000, "Bet must large than 1 EOS");

    std::vector<std::string> inputs;

    if (quantity.symbol == EOS_SYMBOL) {
                split(memo, " ", inputs);

        eosio_assert(inputs.size() == 2, "invalid input" );
        eosio_assert(inputs[1].size() == 64, "invalid checksum" );

        uint64_t type = stoi(inputs[0]);
        eosio_assert( type <= 3, "type invalid" );

        asset pool_balance = token(TOKEN_CONTRACT).get_balance(_self, EOS_SYMBOL);

                eosio_assert( global.begin()->safe_balance.amount < pool_balance.amount, "under safe, can not play");
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

    auto p = bets.find(account);

    eosio_assert( p == bets.end(), "The last game have not finished yet." );

    p = bets.emplace(_self, [&](auto& item){
        item.account = account;
        item.bet_type = type;
        item.seed = seed;
        item.bet_amount = quantity;
    });

    auto global_itr = global.begin();
    eosio_assert(global_itr->queue_size <= 20, "too many bet in queue" );

    global.modify(global_itr, 0, [&](auto &g) {
      g.queue_size = g.queue_size + 1;
    });

    eosio_assert(global_itr->status == 1, "slot machine not start yet." );

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

// For fair, bet queue size should always small than hash queue size. 
void slot_machine::reveal(checksum256& seed, checksum256& hash) {

    require_auth(_self);

    auto hashitem = hashlist.begin();

    assert_sha256( (char *)&seed, sizeof(seed), (const checksum256 *)& hashitem->hash );

        rock(*bets.begin(), seed);
        bets.erase(bets.begin());

    global.modify(global.begin(), 0, [&](auto &g) {
      g.queue_size = g.queue_size - 1;
    });
        hashlist.erase(hashitem);
    hashlist.emplace(_self, [&](auto& g){
        g.id = hashlist.available_primary_key();
        g.hash = hash;
    });
}

void slot_machine::rock(const betitem& item, const checksum256& reveal_seed) {

        require_auth(_self);

    uint64_t result_seed = merge_seed(reveal_seed, item.seed);
    uint64_t reveal_pos;
    uint64_t bonus;
    uint64_t reveal_type;
 
    get_bonus(result_seed, item.bet_amount.amount, item.bet_type, bonus, reveal_type, reveal_pos);

    if (bonus > 0) {
        action(
            permission_level{_self, N(active)},
            LOG_CONTRACT, N(result),
            make_tuple(std::string("win"), item.account, item.bet_amount, asset(bonus, EOS_SYMBOL), item.bet_type, reveal_type, reveal_pos, item.seed, reveal_seed, current_time()))
        .send();

        transaction trx;
        trx.actions.emplace_back(permission_level{_self, N(active)},
                TOKEN_CONTRACT,
                N(transfer),
                make_tuple(_self, item.account, asset(bonus, EOS_SYMBOL), std::string("Congratulations!"))
        );
        trx.delay_sec = 1;
        trx.send(item.account, _self, false);

    } else {
        action(
            permission_level{_self, N(active)},
            LOG_CONTRACT, N(result),
            make_tuple(std::string("lost"), item.account, item.bet_amount, asset(0, EOS_SYMBOL), item.bet_type, reveal_type, reveal_pos, item.seed, reveal_seed, current_time()))
        .send();
    }
}


void slot_machine::update(uint64_t status, uint64_t lkt_bonus_rate, asset safe_balance) {
    require_auth(_self);
    auto itr = global.begin();
    global.modify(itr, 0, [&](auto& g) {
        g.status = status;
        g.lkt_bonus_rate = lkt_bonus_rate;
        g.safe_balance = safe_balance;
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
EOSIO_ABI(slot_machine, (transfer)(init)(addhash)(reveal)(update)(flowtobancor))
