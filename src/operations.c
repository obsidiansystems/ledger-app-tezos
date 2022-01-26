#include "operations.h"

#include "apdu.h"
#include "globals.h"
#include "memory.h"
#include "to_string.h"
#include "ui.h"
#include "michelson.h"

#include <stdint.h>
#include <string.h>

#define STEP_HARD_FAIL -2

// Argument is to distinguish between different parse errors for debugging purposes only
__attribute__((noreturn)) static void parse_error(
#ifndef TEZOS_DEBUG
    __attribute__((unused))
#endif
    uint32_t lineno) {

    global.apdu.u.sign.parse_state.op_step = STEP_HARD_FAIL;
#ifdef TEZOS_DEBUG
    THROW(0x9000 + lineno);
#else
    THROW(EXC_PARSE_ERROR);
#endif
}

#define PARSE_ERROR() parse_error(__LINE__)

// Conversion/check functions

static inline signature_type_t parse_raw_tezos_header_signature_type(
    raw_tezos_header_signature_type_t const *const raw_signature_type) {
    check_null(raw_signature_type);
    switch (READ_UNALIGNED_BIG_ENDIAN(uint8_t, &raw_signature_type->v)) {
        case 0:
            return SIGNATURE_TYPE_ED25519;
        case 1:
            return SIGNATURE_TYPE_SECP256K1;
        case 2:
            return SIGNATURE_TYPE_SECP256R1;
        default:
            PARSE_ERROR();
    }
}

static inline void compute_pkh(cx_ecfp_public_key_t *const compressed_pubkey_out,
                               parsed_contract_t *const contract_out,
                               derivation_type_t const derivation_type,
                               bip32_path_t const *const bip32_path) {
    check_null(bip32_path);
    check_null(compressed_pubkey_out);
    check_null(contract_out);
    cx_ecfp_public_key_t pubkey = {0};
    generate_public_key(&pubkey, derivation_type, bip32_path);
    public_key_hash(contract_out->hash,
                    sizeof(contract_out->hash),
                    compressed_pubkey_out,
                    derivation_type,
                    &pubkey);
    contract_out->signature_type = derivation_type_to_signature_type(derivation_type);
    if (contract_out->signature_type == SIGNATURE_TYPE_UNSET) THROW(EXC_MEMORY_ERROR);
    contract_out->originated = 0;
}

static inline void parse_implicit(parsed_contract_t *const out,
                                  raw_tezos_header_signature_type_t const *const raw_signature_type,
                                  uint8_t const hash[HASH_SIZE]) {
    check_null(raw_signature_type);
    out->originated = 0;
    out->signature_type = parse_raw_tezos_header_signature_type(raw_signature_type);
    memcpy(out->hash, hash, sizeof(out->hash));
}

static inline void parse_contract(parsed_contract_t *const out, struct contract const *const in) {
    out->originated = in->originated;
    if (out->originated == 0) {  // implicit
        out->signature_type = parse_raw_tezos_header_signature_type(&in->u.implicit.signature_type);
        memcpy(out->hash, in->u.implicit.pkh, sizeof(out->hash));
    } else {  // originated
        out->signature_type = SIGNATURE_TYPE_UNSET;
        memcpy(out->hash, in->u.originated.pkh, sizeof(out->hash));
    }
}

#define CALL_SUBPARSER_LN(func, line, ...) \
    if (func(__VA_ARGS__, line)) return true
#define CALL_SUBPARSER(func, ...) CALL_SUBPARSER_LN(func, __LINE__, __VA_ARGS__)

// Subparsers: no function here should be called anywhere in this file without using the
// CALL_SUBPARSER macro above. Verify this with
// /<funcname>\s*(
// the only result should be the functiond definition.

#define NEXT_BYTE (byte)

// TODO: this function cannot parse z values than would not fit in a uint64
static inline bool parse_z(uint8_t current_byte,
                           struct int_subparser_state *state,
                           uint32_t lineno) {
    if (state->lineno != lineno) {
        // New call; initialize.
        state->lineno = lineno;
        state->value = 0;
        state->shift = 0;
    }
    // Fails when the resulting shifted value overflows 64 bits
    if (state->shift > 63 || (state->shift == 63 && current_byte != 1))
      PARSE_ERROR();
    state->value |= ((uint64_t) current_byte & 0x7F) << state->shift;
    state->shift += 7;
    return current_byte & 0x80;  // Return true if we need more bytes.
}

