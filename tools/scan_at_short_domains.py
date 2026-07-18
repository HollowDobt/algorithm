#!/usr/bin/env python3
import asyncio
import csv
import json
import random
import socket
import string
import time
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

import aiohttp
import dns.asyncquery
import dns.message
import dns.rcode
import dns.rdatatype

ALNUM = string.ascii_lowercase + string.digits
AUTHORITATIVE_DNS = [
    "81.91.161.98",      # d.ns.at
    "194.146.106.50",   # j.ns.at
    "81.91.173.130",    # n.ns.at
    "78.104.144.2",     # ns1.univie.ac.at
    "192.92.125.2",     # ns2.univie.ac.at
    "194.0.25.10",      # r.ns.at
    "185.102.12.2",     # u.ns.at
]
RDAP_BASE = "https://rdap.nic.at/domain/"
OUT_DIR = Path("scan-output")


def generate_labels() -> list[str]:
    labels: list[str] = []
    labels.extend(ALNUM)
    labels.extend(a + b for a in ALNUM for b in ALNUM)
    labels.extend(a + b + c for a in ALNUM for b in ALNUM for c in ALNUM)
    labels.extend(a + "-" + c for a in ALNUM for c in ALNUM)
    assert len(labels) == 49_284
    assert len(set(labels)) == len(labels)
    return labels


class StartRateLimiter:
    def __init__(self, rate_per_second: float):
        self.interval = 1.0 / rate_per_second
        self.lock = asyncio.Lock()
        self.next_start = 0.0

    async def wait(self) -> None:
        async with self.lock:
            now = time.monotonic()
            delay = max(0.0, self.next_start - now)
            self.next_start = max(now, self.next_start) + self.interval
        if delay:
            await asyncio.sleep(delay)


async def dns_delegated(
    domain: str,
    semaphore: asyncio.Semaphore,
    limiter: StartRateLimiter,
) -> Optional[bool]:
    query = dns.message.make_query(domain, dns.rdatatype.NS)
    start = sum(domain.encode("ascii")) % len(AUTHORITATIVE_DNS)
    async with semaphore:
        for attempt in range(3):
            await limiter.wait()
            server = AUTHORITATIVE_DNS[(start + attempt) % len(AUTHORITATIVE_DNS)]
            try:
                response = await dns.asyncquery.udp(query, server, timeout=2.5)
                code = response.rcode()
                if code == dns.rcode.NOERROR:
                    return True
                if code == dns.rcode.NXDOMAIN:
                    return False
            except Exception:
                continue
    return None


async def run_dns_prefilter(domains: list[str]) -> dict[str, Optional[bool]]:
    semaphore = asyncio.Semaphore(100)
    limiter = StartRateLimiter(250.0)
    results: dict[str, Optional[bool]] = {}
    completed = 0
    lock = asyncio.Lock()

    async def one(domain: str) -> None:
        nonlocal completed
        result = await dns_delegated(domain, semaphore, limiter)
        results[domain] = result
        async with lock:
            completed += 1
            if completed % 2_000 == 0 or completed == len(domains):
                counts = Counter(results.values())
                print(
                    f"DNS {completed}/{len(domains)} "
                    f"delegated={counts[True]} nxdomain={counts[False]} unknown={counts[None]}",
                    flush=True,
                )

    await asyncio.gather(*(one(domain) for domain in domains))
    return results


async def rdap_lookup(
    session: aiohttp.ClientSession,
    domain: str,
    semaphore: asyncio.Semaphore,
    limiter: StartRateLimiter,
) -> tuple[str, Optional[bool], int, str]:
    url = RDAP_BASE + domain
    last_status = 0
    last_error = ""
    async with semaphore:
        for attempt in range(7):
            await limiter.wait()
            try:
                async with session.head(url, allow_redirects=True) as response:
                    last_status = response.status
                    if response.status == 200:
                        return domain, False, response.status, "rdap-head"
                    if response.status == 404:
                        return domain, True, response.status, "rdap-head"
                    if response.status == 405:
                        async with session.get(url, allow_redirects=True) as get_response:
                            last_status = get_response.status
                            await get_response.read()
                            if get_response.status == 200:
                                return domain, False, get_response.status, "rdap-get"
                            if get_response.status == 404:
                                return domain, True, get_response.status, "rdap-get"
                    if response.status in {400, 401, 403}:
                        last_error = f"http-{response.status}"
                        break
                    if response.status == 429 or response.status >= 500:
                        retry_after = response.headers.get("Retry-After")
                        delay = float(retry_after) if retry_after and retry_after.isdigit() else min(30.0, 0.75 * (2 ** attempt))
                        await asyncio.sleep(delay + random.random() * 0.25)
                        continue
                    last_error = f"http-{response.status}"
            except (aiohttp.ClientError, asyncio.TimeoutError) as exc:
                last_error = f"{type(exc).__name__}: {exc}"
                await asyncio.sleep(min(20.0, 0.5 * (2 ** attempt)) + random.random() * 0.2)
        return domain, None, last_status, last_error or "unresolved"


