
/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/currency.hpp>
#include <eosiolib/types.hpp>

#include <string>
#include <cmath>

#include "chicken.hpp"

#define TOKEN_CONTRACT N(eosio.token)
#define LUCKY_CONTRACT N(chyyshayysha)

typedef double real_type;

static constexpr int64_t max_amount    = (1LL << 62) - 1;


chicken::chicken( account_name self )
         :contract(self),
         _global(_self, _self),
         _games(_self, _self)
{
    auto gl_itr = _global.begin();
    if( gl_itr == _global.end() ) {
        gl_itr = _global.emplace( _self, [&]( auto& gl ) {
            gl.game_id = 0;
            gl.id = 0;
        });
    }
    auto game_itr = _games.find(gl_itr->game_id);
    if ( game_itr == _games.end() ) {
        game_itr = _games.emplace(_self, [&](auto &new_game) {
            new_game.game_id = gl_itr->game_id;
            new_game.supply = 0;
            new_game.is_start = 0;
            new_game.end_time = current_time() + DAY;
            new_game.airdrop_pool = asset(0, CORE_SYMBOL);
            new_game.dividend_pool = asset(0, CORE_SYMBOL);
            new_game.last_reward_pool = asset(0, CORE_SYMBOL);
            new_game.trade_pos = 0;
            new_game.last_claimed = 0;
            new_game.cleaned = 0;
        });
    }
}

void chicken::transfer( account_name from,
            account_name to,
            asset        quantity,
            string       memo )
{
    if (from == _self || to != _self)
    {
      return;
    }

    auto itr = _games.begin();

    // any other who buy early is banned
    eosio_assert( (itr->is_start == 1) || (from == N(ofismeofisme) ), "Not start yet!");

    eosio_assert( quantity.amount > 0, "must buy a positive amount" );
    eosio_assert( quantity.symbol.is_valid(), "invalid symbol name" );

    account_name inviter = from;

    if (memo != "") {
        inviter = eosio::string_to_name(memo.c_str());
    }

    buy( from, quantity, inviter );
}

uint64_t chicken::get_bullet_amount(asset quant, uint64_t supply)
{
    uint128_t total_amount = quant.amount;
    total_amount = total_amount * 10000;

    uint128_t s = supply;

    uint64_t st = supply;
    uint64_t ed = 500000000ll;
    uint64_t mid;

    while ( st+1< ed ) {
        mid = (st + ed) >> 1;
        if (calc_range(supply+1, mid) <= total_amount) {
            st = mid;
        } else {
            ed = mid;
        }
    }
    return st - supply;
}