#define PARSE_Z                                                             \
    ({                                                                      \
        CALL_SUBPARSER(parse_z, (byte), &(state)->subparser_state.integer); \
        (state)->subparser_state.integer.value;                             \
    })

// Only used through the macro
static inline bool parse_z_michelson(uint8_t current_byte,
                                     struct int_subparser_state *state,
                                     uint32_t lineno) {
    if (state->lineno != lineno) {
        // New call; initialize.
        state->lineno = lineno;
        state->value = 0;
        state->shift = 0;
    }
    state->value |= ((uint64_t) current_byte & 0x7F) << state->shift;
    // For some reason we are getting numbers shifted 1 bit to the
    // left. TODO: figure out why this happens
    if (state->shift == 0) {
        state->shift += 6;
    } else {
        state->shift += 7;
    }
    return current_byte & 0x80;  // Return true if we need more bytes.
}

#define PARSE_Z_MICHELSON                                                             \
    ({                                                                                \
        CALL_SUBPARSER(parse_z_michelson, (byte), (&state->subparser_state.integer)); \
        state->subparser_state.integer.value;                                         \
    })

static inline bool parse_next_type(uint8_t current_byte,
                                   struct nexttype_subparser_state *state,
                                   uint32_t sizeof_type,
                                   uint32_t lineno) {
#ifdef DEBUG
    if (sizeof_type > sizeof(state->body))
        PARSE_ERROR();  // Shouldn't happen, but error if it does and we're debugging. Neither side
                        // is dynamic.
#endif

    if (state->lineno != lineno) {
        state->lineno = lineno;
        state->fill_idx = 0;
    }

    state->body.raw[state->fill_idx] = current_byte;
    state->fill_idx++;

    return state->fill_idx < sizeof_type;  // Return true if we need more bytes.
}

// do _NOT_ keep pointers to this data around.
#define NEXT_TYPE(type)                                                                          \
    ({                                                                                           \
        CALL_SUBPARSER(parse_next_type, byte, &(state->subparser_state.nexttype), sizeof(type)); \
        (const type *) &(state->subparser_state.nexttype.body);                                  \
    })

static inline bool michelson_read_length(uint8_t current_byte,
                                         struct nexttype_subparser_state *state,
                                         uint32_t lineno) {
    CALL_SUBPARSER_LN(parse_next_type,
                      lineno,
                      current_byte,
                      state,
                      sizeof(uint32_t));  // Using the line number we were called with.
    uint32_t res = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &state->body.raw);
    state->body.i32 = res;
    return false;
}

#define MICHELSON_READ_LENGTH                                                          \
    ({                                                                                 \
        CALL_SUBPARSER(michelson_read_length, byte, &state->subparser_state.nexttype); \
        state->subparser_state.nexttype.body.i32;                                      \
    })

static inline bool michelson_read_short(uint8_t current_byte,
                                        struct nexttype_subparser_state *state,
                                        uint32_t lineno) {
    CALL_SUBPARSER_LN(parse_next_type, lineno, current_byte, state, sizeof(uint16_t));
    uint32_t res = READ_UNALIGNED_BIG_ENDIAN(uint16_t, &state->body.raw);
    state->body.i16 = res;
    return false;
}

#define MICHELSON_READ_SHORT                                                          \
    ({                                                                                \
        CALL_SUBPARSER(michelson_read_short, byte, &state->subparser_state.nexttype); \
        state->subparser_state.nexttype.body.i16;                                     \
    })

