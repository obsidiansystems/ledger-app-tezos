#include "operations.h"

#include "apdu.h"
#include "globals.h"
#include "memory.h"
#include "to_string.h"
#include "ui.h"
#include "michelson.h"

#include <stdint.h>
#include <string.h>

#define HARD_FAIL -2

// Argument is to distinguish between different parse errors for debugging purposes only
__attribute__((noreturn))
static void parse_error(
#ifndef TEZOS_DEBUG
    __attribute__((unused))
#endif
    uint32_t lineno) {

    global.apdu.u.sign.parse_state.op_step=HARD_FAIL;
#ifdef TEZOS_DEBUG
    THROW(0x9000 + lineno);
#else
    THROW(EXC_PARSE_ERROR);
#endif
}

#define PARSE_ERROR() parse_error(__LINE__)

//static inline void advance_ix(size_t *ix, size_t length, size_t amount) {
//    if (*ix + amount > length) PARSE_ERROR();

//    *ix += amount;
//}

/*
static inline uint8_t next_byte(void const *data, size_t *ix, size_t length, uint32_t lineno) {
    if (*ix >= length) parse_error(lineno);
    uint8_t res = ((const char *)data)[*ix];
    (*ix)++;
    return res;
}

#define NEXT_BYTE(data, ix, length) next_byte(data, ix, length, __LINE__)
*/

#define NEXT_BYTE(data, ix, length) byte

/*
static inline uint64_t parse_z(void const *data, size_t *ix, size_t length, uint32_t lineno) {
    uint64_t acc = 0;
    for (uint8_t shift = 0; ; ) {
        const uint8_t byte = next_byte(data, ix, length, lineno);
        acc |= ((uint64_t)byte & 0x7F) << shift;
        if (!(byte & 0x80)) {
            break;
        }
        shift += 7;
    }
    return acc;
}

#define PARSE_Z(data, ix, length) parse_z(data, ix, length, __LINE__)
*/


static inline bool parse_z(uint8_t current_byte, struct int_subparser_state *state, uint32_t lineno) {
  if(state->lineno != lineno) {
	  // New call; initialize.
	  state->lineno = lineno;
	  state->value = 0;
	  state->shift = 0;
  }
  state->value |= ((uint64_t)current_byte & 0x7F) << state->shift;
  state->shift += 7;
  return current_byte & 0x80; // Return true if we need more bytes.
}

#define PARSE_Z(data, ix, length) ({if(parse_z(byte, &state->subparser_state.integer, __LINE__)) return; state->subparser_state.integer.value;})

static inline bool parse_z_michelson(uint8_t current_byte, struct int_subparser_state *state, uint32_t lineno) {
  if(state->lineno != lineno) {
	  // New call; initialize.
	  state->lineno = lineno;
	  state->value = 0;
	  state->shift = 0;
  }
  state->value |= ((uint64_t)current_byte & 0x7F) << state->shift;
  // For some reason we are getting numbers shifted 1 bit to the
  // left. TODO: figure out why this happens
  if (state->shift == 0) {
      state->shift += 6;
  } else {
      state->shift += 7;
  }
  return current_byte & 0x80; // Return true if we need more bytes.
}

#define PARSE_Z_MICHELSON(data, ix, length) ({if(parse_z_michelson(byte, &state->subparser_state.integer, __LINE__)) return; state->subparser_state.integer.value;})

static inline bool parse_next_type(uint8_t current_byte, struct nexttype_subparser_state *state, uint32_t lineno, uint32_t sizeof_type) {
    #ifdef DEBUG
    if(sizeof_type > sizeof(state->body)) PARSE_ERROR(); // Shouldn't happen, but error if it does and we're debugging. Neither side is dynamic.
    #endif

    if(state->lineno != lineno) {
	    state->lineno = lineno;
	    state->fill_idx = 0;
    }

    state->body.raw[state->fill_idx]=current_byte;
    state->fill_idx++;
    
    return state->fill_idx < sizeof_type; // Return true if we need more bytes.
}

// do _NOT_ keep pointers to this data around.
#define NEXT_TYPE(type) ({if(parse_next_type(byte, &(state->subparser_state.nexttype), __LINE__, sizeof(type))) return; (const type *) &(state->subparser_state.nexttype.body);})

