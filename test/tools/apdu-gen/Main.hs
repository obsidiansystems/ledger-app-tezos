{-# LANGUAGE OverloadedStrings #-}

module Main where

import qualified Data.HexString as Hex

import Tezos.V005.Types

import Ops

main :: IO ()
main = do
  -- A simple example that prints out the apdus for two different Tezos operations
  -- Note: The ledger will switch to a signed hash if the src argument is not the pkh of the ledger's key
  print $ fmap Hex.fromBytes $ makeTezosAPDU LedgerInstruction_Sign Nothing $ transferSim src dst 4 60000
  print $ fmap Hex.fromBytes $ makeTezosAPDU LedgerInstruction_Sign Nothing $ delegationSim src delegate 60000
  where 
    src = "tz1eY5Aqa1kXDFoiebL28emyXFoneAoVg1zh"
    dst = "tz1cCTB1S2UYtGVJyuTQ9ktnnkemuqaSwvZn"
    delegate = "tz1cCTB1S2UYtGVJyuTQ9ktnnkemuqaSwvZn"