static inline bool michelson_read_address(uint8_t byte,
                                          parsed_contract_t *const out,
                                          char *base58_address_buffer,
                                          struct michelson_address_subparser_state *state,
                                          uint32_t lineno) {
    if (state->lineno != lineno) {  // We won't have to use CALL_SUBPARSER_LN because we're
                                    // initializing ourselves here.
        memset(state, 0, sizeof(*state));
        state->lineno = lineno;
    }
    switch (state->address_step) {
        case 0:
            state->micheline_type = byte;
            state->address_step = 1;
            return true;

        case 1:

            CALL_SUBPARSER(michelson_read_length, byte, &state->subsub_state);
            state->addr_length = state->subsub_state.body.i32;

            state->address_step = 2;
            return true;
        default: {
            switch (state->micheline_type) {
                case MICHELSON_TYPE_BYTE_SEQUENCE: {
                    switch (state->address_step) {
                        case 2:

                            // Need 1 byte for signature, plus the rest of the hash.
                            if (state->addr_length != HASH_SIZE + 1) {
                                PARSE_ERROR();
                            }

                            CALL_SUBPARSER(parse_next_type,
                                           byte,
                                           &(state->subsub_state),
                                           sizeof(state->signature_type));
                            memcpy(&(state->signature_type),
                                   &(state->subsub_state.body),
                                   sizeof(state->signature_type));

                        case 3:

                            CALL_SUBPARSER(parse_next_type,
                                           byte,
                                           &(state->subsub_state),
                                           sizeof(state->key_hash));
                            memcpy(&(state->key_hash),
                                   &(state->subsub_state.body),
                                   sizeof(state->key_hash));

                            parse_implicit(out,
                                           &state->signature_type,
                                           (const uint8_t *) &state->key_hash);

                            return false;
                    }
                    case MICHELSON_TYPE_STRING: {
                        if (state->addr_length != HASH_SIZE_B58) {
                            PARSE_ERROR();
                        }

                        CALL_SUBPARSER(parse_next_type,
                                       byte,
                                       &(state->subsub_state),
                                       sizeof(state->subsub_state.body.text_pkh));

                        memcpy(base58_address_buffer,
                               state->subsub_state.body.text_pkh,
                               sizeof(state->subsub_state.body.text_pkh));
                        out->hash_ptr = base58_address_buffer;
                        out->originated = false;
                        out->signature_type = SIGNATURE_TYPE_UNSET;
                        return false;
                    }
                    default:
                        PARSE_ERROR();
                }
            }
        }
    }
}

#define MICHELSON_READ_ADDRESS(out, buffer) \
    CALL_SUBPARSER(michelson_read_address,  \
                   byte,                    \
                   (out),                   \
                   buffer,                  \
                   &state->subparser_state.michelson_address)

// End of subparsers.

void parse_operations_init(struct parsed_operation_group *const out,
                           derivation_type_t derivation_type,
                           bip32_path_t const *const bip32_path,
                           struct parse_state *const state) {
    check_null(out);
    check_null(bip32_path);
    memset(out, 0, sizeof(*out));

    out->operation.tag = OPERATION_TAG_NONE;

    compute_pkh(&out->public_key, &out->signing, derivation_type, bip32_path);

    // Start out with source = signing, for reveals
    // TODO: This is slightly hackish
    memcpy(&out->operation.source, &out->signing, sizeof(out->signing));

    state->op_step = 0;
    state->subparser_state.integer.lineno = -1;
    state->tag = OPERATION_TAG_NONE;  // This and the rest shouldn't be required.
    state->argument_length = 0;
    state->michelson_op = -1;
}

// Named steps in the top-level state machine
#define STEP_END_OF_MESSAGE                         -1
#define STEP_OP_TYPE_DISPATCH                       10001
#define STEP_AFTER_MANAGER_FIELDS                   10002
#define STEP_HAS_DELEGATE                           10003
#define STEP_MICHELSON_FIRST_IS_PUSH                10010
#define STEP_MICHELSON_FIRST_IS_NONE                10011
#define STEP_MICHELSON_SECOND_IS_KEY_HASH           10012
#define STEP_MICHELSON_CONTRACT_TO_CONTRACT         10013
#define STEP_MICHELSON_SET_DELEGATE_CHAIN           10014
#define STEP_MICHELSON_CONTRACT_TO_IMPLICIT_CHAIN   10015
#define STEP_MICHELSON_CONTRACT_END                 10016
#define STEP_MICHELSON_CHECKING_CONTRACT_ENTRYPOINT 10017
#define STEP_MICHELSON_CONTRACT_TO_CONTRACT_CHAIN_2 10018

bool parse_operations_final(struct parse_state *const state,
                            struct parsed_operation_group *const out) {
    if (out->operation.tag == OPERATION_TAG_NONE && !out->has_reveal) {
        return false;
    }
    return state->op_step == STEP_END_OF_MESSAGE || state->op_step == 1;
}