// This macro assumes:
// * Beginning of data: const void *data
// * Total length of data: size_t length
// * Current index of data: size_t ix
// Any function that uses these macros should have these as local variables
//#define NEXT_TYPE(type) ({ \
//    const type *val = data + ix; \
//    advance_ix(&ix, length, sizeof(type)); \
//    val; \
//})

static inline signature_type_t parse_raw_tezos_header_signature_type(
    raw_tezos_header_signature_type_t const *const raw_signature_type
) {
    check_null(raw_signature_type);
    switch (READ_UNALIGNED_BIG_ENDIAN(uint8_t, &raw_signature_type->v)) {
        case 0: return SIGNATURE_TYPE_ED25519;
        case 1: return SIGNATURE_TYPE_SECP256K1;
        case 2: return SIGNATURE_TYPE_SECP256R1;
        default: PARSE_ERROR();
    }
}

static inline void compute_pkh(
    cx_ecfp_public_key_t *const compressed_pubkey_out,
    parsed_contract_t *const contract_out,
    derivation_type_t const derivation_type,
    bip32_path_t const *const bip32_path
) {
    check_null(bip32_path);
    check_null(compressed_pubkey_out);
    check_null(contract_out);
    cx_ecfp_public_key_t const *const pubkey = generate_public_key_return_global(derivation_type, bip32_path);
    public_key_hash(
        contract_out->hash, sizeof(contract_out->hash),
        compressed_pubkey_out,
        derivation_type, pubkey);
    contract_out->signature_type = derivation_type_to_signature_type(derivation_type);
    if (contract_out->signature_type == SIGNATURE_TYPE_UNSET) THROW(EXC_MEMORY_ERROR);
    contract_out->originated = 0;
}

static inline void parse_implicit(
    parsed_contract_t *const out,
    raw_tezos_header_signature_type_t const *const raw_signature_type,
    uint8_t const hash[HASH_SIZE]
) {
    check_null(raw_signature_type);
    out->originated = 0;
    out->signature_type = parse_raw_tezos_header_signature_type(raw_signature_type);
    memcpy(out->hash, hash, sizeof(out->hash));
}

static inline void parse_contract(parsed_contract_t *const out, struct contract const *const in) {
    out->originated = in->originated;
    if (out->originated == 0) { // implicit
        out->signature_type = parse_raw_tezos_header_signature_type(&in->u.implicit.signature_type);
        memcpy(out->hash, in->u.implicit.pkh, sizeof(out->hash));
    } else { // originated
        out->signature_type = SIGNATURE_TYPE_UNSET;
        memcpy(out->hash, in->u.originated.pkh, sizeof(out->hash));
    }
}

/*

static inline uint32_t michelson_read_length(void const *data, size_t *ix, size_t length, uint32_t lineno) {
    if (*ix >= length) parse_error(lineno);
    const uint32_t res = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &data[*ix]);
    (*ix) += sizeof(uint32_t);
    return res;
}

#define MICHELSON_READ_LENGTH(data, ix, length) michelson_read_length(data, ix, length, __LINE__)

*/

static inline bool michelson_read_length(uint8_t current_byte, struct nexttype_subparser_state *state, uint32_t lineno) {
  bool rv = parse_next_type(current_byte, state, lineno, sizeof(uint32_t));
  uint32_t res = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &state->body.raw);
  state->body.i32 = res;
  return rv;
}

#define MICHELSON_READ_LENGTH(data, ix, length) ({if(michelson_read_length(byte, &state->subparser_state.nexttype, __LINE__)) return; state->subparser_state.nexttype.body.i32;})

static inline bool michelson_read_short(uint8_t current_byte, struct nexttype_subparser_state *state, uint32_t lineno) {
  bool rv = parse_next_type(current_byte, state, lineno, sizeof(uint16_t));
  uint32_t res = READ_UNALIGNED_BIG_ENDIAN(uint16_t, &state->body.raw);
  state->body.i16 = res;
  return rv;
}

#define MICHELSON_READ_SHORT(data, ix, length) ({if(michelson_read_short(byte, &state->subparser_state.nexttype, __LINE__)) return; state->subparser_state.nexttype.body.i16;})

/*

static inline uint16_t michelson_read_short(void const *data, size_t *ix, size_t length, uint32_t lineno) {
    if (*ix >= length) parse_error(lineno);
    const uint16_t res = READ_UNALIGNED_BIG_ENDIAN(uint16_t, &data[*ix]);
    (*ix) += sizeof(uint16_t);
    return res;
}

*/