async def run_rdap(domains: list[str]) -> dict[str, tuple[Optional[bool], int, str]]:
    # Fifteen request starts per second keeps the scan controlled while remaining practical.
    limiter = StartRateLimiter(15.0)
    semaphore = asyncio.Semaphore(24)
    timeout = aiohttp.ClientTimeout(total=25, connect=10, sock_read=15)
    connector = aiohttp.TCPConnector(limit=30, ttl_dns_cache=600)
    headers = {
        "Accept": "application/rdap+json, application/json",
        "User-Agent": "at-short-domain-audit/1.0 (availability research; controlled rate)",
    }
    results: dict[str, tuple[Optional[bool], int, str]] = {}
    completed = 0
    lock = asyncio.Lock()

    async with aiohttp.ClientSession(timeout=timeout, connector=connector, headers=headers) as session:
        async def one(domain: str) -> None:
            nonlocal completed
            name, available, status, method = await rdap_lookup(session, domain, semaphore, limiter)
            results[name] = (available, status, method)
            async with lock:
                completed += 1
                if completed % 1_000 == 0 or completed == len(domains):
                    counts = Counter(v[0] for v in results.values())
                    print(
                        f"RDAP {completed}/{len(domains)} "
                        f"available={counts[True]} registered={counts[False]} error={counts[None]}",
                        flush=True,
                    )

        await asyncio.gather(*(one(domain) for domain in domains))
    return results


def whois_fallback(domain: str) -> tuple[Optional[bool], str]:
    try:
        with socket.create_connection(("whois.nic.at", 43), timeout=12) as sock:
            sock.sendall((domain + "\r\n").encode("ascii"))
            chunks = []
            while True:
                chunk = sock.recv(8192)
                if not chunk:
                    break
                chunks.append(chunk)
        text = b"".join(chunks).decode("utf-8", errors="replace")
        lower = text.lower()
        if "nothing found" in lower or "no entries found" in lower or "not found" in lower:
            return True, "whois-not-found"
        if "domain:" in lower or "registrar:" in lower or "nserver:" in lower or "status:" in lower:
            return False, "whois-record"
        return None, "whois-ambiguous"
    except Exception as exc:
        return None, f"whois-{type(exc).__name__}: {exc}"


def write_outputs(
    labels: list[str],
    dns_results: dict[str, Optional[bool]],
    rdap_results: dict[str, tuple[Optional[bool], int, str]],
    started_at: str,
) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    checked_at = datetime.now(timezone.utc).isoformat()
    rows = []
    errors = []

    for label in labels:
        domain = f"{label}.at"
        delegated = dns_results[domain]
        if delegated is True:
            available = False
            source = "authoritative-dns-delegation"
            http_status = ""
            detail = ""
        else:
            available, status, detail = rdap_results.get(domain, (None, 0, "missing-rdap-result"))
            source = "official-rdap"
            http_status = status or ""

        row = {
            "domain": domain,
            "label": label,
            "length": len(label),
            "contains_digit": any(ch.isdigit() for ch in label),
            "contains_hyphen": "-" in label,
            "available": available,
            "verification_source": source,
            "rdap_http_status": http_status,
            "detail": detail,
            "checked_at_utc": checked_at,
        }
        rows.append(row)
        if available is None:
            errors.append(row)

    # WHOIS is used only for the small residual error set, avoiding abusive bulk WHOIS traffic.
    if errors:
        print(f"WHOIS fallback for {len(errors)} unresolved RDAP records", flush=True)
        for index, row in enumerate(errors, 1):
            state, detail = whois_fallback(row["domain"])
            row["available"] = state
            row["verification_source"] = "nic.at-whois-fallback"
            row["detail"] = detail
            if index % 50 == 0:
                print(f"WHOIS fallback {index}/{len(errors)}", flush=True)
            time.sleep(0.35)

    available_rows = sorted(
        (row for row in rows if row["available"] is True),
        key=lambda row: (row["length"], row["label"]),
    )
    unresolved_rows = [row for row in rows if row["available"] is None]

    fieldnames = list(rows[0].keys())
    with (OUT_DIR / "available_at_le3.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(available_rows)

    with (OUT_DIR / "available_at_le3.txt").open("w", encoding="utf-8") as f:
        for row in available_rows:
            f.write(row["domain"] + "\n")

    with (OUT_DIR / "unresolved_at_le3.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(unresolved_rows)

    by_length = Counter(row["length"] for row in available_rows)
    by_kind = Counter(
        "hyphen" if row["contains_hyphen"] else "has-digit" if row["contains_digit"] else "letters-only"
        for row in available_rows
    )
    summary = {
        "scope": "ASCII a-z, 0-9, and a legal middle hyphen; second-level label length 1-3",
        "total_candidates": len(rows),
        "available_count": len(available_rows),
        "registered_or_delegated_count": sum(row["available"] is False for row in rows),
        "unresolved_count": len(unresolved_rows),
        "available_by_length": {str(k): by_length[k] for k in sorted(by_length)},
        "available_by_kind": dict(sorted(by_kind.items())),
        "started_at_utc": started_at,
        "finished_at_utc": checked_at,
        "known_checks": {
            "mem.at": next((r["available"] for r in rows if r["domain"] == "mem.at"), None),
            "003.at": next((r["available"] for r in rows if r["domain"] == "003.at"), None),
        },
    }
    (OUT_DIR / "scan_summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, indent=2), flush=True)


async def main() -> None:
    started_at = datetime.now(timezone.utc).isoformat()
    labels = generate_labels()
    domains = [f"{label}.at" for label in labels]
    print(f"Generated {len(domains)} valid ASCII .at candidates", flush=True)

    dns_results = await run_dns_prefilter(domains)
    rdap_targets = [domain for domain in domains if dns_results[domain] is not True]
    print(f"RDAP verification targets: {len(rdap_targets)}", flush=True)
    rdap_results = await run_rdap(rdap_targets)
    write_outputs(labels, dns_results, rdap_results, started_at)


if __name__ == "__main__":
    asyncio.run(main())