static inline bool parse_byte(uint8_t byte,
                              struct parse_state *const state,
                              struct parsed_operation_group *const out,
                              is_operation_allowed_t is_operation_allowed) {
// OP_STEP finishes the current state transition, setting the state, and introduces the next state.
// For linear chains of states, this keeps the code structurally similar to equivalent imperative
// parsing code.
#define OP_STEP                \
    state->op_step = __LINE__; \
    return true;               \
    case __LINE__:

// The same as OP_STEP, but with a particular name, such that we could jump to this state.
#define OP_NAMED_STEP(name) \
    state->op_step = name;  \
    return true;            \
    case name:

// "jump" to specific state: (set state to foo and return.)
#define JMP(step)          \
    state->op_step = step; \
    return true

// Set the next state to end-of-message
#define JMP_EOM JMP(-1)

// Set the next state to start-of-payload; used after reveal.
#define JMP_TO_TOP JMP(1)

// Conditionally set the next state.
#define OP_JMPIF(step, cond)   \
    if (cond) {                \
        state->op_step = step; \
        return true;           \
    }

// Shortcuts for defining literal-matching states; mostly used for contract-call boilerplate.
#define OP_STEP_REQUIRE_SHORT(constant)                       \
    {                                                         \
        uint16_t val = MICHELSON_READ_SHORT;                  \
        if (val != constant) {                                \
            PRINTF("Expected: %d, got: %d\n", constant, val); \
            PARSE_ERROR();                                    \
        }                                                     \
    }                                                         \
    OP_STEP
#define OP_STEP_REQUIRE_BYTE(constant)                         \
    {                                                          \
        if (byte != constant) {                                \
            PRINTF("Expected: %d, got: %d\n", constant, byte); \
            PARSE_ERROR();                                     \
        }                                                      \
    }                                                          \
    OP_STEP
#define OP_STEP_REQUIRE_LENGTH(constant)      \
    {                                         \
        uint32_t val = MICHELSON_READ_LENGTH; \
        if (val != constant) {                \
            PARSE_ERROR();                    \
        }                                     \
    }                                         \
    OP_STEP

    switch (state->op_step) {
        case STEP_HARD_FAIL:
            PARSE_ERROR();

        case STEP_END_OF_MESSAGE:
            PARSE_ERROR();  // We already hit a hard end of message; fail.

        case 0: {
            // Verify magic byte, ignore block hash
            const struct operation_group_header *ogh = NEXT_TYPE(struct operation_group_header);
            if (ogh->magic_byte != MAGIC_BYTE_UNSAFE_OP) PARSE_ERROR();
        }

            OP_NAMED_STEP(1)

            state->tag = NEXT_BYTE;

            if (!is_operation_allowed(state->tag)) PARSE_ERROR();

            OP_STEP

            // Parse 'source'
            switch (state->tag) {
                // Tags that don't have "originated" byte only support tz accounts, not KT or tz.
                case OPERATION_TAG_PROPOSAL:
                case OPERATION_TAG_BALLOT:
                case OPERATION_TAG_BABYLON_DELEGATION:
                case OPERATION_TAG_BABYLON_ORIGINATION:
                case OPERATION_TAG_BABYLON_REVEAL:
                case OPERATION_TAG_BABYLON_TRANSACTION: {
                    struct implicit_contract const *const implicit_source =
                        NEXT_TYPE(struct implicit_contract);
                    out->operation.source.originated = 0;
                    out->operation.source.signature_type =
                        parse_raw_tezos_header_signature_type(&implicit_source->signature_type);
                    memcpy(out->operation.source.hash,
                           implicit_source->pkh,
                           sizeof(out->operation.source.hash));
                    break;
                }

                case OPERATION_TAG_ATHENS_DELEGATION:
                case OPERATION_TAG_ATHENS_ORIGINATION:
                case OPERATION_TAG_ATHENS_REVEAL:
                case OPERATION_TAG_ATHENS_TRANSACTION: {
                    struct contract const *const source = NEXT_TYPE(struct contract);
                    parse_contract(&out->operation.source, source);
                    break;
                }

                default:
                    PARSE_ERROR();
            }

            OP_JMPIF(STEP_AFTER_MANAGER_FIELDS,
                     (state->tag == OPERATION_TAG_PROPOSAL || state->tag == OPERATION_TAG_BALLOT));

            OP_STEP

            // out->operation.source IS NORMALIZED AT THIS POINT

            // Parse common fields for non-governance related operations.

            out->total_fee += PARSE_Z;  // fee
            OP_STEP
            PARSE_Z;  // counter
            OP_STEP
            PARSE_Z;  // gas limit
            OP_STEP
            out->total_storage_limit += PARSE_Z;  // storage limit

            OP_JMPIF(STEP_AFTER_MANAGER_FIELDS,
                     (state->tag != OPERATION_TAG_ATHENS_REVEAL &&
                      state->tag != OPERATION_TAG_BABYLON_REVEAL))
            OP_STEP

            // We know this is a reveal

            // Public key up next! Ensure it matches signing key.
            // Ignore source :-) and do not parse it from hdr.
            // We don't much care about reveals, they have very little in the way of bad security
            // implications and any fees have already been accounted for
            {
                raw_tezos_header_signature_type_t const *const sig_type =
                    NEXT_TYPE(raw_tezos_header_signature_type_t);
                if (parse_raw_tezos_header_signature_type(sig_type) != out->signing.signature_type)
                    PARSE_ERROR();
            }

            OP_STEP

            {
                size_t klen = out->public_key.W_len;

                CALL_SUBPARSER(parse_next_type, byte, &(state->subparser_state.nexttype), klen);

                if (memcmp(out->public_key.W, &(state->subparser_state.nexttype.body.raw), klen) !=
                    0)
                    PARSE_ERROR();

                out->has_reveal = true;

                JMP_TO_TOP;
            }

        case STEP_AFTER_MANAGER_FIELDS:  // Anything but a reveal

            if (out->operation.tag != OPERATION_TAG_NONE) {
                // We are only currently allowing one non-reveal operation
                PARSE_ERROR();
            }

            // This is the one allowable non-reveal operation per set

            out->operation.tag = (uint8_t) state->tag;

            // If the source is an implicit contract,...
            if (out->operation.source.originated == 0) {
                // ... it had better match our key, otherwise why are we signing it?
                if (COMPARE(&out->operation.source, &out->signing) != 0) PARSE_ERROR();
            }
            // OK, it passes muster.

            // This should by default be blanked out
            out->operation.delegate.signature_type = SIGNATURE_TYPE_UNSET;
            out->operation.delegate.originated = 0;

            // Deliberate epsilon-transition.
            state->op_step = STEP_OP_TYPE_DISPATCH;
        default:

            switch (state->tag) {
                case OPERATION_TAG_PROPOSAL:
                    switch (state->op_step) {
                        case STEP_OP_TYPE_DISPATCH: {
                            const struct proposal_contents *proposal_data =
                                NEXT_TYPE(struct proposal_contents);

                            const size_t payload_size =
                                READ_UNALIGNED_BIG_ENDIAN(int32_t, &proposal_data->num_bytes);
                            if (payload_size != PROTOCOL_HASH_SIZE)
                                PARSE_ERROR();  // We only accept exactly 1 proposal hash.

                            out->operation.proposal.voting_period =
                                READ_UNALIGNED_BIG_ENDIAN(int32_t, &proposal_data->period);

                            memcpy(out->operation.proposal.protocol_hash,
                                   proposal_data->hash,
                                   sizeof(out->operation.proposal.protocol_hash));
                        }

                            JMP_EOM;
                    }
                    break;
                case OPERATION_TAG_BALLOT:
                    switch (state->op_step) {
                        case STEP_OP_TYPE_DISPATCH: {
                            const struct ballot_contents *ballot_data =
                                NEXT_TYPE(struct ballot_contents);

                            out->operation.ballot.voting_period =
                                READ_UNALIGNED_BIG_ENDIAN(int32_t, &ballot_data->period);
                            memcpy(out->operation.ballot.protocol_hash,
                                   ballot_data->proposal,
                                   sizeof(out->operation.ballot.protocol_hash));

                            const int8_t ballot_vote =
                                READ_UNALIGNED_BIG_ENDIAN(int8_t, &ballot_data->ballot);
                            switch (ballot_vote) {
                                case 0:
                                    out->operation.ballot.vote = BALLOT_VOTE_YEA;
                                    break;
                                case 1:
                                    out->operation.ballot.vote = BALLOT_VOTE_NAY;
                                    break;
                                case 2:
                                    out->operation.ballot.vote = BALLOT_VOTE_PASS;
                                    break;
                                default:
                                    PARSE_ERROR();
                            }
                            JMP_EOM;
                        }
                    }

                case OPERATION_TAG_ATHENS_DELEGATION:
                case OPERATION_TAG_BABYLON_DELEGATION:
                    switch (state->op_step) {
                        case STEP_OP_TYPE_DISPATCH: {
                            uint8_t delegate_present = NEXT_BYTE;

                            OP_JMPIF(STEP_HAS_DELEGATE, delegate_present)
                        }
                            // Else branch: Encode "not present"
                            out->operation.destination.originated = 0;
                            out->operation.destination.signature_type = SIGNATURE_TYPE_UNSET;

                            JMP_TO_TOP;  // These go back to the top to catch any reveals.

                        case STEP_HAS_DELEGATE: {
                            const struct delegation_contents *dlg =
                                NEXT_TYPE(struct delegation_contents);
                            parse_implicit(&out->operation.destination,
                                           &dlg->signature_type,
                                           dlg->hash);
                        }
                            JMP_TO_TOP;  // These go back to the top to catch any reveals.
                    }
                case OPERATION_TAG_ATHENS_ORIGINATION:
                case OPERATION_TAG_BABYLON_ORIGINATION:
                    PARSE_ERROR();  // We can't parse the script yet, and all babylon originations
                                    // have a script; we have to just reject originations.

                case OPERATION_TAG_ATHENS_TRANSACTION:
                case OPERATION_TAG_BABYLON_TRANSACTION:
                    switch (state->op_step) {
                        case STEP_OP_TYPE_DISPATCH:

                            out->operation.amount = PARSE_Z;

                            OP_STEP {
                                const struct contract *destination = NEXT_TYPE(struct contract);
                                parse_contract(&out->operation.destination, destination);
                            }

                            OP_STEP {
                                char has_params = NEXT_BYTE;

                                if (has_params == MICHELSON_PARAMS_NONE) {
                                    JMP_TO_TOP;
                                }

                                if (has_params != MICHELSON_PARAMS_SOME) {
                                    PARSE_ERROR();
                                }

                                // From this point on we are _only_ parsing manager.tz operatinos,
                                // so we show the outer destination (the KT1) as the source of the
                                // transaction.
                                out->operation.is_manager_tz_operation = true;
                                memcpy(&out->operation.implicit_account,
                                       &out->operation.source,
                                       sizeof(parsed_contract_t));
                                memcpy(&out->operation.source,
                                       &out->operation.destination,
                                       sizeof(parsed_contract_t));

                                // manager.tz operations cannot actually transfer any amount.
                                if (out->operation.amount > 0) {
                                    PARSE_ERROR();
                                }
                            }

                            OP_STEP {
                                const enum entrypoint_tag entrypoint = NEXT_BYTE;

                                // Don't bother parsing the name, we'll reject if it's there either
                                // way.

                                // Anything that’s not “do” is not a
                                // manager.tz contract.
                                if (entrypoint != ENTRYPOINT_DO) {
                                    PARSE_ERROR();
                                }
                            }

                            OP_STEP {
                                state->argument_length = MICHELSON_READ_LENGTH;
                            }

                            OP_STEP

                            // Error on anything but a michelson sequence.
                            OP_STEP_REQUIRE_BYTE(MICHELSON_TYPE_SEQUENCE);

                            {
                                const uint32_t sequence_length = MICHELSON_READ_LENGTH;

                                // Only allow single sequence (5 is needed
                                // in argument length for above two
                                // bytes). Also bail out on really big
                                // Michelson that we don’t support.
                                if (sequence_length + sizeof(uint8_t) + sizeof(uint32_t) !=
                                        state->argument_length ||
                                    state->argument_length > MAX_MICHELSON_SEQUENCE_LENGTH) {
                                    PARSE_ERROR();
                                }
                            }

                            OP_STEP

                            // All manager.tz operations should begin
                            // with this. Otherwise, bail out.
                            OP_STEP_REQUIRE_SHORT(MICHELSON_DROP)
                            OP_STEP_REQUIRE_SHORT(MICHELSON_NIL)
                            OP_STEP_REQUIRE_SHORT(MICHELSON_OPERATION)

                            {
                                state->michelson_op = MICHELSON_READ_SHORT;

                                // First real michelson op.
                                switch (state->michelson_op) {
                                    case MICHELSON_PUSH:
                                        JMP(STEP_MICHELSON_FIRST_IS_PUSH);
                                    case MICHELSON_NONE:  // withdraw delegate
                                        JMP(STEP_MICHELSON_FIRST_IS_NONE);
                                    default:
                                        PARSE_ERROR();
                                }
                            }

                        case STEP_MICHELSON_FIRST_IS_PUSH: {
                            state->michelson_op = MICHELSON_READ_SHORT;

                            // First real michelson op.
                            switch (state->michelson_op) {
                                case MICHELSON_KEY_HASH:
                                    JMP(STEP_MICHELSON_SECOND_IS_KEY_HASH);
                                case MICHELSON_ADDRESS:  // transfer contract to contract
                                    JMP(STEP_MICHELSON_CONTRACT_TO_CONTRACT);
                                default:
                                    PARSE_ERROR();
                            }
                        }

                        case STEP_MICHELSON_SECOND_IS_KEY_HASH:

                            MICHELSON_READ_ADDRESS(&out->operation.destination, state->base58_pkh1);

                            OP_STEP {
                                state->michelson_op = MICHELSON_READ_SHORT;

                                switch (state->michelson_op) {
                                    case MICHELSON_SOME:  // Set delegate
                                        JMP(STEP_MICHELSON_SET_DELEGATE_CHAIN);
                                    case MICHELSON_IMPLICIT_ACCOUNT:  // transfer contract to
                                                                      // implicit
                                        JMP(STEP_MICHELSON_CONTRACT_TO_IMPLICIT_CHAIN);
                                    default:
                                        PARSE_ERROR();
                                }
                            }

                        case STEP_MICHELSON_SET_DELEGATE_CHAIN:

                        {
                            uint16_t val = MICHELSON_READ_SHORT;
                            if (val != MICHELSON_SET_DELEGATE) PARSE_ERROR();

                            out->operation.tag = OPERATION_TAG_BABYLON_DELEGATION;
                            out->operation.destination.originated = true;
                            JMP(STEP_MICHELSON_CONTRACT_END);
                        }

                        case STEP_MICHELSON_CONTRACT_TO_IMPLICIT_CHAIN:
                            // Matching: PUSH key_hash <adr> ; IMPLICIT_ACCOUNT ; PUSH mutez <val> ;
                            // UNIT ; TRANSFER_TOKENS

                            OP_STEP_REQUIRE_SHORT(MICHELSON_PUSH)
                            OP_STEP_REQUIRE_SHORT(MICHELSON_MUTEZ)

                            {
                                uint8_t contractOrImplicit = NEXT_BYTE;
                                if (contractOrImplicit != 0) PARSE_ERROR();  // what is this?
                            }

                            OP_STEP

                            out->operation.amount = PARSE_Z_MICHELSON;

                            OP_STEP

                            OP_STEP_REQUIRE_SHORT(MICHELSON_UNIT)

                            {
                                uint16_t val = MICHELSON_READ_SHORT;
                                if (val != MICHELSON_TRANSFER_TOKENS) PARSE_ERROR();
                                out->operation.tag = OPERATION_TAG_BABYLON_TRANSACTION;

                                JMP(STEP_MICHELSON_CONTRACT_END);
                            }

                        case STEP_MICHELSON_CONTRACT_TO_CONTRACT:

                        {
                            // Matching: PUSH address <adr> ; CONTRACT <par> ; ASSERT_SOME ; PUSH
                            // mutez <val> ; UNIT ; TRANSFER_TOKENS
                            MICHELSON_READ_ADDRESS(&out->operation.destination, state->base58_pkh2);
                        }

                            OP_STEP

                            state->contract_code = MICHELSON_READ_SHORT;

                            OP_STEP

                            {
                                const enum michelson_code type = MICHELSON_READ_SHORT;

                                // Can’t display any parameters, need
                                // to throw anything but unit out for now.
                                // TODO: display michelson arguments
                                if (type != MICHELSON_CONTRACT_UNIT) {
                                    PARSE_ERROR();
                                }

                                switch (state->contract_code) {
                                    case MICHELSON_CONTRACT_WITH_ENTRYPOINT: {
                                        JMP(STEP_MICHELSON_CHECKING_CONTRACT_ENTRYPOINT);
                                    }
                                    case MICHELSON_CONTRACT: {
                                        JMP(STEP_MICHELSON_CONTRACT_TO_CONTRACT_CHAIN_2);
                                    }
                                    default:
                                        PARSE_ERROR();
                                }
                            }

                        case STEP_MICHELSON_CHECKING_CONTRACT_ENTRYPOINT: {
                            // No way to display
                            // entrypoint now, so need to
                            // bail out on anything but
                            // default.
                            // TODO: display entrypoints
                            uint8_t entrypoint_byte = NEXT_BYTE;
                            if (entrypoint_byte != ENTRYPOINT_DEFAULT) {
                                PARSE_ERROR();
                            }
                        }
                            JMP(STEP_MICHELSON_CONTRACT_TO_CONTRACT_CHAIN_2);

                        case STEP_MICHELSON_CONTRACT_TO_CONTRACT_CHAIN_2:

                            OP_STEP_REQUIRE_BYTE(MICHELSON_TYPE_SEQUENCE);

                            OP_STEP_REQUIRE_LENGTH(0x15);
                            OP_STEP_REQUIRE_SHORT(MICHELSON_IF_NONE);
                            OP_STEP_REQUIRE_BYTE(MICHELSON_TYPE_SEQUENCE);
                            OP_STEP_REQUIRE_LENGTH(9);
                            OP_STEP_REQUIRE_BYTE(MICHELSON_TYPE_SEQUENCE);
                            OP_STEP_REQUIRE_LENGTH(4);
                            OP_STEP_REQUIRE_SHORT(MICHELSON_UNIT);
                            OP_STEP_REQUIRE_SHORT(MICHELSON_FAILWITH);
                            OP_STEP_REQUIRE_BYTE(MICHELSON_TYPE_SEQUENCE);
                            OP_STEP_REQUIRE_LENGTH(0);
                            OP_STEP_REQUIRE_SHORT(MICHELSON_PUSH);
                            OP_STEP_REQUIRE_SHORT(MICHELSON_MUTEZ);
                            OP_STEP_REQUIRE_BYTE(0);

                            { out->operation.amount = PARSE_Z_MICHELSON; }

                            OP_STEP

                            OP_STEP_REQUIRE_SHORT(MICHELSON_UNIT);

                            {
                                uint16_t val = MICHELSON_READ_SHORT;
                                if (val != MICHELSON_TRANSFER_TOKENS) PARSE_ERROR();
                                out->operation.tag = OPERATION_TAG_BABYLON_TRANSACTION;
                                JMP(STEP_MICHELSON_CONTRACT_END);
                            }

                        case STEP_MICHELSON_FIRST_IS_NONE:  // withdraw delegate

                            OP_STEP_REQUIRE_SHORT(MICHELSON_KEY_HASH);

                            {
                                uint16_t val = MICHELSON_READ_SHORT;
                                if (val != MICHELSON_SET_DELEGATE) PARSE_ERROR();

                                out->operation.tag = OPERATION_TAG_BABYLON_DELEGATION;
                                out->operation.destination.originated = 0;
                                out->operation.destination.signature_type = SIGNATURE_TYPE_UNSET;
                            }

                            JMP(STEP_MICHELSON_CONTRACT_END);

                        case STEP_MICHELSON_CONTRACT_END:

                        {
                            uint16_t val = MICHELSON_READ_SHORT;
                            if (val != MICHELSON_CONS) PARSE_ERROR();
                        }

                            JMP_EOM;

                        default:
                            PARSE_ERROR();
                    }
                default:  // Any other tag; probably not possible here.
                    PARSE_ERROR();
            }
    }

    PARSE_ERROR();  // Probably not reachable, but removes a warning.
}

