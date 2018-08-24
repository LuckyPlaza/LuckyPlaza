/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "lucky_token.hpp"

void luckytoken::create( account_name issuer,
                    asset        maximum_supply,
                    account_name token_pool)
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    asset init_bancor_pool = asset(10000000000ll, sym);
    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
       // init bancor
       s.supply = init_bancor_pool;
    });
    add_balance( token_pool, init_bancor_pool, _self );
}

void luckytoken::issue( account_name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    uint64_t bancor_part = 10000000000;
    uint64_t epubg_part = 1000000000000;
    uint64_t mine_part = 4990000000000;
    uint64_t team_part = 4000000000000;

    uint64_t fronze_part = 0;
    
    uint64_t seconds_per_year = 365*24*60*60;
    uint64_t c_time = now();


    // The team part will went live on 2019-01-01
    uint64_t team_release_time = 1546272000;
    uint64_t mine_release_time = 1546272000;

    // slow release the mined token for 2 years
    if (c_time < mine_release_time) {
        fronze_part += mine_part;
    } else if (mine_release_time + seconds_per_year * 2 > c_time) {
        double percent = (double)(c_time - mine_release_time)/(double)(seconds_per_year*2);
        uint64_t delta = (uint64_t)(mine_part * (1 - percent));
        fronze_part += delta;
    }

    // slow release the team token for 4 years
    if (c_time < team_release_time) {
        fronze_part += team_part;
    } else if (team_release_time + seconds_per_year * 4 > c_time) {
        double percent = (double)(c_time - team_release_time)/(double)(seconds_per_year*4);
        uint64_t delta = (uint64_t)(team_part * (1 - percent));
        fronze_part += delta;
    }
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount - fronze_part, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
       SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void luckytoken::transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


    sub_balance( from, quantity );
    add_balance( to, quantity, from );
}

void luckytoken::sub_balance( account_name owner, asset value ) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );


   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}

void luckytoken::add_balance( account_name owner, asset value, account_name ram_payer )
{
   accounts to_acnts( _self, owner );
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}


EOSIO_ABI( luckytoken, (create)(issue)(transfer) )

