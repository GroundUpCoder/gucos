# One-off native eventfd write-length check

Executed on agentone's native Linux with Python, under machine-job build/general
reservation fd509775-c760-49a7-b3fa-712a131390e9 on 2026-10-11. Raw scheduler and
process output is preserved in eventfd-write16-native.log.

The probe created a nonblocking eventfd and attempted a sixteen-byte write: an
eight-byte little-endian integer one followed by eight extra bytes. os.write
raised EINVAL. The subsequent read was not reached. This resolves the review
question about whether this host accepts oversized writes; it agrees with the
candidate service's exactly-eight-byte write requirement.

This is a small original reference probe, not an upstream test, a guest test,
or evidence of eventfd acceptance on gucOS. No recurring regression was added.