#define G global.apdu.u.sign
#ifdef BAKING_APP

static void parse_operations_throws_parse_error(struct parsed_operation_group *const out,
                                                void const *const data,
                                                size_t length,
                                                derivation_type_t derivation_type,
                                                bip32_path_t const *const bip32_path,
                                                is_operation_allowed_t is_operation_allowed) {
    size_t ix = 0;

    parse_operations_init(out, derivation_type, bip32_path, &G.parse_state);

    while (ix < length) {
        uint8_t byte = ((uint8_t *) data)[ix];
        parse_byte(byte, &G.parse_state, out, is_operation_allowed);
        PRINTF("Byte: %x - Next op_step state: %d\n", byte, G.parse_state.op_step);
        ix++;
    }

    if (!parse_operations_final(&G.parse_state, out)) PARSE_ERROR();
}

bool parse_operations(struct parsed_operation_group *const out,
                      uint8_t const *const data,
                      size_t length,
                      derivation_type_t derivation_type,
                      bip32_path_t const *const bip32_path,
                      is_operation_allowed_t is_operation_allowed) {
    BEGIN_TRY {
        TRY {
            parse_operations_throws_parse_error(out,
                                                data,
                                                length,
                                                derivation_type,
                                                bip32_path,
                                                is_operation_allowed);
        }
        CATCH(EXC_PARSE_ERROR) {
            return false;
        }
        CATCH_OTHER(e) {
            THROW(e);
        }
        FINALLY {
        }
    }
    END_TRY;
    return true;
}

#else

bool parse_operations_packet(struct parsed_operation_group *const out,
                             uint8_t const *const data,
                             size_t length,
                             is_operation_allowed_t is_operation_allowed) {
    BEGIN_TRY {
        TRY {
            size_t ix = 0;
            while (ix < length) {
                uint8_t byte = ((uint8_t *) data)[ix];
                parse_byte(byte, &G.parse_state, out, is_operation_allowed);
                PRINTF("Byte: %x - Next op_step state: %d\n", byte, G.parse_state.op_step);
                ix++;
            }
        }
        CATCH(EXC_PARSE_ERROR) {
            return false;
        }
        CATCH_OTHER(e) {
            THROW(e);
        }
        FINALLY {
        }
    }
    END_TRY;
    return true;
}

#endif
