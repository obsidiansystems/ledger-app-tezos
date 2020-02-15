# Version 1.3 of the Tezos Wallet and Baking Applications for Ledger Nano S

## Release Highlights

### Ledger Nano S Wallet Application
- [x] Transactions now display with: source, destination, amount and fee
- [x] Delegations now display with: source, delegate, amount and fee
- [x] Account originations now display with: source, manager, fee, amount and delegation
- [x] Support for browser access through U2F

In addition to the improved user experience, these changes are important security enablers, as it
can help a cautious user protect against a certain type of attack. There are also instances where the Ledger device will not display operation information listed above. See more details below.

### Ledger Nano S Baking Application
- [x] High watermark feature extended to protect against double-endorsing as well as double-baking.

## Ledger Nano S Wallet Application -- Release Details
### Operation Display
The new version of the Wallet App will display certain fields of most
transactions, delegations, and account origination in the prompt where
the user is asked to approve or reject the delegation. In addition to the
improved user experience, this is an important security improvement, as it
can help a cautious user protect against a certain type of attack. Without
this measure, an attacker with control over your computer can replace a
legitimate transaction with a falsified transaction, which could send
any amount of tez from any wallet on the Ledger hardware wallet (with any derivation
path) to the attacker, and the user would approve it thinking it was the
transaction they intended. The security benefit is only realized if the
user manually verifies the relevant fields.

The fields are displayed in sequence, one after another. To verify all
the fields, you must wait for all of the fields to display in order.
The sequence is repeated 3 times, after which the app will default to
a rejection of the transaction.

#### Transactions
* Source:
This is the account from which the tez are to be transferred.  Because
the ledger embodies as many accounts as there are derivation paths,
it might be important to verify that the transaction originates from
the intended account.

* Destination:
This is the account to which the tez are to be transferred. If this were
faked, the attacker could send to their own account instead.

* Amount:
This is the amount of tez to be sent. If this were faked, an attacker
with a relationship with the recipient could cause more tez to be sent
than desired.

* Fee:
This is the fee given to the baker of the block on which the transaction
will be included. If this were faked, a baker with a relationship to
the attacker could end up with the stolen tez, especially if the fee
were astronomical.

#### Delegations
* Source:
This is which account is to be delegated. If this were faked, the attacker
could prevent the user from using a delegation service or registering
for delegation, or change the delegation on a different account to the
delegation service.

* Delegate:
This is the address to which the user is delegating baking rights. If
this were faked, the attacker could substitute their own delegation
service for the intended one. We also indicate the distinction between
delegating and withdrawing delegation.

* Fee: See above.

#### Originations
* Source:
This is where the original funding of the originated account comes
from. If this were faked, an attacker could affect the allocation of
the user's tez between accounts.

* Manager:
This is what key will be used to manage the originated account. If this
were faked, the attacker could set it to their own key, and gain control
over the new account and its tez.

* Fee: See above.

* Amount:
This is the amount that will be transferred into the new, originated
account.  If this were faked, it could prevent the user from using the
new account as intended.

* Delegation:
We display both whether the originated account is set up to allow
delegation, and which account it is originally set up to delegate to. If
it is set up to allow delegation but not set up with an initial delegate,
we display "Any" as the delegate. If it is set up to delegate but not
to change delegation, we display "Fixed Delegate" as the label for the
account. Any changes to the delegation settings on an originated account
could cause various inconveniences to the user, and potentially could
be useful in a sophisticated attack.

### Unverified Operations

Sometimes the wallet app will not be able to parse an operation, and
will prompt for an unverified signature. In this case, most users will
want to reject the operation.

The wallet app may not capable of parsing the message because of
advanced features in it. In this case, it displays a special prompt:
"Unrecognized Operation, Sign Unverified?" This should not happen with
ordinary transactions and delegations, but may happen in the presence
of optional information like fields that will be sent to contracts or
smart contract originations. If you are not using these features, you
should reject the transaction. You should only approve it if you are
using these features, and are confident in your computer's security.
Among advanced features is included non-zero storage limits. This
is because storage limits can cost additional tez, and so we want
to make sure that users are not surprised by these additional costs.

The wallet app also allows the signing of prehashed data, which it will
not be able to parse. In this situation, it will display "Pre-hashed
Operation, Sign Unverified?" You should not approve this transaction
unless you intentionally sent pre-hashed data to the ledger and are
confident in your computer's security. Pre-hashed data is not
exposed as a feature in `tezos-client`, and can only be sent
manually. Most users will never need to use this feature.

### U2F Support

The Wallet Application now supports U2F protocol, the standard method for enabling 
browser access by 3rd party wallet providers for all tokens. Recent versions 
of Ledger Nano S firmware (v1.4.1+) allow us to support browsers seamlessly without the need 
to toggle it in settings; the app will automatically detect which protocol is 
being used.

#### APDU level error: Unexpected sequence number (expect 0, got 191)

As a side effect of adding U2F support, users will see this error when sending operations 
to the Wallet Application. There are two situations where this error will fire:

* If you send an operation to the Tezos Wallet Application. `tezos-client` might interpret 
the presence of U2F support as a sequence number error, but it will recover from this error 
and successfully be able to communicate with the device over APDU protocol. In our experience, 
the operation always succeeds despite this error. We intend to have the error message from `tezos-client`
adjusted to reflect the success of these operations.
* If you send an operation to the Ledger device and neither Tezos Application is open. 
You'll be communicating with the ledger OS, not one of the Tezos Applications.

## Baking Application -- Release Details
The new version of the Baking Application extends the concept of the high watermark to
endorsements as well as block headers, as a precaution against double
baking and double endorsing. No block header or endorsement will be signed at a lower block
level than a previous block or endorsement. Furthermore, only one block
and one endorsement is allowed at each level, and the block must come
before the endorsement. Both block headers and endorsements are
signed without prompting if they satisfy the high watermark requirement,
and rejected without prompting if they do not.

This covers all legitimate operation of a baker in a single chain.
If a single baker has multiple endorsement slots at the same block
level, only one endorsement will actually need to be signed, and you
will receive the reward for all the endorsement slots at that level.

As before, you may reset the high watermark with a reset command
(`tezos-client set ledger high watermark for <uri> to <high watermark>`), which 
will prompt "Reset HWM" and the new value. Legitimate reasons to change the high 
watermark include switching to a test network at a different block level or 
restoring a baker after an attacker or software error caused a block to be signed 
with too high a level.

## Acknowledgements

Thank you to everyone in the tezos-baking Slack channel, especially Tom
Knudsen and Tom Jack, for their testing and bug reports.
