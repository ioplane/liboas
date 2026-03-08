#!/usr/bin/env python3
"""RFC scraper for the liboas OpenAPI library project.

Searches datatracker.ietf.org API for RFCs and active Internet-Drafts
relevant to OpenAPI 3.2, JSON Schema, JSON Pointer, URI Templates,
HTTP semantics, and web security (OAuth/JWT).
Outputs a structured Markdown registry.

Usage:
    python3 rfc-scraper.py                          # stdout
    python3 rfc-scraper.py -o liboas-rfcs.md        # file
    python3 rfc-scraper.py --download docs/rfc/     # download .txt
    python3 rfc-scraper.py --json                   # JSON output
"""

from __future__ import annotations

import argparse
import json
import logging
import sys
import time
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import requests
from requests.adapters import HTTPAdapter, Retry

log = logging.getLogger("rfc-scraper")

# -- Datatracker API ----------------------------------------------------------

DATATRACKER_BASE = "https://datatracker.ietf.org"
DATATRACKER_API = f"{DATATRACKER_BASE}/api/v1"
RFC_EDITOR_BASE = "https://www.rfc-editor.org"
USER_AGENT = "liboas-rfc-scraper/1.0"
REQUEST_TIMEOUT = 15
RATE_LIMIT_DELAY = 0.3

# Full URI references for Tastypie filtered fields
DOCTYPE_RFC = f"{DATATRACKER_API}/name/doctypename/rfc/"
DOCTYPE_DRAFT = f"{DATATRACKER_API}/name/doctypename/draft/"


# -- Data model ---------------------------------------------------------------


@dataclass
class RfcEntry:
    """Metadata for a single RFC."""

    number: int
    title: str
    status: str = ""
    pages: int | None = None
    categories: set[str] = field(default_factory=set)

    @property
    def url(self) -> str:
        return f"{RFC_EDITOR_BASE}/rfc/rfc{self.number}"

    @property
    def txt_url(self) -> str:
        return f"{RFC_EDITOR_BASE}/rfc/rfc{self.number}.txt"


@dataclass
class DraftEntry:
    """Metadata for an active Internet-Draft."""

    name: str
    title: str
    status: str = "ACTIVE DRAFT"
    rev: str = ""
    categories: set[str] = field(default_factory=set)

    @property
    def url(self) -> str:
        return f"{DATATRACKER_BASE}/doc/{self.name}/"


# -- Search categories --------------------------------------------------------

CATEGORIES: dict[str, list[str]] = {
    "JSON & Data Formats": [
        "JSON",
        "JSON Pointer",
        "JSON Patch",
        "JSON Merge Patch",
        "JSON Path",
    ],
    "URI & Templates": [
        "URI",
        "URI Template",
        "URI Reference",
        "IRI",
    ],
    "HTTP Semantics": [
        "HTTP semantics",
        "HTTP methods",
        "HTTP status codes",
        "content negotiation",
        "HTTP authentication",
    ],
    "Media Types & Encoding": [
        "MIME",
        "multipart form-data",
        "Base64",
        "Base16",
        "content encoding",
    ],
    "Security & Auth": [
        "OAuth 2.0",
        "Bearer token",
        "JSON Web Token",
        "JSON Web Signature",
        "JSON Web Key",
        "JSON Web Encryption",
        "HTTP authentication",
        "API key",
        "OpenID Connect",
    ],
    "Web APIs": [
        "Problem Details HTTP",
        "Web Linking",
        "Structured Fields",
        "HTTP CORS",
    ],
    "Data Validation": [
        "JSON Schema",
        "JSON Hyper-Schema",
        "JSON Schema Validation",
    ],
}

