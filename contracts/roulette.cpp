#include <string>
#include <vector>
#include <eosiolib/transaction.hpp>

#include "roulette.hpp"

#define EOS_SYMBOL S(4, EOS)
#define LKT_SYMBOL S(4, LKT)

#define EOS_CONTRACT N(eosio.token)
#define LKT_CONTRACT N(chyyshayysha)
#define BANCOR_CONTRACT N(bkyyshayysha)
#define LOG_CONTRACT N(roulettelog1)
#define DIVIDEND_ACCOUNT N(roulettedivi)
#define OFFICE_ACCOUNT N(rouletteoffi)

#define POS_NUM 40

void roulette::init() {
    require_auth(_self);
    auto g = global.begin();
    if (g == global.end()) {
      global.emplace(_self, [&](auto& g){
        g.id = 0;
        g.queue_size = 0;
        g.status = 0;
        g.eos_safe_balance = asset(40000000, EOS_SYMBOL);
        g.lkt_safe_balance = asset(5000000000, LKT_SYMBOL);
      });
    }
  }

void roulette::addhash(checksum256 hash) {
    require_auth(_self);
    hashlist.emplace(_self, [&](auto& g){
        g.hash = hash;
        g.id = hashlist.available_primary_key();
    });

}

uint64_t roulette::merge_seed(const checksum256& s1, const checksum256& s2) {
  uint64_t hash = 0, x;
  for (int i = 0; i < 32; ++i) {
      hash ^= (s1.hash[i] ^ s2.hash[i]) << ((i & 7) << 3);
  }
  return hash;
}

uint8_t roulette::char2int(char input)
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

void roulette::string2seed(const string& str, checksum256& seed) {
    for (uint8_t i = 0; i < 32; i ++) {
        seed.hash[i] = char2int(str[i*2])*16 + char2int(str[i*2+1]);
    }
}

void roulette::ontransfer(account_name from, account_name to, extended_asset quantity, std::string memo) {
    if (from == _self || to != _self) {
      return;
    }

    if (memo == "flow into roulette") {
        return;
    }

    eosio_assert(global.begin()->queue_size <= 20, "too many bet in queue" );
    eosio_assert(global.begin()->status == 1, "roulette not start yet." );


    eosio_assert(quantity.is_valid(), "Invalid token transfer");

    std::vector<std::string> inputs;
    std::vector<std::string> bet_strs;
    std::vector<uint64_t> bet_nums;

    split(memo, " ", inputs);

    if (inputs.size() == 1) {
        eosio_assert(quantity.contract == LKT_CONTRACT, "Only accept LKT");
        bebanker(from, quantity, stoi(memo));
        return;
    }
    
    eosio_assert(inputs.size() == 2, "invalid input" );
    eosio_assert(inputs[0].size() == 64, "invalid checksum" );
    checksum256 seed;
    string2seed(inputs[0], seed);

    split(inputs[1], ",", bet_strs);

    eosio_assert(0 < bet_strs.size() && bet_strs.size() < 39, "should choose more than zero and less than 39 numbers");
    eosio_assert( bets.find(from) == bets.end(), "The last game have not finished yet." );

    auto hash_itr = hashlist.begin();
    eosio_assert( hash_itr != hashlist.end(), "hash list empty");

    if (quantity.contract == EOS_CONTRACT) {
        asset eos_balance = token(EOS_CONTRACT).get_balance(_self, EOS_SYMBOL);
        eosio_assert( global.begin()->eos_safe_balance.amount < eos_balance.amount, "under safe, can not play" );
        eosio_assert(quantity.amount >= 10000, "Bet must large than 1 EOS");
        eosio_assert(quantity.amount * 97 * 40 * 30 / ( 100 * bet_strs.size() ) < eos_balance.amount, "can not too large");
    } else if (quantity.contract == LKT_CONTRACT) {
        eosio_assert(false, "LKT not start yet");
        asset lkt_balance = token(LKT_CONTRACT).get_balance(_self, LKT_SYMBOL);
        eosio_assert( global.begin()->lkt_safe_balance.amount < lkt_balance.amount, "under safe, can not play" );
        eosio_assert(quantity.amount >= 10000, "Bet must large than 1 LKT");
        eosio_assert(quantity.amount * 97 * 40 * 30 / ( 100 * bet_strs.size() ) < lkt_balance.amount, "can not too large");
    } else {
        eosio_assert( false, "Invalid code" );
    }


    bets.emplace(_self, [&](auto& item){
        item.account = from;
        for (uint64_t i = 0; i < bet_strs.size(); i ++) {
            uint64_t number = stoi(bet_strs[i]);
            eosio_assert(number >= 1 && number <= 40, "invalid range");
            item.bet_nums.push_back(number);
        }
        item.house_hash = hash_itr->hash;
        item.seed = seed;
        item.bet_amount = quantity;
    });
    global.modify(global.begin(), 0, [&](auto &g) {
      g.queue_size = g.queue_size + 1;
    });
    hashlist.erase(hash_itr);

}

