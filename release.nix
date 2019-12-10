let
  ledger-app-tezos = import ./. {};
in {
  analysis-nanos-wallet = ledger-app-tezos.clangAnalysis.s.wallet;
  analysis-nanos-baking = ledger-app-tezos.clangAnalysis.s.baking;
  release-nanos-wallet = ledger-app-tezos.nano.s.release.wallet;
  release-nanos-baking = ledger-app-tezos.nano.s.release.baking;
  release-nanos-all = ledger-app-tezos.nano.s.release.all;
}