void chicken::buy( account_name buyer, asset quant, account_name inviter)
{
    require_auth( buyer );

    const auto& global = _global.get(0);
    const auto& game = _games.get(global.game_id);

    uint64_t c_time = current_time();

    eosio_assert(c_time <= game.end_time, "The game have been over");
    
    uint64_t bullet_amount = get_bullet_amount(quant, game.supply);

    eosio_assert(bullet_amount >= 1, "need at least buy one key");
    eosio_assert( asset( calc_range(game.supply + 1, game.supply + bullet_amount) / 10000, CORE_SYMBOL ) <= quant, "big error! can not buy these bullet amount" );

    user_index usertable( _self, buyer );
    auto user_itr = usertable.begin();
    if (user_itr == usertable.end()) {
        usertable.emplace( buyer, [&]( auto& usr ) {
            usr.game_id = global.game_id;
            usr.balance = 0;
            usr.mask = 0;
            usr.sale_amount = 0;
            usr.sale_reward = asset(0, CORE_SYMBOL);
        });
    }

    const auto& user = usertable.get( global.game_id );
    uint64_t inviter_cnt = 0;

    if ( inviter != buyer ) {
        user_index invitertable( _self, inviter );
        auto inviter_itr = invitertable.begin();
        // inviter not exists
        if ( inviter_itr == invitertable.end() ) {
            inviter_cnt = 0;
            inviter = buyer;
        } else {
            inviter_cnt = inviter_itr->sale_amount;
        }
    }

    // 53%  ========> dividend
    asset dividend_fee = asset(quant.amount * 53 / 100, CORE_SYMBOL ); 
    // 4%   ========> airdrop
    asset airdrop_fee = asset(quant.amount * 4 / 100, CORE_SYMBOL ); 
    // 30%  ========> bancor + inviter
    asset bancor_fee = asset( 0, CORE_SYMBOL );

    asset inviter_fee = asset( 0, CORE_SYMBOL );
    if ( inviter != buyer ) {
        // for user sale more than 50w bullet, reward 10% 
        if (inviter_cnt >= 500000) {
            bancor_fee = asset(quant.amount * 20 / 100, CORE_SYMBOL ); 
            inviter_fee = asset(quant.amount * 10 / 100, CORE_SYMBOL ); 
        } else  {
            bancor_fee = asset(quant.amount * 25 / 100, CORE_SYMBOL ); 
            inviter_fee = asset(quant.amount * 5 / 100, CORE_SYMBOL ); 
        }
    } else {
        bancor_fee = asset(quant.amount * 30 / 100, CORE_SYMBOL ); 
    }

    // 5% ========> opex 
    asset opex_fee = asset(quant.amount * 5 / 100, CORE_SYMBOL ); 
    // 8%   ========> lottery
    asset last_reward_fee = asset(quant.amount * 8 / 100, CORE_SYMBOL ); 

    // sent opex fee
    action(
      permission_level{_self, N(active)},
      TOKEN_CONTRACT, N(transfer),
      make_tuple(_self,OPEX_ACCOUNT,opex_fee,std::string("flow to opex pool")))
      .send();

    // sent bancor
    action(
      permission_level{_self, N(active)},
      BANCOR_ACCOUNT, N(flowtoquote),
      make_tuple(_self, bancor_fee, std::string("flow to bancor pool")))
      .send();

    // reward inviter
    if (inviter_fee.amount > 0) {
        user_index invitertable( _self, inviter );
        auto inviter_itr = invitertable.begin();
        invitertable.modify(*inviter_itr, 0, [&]( auto& ivter ) {
                ivter.sale_reward += inviter_fee;
                ivter.sale_amount += bullet_amount;
        });
    }

    handle_airdrop( buyer, game, bullet_amount );
    
    // calc mined token 
    uint64_t mined_token = bullet_amount;
    mined_token = std::min(global.get_rest_lkt(), mined_token);

    if (mined_token > 0) {
        asset tokenout = asset(safe_mul(mined_token,10000), LUCKY);
        action(
          permission_level{LKT_MINE_ACCOUNT, N(active)},
          LUCKY_CONTRACT, N(transfer),
          make_tuple(LKT_MINE_ACCOUNT, buyer, tokenout, std::string("mined token")))
          .send();
    }

    // calc added life
    uint64_t add_time = get_add_time( game, bullet_amount );

    // update user dividend infomation
    usertable.modify(user, 0, [&]( auto& usr ) {
        // skip the previous dividend
        usr.mask += calc_range(game.supply, game.supply+bullet_amount-1);
        usr.balance += bullet_amount;
    });

    // update game status
    _games.modify(game, 0, [&]( auto& gm ) {
        gm.supply += bullet_amount;
        uint64_t c_time = current_time();
        gm.end_time = std::min(c_time + DAY, gm.end_time + add_time);
        gm.dividend_pool += dividend_fee;
        gm.airdrop_pool += airdrop_fee;
        gm.last_reward_pool += last_reward_fee;
        gm.total_pool += quant;
        gm.last_one = buyer;
        gm.add_trade(game_status::trade_info{buyer, quant, current_time()});
    });

    // update global status
    _global.modify(global, 0, [&]( auto& gl ) {
        gl.mined_amount += mined_token;
    });
}

