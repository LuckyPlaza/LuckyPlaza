/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

namespace eosiosystem {
   class system_contract;
}

using std::string;
using eosio::asset;
using eosio::symbol_name;

class luckytoken : public eosio::contract {
  public:
	 luckytoken( account_name self ):contract(self){}

	 void create( account_name issuer,
				  asset        maximum_supply,
                  account_name token_pool);

	 void issue( account_name to, asset quantity, string memo );

	 void transfer( account_name from,
					account_name to,
					asset        quantity,
					string       memo );
  
  
	 inline asset get_supply( symbol_name sym )const;
	 
	 inline asset get_balance( account_name owner, symbol_name sym )const;

  private:
     // @abi table accounts i64
	 struct account {
		asset    balance;

		uint64_t primary_key()const { return balance.symbol.name(); }
	 };

     // @abi table stat i64
	 struct cstat {
		asset          supply;
		asset          max_supply;
		account_name   issuer;

		uint64_t primary_key()const { return supply.symbol.name(); }
	 };

	 typedef eosio::multi_index<N(accounts), account> accounts;
	 typedef eosio::multi_index<N(stat), cstat> stats;

	 void sub_balance( account_name owner, asset value );
	 void add_balance( account_name owner, asset value, account_name ram_payer );

  public:
	 struct transfer_args {
		account_name  from;
		account_name  to;
		asset         quantity;
		string        memo;
	 };
};

asset luckytoken::get_supply( symbol_name sym )const
{
  stats statstable( _self, sym );
  const auto& st = statstable.get( sym );
  return st.supply;
}

asset luckytoken::get_balance( account_name owner, symbol_name sym )const
{
  accounts accountstable( _self, owner );
  const auto& ac = accountstable.get( sym );
  return ac.balance;
}
