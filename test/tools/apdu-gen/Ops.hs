{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE OverloadedLists #-}
{-# LANGUAGE DataKinds #-}
{-# LANGUAGE KindSignatures #-}

module Main where

import qualified Tezos.Common.Binary as B
import qualified Data.ByteString.Base16 as BS16
import qualified Data.HexString as Hex
import Data.Dependent.Sum
import Tezos.V005.Micheline
import Tezos.V005.Operation
import Tezos.V005.Types

main :: IO ()
main = print $ Hex.fromBytes $ B.encode $ tranKindTag :=> (transferSim src dst 5 0)
  where 
    src = "tz1h3QwQMhFhAbdWcKDsTWWmF7iMbo8py2MD"
    dst = "tz1cCTB1S2UYtGVJyuTQ9ktnnkemuqaSwvZn"
    tranKindTag = OpsKindTag_Single $ OpKindTag_Manager OpKindManagerTag_Transaction

transferSim :: PublicKeyHash -> ContractId -> Tez -> TezosWord64 -> Op ('OpKind_Manager '[ 'OpKindManager_Transaction] :: OpKind)
transferSim source destination amount storageLimit = Op 
  { _op_branch = "BLM3c4UQi5ebKtfKyVZfN3g9xxbw1EKczvJvGhMDrBCP8DVcWmm"
  , _op_contents = contents
  , _op_signature = Nothing
  }
  where 
    contents = OpContentsList_Single $ OpContents_Transaction $ 
      OpContentsManager
        { _opContentsManager_source = source
        , _opContentsManager_fee = 1
        , _opContentsManager_counter = 2590
        , _opContentsManager_gasLimit = 800000
        , _opContentsManager_storageLimit = 60000
        , _opContentsManager_operation = transferOp
        }
    transferOp = OpContentsTransaction
      { _opContentsTransaction_amount = amount
      , _opContentsTransaction_destination = destination
      , _opContentsTransaction_parameters = Nothing
      }

delegationSim :: PublicKeyHash -> PublicKeyHash -> TezosWord64 -> Op ('OpKind_Manager '[ 'OpKindManager_Delegation] :: OpKind)
delegationSim source delegate storageLimit = Op 
  { _op_branch = "BLM3c4UQi5ebKtfKyVZfN3g9xxbw1EKczvJvGhMDrBCP8DVcWmm"
  , _op_contents = contents
  , _op_signature = Nothing
  }
  where 
    contents = OpContentsList_Single $ OpContents_Delegation $ 
      OpContentsManager
        { _opContentsManager_source = source
        , _opContentsManager_fee = 1
        , _opContentsManager_counter = 2590
        , _opContentsManager_gasLimit = 800000
        , _opContentsManager_storageLimit = storageLimit
        , _opContentsManager_operation = delegateOp
        }
    delegateOp = OpContentsDelegation
			{ _opContentsDelegation_delegate = delegate
			}



-- data SignatureType = SignatureType
--      SignatureType_UNSET
--   |  SignatureType_SECP256K1
--   |  SignatureType_SECP256R1
--   |  SignatureType_ED25519EC

-- data Instruction = 
--     Instruction_Version
--   | Instruction_AuthorizeBaking
--   | Instruction_GetPublicKey
--   | Instruction_PromptPublicKey
--   | Instruction_Sign
--   | Instruction_SignUnsafe
--   | Instruction_Reset
--   | Instruction_Git
--   | Instruction_SignWithHash
--   | Instruction_HMAC

-- data APDU = APDU
--   { _apdu_cla :: Int
--   , _apdu_instruction :: Instruction
--   , _apdu_p1_byte :: Int
--   , _apdu_signCurve :: SigningCurve
--   , _apdu_msgLength :: Int
--   , _apdu_msg :: String
--   }

-- This first one sets up the derivation path for the key.
-- How many different sets of transactions can be sent? Document
-- Does the src need to be a tz1 if the encryption is 
-- echo 8004000311048000002c800006c18000000080000000
-- echo 800481005d035379ba9122785f71ec283a3b6398c05edc8f8d77eef885d167d22c50e9f9c26c6c00cf49f66b9ea137e11818f2a78b4b6fc9895b4e50c0843d9e1480ea30e0d403c096b1020000b5a3c247300abfea1242d10f347c321f796c1b8800