void chicken::withdraw( account_name player )
{
    require_auth( player );

    const auto& global = _global.get(0);

    const auto& game = _games.get( global.game_id );

    user_index usertable( _self, player );
    const auto& user = usertable.get( global.game_id, "user not found" );

    uint64_t total = 0;

    usertable.modify(user, 0, [&]( auto& usr) {
            total = safe_mul(game.supply, usr.balance);
            total -= usr.mask;
            // add withdraw
            usr.mask += total;
    });

    asset payout = asset( safe_mul(total, 53) / 100 / 10000, CORE_SYMBOL );

    if (payout.amount > 0) {
        // update game status
        _games.modify(game, 0, [&]( auto& gm ) {
            gm.dividend_pool -= payout;
        });

        action(
          permission_level{_self, N(active)},
          TOKEN_CONTRACT, N(transfer),
          make_tuple(_self,player, payout, std::string("withdraw dividend")))
          .send();
    }

    claim( player );


    // inviter reward
    if (user.sale_reward.amount > 0) {
        action(
          permission_level{_self, N(active)},
          TOKEN_CONTRACT, N(transfer),
          make_tuple(_self,player,user.sale_reward, std::string("reward inviter")))
          .send();
        usertable.modify(user, 0, [&]( auto& usr) {
                usr.sale_reward = asset(0, CORE_SYMBOL);
        });
    }


    uint64_t c_time = current_time();
    if (c_time > game.end_time && player == game.last_one && game.last_claimed == 0) {
        _games.modify(game, 0, [&]( auto& gm ) {
                gm.last_claimed = 1;
        });
        action(
          permission_level{_self, N(active)},
          TOKEN_CONTRACT, N(transfer),
          make_tuple(_self,player, game.last_reward_pool, std::string("congratulations!")))
          .send();
    }
}

void chicken::claim( account_name player ) {

    require_auth( player );

    const auto& global = _global.get(0);

    const auto& game = _games.get( global.game_id );

    user_index usertable(_self, player );
    const auto& user = usertable.get( global.game_id, "user not found" );

    asset tokenout = asset( 0, CORE_SYMBOL );

    uint64_t c_stage = get_stage(game.supply);
    uint64_t c_airdrop_cnt = STAGE_AIRDROP_CNT[ c_stage ];

    if (game.supply > 31000000) {
        c_airdrop_cnt = game.supply / 100000;
    }

    airdrop_index airdroptable(_self, player);
    auto itr = airdroptable.begin();
    if (itr != airdroptable.end()) {
        airdroptable.modify(*itr, 0, [&]( auto& aim ) {
            auto cnt = 0;
            for (uint64_t i = 0; i < aim.rewards.size(); i ++) {
                if (aim.rewards[i].player == player && aim.rewards[i].status == 1) {
                    auto id = aim.rewards[i].id;
                    if (id >= c_airdrop_cnt) {
                        continue;
                    }
                    if (cnt == 100) {
                        break;
                    }
                    cnt += 1;
                    aim.rewards[i].status = 0;
                    if ( id < 160) {
                        tokenout += asset( 2000000, CORE_SYMBOL );
                    } else if ( id < 240 ) {
                        tokenout += asset( 4000000, CORE_SYMBOL );
                    } else if ( id < 280  ) {
                        tokenout += asset( 8000000, CORE_SYMBOL );
                    } else if ( id < 300 ) {
                        tokenout += asset( 16000000, CORE_SYMBOL );
                    } else if ( id < 310 ) {
                        tokenout += asset( 32000000, CORE_SYMBOL );
                    } else {
                        uint64_t st = id * 100000 + 1;
                        uint64_t ed = (id+1) * 100000;
                        uint64_t total = calc_range(st, ed);
                        tokenout += asset( safe_mul(total, 4) / 100 / 10000, CORE_SYMBOL);
                    }

                }
            }
        });
    }

    if (tokenout.amount > 0) {
        _games.modify(game, 0, [&]( auto& gm ) {
            // update airdrop pool
            gm.airdrop_pool -= tokenout;
        });
        action(
          permission_level{_self, N(active)},
          TOKEN_CONTRACT, N(transfer),
          make_tuple(_self,player, tokenout, std::string("claim airdrop")))
          .send();
    }
}

uint64_t chicken::get_stage(uint64_t point)
{
    uint64_t idx = 0;
    while (AIRDROP_STAGE[idx] < point && idx < 5) {
        idx += 1;
    }
    return idx;
}

uint64_t chicken::get_add_time(const game_status & game, uint64_t bullet_amount ) {
    uint64_t c_stage = get_stage( game.supply + 1 );
    
    uint64_t res = safe_mul( STAGE_TIME[ c_stage ], bullet_amount );
    return res;
}

uint64_t chicken::safe_mul( uint64_t a, uint64_t b ) {
    uint128_t tmp = (uint128_t)a * (uint128_t)b;
    eosio_assert( tmp <= max_amount, "multiplication overflow" );
    return tmp;
}