static inline bool michelson_read_address(uint8_t byte, parsed_contract_t *const out, struct michelson_address_subparser_state *state, uint32_t lineno) {

    if(state->lineno != lineno) {
        memset(state, 0, sizeof(*state));
	state->lineno=lineno;
    }
    switch (state->address_step) {
	case 0:
	state->micheline_type=byte;
	state->address_step=1;
	return true;
        
        case 1:

	if(michelson_read_length(byte, &state->subsub_state, __LINE__)) return true;
	state->addr_length = state->subsub_state.body.i32;

	state->address_step=2;
	return true;
	default:

	switch (state->micheline_type) {
          case MICHELSON_TYPE_BYTE_SEQUENCE: {
	    switch (state->address_step) {
              case 2:

              // Need 1 byte for signature, plus the rest of the hash.
              if (state->addr_length != HASH_SIZE + 1) {
                PARSE_ERROR();
              }

              if(parse_next_type(byte, &(state->subsub_state), __LINE__, sizeof(state->signature_type))) return true;
	      memcpy(&(state->signature_type), &(state->subsub_state.body), sizeof(state->signature_type));

              case 3:

              if(parse_next_type(byte, &(state->subsub_state), __LINE__, sizeof(state->key_hash))) return true;
	      memcpy(&(state->key_hash), &(state->subsub_state.body), sizeof(state->key_hash));
	    
              parse_implicit(out, &state->signature_type, (const uint8_t *)&state->key_hash);
	      
	      return false;
          }
          case MICHELSON_TYPE_STRING: {
            if (state->addr_length != HASH_SIZE_B58) {
              PARSE_ERROR();
            }

	    PARSE_ERROR(); // Not supported yet; need to sort out storage.
              
	    if(parse_next_type(byte, &(state->subsub_state), __LINE__, sizeof(state->key_hash))) return true;
	    memcpy(&(state->key_hash), &(state->subsub_state.body), sizeof(state->key_hash));

	    /*
            out->hash_ptr = (char*)data + *ix;
            (*ix) += HASH_SIZE_B58;
            out->originated = false;
            out->signature_type = SIGNATURE_TYPE_UNSET;
            break; */
          }
          default: PARSE_ERROR();
    }
}
}
}

void parse_operations_init(
    struct parsed_operation_group *const out,
    derivation_type_t derivation_type,
    bip32_path_t const *const bip32_path,
    struct parse_state *const state
    ) {

    check_null(out);
    check_null(bip32_path);
    memset(out, 0, sizeof(*out));

    out->operation.tag = OPERATION_TAG_NONE;

    compute_pkh(&out->public_key, &out->signing, derivation_type, bip32_path);
    
    // Start out with source = signing, for reveals
    // TODO: This is slightly hackish
    memcpy(&out->operation.source, &out->signing, sizeof(out->signing));

    state->op_step=0;
    state->subparser_state.integer.lineno=-1;
    state->tag=OPERATION_TAG_NONE; // This and the rest shouldn't be required.
    state->argument_length=0;
    state->michelson_op=-1;
}

#define END_OF_MESSAGE -1    

bool parse_operations_final(struct parse_state *const state) {
    return state->op_step == END_OF_MESSAGE || state->op_step == 1;
}