# Known critical RFCs -- manually curated
KNOWN_CRITICAL: dict[int, tuple[str, str]] = {
    # JSON Core
    8259: ("The JavaScript Object Notation (JSON) Data Interchange Format", "INTERNET STANDARD"),
    7493: ("The I-JSON Message Format", "PROPOSED STANDARD"),
    # JSON Pointer & Patch
    6901: ("JavaScript Object Notation (JSON) Pointer", "PROPOSED STANDARD"),
    6902: ("JavaScript Object Notation (JSON) Patch", "PROPOSED STANDARD"),
    7396: ("JSON Merge Patch", "PROPOSED STANDARD"),
    9535: ("JSONPath: Query Expressions for JSON", "PROPOSED STANDARD"),
    # URI
    3986: ("Uniform Resource Identifier (URI): Generic Syntax", "INTERNET STANDARD"),
    6570: ("URI Template", "PROPOSED STANDARD"),
    3987: ("Internationalized Resource Identifiers (IRIs)", "PROPOSED STANDARD"),
    # HTTP Semantics (needed for OpenAPI HTTP binding)
    9110: ("HTTP Semantics", "INTERNET STANDARD"),
    9111: ("HTTP Caching", "INTERNET STANDARD"),
    9457: ("Problem Details for HTTP APIs", "INTERNET STANDARD"),
    7235: ("HTTP/1.1: Authentication", "PROPOSED STANDARD"),
    7617: ("The 'Basic' HTTP Authentication Scheme", "PROPOSED STANDARD"),
    6585: ("Additional HTTP Status Codes", "PROPOSED STANDARD"),
    # Media Types & Encoding
    2045: ("MIME Part One: Format of Internet Message Bodies", "DRAFT STANDARD"),
    2046: ("MIME Part Two: Media Types", "DRAFT STANDARD"),
    7578: ("Returning Values from Forms: multipart/form-data", "PROPOSED STANDARD"),
    4648: ("The Base16, Base32, and Base64 Data Encodings", "PROPOSED STANDARD"),
    6838: ("Media Type Specifications and Registration Procedures", "BCP"),
    # Authentication & Security
    6749: ("The OAuth 2.0 Authorization Framework", "PROPOSED STANDARD"),
    6750: ("The OAuth 2.0 Authorization Framework: Bearer Token Usage", "PROPOSED STANDARD"),
    7519: ("JSON Web Token (JWT)", "PROPOSED STANDARD"),
    7515: ("JSON Web Signature (JWS)", "PROPOSED STANDARD"),
    7516: ("JSON Web Encryption (JWE)", "PROPOSED STANDARD"),
    7517: ("JSON Web Key (JWK)", "PROPOSED STANDARD"),
    7636: ("Proof Key for Code Exchange by OAuth Public Clients", "PROPOSED STANDARD"),
    8414: ("OAuth 2.0 Authorization Server Metadata", "PROPOSED STANDARD"),
    # Web APIs
    8288: ("Web Linking", "PROPOSED STANDARD"),
    8941: ("Structured Field Values for HTTP", "PROPOSED STANDARD"),
    6265: ("HTTP State Management Mechanism", "PROPOSED STANDARD"),
    6797: ("HTTP Strict Transport Security (HSTS)", "PROPOSED STANDARD"),
    # YAML (informational)
    # (YAML is not an RFC, it's a spec from yaml.org)
}

# Manually curated important drafts
IMPORTANT_DRAFTS: dict[str, str] = {
    "draft-bhutton-json-schema": "JSON Schema (2020-12)",
    "draft-bhutton-json-schema-validation": "JSON Schema Validation (2020-12)",
    "draft-bhutton-json-schema-00": "JSON Schema Core (2020-12)",
    "draft-bhutton-relative-json-pointer": "Relative JSON Pointer",
}

# Critical RFC groups for the summary section
CRITICAL_GROUPS: dict[str, list[int]] = {
    "JSON Core": [8259, 7493],
    "JSON Pointer & Patch": [6901, 6902, 7396, 9535],
    "URI & Templates": [3986, 6570, 3987],
    "HTTP Semantics": [9110, 9111, 9457, 7235, 7617, 6585],
    "Media Types & Encoding": [2045, 2046, 7578, 4648, 6838],
    "Authentication (JWT/OAuth)": [6749, 6750, 7519, 7515, 7516, 7517, 7636, 8414],
    "Web APIs": [8288, 8941, 6265, 6797],
}

