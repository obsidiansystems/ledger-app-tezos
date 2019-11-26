{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE OverloadedLists #-}
{-# LANGUAGE DataKinds #-}
{-# LANGUAGE KindSignatures #-}

module Ops
(
    transferSim
  , delegationSim
  , makeTezosAPDU
  , LedgerInstruction(..)
  , SignatureType(..)

) where

-- import qualified Data.ByteString.Base16 as BS16
import qualified Data.ByteString as BS
import qualified Data.ByteString.Builder as BSB
import Data.ByteString.Lazy (toStrict)
import Data.Dependent.Sum
import qualified Data.HexString as Hex
import Data.Int (Int8)
import Data.Maybe (fromMaybe)

import qualified Tezos.Common.Binary as B
import Tezos.V005.Micheline
import Tezos.V005.Operation
import Tezos.V005.Types

data SignatureType =
     SignatureType_UNSET
  |  SignatureType_SECP256K1
  |  SignatureType_SECP256R1
  |  SignatureType_ED25519EC
  deriving (Eq, Show, Enum)
 
-- Order of the instructions listed in the type matters so that derive enum corresponds with 
-- the apdu instruction bytecodes
-- TODO: Fix the above
data LedgerInstruction = 
    LedgerInstruction_Version
  | LedgerInstruction_AuthorizeBaking
  | LedgerInstruction_GetPublicKey
  | LedgerInstruction_PromptPublicKey
  | LedgerInstruction_Sign
  | LedgerInstruction_SignUnsafe
  | LedgerInstruction_Reset
  | LedgerInstruction_QueryAuthKey
  | LedgerInstruction_QueryMainHWM
  | LedgerInstruction_Git
  | LedgerInstruction_Setup
  | LedgerInstruction_QueryAllHWM
  | LedgerInstruction_Deauthorize
  | LedgerInstruction_QueryAuthKeyWithCurve
  | LedgerInstruction_HMAC
  | LedgerInstruction_SignWithHash
  deriving (Eq, Show, Enum)

transferSim :: PublicKeyHash -> ContractId -> Tez -> TezosWord64 -> BS.ByteString
transferSim source destination amount storageLimit = 
  B.encode $ tag :=> Op 
    { _op_branch = "BLM3c4UQi5ebKtfKyVZfN3g9xxbw1EKczvJvGhMDrBCP8DVcWmm" -- mock branch name
    , _op_contents = contents
    , _op_signature = Nothing
    }
  where 
    tag = OpsKindTag_Single $ OpKindTag_Manager OpKindManagerTag_Transaction
    contents = OpContentsList_Single $ OpContents_Transaction $ 
      OpContentsManager
        { _opContentsManager_source = source
        , _opContentsManager_fee = 1
        , _opContentsManager_counter = 2590
        , _opContentsManager_gasLimit = 800000
        , _opContentsManager_storageLimit = storageLimit
        , _opContentsManager_operation = transferOp
        }
    transferOp = OpContentsTransaction
      { _opContentsTransaction_amount = amount
      , _opContentsTransaction_destination = destination
      , _opContentsTransaction_parameters = Nothing
      }


delegationSim :: PublicKeyHash -> PublicKeyHash -> TezosWord64 -> BS.ByteString
delegationSim source delegate storageLimit = B.encode $ tag :=> Op 
  { _op_branch = "BLM3c4UQi5ebKtfKyVZfN3g9xxbw1EKczvJvGhMDrBCP8DVcWmm"
  , _op_contents = contents
  , _op_signature = Nothing
  }
  where 
    tag = OpsKindTag_Single $ OpKindTag_Manager OpKindManagerTag_Delegation
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
      { _opContentsDelegation_delegate = Just delegate
      }

 --TODO: Cover enums with over 128 elements
enumToByteB :: Enum a => a -> BSB.Builder
enumToByteB = BSB.int8 . (fromIntegral :: Int -> Int8) . fromEnum

makeTezosAPDU :: LedgerInstruction -> Maybe SignatureType -> BS.ByteString -> [BS.ByteString]
makeTezosAPDU instruction sigTypeM payload = 
  fmap (\bs -> toStrict $ BSB.toLazyByteString $ toAPDU (enumToByteB instruction) p2FinalB (enumToByteB sigType) bs) $ splitMsg payload
  where 
    -- TODO: Get this to work
    -- setupKeyAPDU :: BS.ByteString
    -- setupKeyAPDU = "8004000311048000002c800006c18000000080000000" --this should be the same for all apdus
    -- fullMsg =  setupKeyAPDU : (splitMsg payload)

    sigType = fromMaybe SignatureType_UNSET sigTypeM
    claByteB = BSB.int8 128 -- 0x80
    p2InitB = BSB.int8 1 -- 0x01
    p2MiddleB = BSB.int8 0 -- 0x00
    p2FinalB = BSB.int8 129 -- 0x81
    magicByteB = BSB.int8 3 -- 0x03
    maxRawMsgSize = 230

    -- splitMsg :: BS.ByteString -> [BS.ByteString]
    -- splitMsg rawTxn = 
    --   let txnWithMB = BSB.lazyByteString $ magicByteB <> byteString rawTxn in
    --   if BS.length txnWithMB > maxRawMsgSize 
    --     then (BS.take maxRawTxnSize txnWithMB) : (splitMsg $ BS.drop maxRawTxnSize txnWithMB)
    --     else [txnWithMB]

    splitMsg :: BS.ByteString -> [BS.ByteString]
    splitMsg rawTxn = 
    -- This is just a tmp implementation of the function, the above one has the real logic that should be used
      [toStrict $ BSB.toLazyByteString $ magicByteB <> (BSB.byteString rawTxn) ]
     
    toAPDU insB p2B curveB msg = 
      claByteB <> insB <> p2B <> curveB <> (enumToByteB $ BS.length msg) <> BSB.byteString msg