void roulette::bebanker(account_name from, extended_asset quantity, uint64_t pos) {
    
    auto itr = bankers.find(pos);

    if (itr == bankers.end()) {

        eosio_assert(quantity.amount == 250000000, "lkt amount error");

        bankers.emplace(_self, [&](auto& banker) {
            banker.pos = pos;
            banker.owner = from;
            banker.bid = asset(quantity.amount, LKT_SYMBOL);
            banker.dividend = asset(0, EOS_SYMBOL);
            banker.history_dividend = asset(0, EOS_SYMBOL);
        });
    } else {

        asset need_lkt = asset(250000000, LKT_SYMBOL);
        bool clear_dividend = false;

        // not belong system
        if (itr->owner != _self) {
            need_lkt = asset(itr->bid.amount + 50000000, LKT_SYMBOL);
        }

        eosio_assert(quantity.amount == need_lkt.amount, "lkt amount error");

        // If it was not owned, the dividend give who buy it
        if (itr->dividend.amount > 0 && itr->owner != _self) {
            // transfer rest dividend 
            account_name dividend_owner = itr->owner;

            action(
                permission_level{DIVIDEND_ACCOUNT, N(active)},
                EOS_CONTRACT, 
                N(transfer),
                make_tuple(DIVIDEND_ACCOUNT, dividend_owner, itr->dividend, std::string("Return dividend")))
            .send();
            clear_dividend = true;
        }
        
        // return back lkt to the previous owner
        if (itr->owner != _self) {
            action(
                permission_level{_self, N(active)},
                LKT_CONTRACT, 
                N(transfer),
                make_tuple(_self, itr->owner, itr->bid, std::string("Return LKT")))
            .send();
        }
        
        bankers.modify(itr, 0, [&](auto &banker) {
            banker.owner = from;
            banker.bid = quantity;
            if (clear_dividend == true) {
                banker.dividend = asset(0, EOS_SYMBOL);
            }
        });

    }
    
}

void roulette::reveal(account_name player, checksum256& seed, checksum256& hash) {

    require_auth(_self);

    auto bet_itr = bets.find(player);

    assert_sha256( (char *)&seed, sizeof(seed), (const checksum256 *)& bet_itr->house_hash );

    rock(*bet_itr, seed);

    bets.erase(bet_itr);
    global.modify(global.begin(), 0, [&](auto &g) {
      g.queue_size = g.queue_size - 1;
    });

    hashlist.emplace(_self, [&](auto& g){
        g.id = hashlist.available_primary_key();
        g.hash = hash;
    });
}

void roulette::rewardbanker(uint64_t reveal_pos, uint64_t reward_amount) {

    auto itr = bankers.find(reveal_pos);
    
    if (itr == bankers.end()) {
        bankers.emplace(_self, [&](auto& banker) {
            banker.pos = reveal_pos;
            banker.owner = _self;
            banker.bid = asset(0, LKT_SYMBOL);
            banker.dividend = asset(reward_amount, EOS_SYMBOL);
            banker.history_dividend = asset(reward_amount, EOS_SYMBOL);
        });
    } else {
        bankers.modify(itr, 0, [&](auto &banker) {
            banker.dividend.amount += reward_amount;
            banker.history_dividend.amount += reward_amount;
        });
    }

    action(
        permission_level{_self, N(active)},
        EOS_CONTRACT, 
        N(transfer),
        make_tuple(_self, DIVIDEND_ACCOUNT, asset(reward_amount, EOS_SYMBOL), std::string("Flow to dividend pool")))
    .send();
}


void roulette::rewarddev(uint64_t reward_amount) {
    require_auth(_self);
    action(
        permission_level{_self, N(active)},
        EOS_CONTRACT, 
        N(transfer),
        make_tuple(_self, OFFICE_ACCOUNT, asset(reward_amount, EOS_SYMBOL), std::string("Reward Developer")))
    .send();
}

void roulette::flowbancor(uint64_t flow_amount) {
    require_auth(_self);
    action(
        permission_level{_self, N(active)},
        BANCOR_CONTRACT, N(flowtoquote),
        make_tuple( _self, asset(flow_amount, EOS_SYMBOL), std::string("roulette dividend") ))
    .send();
}

