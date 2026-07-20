# Shared SxPB corpus

The corpus separates semantic roundtrip coverage from canonical printer coverage.

- `golden/*.sxpb` must parse, print, and reparse without changing the precise
  SxPB value. The source itself does not need to be canonical.
- `idempotent/*.sxpb` must additionally equal its first printed form. Tests
  ignore one terminal LF so implementations may follow either library-string
  or command-line text framing conventions.

Cases belong here only after their behavior is implementation-neutral. Keep
implementation-defined or deliberately unsupported syntax in the individual
implementation repositories.
