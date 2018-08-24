#pragma once

#include <eosiolib/asset.hpp>
#include <cmath>

using eosio::asset;
using eosio::symbol_type;
using eosio::permission_level;
using eosio::action;
using eosio::symbol_name;

using std::string;

class chicken: public eosio::contract {
  public:
     chicken( account_name self );

     void transfer( account_name from, account_name to, asset quantity, string memo );

     void withdraw( account_name user );

     void finish();

     void start();

  private:
     // @abi table userinf i64
     struct user{
        uint64_t game_id;
        uint64_t balance;
        uint64_t mask;
        uint64_t sale_amount;
        asset sale_reward;
        uint64_t primary_key() const { return game_id; }
        void print() {
            eosio::print(balance, ' ', mask, "\n");
        }
        EOSLIB_SERIALIZE(user, (game_id)(balance)(mask)(sale_amount)(sale_reward))
     };
     typedef eosio::multi_index<N(userinf), user> user_index;

     // @abi table globalstatus i64
     struct global_status {
         uint64_t id;
         uint64_t game_id;
         uint64_t mined_amount;
         uint64_t primary_key() const { return id; }
         uint64_t get_rest_lkt() const {
             // send 100000000 LKT
             return 100000000 - mined_amount;
         } 
         EOSLIB_SERIALIZE(global_status, (id)(game_id)(mined_amount))
     };
     typedef eosio::multi_index<N(globalstatus), global_status> global_index;

     // @abi table cgamestatus i64
     struct game_status {
        struct trade_info {
            account_name player;
            asset amount;
            uint64_t time;
            EOSLIB_SERIALIZE(trade_info, (player)(amount)(time) )
        };

        uint64_t game_id;
        uint64_t supply;
        uint64_t is_start;
        uint64_t end_time;
        account_name last_one;
        asset airdrop_pool;
        asset dividend_pool;
        asset last_reward_pool;
        uint64_t last_claimed;
        asset total_pool;
        std::vector<trade_info> trades;
        uint64_t trade_pos;
        uint64_t cleaned;
        uint64_t primary_key() const { return game_id; }
        void print() {
            eosio::print(supply, ' ', end_time, ' ', last_one,  "\n");
        }
        void add_trade(trade_info trade) {
            if (trades.size() < 10) {
                trades.push_back(trade);
                trade_pos = (trade_pos + 1) % 10;
                return;
            }
            trades[trade_pos] = trade;
            trade_pos = (trade_pos + 1) % 10;
        }
        EOSLIB_SERIALIZE(game_status, (game_id)(supply)(is_start)(end_time)(last_one)(airdrop_pool)(dividend_pool)(last_reward_pool)(last_claimed)(total_pool)(trades)(trade_pos)(cleaned))
     };
     typedef eosio::multi_index<N(cgamestatus), game_status> game_index;

     // @abi table airdrop i64
     struct airdrop {
         struct airdropitem {
            account_name player;
            uint64_t status;
            uint64_t id;
            EOSLIB_SERIALIZE(airdropitem, (player)(status)(id) )
         };
         std::vector<airdropitem> rewards;
         uint64_t id;
         uint64_t primary_key() const { return id; }
         EOSLIB_SERIALIZE(airdrop, (rewards)(id) )
     };
     typedef eosio::multi_index<N(airdrop), airdrop> airdrop_index;

     uint64_t get_stage(uint64_t point);
     uint64_t get_add_time( const game_status & game, uint64_t bullet_amount );
     uint64_t calc_range(uint64_t st, uint64_t ed);
     void handle_airdrop(account_name buyer, const game_status& game, const uint64_t& bullet_amount);
     void claim( account_name player );
     uint64_t safe_mul(uint64_t a, uint64_t b);
     void buy( account_name buyer, asset quantity, account_name inviter );
     uint64_t get_bullet_amount(asset quant, uint64_t supply);

     global_index _global;
     game_index _games;

     const symbol_type BULLET = S(0, BULLET);
     const symbol_type LUCKY = S(4, LKT);
     const uint64_t STEP = 1;
     const uint64_t AIRDROP_STAGE[5] = {16000000, 24000000, 28000000, 30000000, 31000000};
     const uint64_t AIRDROP_REWARD[5] = {2000000, 4000000, 8000000, 16000000, 32000000};
     const uint64_t AIRDROP_STEP = 100000;
     const uint64_t STAGE_TIME[6] = {32000000, 16000000, 8000000, 4000000, 2000000, 1000000};
     const uint64_t STAGE_AIRDROP_CNT[6] = {0, 160, 240, 280, 300, 310};
     const uint64_t LUCKY_COIN_REWARD[6] = {1, 2, 2, 2, 2, 2};
     const uint64_t REMAIN_TOKEN = 100000000 - 62000000;
     const uint64_t DAY = 86400000000;

     const account_name BANCOR_ACCOUNT = N(bkyyshayysha);
     const account_name OPEX_ACCOUNT = N(opyyshayysha);
     const account_name LKT_MINE_ACCOUNT = N(lmyyshayysha);

};
