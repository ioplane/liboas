[![GitHub](https://img.shields.io/badge/GitHub-liboas-181717)](https://github.com/ioplane/liboas)

# liboas Agent Development Plan

Date: 2026-10-26. Audited default branch: `master` at
`a2f7f9ea34fcd190132471c65f6151d29a85bbe3`. Planning audit: `ngtun-i8s.42.2`.
Durable execution work order: `ngtun-4zy.4` under independent portfolio `ngtun-4zy`.

Historical plans and local uncommitted August plans remain preserved. This dated
plan supplies the next validation/implementation contracts; it does not rewrite past
measurements, sprint completion records or current publish decisions.

## Verified baseline

The default tree contains parser, OAS model, compiler, validator, emitter and adapter
layers, plus external-reference helpers. `include/liboas/oas_parser.h:28-38` declares
remote/file opt-in, document/size/timeout/redirect limits and a caller fetch callback.
`src/parser/oas_ref_fetch.c:375-451` supplies a built-in HTTP redirect loop. Source/API
presence does not establish the complete network/file trust policy. The old roadmaps
still propose features whose source is already present; validate actual behavior before
creating another implementation.

## W0: image and external-validator entry (2026-10-26)

Source `383ca8a8f4548dd817ea28728c13bf510afdd3f3`: job `121827118911`
/ run `40395441473` cannot pull `ghcr.io/ioplane/liboas-dev:latest`
(`manifest unknown`) and stops before compilation. Qualify the exact image
identity and authorized CI readback before build/product claims; tag visibility
and registry policy need verification. Separately, validator job `121827117924`
/ run `40395441500` reports `attribute openapi is unexpected` and
`attribute swagger is missing` for `tests/fixtures/petstore.json`. Verify the
selected external validator/input dialect before attributing those symptoms to
application semantics. Workflow, recipe and fixture bytes are unchanged;
old pull-failure annotations exist, while full historical logs return HTTP 410.

## Next functional slice

Qualify bounded external-reference resolution from parse options through cache and model
compilation. Reproduce malformed URL, redirect/cycle/depth/size exhaustion, partial read,
timeout and denied file-path/content cases with deterministic fixtures. Preserve local-only
defaults and the existing caller fetch seam. For enabled built-in fetching, freeze allowed
schemes/destinations, redirect revalidation, total deadlines and certificate/hostname
verification policy; reproduce a violation before implementing a scoped correction.

| Order | Owned paths | Exit evidence |
| --- | --- | --- |
| 1 | `src/parser/oas_ref*`, `oas_uri.*`, parser options, fixture tests | Explicit trust/ownership/error contract with failing boundary controls |
| 2 | The shared resolver/fetch code implicated by a reproduction | Bounded documents/bytes/time, no unauthorized read/fetch, consistent cache identity and cleanup |
| 3 | Existing model/compiler/validator and adapter integration tests | Model outlives compiled views; originating destructors/cleanup pass; resolved document validates and rejected input cannot yield partial valid state |

## Dependencies and overlap

PR #5 owns the shared image and Python quality/compatibility/release controllers. Review
that work in place before consuming it; do not rewrite its scripts. Keep transport,
routing and application authorization outside the OAS core. iohttp integration consumes
the adapter only after the iohttp contract is qualified; keep the existing regex/YAML
choices. ngtun's crypto/profile/session policy is not a dependency or replacement here.

## Planned GitLab handoff

The owner plans import to `https://gitlab.infra4.dev/ioplane/network/liboas`;
import completion is unverified. This GitHub PR is a pre-import transferable
patchset: retain its exact base/head and parent history. After the owner import,
bind actual GitLab project ID, default SHA/refs and reviews, compare history,
and read the post-import contribution contract. Future implementation uses the
accepted GitLab destination after CI, registry, ACL and host-identity receipts;
the GitHub publication rules below describe only pre-import delivery. This plan
performs no import, group transfer, remote/config change or PR closure.

## Publication contract

Follow the existing `master` PR and full container quality policy, with exact compiler,
source and analyzer inputs. Keep English contract docs, agreed translations and CHANGELOG
with behavior/evidence changes. Correct broad README support claims only against tested
feature coverage; do not infer complete OAS support from a source list. Preserve dated
Sprint 17-18 recipes as historical context, including their unqualified TLS-test omission;
new TLS-capable paths need actual positive/negative peer verification before admission.

## Execution and evidence rules

Read the target repository contract and applicable local skills before each leaf.
This independent portfolio does not gate ngtun release or require ngtun adoption.
Use an actually qualified project OCI; shared-base publication gates only its adoption.
Preserve dirty worktrees, unpublished plans, existing PRs and their path ownership.
Refresh exact default/PR identities before execution; keep compatibility explicit.
Record actual nonempty test/analyzer coverage and exact source/tool/image evidence.
Follow the repository task, changelog, review and delivery contract for each leaf.
This planning update claims no runtime success and authorizes no live mutation.
Repository quality gates remain pending until run; no host build or engine was used.

## Source baseline

- [Default README](https://github.com/ioplane/liboas/blob/a2f7f9ea34fcd190132471c65f6151d29a85bbe3/README.md).
- [Default repository contract](https://github.com/ioplane/liboas/blob/a2f7f9ea34fcd190132471c65f6151d29a85bbe3/CLAUDE.md); applicable dirty local contracts were read without importing their changes.
- [Existing PR #5](https://github.com/ioplane/liboas/pull/5), audited head `3a182bf6a9f28a56d49784d46260593b37d05db7`; recheck the patchset before execution.
- [include/liboas/oas_parser.h:28](https://github.com/ioplane/liboas/blob/a2f7f9ea34fcd190132471c65f6151d29a85bbe3/include/liboas/oas_parser.h#L28).
- [src/parser/oas_ref_fetch.c:375](https://github.com/ioplane/liboas/blob/a2f7f9ea34fcd190132471c65f6151d29a85bbe3/src/parser/oas_ref_fetch.c#L375).
- [docs/plans/2026-03-10-sprint17-18-plan.md:801](https://github.com/ioplane/liboas/blob/a2f7f9ea34fcd190132471c65f6151d29a85bbe3/docs/plans/2026-03-10-sprint17-18-plan.md#L801).