# Protocol-to-RFC mapping for the matrix section
PROTOCOL_MATRIX: dict[str, list[str]] = {
    "OpenAPI Document Parsing": [
        "RFC 8259 (JSON format)",
        "RFC 6901 (JSON Pointer — $ref resolution)",
        "RFC 3986 (URI — $ref URLs)",
        "RFC 6570 (URI Template — server URLs)",
    ],
    "Schema Validation": [
        "draft-bhutton-json-schema (JSON Schema 2020-12)",
        "RFC 8259 (JSON data types)",
        "RFC 3986 (URI format validation)",
        "RFC 4648 (Base64 format validation)",
    ],
    "HTTP Request/Response Validation": [
        "RFC 9110 (HTTP Semantics — methods, status codes, headers)",
        "RFC 7578 (multipart/form-data)",
        "RFC 2045/2046 (MIME types)",
        "RFC 9457 (Problem Details — error responses)",
    ],
    "Security Schemes": [
        "RFC 7235 (HTTP Authentication)",
        "RFC 7617 (Basic Auth)",
        "RFC 6750 (Bearer Token / OAuth)",
        "RFC 7519 (JWT)",
        "RFC 7515 (JWS)",
        "RFC 7517 (JWK)",
        "RFC 8414 (OAuth Server Metadata / OpenID Discovery)",
    ],
}

# Relevance scoring keywords
HIGH_RELEVANCE_KEYWORDS: list[str] = [
    "json",
    "json schema",
    "json pointer",
    "json patch",
    "openapi",
    "uri",
    "uri template",
    "oauth",
    "jwt",
    "bearer",
    "http semantics",
    "http authentication",
    "problem details",
    "multipart",
    "media type",
    "structured field",
    "web linking",
]

MEDIUM_RELEVANCE_KEYWORDS: list[str] = [
    "base64",
    "mime",
    "cookie",
    "hsts",
    "cors",
    "content negotiation",
    "status code",
    "form-data",
    "api key",
]

DRAFT_RELEVANCE_KEYWORDS: list[str] = [
    "json schema",
    "openapi",
    "json pointer",
    "json path",
    "http api",
]


# -- API client ---------------------------------------------------------------


def _create_session() -> requests.Session:
    """Create a reusable HTTP session with connection pooling."""
    session = requests.Session()
    session.headers["User-Agent"] = USER_AGENT
    adapter = HTTPAdapter(
        max_retries=Retry(
            total=3,
            backoff_factor=1,
            status_forcelist=[429, 500, 502, 503, 504],
        ),
    )
    session.mount("https://", adapter)
    return session


def _parse_rfc_number(name: str) -> int | None:
    """Extract RFC number from a document name like 'rfc8446'."""
    if name.startswith("rfc"):
        try:
            return int(name[3:])
        except ValueError:
            pass
    return None


def search_datatracker_rfcs(
    session: requests.Session,
    query: str,
    *,
    max_results: int = 10,
) -> list[dict[str, Any]]:
    """Search datatracker for RFCs matching a title query."""
    params: dict[str, str | int] = {
        "title__icontains": query,
        "type": DOCTYPE_RFC,
        "limit": max_results,
        "format": "json",
        "order_by": "-rfc_number",
    }
    try:
        resp = session.get(
            f"{DATATRACKER_API}/doc/document/",
            params=params,
            timeout=REQUEST_TIMEOUT,
        )
        resp.raise_for_status()
        data = resp.json()
        results = []
        for obj in data.get("objects", []):
            rfc_num = _parse_rfc_number(obj.get("name", ""))
            if rfc_num is not None:
                results.append(
                    {
                        "rfc": rfc_num,
                        "title": obj.get("title", ""),
                        "status": obj.get("std_level", ""),
                        "pages": obj.get("pages"),
                    }
                )
        return results
    except requests.RequestException as exc:
        log.warning("Datatracker RFC search failed for %r: %s", query, exc)
        return []


def search_datatracker_drafts(
    session: requests.Session,
    query: str,
    *,
    max_results: int = 5,
) -> list[dict[str, Any]]:
    """Search datatracker for active Internet-Drafts."""
    params: dict[str, str | int] = {
        "title__icontains": query,
        "type": DOCTYPE_DRAFT,
        "states__slug__in": "active",
        "limit": max_results,
        "format": "json",
        "order_by": "-time",
    }
    try:
        resp = session.get(
            f"{DATATRACKER_API}/doc/document/",
            params=params,
            timeout=REQUEST_TIMEOUT,
        )
        resp.raise_for_status()
        data = resp.json()
        return [
            {
                "name": obj.get("name", ""),
                "title": obj.get("title", ""),
                "rev": obj.get("rev", ""),
            }
            for obj in data.get("objects", [])
        ]
    except requests.RequestException as exc:
        log.warning("Datatracker draft search failed for %r: %s", query, exc)
        return []


