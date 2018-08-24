/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/currency.hpp>

#include <string>

#include "exchange_state.hpp"

using std::string;

class tokenpool: public eosio::contract {
  public:
    tokenpool( account_name self )
         //_tokenmarket(_self, _self),
         :contract(self)
    {}

    void create();

    void transfer( account_name from,
                    account_name to,
                    asset        quantity,
                    string       memo );

    void flowtoquote( account_name from, asset quant, string memo);

    void addwhitelist(account_name white_account);

    void start();

    // @abi table twhitelist i64
    struct whitelist {
        uint64_t id;
        account_name white_account;
        uint64_t primary_key() const { return id; }
        EOSLIB_SERIALIZE(whitelist, (id)(white_account))
    };
    typedef eosio::multi_index<N(twhitelist), whitelist> whitelist_index;

    // @abi table tcontrol i64
    struct control {
        uint64_t id;
        uint64_t is_start;
        uint64_t primary_key() const { return id; }
        EOSLIB_SERIALIZE(control, (id)(is_start))
    };
    typedef eosio::multi_index<N(tcontrol), control> control_index;


  private:
    void update_quote(uint64_t delta);
    void sell( account_name seller, asset quantity);
    void buy( account_name buyer, asset quantity);

  public:
     struct transfer_args {
        account_name  from;
        account_name  to;
        asset         quantity;
        string        memo;
     };
};
