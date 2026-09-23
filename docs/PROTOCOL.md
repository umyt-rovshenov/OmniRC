# Wire protocol

Version 1.

The protocol is deliberately small. It has to fit a 32 byte nRF24L01+ payload, run on an 8-bit
AVR receiver, and stay understandable to someone integrating it into their own vehicle firmware
for the first time.

## Frame layout

Both directions use the same layout, which keeps the encoder, the decoder and the receiver side
parser to one implementation each.

| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | 1 | `magic` | `0xC7` control, `0xC8` telemetry |
| 1 | 1 | `version` | Protocol version, currently `1` |
| 2 | 1 | `seq` / `ackSeq` | Rolling counter, or the last counter the receiver saw |
| 3 | 1 | `flags` / `status` | See the bit tables below |
| 4 | 1 | `count` | Number of 16-bit values that follow, 0 to 12 |
| 5 | 2 × N | `values` | Little endian `int16` |
| 5 + 2N | 2 | `crc` | CRC-16/CCITT-FALSE over bytes `[0, 5 + 2N)`, little endian |

Frames are variable length. A profile with three channels sends 13 bytes, not 31. Shorter frames
spend less time on air, which directly improves range and packet loss.

```
count   0     1     3     8    12
size    7     9    13    21    31 bytes
```

The maximum frame is 31 bytes, one byte under the nRF24L01+ payload limit, so a profile behaves
identically on every transport.

### Why explicit serialisation

Values are written byte by byte rather than by copying a packed struct. This removes struct
padding, member alignment and endianness from the list of things that can silently differ
between an ESP32 transmitter and an AVR receiver — a class of bug that only shows up in the
field, on one particular combination of hardware.

### Why a CRC when the radio already has one

The nRF24's CRC covers a packet in flight. It does not cover a payload that was truncated, a
buffer decoded with the wrong length, or a frame that arrived on a pipe it was not meant for.
The end-to-end CRC makes those cases fail loudly instead of being interpreted as control input.

## Control frame flags

| Bit | Name | Meaning |
|---|---|---|
| 0 | `Armed` | The operator has armed the vehicle |
| 1 | `FailsafeRequest` | Enter failsafe immediately, regardless of link state |

## Telemetry frame status

| Bit | Name | Meaning |
|---|---|---|
| 0 | `Armed` | Receiver considers itself armed |
| 1 | `FailsafeActive` | Receiver is running its failsafe action |
| 2 | `LowBattery` | Vehicle battery is below its warning level |
| 3 | `Error` | Receiver reports a fault |

## Telemetry transport

On nRF24, telemetry rides in the ACK payload, so it costs no extra air time: the receiver's reply
is the acknowledgement the transmitter was already waiting for. On bidirectional transports such
as ESP-NOW it is sent as a normal packet.

Telemetry values are plain `int16`. The profile on the transmitter decides what each index means,
its unit, its scale and its warning threshold. The receiver only has to agree on the order. A
battery voltage of 11.80 V is typically sent as `1180` with a scale of `0.01`.

## Sequence numbers and link quality

Every control frame carries an 8-bit rolling sequence number, and every telemetry frame echoes
the last one the receiver saw. That single byte gives both sides packet loss, and gives the
transmitter round trip latency, without any extra traffic.

`SequenceTracker` turns those numbers into a loss percentage:

- A gap of *n* means *n − 1* frames were lost.
- A repeated sequence number is a duplicate: neither progress nor loss.
- A gap larger than 32 is treated as a resync, not as loss. A receiver that was power cycled
  would otherwise report a burst of losses that never happened.
- The counting window halves every 128 frames, so the reported figure describes the link now
  rather than averaging away a problem that started ten minutes ago.

## Decoding

A decode either succeeds completely or changes nothing. The output frame is only written after
the magic byte, version, count, length and CRC have all been validated.

| Error | Cause |
|---|---|
| `TooShort` | Fewer bytes than the smallest valid frame |
| `BadMagic` | Not a frame of the requested type |
| `UnsupportedVersion` | Transmitter and receiver run different protocol versions |
| `BadCount` | Declared value count exceeds the protocol maximum |
| `LengthMismatch` | Buffer length does not match the declared count |
| `BadCrc` | Payload was corrupted in transit |

Anything other than `Ok` means drop the frame. Never act on partially decoded data: holding the
previous values is always safer than acting on invented ones.

## Failsafe

Failsafe is part of the protocol, not an application concern.

The receiver arms a deadline on every valid frame. When that deadline expires — the configured
`timeoutMs`, typically 500 ms — it applies the profile's failsafe action and sets
`FailsafeActive` in its telemetry, so the transmitter can tell the operator what happened. The
transmitter can also force this immediately with the `FailsafeRequest` flag.

## Versioning

Any change to the frame layout, to the meaning of a field, or to the flag bits increments
`kProtocolVersion`. Receivers reject frames carrying a version they do not implement, rather than
guessing. A vehicle in the field never silently misinterprets a newer transmitter.