def fetch_rfc_metadata(
    session: requests.Session,
    rfc_numbers: list[int],
    *,
    batch_size: int = 20,
) -> dict[int, dict[str, Any]]:
    """Fetch metadata for multiple RFCs using name__in batches."""
    result: dict[int, dict[str, Any]] = {}
    for i in range(0, len(rfc_numbers), batch_size):
        batch = rfc_numbers[i : i + batch_size]
        names = ",".join(f"rfc{n}" for n in batch)
        try:
            resp = session.get(
                f"{DATATRACKER_API}/doc/document/",
                params={"name__in": names, "format": "json", "limit": batch_size},
                timeout=REQUEST_TIMEOUT,
            )
            resp.raise_for_status()
            for obj in resp.json().get("objects", []):
                rfc_num = _parse_rfc_number(obj.get("name", ""))
                if rfc_num is not None:
                    result[rfc_num] = {
                        "title": obj.get("title", ""),
                        "status": obj.get("std_level", ""),
                        "pages": obj.get("pages"),
                        "abstract": obj.get("abstract", ""),
                    }
        except requests.RequestException as exc:
            log.warning("Batch metadata fetch failed: %s", exc)
        time.sleep(RATE_LIMIT_DELAY)
    return result


def download_rfc_txt(
    session: requests.Session,
    rfc_num: int,
    dest_dir: Path,
) -> Path | None:
    """Download RFC plain-text to dest_dir/rfcNNNN.txt."""
    dest = dest_dir / f"rfc{rfc_num}.txt"
    if dest.exists():
        log.debug("Already exists: %s", dest)
        return dest
    url = f"{RFC_EDITOR_BASE}/rfc/rfc{rfc_num}.txt"
    try:
        resp = session.get(url, timeout=30)
        resp.raise_for_status()
        dest.write_bytes(resp.content)
        log.info("Downloaded rfc%d (%d bytes)", rfc_num, len(resp.content))
        return dest
    except requests.RequestException as exc:
        log.warning("Failed to download rfc%d: %s", rfc_num, exc)
        return None


# -- Collection ----------------------------------------------------------------


def collect_rfcs(
    session: requests.Session,
) -> tuple[dict[int, RfcEntry], dict[str, DraftEntry]]:
    """Search all categories and collect RFC + draft entries."""
    rfcs: dict[int, RfcEntry] = {}
    drafts: dict[str, DraftEntry] = {}

    for cat_name, queries in CATEGORIES.items():
        log.info("Category: %s", cat_name)

        for query in queries:
            log.debug("  Search: %s", query)

            for r in search_datatracker_rfcs(session, query):
                num = r["rfc"]
                if num not in rfcs:
                    rfcs[num] = RfcEntry(
                        number=num,
                        title=r["title"],
                        status=r.get("status", ""),
                        pages=r.get("pages"),
                    )
                rfcs[num].categories.add(cat_name)

            for d in search_datatracker_drafts(session, query):
                name = d["name"]
                if name not in drafts:
                    drafts[name] = DraftEntry(
                        name=name,
                        title=d["title"],
                        rev=d.get("rev", ""),
                    )
                drafts[name].categories.add(cat_name)

            time.sleep(RATE_LIMIT_DELAY)

    # Merge known critical RFCs
    for rfc_num, (title, status) in KNOWN_CRITICAL.items():
        if rfc_num not in rfcs:
            rfcs[rfc_num] = RfcEntry(number=rfc_num, title=title, status=status)
        rfcs[rfc_num].categories.add("Curated critical")

    return rfcs, drafts


# -- Scoring -------------------------------------------------------------------


def relevance_score(entry: RfcEntry) -> int:
    """Score an RFC's relevance to the liboas project."""
    score = 0
    title_lower = entry.title.lower()

    for kw in HIGH_RELEVANCE_KEYWORDS:
        if kw in title_lower:
            score += 10
    for kw in MEDIUM_RELEVANCE_KEYWORDS:
        if kw in title_lower:
            score += 5

    if entry.number in KNOWN_CRITICAL:
        score += 20

    status_lower = entry.status.lower() if entry.status else ""
    if "standard" in status_lower:
        score += 3
    elif "proposed" in status_lower:
        score += 2

    return score


