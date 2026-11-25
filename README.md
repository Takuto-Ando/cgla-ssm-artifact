# CGLA SSM / Mamba Artifact

Experiment artifact for the journal manuscript on **Mamba (selective
state-space model) inference on a programmable CGLA** (Mamba-130M,
128-token generation, grouped projections and request-scoped DDR weight
reuse on a CPU-Grounded Linear Array, CGLA/IMAX).

## Contents

- `src/kernel/` — The projection kernel (`projection_emax5a.c`) and the
  ordered recurrent-state kernel (`state_ordered_emax5a.c`) that preserve
  the hard32 accumulation order.
- `src/provider/` — The Native provider (`native_provider.c`) and the
  dense-baseline / configuration-reuse headers (`ssm_dense_baseline.h`,
  `ssm_config_reuse.h`).
- `src/runtime/` — The model driver (`driver.c`) that sequences Host and
  Native work across the 24 layers.
- `evidence/` — Frozen measurement records from the reported campaigns:
  the Host-guard factorial records and clock audit (`factorial_native.json`,
  `clock_audit.json`, `saved_evidence_audit.json`) and the configuration
  / native-run summaries (`config-ssm.json`, `ssm-native.json`,
  `ssm-derived-counts.json`).

## Reproduction

The measurements in the manuscript were produced on a laboratory system
built around a Xilinx VPK180 FPGA board with an ARM Cortex-A72 host and
LPDDR4 memory, using the CGLA toolchain and runtime developed in the
authors' laboratory. The FPGA device image (PDI) and the CGLA compiler
toolchain are laboratory-internal and are **not** redistributed here, so a
full hardware rerun requires that environment. The sources and records
in this repository document the grouped projection mapping, the call
accounting (15,368,544 to 1,145,520 Native calls), and the numerical
validation outcomes (bit-exact agreement with the hard32 reference) that
the manuscript reports.

## Packaging notes

This repository is a curated subset of the experiment project: production
sources named in the project's kernel-facts audit, plus the frozen
evidence records for the merged manuscript. Build products, device
images, and large intermediate files are excluded. In the evidence
records, absolute home-directory prefixes were shortened to `~/` at
packaging time; measurement values and recorded file hashes are
unchanged.

## License

MIT License. See `LICENSE`.
