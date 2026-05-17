# Release Checklist

- Fresh clone build passes on Linux.
- Full C test suite passes.
- CLI smoke tests pass for `g729enc` and `g729dec`, including stdin/stdout and
  trailing partial-frame rejection.
- AddressSanitizer and UndefinedBehaviorSanitizer pass when available.
- Decoder output matches Go decoder output sample-for-sample on committed
  fixtures.
- Encoder output matches Go `EncoderProfileCore` output byte-for-byte on
  committed fixtures.
- C encode followed by C decode matches Go encode followed by Go decode on
  committed fixtures.
- No forbidden G.729 implementation source or binary is tracked.
- README and clean-room documentation make only conservative claims.
- Release notes state unsupported Annex B, G.729.1, G.729D, G.729E, ITU
  certification, and ITU endorsement.
