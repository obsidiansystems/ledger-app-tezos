# blue-app-ssh-agent

A simple SSH agent for Ledger Blue, supporting prime256v1 keys.

To use it : run getPublicKey.py to get the public key in SSH format, to be added to your authorized keys on the target

```
python getPublicKey.py
ecdsa-sha2-nistp256 AAAA....
```

Run agent.py, providing the base64 encoded key retrieved earlier 

```
python agent.py --key AAAA....
```

Export the environment variables in your shell to use it

You can also set the derivation path from the master seed by providing it with the --path parameter.