static inline void parse_byte(
		uint8_t byte,
	       	struct parse_state *const state,
	       	struct parsed_operation_group *const out,
                is_operation_allowed_t is_operation_allowed
	    ){

#define OP_STEP state->op_step=__LINE__; return; case __LINE__:
#define OP_NAMED_STEP(name) state->op_step=name; return; case name:
    switch(state->op_step) {

	case HARD_FAIL:
	PARSE_ERROR();

        case END_OF_MESSAGE:
	PARSE_ERROR(); // We already hit a hard end of message; fail.
        #define JMP_EOM state ->op_step=-1; return;


	case 0:
	{
        // Verify magic byte, ignore block hash
        const struct operation_group_header *ogh = NEXT_TYPE(struct operation_group_header);
        if (ogh->magic_byte != MAGIC_BYTE_UNSAFE_OP) PARSE_ERROR();
	}

	OP_NAMED_STEP(1);

	state->tag = NEXT_BYTE(data, &ix, length);

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
                struct implicit_contract const *const implicit_source = NEXT_TYPE(struct implicit_contract);
                out->operation.source.originated = 0;
                out->operation.source.signature_type = parse_raw_tezos_header_signature_type(&implicit_source->signature_type);
                memcpy(out->operation.source.hash, implicit_source->pkh, sizeof(out->operation.source.hash));
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

            default: PARSE_ERROR();
        }


#define OP_JMPIF(step, cond) if(cond) { state->op_step=step; return; }
#define AFTER_MANAGER_FIELDS 10002
	OP_JMPIF(AFTER_MANAGER_FIELDS, (state->tag == OPERATION_TAG_PROPOSAL || state->tag == OPERATION_TAG_BALLOT))

	
	OP_STEP

        // out->operation.source IS NORMALIZED AT THIS POINT

        // Parse common fields for non-governance related operations.
        // if (tag != OPERATION_TAG_PROPOSAL && tag != OPERATION_TAG_BALLOT) {
	
            out->total_fee += PARSE_Z(data, &ix, length); // fee
	OP_STEP
            PARSE_Z(data, &ix, length); // counter
	OP_STEP
            PARSE_Z(data, &ix, length); // gas limit
	OP_STEP
            out->total_storage_limit += PARSE_Z(data, &ix, length); // storage limit
        //}
        
	OP_JMPIF(AFTER_MANAGER_FIELDS, (state->tag != OPERATION_TAG_ATHENS_REVEAL && state->tag != OPERATION_TAG_BABYLON_REVEAL))
	OP_STEP

        //if (tag == OPERATION_TAG_ATHENS_REVEAL || tag == OPERATION_TAG_BABYLON_REVEAL) {
            // Public key up next! Ensure it matches signing key.
            // Ignore source :-) and do not parse it from hdr.
            // We don't much care about reveals, they have very little in the way of bad security
            // implications and any fees have already been accounted for
	{
            raw_tezos_header_signature_type_t const *const sig_type = NEXT_TYPE(raw_tezos_header_signature_type_t);
            if (parse_raw_tezos_header_signature_type(sig_type) != out->signing.signature_type) PARSE_ERROR();
	}

        OP_STEP

	{
            size_t klen = out->public_key.W_len;

            if(parse_next_type(byte, &(state->subparser_state.nexttype), __LINE__, klen)) return;

	    if(memcmp(out->public_key.W, &(state->subparser_state.nexttype.body.raw), klen) != 0) PARSE_ERROR();

            out->has_reveal = true;

#define JMP_TO_TOP state ->op_step=1; return;
	    JMP_TO_TOP
	}
        //}
	
        case AFTER_MANAGER_FIELDS:

        if (out->operation.tag != OPERATION_TAG_NONE) {
            // We are only currently allowing one non-reveal operation
            PARSE_ERROR();
        }

        // This is the one allowable non-reveal operation per set

        out->operation.tag = (uint8_t)state->tag;

        // If the source is an implicit contract,...
        if (out->operation.source.originated == 0) {
            // ... it had better match our key, otherwise why are we signing it?
            if (COMPARE(&out->operation.source, &out->signing) != 0) PARSE_ERROR();
        }
        // OK, it passes muster.

        // This should by default be blanked out
        out->operation.delegate.signature_type = SIGNATURE_TYPE_UNSET;
        out->operation.delegate.originated = 0;

#define OP_TYPE_DISPATCH 10001
	state->op_step=OP_TYPE_DISPATCH;
        default:

        switch (state->tag) {
            case OPERATION_TAG_PROPOSAL:
                switch(state->op_step) {
                  case OP_TYPE_DISPATCH:
			  {
                    const struct proposal_contents *proposal_data = NEXT_TYPE(struct proposal_contents);

                    const size_t payload_size = READ_UNALIGNED_BIG_ENDIAN(int32_t, &proposal_data->num_bytes);
                    if (payload_size != PROTOCOL_HASH_SIZE) PARSE_ERROR(); // We only accept exactly 1 proposal hash.

                    out->operation.proposal.voting_period = READ_UNALIGNED_BIG_ENDIAN(int32_t, &proposal_data->period);

                    memcpy(out->operation.proposal.protocol_hash, proposal_data->hash, sizeof(out->operation.proposal.protocol_hash));
			  }

                    JMP_EOM
                }
                break;
            case OPERATION_TAG_BALLOT:
                switch(state->op_step) {
                  case OP_TYPE_DISPATCH:
			  {
                    const struct ballot_contents *ballot_data = NEXT_TYPE(struct ballot_contents);

                    out->operation.ballot.voting_period = READ_UNALIGNED_BIG_ENDIAN(int32_t, &ballot_data->period);
                    memcpy(out->operation.ballot.protocol_hash, ballot_data->proposal, sizeof(out->operation.ballot.protocol_hash));

                    const int8_t ballot_vote = READ_UNALIGNED_BIG_ENDIAN(int8_t, &ballot_data->ballot);
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
		    JMP_EOM
			  }
                }
                break;
            case OPERATION_TAG_ATHENS_DELEGATION:
            case OPERATION_TAG_BABYLON_DELEGATION:
                switch(state->op_step) {
                  case OP_TYPE_DISPATCH:
			  {
                    uint8_t delegate_present = NEXT_BYTE(data, &ix, length);

#define HAS_DELEGATE 10003
	            OP_JMPIF(HAS_DELEGATE, delegate_present)
			  }
		    // Else branch: Encode "not present"
                    out->operation.destination.originated = 0;
                    out->operation.destination.signature_type = SIGNATURE_TYPE_UNSET;
		    
		    JMP_TO_TOP // These go back to the top to catch any reveals.

		    OP_NAMED_STEP(HAS_DELEGATE)

		    {
                    const struct delegation_contents *dlg = NEXT_TYPE(struct delegation_contents);
                    parse_implicit(&out->operation.destination, &dlg->signature_type, dlg->hash);
		    }
		    JMP_TO_TOP // These go back to the top to catch any reveals.

                }
                break;
            case OPERATION_TAG_ATHENS_ORIGINATION:
            case OPERATION_TAG_BABYLON_ORIGINATION:
		PARSE_ERROR(); // We can't parse the script yet, and all babylon originations have a script.
            /*    switch(state->op_step) {
                  case OP_TYPE_DISPATCH:
                    struct origination_header {
                        raw_tezos_header_signature_type_t signature_type;
                        uint8_t hash[HASH_SIZE];
                    } __attribute__((packed));
                    struct origination_header const *const hdr = NEXT_TYPE(struct origination_header);

                    parse_implicit(&out->operation.destination, &hdr->signature_type, hdr->hash);
                    out->operation.amount = PARSE_Z(data, &ix, length);
                    if (NEXT_BYTE(data, &ix, length) != 0) {
                        out->operation.flags |= ORIGINATION_FLAG_SPENDABLE;
                    }
                    if (NEXT_BYTE(data, &ix, length) != 0) {
                        out->operation.flags |= ORIGINATION_FLAG_DELEGATABLE;
                    }
                    if (NEXT_BYTE(data, &ix, length) != 0) {
                        // Has delegate
                        const struct delegation_contents *dlg = NEXT_TYPE(struct delegation_contents);
                        parse_implicit(&out->operation.delegate, &dlg->signature_type, dlg->hash);
                    }
                    if (NEXT_BYTE(data, &ix, length) != 0) PARSE_ERROR(); // Has script
                }*/
		break;
            case OPERATION_TAG_ATHENS_TRANSACTION:
            case OPERATION_TAG_BABYLON_TRANSACTION:
                switch(state->op_step) {
                  case OP_TYPE_DISPATCH:

                    out->operation.amount = PARSE_Z(data, &ix, length);

		    OP_STEP

		    {
                    const struct contract *destination = NEXT_TYPE(struct contract);
                    parse_contract(&out->operation.destination, destination);
		    }

		    OP_STEP

		    {
                    char has_params = NEXT_BYTE(data, &ix, length);

		    if(has_params == MICHELSON_PARAMS_NONE) {
			    JMP_TO_TOP
		    }

		    if(has_params != MICHELSON_PARAMS_SOME) {
			    PARSE_ERROR();
		    }
		    }

                    // From this point on we are _only_ parsing manager.tz operatinos, so we show the outer destination (the KT1) as the source of the transaction.
                            out->operation.is_manager_tz_operation = true;
                            memcpy(&out->operation.implicit_account, &out->operation.source, sizeof(parsed_contract_t));
                            memcpy(&out->operation.source, &out->operation.destination, sizeof(parsed_contract_t));

                            // manager.tz operations cannot actually transfer any amount.
                            if (out->operation.amount > 0) {
                                PARSE_ERROR();
                            }


			    OP_STEP 
			    {
                            const enum entrypoint_tag entrypoint = NEXT_BYTE(data, &ix, length);

			    /* We don't care yet.
                            // Named entrypoints have up to 31 bytes.
                            if (entrypoint == ENTRYPOINT_NAMED) {
                                const uint8_t entrypoint_length = NEXT_BYTE(data, &ix, length);
                                if (entrypoint_length > MAX_ENTRYPOINT_LENGTH) {
                                    PARSE_ERROR();
                                }
                                for (int n = 0; n < entrypoint_length; n++) {
                                    NEXT_BYTE(data, &ix, length);
                                }
                            } */

                            // Anything that’s not “do” is not a
                            // manager.tz contract.
                            if (entrypoint != ENTRYPOINT_DO) {
                                PARSE_ERROR();
                            }
			    }

			    OP_STEP

			    {
                            // const uint32_t argument_length = MICHELSON_READ_LENGTH(data, &ix, length);
			    state->argument_length = MICHELSON_READ_LENGTH(data, &ix, length);
			    }

			    OP_STEP

			    {
	                    const uint8_t michelson_type = NEXT_BYTE(data, &ix, length);	
                            // Error on anything but a michelson
                            // sequence.
                            if (michelson_type != MICHELSON_TYPE_SEQUENCE) {
                                PARSE_ERROR();
                            }
			    }

			    OP_STEP

			    {
                            const uint32_t sequence_length = MICHELSON_READ_LENGTH(data, &ix, length);

                            // Only allow single sequence (5 is needed
                            // in argument length for above two
                            // bytes). Also bail out on really big
                            // Michelson that we don’t support.
                            if (   sequence_length + sizeof(uint8_t) + sizeof(uint32_t) != state->argument_length
                                || state->argument_length > MAX_MICHELSON_SEQUENCE_LENGTH) {
                                PARSE_ERROR();
                            }
			    }

			    OP_STEP

#define OP_STEP_REQUIRE_SHORT(constant) { uint16_t val = MICHELSON_READ_SHORT(data, &ix, length); if(val != constant) PARSE_ERROR(); } OP_STEP

                            // All manager.tz operations should begin
                            // with this. Otherwise, bail out.
	                    OP_STEP_REQUIRE_SHORT(MICHELSON_DROP)
	                    OP_STEP_REQUIRE_SHORT(MICHELSON_NIL)
	                    OP_STEP_REQUIRE_SHORT(MICHELSON_OPERATION)

			    state->michelson_op = MICHELSON_READ_SHORT(data, &ix, length);

                            // First real michelson op.
#define MICHELSON_FIRST_IS_PUSH 10010
#define MICHELSON_FIRST_IS_NONE 10011
                            switch (state->michelson_op) {
                                case MICHELSON_PUSH: 
					state->op_step = MICHELSON_FIRST_IS_PUSH;
					return;
                                case MICHELSON_NONE: // withdraw delegate
					state->op_step = MICHELSON_FIRST_IS_NONE;
					return;
				default:
					PARSE_ERROR();
			    }


                            case MICHELSON_FIRST_IS_PUSH:

			    state->michelson_op = MICHELSON_READ_SHORT(data, &ix, length);

                            // First real michelson op.
#define MICHELSON_SECOND_IS_KEY_HASH 10012
#define MICHELSON_CONTRACT_TO_CONTRACT 10013
                            switch (state->michelson_op) {
                                case MICHELSON_KEY_HASH: 
					state->op_step = MICHELSON_SECOND_IS_KEY_HASH;
					return;
                                case MICHELSON_ADDRESS: // transfer contract to contract
                                        state->op_step = MICHELSON_CONTRACT_TO_CONTRACT;
					return;
				default:
					PARSE_ERROR();
			    }

                            case MICHELSON_SECOND_IS_KEY_HASH:

                            michelson_read_address(byte, &out->operation.destination, &(state->subparser_state.michelson_address), __LINE__);

			    OP_STEP
			    
			    state->michelson_op = MICHELSON_READ_SHORT(data, &ix, length);

#define MICHELSON_SET_DELEGATE_CHAIN 10014
#define MICHELSON_CONTRACT_TO_IMPLICIT_CHAIN 10015
                            switch (state->michelson_op) {
                                case MICHELSON_SOME: // Set delegate
					state->op_step = MICHELSON_SET_DELEGATE_CHAIN;
					return;
                                case MICHELSON_IMPLICIT_ACCOUNT: // transfer contract to implicit
                                        state->op_step = MICHELSON_CONTRACT_TO_IMPLICIT_CHAIN;
					return;
				default:
					PARSE_ERROR();
			    }

                            case MICHELSON_SET_DELEGATE_CHAIN:

	                    OP_STEP_REQUIRE_SHORT(MICHELSON_SOME)
			    {

                            uint16_t val = MICHELSON_READ_SHORT(data, &ix, length); 
			    if(val != MICHELSON_SET_DELEGATE) PARSE_ERROR();
                                                    
			    out->operation.tag = OPERATION_TAG_BABYLON_DELEGATION;
                            out->operation.destination.originated = true;
			    }
#define MICHELSON_CONTRACT_END 10016
			    state->op_step=MICHELSON_CONTRACT_END;
			    return;

			    case MICHELSON_CONTRACT_TO_IMPLICIT_CHAIN:
                            // Matching: PUSH key_hash <adr> ; IMPLICIT_ACCOUNT ; PUSH mutez <val> ; UNIT ; TRANSFER_TOKENS

			    OP_STEP_REQUIRE_SHORT(MICHELSON_PUSH)
			    OP_STEP_REQUIRE_SHORT(MICHELSON_MUTEZ)
			    
			    {

			    uint8_t contractOrImplicit = NEXT_BYTE(data, &ix, length);
			    if(contractOrImplicit != 0) PARSE_ERROR(); // what is this?

			    }

			    OP_STEP

			    out->operation.amount = PARSE_Z_MICHELSON(data, &ix, length);

			    OP_STEP

			    OP_STEP_REQUIRE_SHORT(MICHELSON_UNIT)

			    {

                            uint16_t val = MICHELSON_READ_SHORT(data, &ix, length); 
			    if(val != MICHELSON_TRANSFER_TOKENS) PARSE_ERROR();
			    out->operation.tag = OPERATION_TAG_BABYLON_TRANSACTION;

			    }
			    
			    state->op_step=MICHELSON_CONTRACT_END;
			    return;


			    case MICHELSON_CONTRACT_TO_CONTRACT:
                            // Matching: PUSH address <adr> ; CONTRACT <par> ; ASSERT_SOME ; PUSH mutez <val> ; UNIT ; TRANSFER_TOKENS
                            michelson_read_address(byte, &out->operation.destination, &(state->subparser_state.michelson_address), __LINE__);

			    OP_STEP

                            state->contract_code = MICHELSON_READ_SHORT(data, &ix, length);

			    OP_STEP

			    {

                            const enum michelson_code type = MICHELSON_READ_SHORT(data, &ix, length);
                                            
			    // Can’t display any parameters, need
                            // to throw anything but unit out for now.
                            // TODO: display michelson arguments
                            if (type != MICHELSON_CONTRACT_UNIT) {
                                PARSE_ERROR();
                            }

			    }
                                  
#define MICHELSON_CHECKING_CONTRACT_ENTRYPOINT 10017
#define MICHELSON_CONTRACT_TO_CONTRACT_CHAIN_2 10018
			    switch (state->contract_code) {
                                case MICHELSON_CONTRACT_WITH_ENTRYPOINT: {
				    state->op_step=MICHELSON_CHECKING_CONTRACT_ENTRYPOINT;
				    return;
                                }
                                case MICHELSON_CONTRACT: {
				    state->op_step=MICHELSON_CONTRACT_TO_CONTRACT_CHAIN_2;
				    return;
                                }
                                default: PARSE_ERROR();
                            }

			    case MICHELSON_CHECKING_CONTRACT_ENTRYPOINT:
			    {
                                // No way to display
                                // entrypoint now, so need to
                                // bail out on anything but
                                // default.
                                // TODO: display entrypoints
			        uint8_t entrypoint_byte = NEXT_BYTE(data, &ix, length);
				if(entrypoint_byte != ENTRYPOINT_DEFAULT) {
					PARSE_ERROR();
				}
			    }
		            state->op_step=MICHELSON_CONTRACT_TO_CONTRACT_CHAIN_2;
			    return;
			    case MICHELSON_CONTRACT_TO_CONTRACT_CHAIN_2:

#define OP_STEP_REQUIRE_BYTE(constant) { if(byte != constant) PARSE_ERROR(); } OP_STEP
#define OP_STEP_REQUIRE_LENGTH(constant) { uint32_t val = MICHELSON_READ_LENGTH(data, &ix, length); if(val != constant) PARSE_ERROR(); } OP_STEP

			    OP_STEP_REQUIRE_BYTE(MICHELSON_TYPE_SEQUENCE);

			    OP_STEP_REQUIRE_LENGTH(0x15);
			    OP_STEP_REQUIRE_SHORT(MICHELSON_IF_NONE)
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

                            out->operation.amount = PARSE_Z_MICHELSON(data, &ix, length);
			    OP_STEP

			    OP_STEP_REQUIRE_SHORT(MICHELSON_UNIT);
			    
			    uint16_t val = MICHELSON_READ_SHORT(data, &ix, length); 
			    if(val != MICHELSON_TRANSFER_TOKENS) PARSE_ERROR();
			    out->operation.tag = OPERATION_TAG_BABYLON_TRANSACTION;
			    
			    state->op_step=MICHELSON_CONTRACT_END;
			    return;
                            
	                    case MICHELSON_FIRST_IS_NONE: // withdraw delegate

			    OP_STEP_REQUIRE_SHORT(MICHELSON_KEY_HASH);

			    {
			    
			    uint16_t val = MICHELSON_READ_SHORT(data, &ix, length); 
			    if(val != MICHELSON_SET_DELEGATE) PARSE_ERROR();

                            out->operation.tag = OPERATION_TAG_BABYLON_DELEGATION;
                            out->operation.destination.originated = 0;
                            out->operation.destination.signature_type = SIGNATURE_TYPE_UNSET;

			    }

			    state->op_step=MICHELSON_CONTRACT_END;
			    return;

			    case MICHELSON_CONTRACT_END:
                            
			    {
			    uint16_t val = MICHELSON_READ_SHORT(data, &ix, length); 
			    if(val != MICHELSON_CONS) PARSE_ERROR();
			    }

		            JMP_EOM

                            default:
                                PARSE_ERROR();
        }
	    default: // Any other tag; probably not possible here.
		PARSE_ERROR();
    }

    //if (out->operation.tag == OPERATION_TAG_NONE && !out->has_reveal) {
    //    PARSE_ERROR(); // Must have at least one op
    //}
  }
}

