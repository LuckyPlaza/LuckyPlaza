#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/types.hpp>
#include <iostream>

using namespace eosio;

#define HAPPY_CONTRACT N(youhappyisok)

class slog_machine_log : public contract
{
  public:
    slog_machine_log(account_name self) : contract(self){};

    // @abi action
    void result(std::string result, account_name account, asset bet_amount, asset win_amount, uint64_t bet_type, uint64_t reveal_type, uint64_t reveal_pos, checksum256 player_seed, checksum256 reveal_seed, uint64_t reveal_time)
    {
        require_auth(HAPPY_CONTRACT);
    }
};

EOSIO_ABI(slog_machine_log, (result));