void roulette::rock(betitem item, const checksum256& reveal_seed) {

    require_auth(_self);

    uint64_t reveal_pos = merge_seed(reveal_seed, item.seed) % POS_NUM + 1;
    uint64_t reveal_type;
 
    extended_asset bonus = item.get_bonus(reveal_pos);

    // reward 0.5% to banker
    rewardbanker(reveal_pos, item.bet_amount.amount / 200);

    // reward 0.5% to dev
    rewarddev(item.bet_amount.amount / 200);

    // flow 0.5% to LKT POOL
    flowbancor(item.bet_amount.amount / 200);

    // win
    if (bonus.amount > 0) {

        if (item.bet_amount.contract == EOS_CONTRACT) {
            action(
                permission_level{_self, N(active)},
                LOG_CONTRACT, N(result),
                make_tuple(std::string("win"), item.account, asset(item.bet_amount.amount, EOS_SYMBOL), asset(bonus.amount, EOS_SYMBOL), item.get_bets(), reveal_pos, item.seed, reveal_seed, current_time()))
            .send();
        } else if (item.bet_amount.contract == LKT_CONTRACT) {
            action(
                permission_level{_self, N(active)},
                LOG_CONTRACT, N(result),
                make_tuple(std::string("win"), item.account, asset(item.bet_amount.amount, LKT_SYMBOL), asset(bonus.amount, LKT_SYMBOL), item.get_bets(), reveal_pos, item.seed, reveal_seed, current_time()))
            .send();
        }

        transaction trx;
        trx.actions.emplace_back(permission_level{_self, N(active)},
                bonus.contract,
                N(transfer),
                make_tuple(_self, item.account, asset(bonus.amount, bonus.symbol), std::string("Congratulations! "))
        );
        trx.delay_sec = 1;
        trx.send(item.account, _self, false);

    } else {

        if (item.bet_amount.contract == EOS_CONTRACT) {
            action(
                permission_level{_self, N(active)},
                LOG_CONTRACT, N(result),
                make_tuple(std::string("lost"), item.account, asset(item.bet_amount.amount, EOS_SYMBOL), asset(bonus.amount, EOS_SYMBOL), item.get_bets(), reveal_pos, item.seed, reveal_seed, current_time()))
            .send();
        } else if (item.bet_amount.contract == LKT_CONTRACT) {
            action(
                permission_level{_self, N(active)},
                LOG_CONTRACT, N(result),
                make_tuple(std::string("lost"), item.account, asset(item.bet_amount.amount, LKT_SYMBOL), asset(bonus.amount, LKT_SYMBOL), item.get_bets(), reveal_pos, item.seed, reveal_seed, current_time()))
            .send();
        }
    }
}


void roulette::clearhash() {
    require_auth(_self);
    while (hashlist.begin() != hashlist.end()) {
        hashlist.erase(hashlist.begin());
    }
}

void roulette::update(uint64_t status, asset eos_safe_balance, asset lkt_safe_balance) {
    require_auth(_self);
    global.modify(global.begin(), 0, [&](auto& g) {
       g.status = status;
       g.eos_safe_balance = eos_safe_balance;
       g.lkt_safe_balance = lkt_safe_balance;
    });
}

void roulette::withdraw(account_name owner) {

    require_auth(owner);

    asset total_dividend = asset(0, EOS_SYMBOL);

    for (auto itr = bankers.begin(); itr != bankers.end(); itr ++) {
        if (itr->owner == owner && itr->dividend.amount > 0) {
            total_dividend.amount += itr->dividend.amount;
            bankers.modify(itr, 0, [&](auto &banker) {
                    banker.dividend = asset(0, EOS_SYMBOL);
            });
        }

    }

    eosio_assert(total_dividend.amount > 0, "no dividend to withdraw");

    action(
        permission_level{DIVIDEND_ACCOUNT, N(active)},
        EOS_CONTRACT, 
        N(transfer),
        make_tuple(DIVIDEND_ACCOUNT, owner, total_dividend, std::string("Get dividend")))
    .send();
}

void roulette::sell(uint64_t pos, account_name owner) {
    require_auth(owner);

    auto itr = bankers.find(pos);

    eosio_assert(itr != bankers.end(), "not exist");
    eosio_assert(itr->owner == owner, "the banker not belong to you");

    // transfer rest dividend 
    if (itr->dividend.amount > 0) {
        action(
            permission_level{DIVIDEND_ACCOUNT, N(active)},
            EOS_CONTRACT, 
            N(transfer),
            make_tuple(DIVIDEND_ACCOUNT, owner, itr->dividend, std::string("Return dividend")))
        .send();
    }

    // return 80% lkt
    asset return_lkt = itr->bid;
    return_lkt.amount = itr->bid.amount * 80 / 100;
    action(
        permission_level{_self, N(active)},
        LKT_CONTRACT, 
        N(transfer),
        make_tuple(_self, itr->owner, return_lkt, std::string("Return LKT")))
    .send();

    bankers.modify(itr, 0, [&](auto &banker) {
            banker.dividend = asset(0, EOS_SYMBOL);
            banker.owner = _self;
            banker.bid = asset(0, LKT_SYMBOL);
    });
}

struct transfer_args
{
    account_name from;
    account_name to;
    asset quantity;
    string memo;
};

extern "C"
{
    void apply(uint64_t receiver, uint64_t code, uint64_t action)
    {
        auto self = receiver;
        roulette thiscontract(self);
        if (code == self)
        {
            switch (action)
            {
                EOSIO_API(roulette, (init)(addhash)(reveal)(update)(clearhash)(withdraw)(sell))
            }
        }
        else
        {
            if ( action == N(transfer) && (code == LKT_CONTRACT || code == EOS_CONTRACT) )
            {
                auto transfer_data = unpack_action_data<transfer_args>();
                thiscontract.ontransfer(transfer_data.from, transfer_data.to, extended_asset(transfer_data.quantity, code), transfer_data.memo);
            }
        }
    }
}


//EOSIO_ABI(roulette, (init)(addhash)(reveal)(update)(clearhash)(withdraw)(sell))
