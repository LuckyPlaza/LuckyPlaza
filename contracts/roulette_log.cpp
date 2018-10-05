#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/types.hpp>
#include <iostream>

using namespace eosio;

#define ROULETTE_CONTRACT N(rouletteplay)

class roulette_log : public contract
{
  public:
    roulette_log(account_name self) : contract(self){};

    // @abi action
    void result(std::string result, account_name account, asset bet_amount, asset win_amount, std::string bet_nums, uint64_t reveal_num,  checksum256 player_seed, checksum256 reveal_seed, uint64_t reveal_time)
    {
        require_auth(ROULETTE_CONTRACT);
    }
};

EOSIO_ABI(roulette_log, (result));
