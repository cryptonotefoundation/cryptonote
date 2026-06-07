# walletd HD Addresses

`walletd` creates HD address containers by default when `--generate-container` is used.
An HD container keeps one mnemonic seed and derives new walletd addresses from it.
This makes exchange backups simpler because the mnemonic seed, wallet password, and restore address count are enough to recreate the generated addresses.

## Creating a New HD Container

```bash
walletd --generate-container --container-file exchange.wallet --container-password "password"
```

The first generated address uses HD index `0`. Every later address generated through walletd uses the next HD index.

To pre-create more addresses during generation, use:

```bash
walletd --generate-container --container-file exchange.wallet --container-password "password" --restore-address-count 100
```

`--restore-address-count` is also used when restoring an HD container from a mnemonic seed.

## Restoring an HD Container

```bash
walletd --generate-container \
  --container-file restored.wallet \
  --container-password "password" \
  --mnemonic-seed "25 word seed ..." \
  --restore-address-count 100
```

The wallet cannot know how many addresses an exchange created after the seed was backed up.
Choose a restore count at least as high as the largest address index that may have received funds.
If you are unsure, restore with a higher count.

`--scan-height` can be combined with restore to avoid scanning the whole chain:

```bash
walletd --generate-container \
  --container-file restored.wallet \
  --container-password "password" \
  --mnemonic-seed "25 word seed ..." \
  --restore-address-count 100 \
  --scan-height 123456
```

## Independent Address Containers

Older walletd containers created every address with an independent random spend key.
That behavior is still available for new containers with:

```bash
walletd --generate-container \
  --container-file legacy-style.wallet \
  --container-password "password" \
  --independent-addresses
```

Use this mode only when you intentionally need the old backup model.
Each address spend key must be backed up independently.

## How Derivation Works

The mnemonic encodes the HD master spend secret key using the existing 25-word Electrum-style Karbo/Monero mnemonic format.
The wallet derives the container view key from that master spend key.

Address derivation is deterministic:

- index `0` uses the master spend secret key directly, preserving compatibility with the existing mnemonic model
- index `1` and later use a domain-separated scalar hash of the master spend key, view secret key, and little-endian address index
- each derived spend public key is paired with the same container view public key, so the resulting addresses are normal Karbo addresses

The HD mode is stored inside the wallet container.
Existing containers keep their original address mode when opened by newer walletd builds.

## Notes for Exchanges

Back up the container file, container password, mnemonic seed, and the highest address count you have generated.
The mnemonic alone cannot reveal how many addresses were created.

Addresses imported from raw spend keys are not recoverable from the HD mnemonic.
If you use raw key imports, keep those key backups separately.

HD walletd addresses do not change consensus rules or transaction format.
They remain compatible with normal Karbo transfers and future transaction privacy changes because only local spend-key generation changes.
