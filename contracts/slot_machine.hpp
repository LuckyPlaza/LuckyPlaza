#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/crypto.h>
#include <string>

using namespace eosio;
using namespace std;

#define EOS_SYMBOL S(4, EOS)
#define TOKEN_CONTRACT N(eosio.token)


class slot_machine : public contract { 
public:
        slot_machine(account_name self)
                : contract(self),
        queues(_self, _self),
        bets(_self, _self),
                hashlist(_self, _self),
        global(_self, _self) {
        }

    void init();

    void addhash(checksum256 hash);

    void transfer(account_name from, account_name to, asset quantity, std::string memo);

        void reveal(checksum256& seed, checksum256& hash);

        void update(uint64_t status, uint64_t lkt_bonus_rate, asset safe_balance);

    void flowtobancor();

private:
    const uint64_t tps[4] = { 126, 60, 24, 6 };
    const double pl[4] = { 1.6, 3.2, 8.5, 32.0 };
    const string PUB_KEY = "EOS7vM8potbuXDwBAmErSj3Az2cQ1AHBTX1B8U25LM9hHE3tzPCeZ";

    // @abi table global i64
    struct global {
        uint64_t id = 0;
        asset safe_balance;
        uint64_t current_id;
        uint64_t status;
        uint64_t lkt_bonus_rate;
        uint64_t queue_size;
        uint64_t primary_key() const { return id; }
        EOSLIB_SERIALIZE(global, (id)(safe_balance)(current_id)(status)(lkt_bonus_rate)(queue_size))
    };
    typedef eosio::multi_index<N(global), global> global_index;
    global_index global;  


    // @abi table queue i64
    struct queueitem {
        account_name account;
        uint64_t primary_key() const { return account; }
        EOSLIB_SERIALIZE(queueitem, (account))
    };
    typedef eosio::multi_index<N(queue), queueitem> queue_index;
    queue_index queues;

    // @abi table player i64
    struct player {
        uint64_t id;
        //0 ready 1 in progress
        uint64_t status;
        // 0 lose 1 win
        asset win_amount;
        asset bet_amount;
        uint64_t bet_type;
        uint64_t slot_pos;
        checksum256 seed;
        uint64_t primary_key() const { return id; }
        EOSLIB_SERIALIZE(player, (id)(status)(win_amount)(bet_amount)(bet_type)(slot_pos)(seed))
    };
    typedef eosio::multi_index<N(player), player> player_index;

    // @abi table bet i64
    struct betitem {
        account_name account;
        uint64_t bet_type;
        checksum256 seed;
        asset bet_amount;
        uint64_t primary_key() const { return account; }
        EOSLIB_SERIALIZE(betitem, (account)(bet_type)(seed)(bet_amount))
    };
    typedef eosio::multi_index<N(bet), betitem> bet_index;
    bet_index bets;

    // @abi table househash i64
    struct househash {
        uint64_t id;
        checksum256 hash;
        uint64_t primary_key() const { return id; }
        EOSLIB_SERIALIZE(househash, (id)(hash))
    };
    typedef eosio::multi_index<N(househash), househash> househash_index;
    househash_index hashlist;

    struct token
    {
        token(account_name tkn) : _self(tkn) {}
        struct account
        {
            eosio::asset    balance;
            uint64_t primary_key()const { return balance.symbol.name(); }
        };
        typedef eosio::multi_index<N(accounts), account> accounts;

        asset get_balance(account_name owner,  eosio::symbol_type sym) const
        {
            accounts acnt(_self, owner);
            auto itr = acnt.find( sym.name() );
            if (itr != acnt.end()) {
                return acnt.find(sym.name())->balance;
            } else {
                return asset(0, sym);
            }
        }

    private:
        account_name _self;
    };

    uint64_t merge_seed(const checksum256& s1, const checksum256& s2); 

    void bet(account_name account, uint64_t type, asset amount, const checksum256& seed );

    void rock(const betitem& item, const checksum256& reveal_seed);

    void get_bonus(uint64_t seed, uint64_t amount, uint64_t bet_type, uint64_t& bonus, uint64_t& reveal_type, uint64_t& reveal_pos );

    uint8_t char2int(char input);

    void string2seed(const string& str, checksum256& seed);

    void split(std::string str, std::string splitBy, std::vector<std::string>& tokens)
    {
        /* Store the original string in the array, so we can loop the rest
         * of the algorithm. */
        tokens.push_back(str);

        // Store the split index in a 'size_t' (unsigned integer) type.
        size_t splitAt;
        // Store the size of what we're splicing out.
        size_t splitLen = splitBy.size();
        // Create a string for temporarily storing the fragment we're processing.
        std::string frag;
        // Loop infinitely - break is internal.
        while(true)
        {
            /* Store the last string in the vector, which is the only logical
             * candidate for processing. */
            frag = tokens.back();
            /* The index where the split is. */
            splitAt = frag.find(splitBy);
            // If we didn't find a new split point...
            if(splitAt == string::npos)
            {
                // Break the loop and (implicitly) return.
                break;
            }
            /* Put everything from the left side of the split where the string
             * being processed used to be. */
            tokens.back() = frag.substr(0, splitAt);
            /* Push everything from the right side of the split to the next empty
             * index in the vector. */
            tokens.push_back(frag.substr(splitAt+splitLen, frag.size()-(splitAt+splitLen)));
        }
    }
};