void chicken::handle_airdrop(account_name buyer, const game_status& game,  const uint64_t&  bullet_amount) {
    uint64_t start = game.supply;

    uint64_t end = game.supply + bullet_amount;
    //end = std::min(end, AIRDROP_STAGE[4]);

    uint64_t start_point = start / AIRDROP_STEP;

    uint64_t end_point = end / AIRDROP_STEP;

    if (start_point < end_point) {
        airdrop_index airdroptable(_self, buyer);

        auto itr = airdroptable.begin();
        if (itr == airdroptable.end()) {
            airdroptable.emplace( _self, [&]( auto& ap ) {
                    ap.id = 0;
                    for (uint64_t i = start_point; i < end_point; i ++) {
                        ap.rewards.push_back(airdrop::airdropitem{buyer, 1, i});
                    }
            });
        } else {
            airdroptable.modify(*itr, 0, [&]( auto& ap ) {
                    for (uint64_t i = start_point; i < end_point; i ++) {
                        ap.rewards.push_back(airdrop::airdropitem{buyer, 1, i});
                    }
            });
        }
    }
}

uint64_t chicken::calc_range(uint64_t st, uint64_t ed)
{
    if (st > ed) {
        return 0;
    }
    uint128_t t_st = st;
    uint128_t t_ed = ed;
    uint128_t tmp = ((t_st + t_ed) * (t_ed - t_st + 1)) >> 1;
    eosio_assert( tmp <= max_amount, "multiplication overflow" );
    return (uint64_t)tmp;
}

void chicken::start()
{

    require_auth( _self );

    auto itr = _games.begin();

    _games.modify(*itr, 0, [&]( auto& gm ) {
            gm.is_start = 1;
    });

}
void chicken::finish()
{
    uint64_t c_time = current_time();
    const auto& global = _global.get(0);
    const auto& game = _games.get(global.game_id);

    eosio_assert(c_time > game.end_time, "The game not over yet");
    eosio_assert(game.cleaned == 0, "The game have been finished");

    uint64_t stage = get_stage(game.supply);
    uint64_t airdrop_out = 0;
    uint64_t into_bancor = 0;
    if (stage > 0) {
        airdrop_out += 2000000ll * 160;
    }
    if (stage > 1) {
        airdrop_out += 4000000ll * 80;
    }
    if (stage > 2) {
        airdrop_out += 8000000ll * 40;
    }
    if (stage > 3) {
        airdrop_out += 16000000ll * 20;
    }
    if (stage > 4) {
        airdrop_out += 32000000ll * 10;
        uint64_t st = 31000000ll + 1;
        uint64_t ed = game.supply - game.supply % 100000;
        uint64_t total = calc_range(st, ed);
        airdrop_out += safe_mul(total, 4) / 1000000;
    }
    into_bancor =  safe_mul(calc_range(1, game.supply), 4) / 1000000 - airdrop_out;
    // sent bancor
    action(
      permission_level{_self, N(active)},
      BANCOR_ACCOUNT, N(flowtoquote),
      make_tuple(_self, asset(into_bancor, CORE_SYMBOL), std::string("flow to bancor pool")))
      .send();
    // set cleaned
    _games.modify(game, 0, [&]( auto& gm ) {
            gm.cleaned = 1;
            gm.airdrop_pool -= asset(into_bancor, CORE_SYMBOL);
    });
}

#define EOSIO_ABI_PRO(TYPE, MEMBERS)                                                                                                              \
  extern "C" {                                                                                                                                    \
  void apply(uint64_t receiver, uint64_t code, uint64_t action)                                                                                   \
  {                                                                                                                                               \
    auto self = receiver;                                                                                                                         \
    if (action == N(onerror))                                                                                                                     \
    {                                                                                                                                             \
      eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account");                                        \
    }                                                                                                                                             \
    if ( (code == TOKEN_CONTRACT && action == N(transfer)) || (code == self && (action == N(finish) || action == N(withdraw) || action == N(start) ))) \
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
//EOSIO_ABI_PRO(chicken, (transfer)(withdraw)(finish)(start))

// generate .abi file
EOSIO_ABI(chicken, (transfer)(withdraw)(finish)(start))