# -- Markdown generation -------------------------------------------------------


def _status_label(rfc_num: int, entry: RfcEntry) -> str:
    """Resolve display status: prefer curated, fall back to API."""
    if rfc_num in KNOWN_CRITICAL:
        return KNOWN_CRITICAL[rfc_num][1]
    status = entry.status
    if isinstance(status, str) and "/" in status:
        return status.rsplit("/", maxsplit=1)[-1].strip()
    return status or ""


def generate_markdown(rfcs: dict[int, RfcEntry], drafts: dict[str, DraftEntry]) -> str:
    """Generate the full Markdown registry."""
    now = datetime.now(tz=timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    lines: list[str] = []
    w = lines.append

    w("# liboas OpenAPI Library — RFC Registry\n")
    w(f"**Generated**: {now}")
    w(f"**RFCs found**: {len(rfcs)}")
    w(f"**Active Internet-Drafts found**: {len(drafts)}")
    w("")

    # Statistics
    status_counts: dict[str, int] = defaultdict(int)
    for entry in rfcs.values():
        label = (entry.status or "UNKNOWN").upper()
        if "/" in label:
            label = label.rsplit("/", maxsplit=1)[-1].strip()
        status_counts[label] += 1

    w("---\n")
    w("## Statistics\n")
    for status, count in sorted(status_counts.items(), key=lambda x: -x[1]):
        w(f"- **{status}**: {count}")
    w("")

    # Critical RFCs
    w("---\n")
    w("## Critical RFCs (must-implement)\n")

    for group_name, rfc_nums in CRITICAL_GROUPS.items():
        w(f"\n### {group_name}\n")
        w("| RFC | Title | Status | Relevance |")
        w("|-----|-------|--------|-----------|")
        for num in rfc_nums:
            entry = rfcs.get(num)
            title = entry.title if entry else KNOWN_CRITICAL.get(num, ("?",))[0]
            status = (
                _status_label(num, entry)
                if entry
                else KNOWN_CRITICAL.get(num, ("", ""))[1]
            )
            score = relevance_score(entry) if entry else 0
            w(
                f"| [{num}]({RFC_EDITOR_BASE}/rfc/rfc{num}) | {title} | {status} | {score} |"
            )

    # High-relevance non-critical RFCs
    critical_set: set[int] = set()
    for nums in CRITICAL_GROUPS.values():
        critical_set.update(nums)

    scored = sorted(
        ((relevance_score(e), e) for e in rfcs.values()),
        key=lambda x: (-x[0], x[1].number),
    )

    w("\n---\n")
    w("## High-relevance RFCs (not in critical list)\n")
    w("| RFC | Title | Status | Score |")
    w("|-----|-------|--------|-------|")
    count = 0
    for score, entry in scored:
        if entry.number in critical_set or score < 5:
            continue
        status = _status_label(entry.number, entry)
        w(
            f"| [{entry.number}]({entry.url}) | {entry.title[:80]} | {status} | {score} |"
        )
        count += 1
        if count >= 60:
            break

    # Important drafts
    w("\n---\n")
    w("## Active Internet-Drafts\n")
    w("| Draft | Description | Status |")
    w("|-------|-------------|--------|")
    for name, desc in IMPORTANT_DRAFTS.items():
        w(f"| [{name}]({DATATRACKER_BASE}/doc/{name}/) | {desc} | Curated |")

    # Discovered drafts
    seen = set(IMPORTANT_DRAFTS.keys())
    draft_scored: list[tuple[int, DraftEntry]] = []
    for entry in drafts.values():
        base = (
            entry.name.rsplit("-", maxsplit=1)[0]
            if entry.name[-1:].isdigit()
            else entry.name
        )
        if base in seen or entry.name in seen:
            continue
        seen.add(base)
        t_lower = entry.title.lower()
        dscore = sum(10 for kw in DRAFT_RELEVANCE_KEYWORDS if kw in t_lower)
        if dscore >= 10:
            draft_scored.append((dscore, entry))

    if draft_scored:
        draft_scored.sort(key=lambda x: -x[0])
        w("\n### Additional discovered drafts\n")
        w("| Draft | Title | Categories |")
        w("|-------|-------|------------|")
        for _, entry in draft_scored[:30]:
            cats = ", ".join(sorted(entry.categories)) if entry.categories else "-"
            w(f"| [{entry.name}]({entry.url}) | {entry.title[:80]} | {cats} |")

    # Protocol matrix
    w("\n---\n")
    w("## Protocol-to-RFC matrix for liboas\n")
    for component, rfc_refs in PROTOCOL_MATRIX.items():
        w(f"\n### {component}\n")
        for ref in rfc_refs:
            w(f"- {ref}")

    return "\n".join(lines)


def generate_json(rfcs: dict[int, RfcEntry], drafts: dict[str, DraftEntry]) -> str:
    """Generate JSON output."""
    data = {
        "generated": datetime.now(tz=timezone.utc).isoformat(),
        "rfcs": {
            num: {
                "title": e.title,
                "status": e.status,
                "pages": e.pages,
                "categories": sorted(e.categories),
                "relevance": relevance_score(e),
                "url": e.url,
            }
            for num, e in sorted(rfcs.items())
        },
        "drafts": {
            e.name: {
                "title": e.title,
                "rev": e.rev,
                "categories": sorted(e.categories),
                "url": e.url,
            }
            for e in sorted(drafts.values(), key=lambda x: x.name)
        },
    }
    return json.dumps(data, indent=2, ensure_ascii=False)


# -- CLI -----------------------------------------------------------------------


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="RFC scraper for the liboas OpenAPI library project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="Output file path (default: stdout)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        dest="json_output",
        help="Output as JSON instead of Markdown",
    )
    parser.add_argument(
        "--download",
        type=Path,
        default=None,
        metavar="DIR",
        help="Download RFC .txt files to DIR (only critical RFCs)",
    )
    parser.add_argument(
        "--download-all",
        type=Path,
        default=None,
        metavar="DIR",
        help="Download ALL discovered RFC .txt files to DIR",
    )
    parser.add_argument(
        "--skip-search",
        action="store_true",
        help="Skip API search, only use curated KNOWN_CRITICAL list",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="count",
        default=0,
        help="Increase verbosity (-v info, -vv debug)",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    # Configure logging
    level = logging.WARNING
    if args.verbose >= 2:
        level = logging.DEBUG
    elif args.verbose >= 1:
        level = logging.INFO
    logging.basicConfig(
        level=level,
        format="%(levelname)-5s %(message)s",
        stream=sys.stderr,
    )

    session = _create_session()

    # Collect RFCs
    if args.skip_search:
        log.info("Skipping API search, using curated list only")
        rfcs: dict[int, RfcEntry] = {}
        for rfc_num, (title, status) in KNOWN_CRITICAL.items():
            rfcs[rfc_num] = RfcEntry(
                number=rfc_num,
                title=title,
                status=status,
                categories={"Curated critical"},
            )
        drafts: dict[str, DraftEntry] = {}
    else:
        log.info("Scanning datatracker.ietf.org...")
        rfcs, drafts = collect_rfcs(session)

    log.info("Found %d RFCs and %d drafts", len(rfcs), len(drafts))

    # Download RFC text files
    download_dir = args.download_all or args.download
    if download_dir is not None:
        download_dir.mkdir(parents=True, exist_ok=True)
        if args.download_all:
            nums_to_download = sorted(rfcs.keys())
        else:
            nums_to_download = sorted(KNOWN_CRITICAL.keys())
        log.info("Downloading %d RFCs to %s", len(nums_to_download), download_dir)
        ok = 0
        for rfc_num in nums_to_download:
            if download_rfc_txt(session, rfc_num, download_dir):
                ok += 1
            time.sleep(RATE_LIMIT_DELAY)
        log.info("Downloaded %d/%d RFC files", ok, len(nums_to_download))

    # Generate output
    if args.json_output:
        output = generate_json(rfcs, drafts)
    else:
        output = generate_markdown(rfcs, drafts)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
        log.info("Written to %s (%d chars)", args.output, len(output))
    else:
        sys.stdout.write(output)
        sys.stdout.write("\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
