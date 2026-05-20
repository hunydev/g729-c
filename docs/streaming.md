# Streaming And Frame Boundaries

g729-c exposes a fixed-frame API. The codec does not accept or produce partial
frames:

- Input PCM is 8 kHz mono signed 16-bit samples.
- One encoder frame is 80 PCM samples, or 10 ms.
- One packed G.729 speech frame is 10 bytes.
- A decoder input stream must be an exact multiple of 10 bytes.

The public constants are available from `g729.h`:

```c
G729_SAMPLE_RATE     /* 8000 */
G729_FRAME_SAMPLES   /* 80 */
G729_FRAME_BYTES     /* 10 */
```

## Encoder Input

Callers should buffer incoming PCM until at least 80 samples are available,
then pass exactly one frame to `g729_encode_frame`.

```c
int16_t frame[G729_FRAME_SAMPLES];
uint8_t bits[G729_FRAME_BYTES];

/* Fill frame[0..79] from the caller's stream buffer. */
rc = g729_encode_frame(&enc, frame, bits);
```

If a live stream receives fewer than 80 samples, keep those samples in the
caller-side buffer and wait for more input. Do not emit a partial G.729 frame.

If a finite input, such as a file recording, ends with fewer than 80 samples,
choose one of these policies:

- Reject the input as not frame-aligned. This is what `g729enc` currently does.
- Zero-pad the final PCM frame to 80 samples, encode it, and store the original
  sample count or duration in an outer container so playback can trim the
  padded samples after decoding.

Raw `.g729` data has no built-in length metadata, so zero-padding cannot be
recovered automatically from the bitstream alone.

## Decoder Input

Callers should read exactly 10 bytes for each `g729_decode_frame` call. A
trailing input shorter than 10 bytes is invalid for this raw frame format.

```c
uint8_t bits[G729_FRAME_BYTES];
int16_t frame[G729_FRAME_SAMPLES];

/* Fill bits[0..9] from the caller's packet or file buffer. */
rc = g729_decode_frame(&dec, bits, frame);
```

Each successful decode produces exactly 80 PCM samples. If the original encoder
side zero-padded a final short frame, the application or container metadata must
trim those padded output samples.

## Packet Streams

For RTP or similar packet transports, packet payload length should be a multiple
of 10 bytes. A packet can contain one or more consecutive G.729 frames:

| Frames | Payload Bytes | Audio Duration |
| --- | ---: | ---: |
| 1 | 10 | 10 ms |
| 2 | 20 | 20 ms |
| 3 | 30 | 30 ms |
| 4 | 40 | 40 ms |

RTP timestamps for 8 kHz audio advance by 80 samples per G.729 frame. For
example, a 20 ms packet carrying two frames advances by 160 timestamp units.

This library does not currently provide packet-loss concealment, Annex B
SID/CNG/DTX, or a jitter buffer. Those policies belong to the application or
media stack above the codec.

## Stream State

Use one `g729_encoder` or `g729_decoder` instance per media stream. The state is
history-dependent across frames, so resetting or sharing an instance changes
subsequent encoded or decoded audio.

Concurrent calls on the same instance are not safe unless the caller serializes
access.