static void parse_operations_throws_parse_error(
    struct parsed_operation_group *const out,
    void const *const data,
    size_t length,
    derivation_type_t derivation_type,
    bip32_path_t const *const bip32_path,
    is_operation_allowed_t is_operation_allowed
) {

    size_t ix = 0;

#define G global.apdu.u.sign

    parse_operations_init(out, derivation_type, bip32_path, &G.parse_state);

    while (ix < length) {
	    uint8_t byte = ((uint8_t*)data)[ix];
	    parse_byte(byte, &G.parse_state, out, is_operation_allowed);
    PRINTF("Byte: %x - Next op_step state: %d\n", byte, G.parse_state.op_step);
	    ix++;
    }

    if(! parse_operations_final(&G.parse_state)) PARSE_ERROR();

//    PRINTF("Operation parsing succeeded.\n");
}


bool parse_operations(
    struct parsed_operation_group *const out,
    uint8_t const *const data,
    size_t length,
    derivation_type_t derivation_type,
    bip32_path_t const *const bip32_path,
    is_operation_allowed_t is_operation_allowed
) {
    BEGIN_TRY {
        TRY {
            parse_operations_throws_parse_error(out, data, length, derivation_type, bip32_path, is_operation_allowed);
        }
        CATCH(EXC_PARSE_ERROR) {
            return false;
        }
        CATCH_OTHER(e) {
            THROW(e);
        }
        FINALLY { }
    }
    END_TRY;
    return true;
}


bool parse_operations_packet(
    struct parsed_operation_group *const out,
    uint8_t const *const data,
    size_t length,
    is_operation_allowed_t is_operation_allowed
) {
    BEGIN_TRY {
        TRY {
            size_t ix = 0;
            while (ix < length) {
	        uint8_t byte = ((uint8_t*)data)[ix];
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
        FINALLY { }
    }
    END_TRY;
    return true;
}
