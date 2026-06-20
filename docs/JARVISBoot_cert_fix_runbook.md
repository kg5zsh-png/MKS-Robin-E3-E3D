# JARVISBoot — `first GET fails` Cert Fix Runbook

**Owner:** KG5ZSH mesh (CiC node) · **Status:** active workaround · **Last updated:** 2026-06-20

> Committed here so it survives remote-container teardown. This is bench/ops
> documentation for the `JARVISBoot_signed*.shortcut` Apple Shortcut — it is **not**
> Robin E3 firmware. Kept in-repo only because this branch is the working surface.

## Symptom

The first `GET` issued by `JARVISBoot_signed2.shortcut` fails on a cold start; a
retry succeeds. Downstream actions that depend on the first response intermittently
break.

## Root cause

The signed shortcut targets the **HTTPS** LAB endpoint:

```
https://100.82.240.6:5082   (LABS — TLS)
```

On cold start JARVIS does not (yet) trust the LAB TLS cert, so the first request is
rejected/aborted before the trust state settles. The retry lands after the trust
path is warm, which is why "the second GET works."

## Fix

Repoint the shortcut's HTTP action at the **plaintext** LAB endpoint and re-sign.

| Field        | Old (broken)                   | New (fixed)                      |
|--------------|--------------------------------|----------------------------------|
| Scheme       | `https`                        | `http`                           |
| Host         | `100.82.240.6`                 | `100.82.240.6`                   |
| Port         | `5082` (LABS / TLS)            | `5081` (LABH / plaintext)        |
| Alias        | LABS                           | **LABH**                         |

### Procedure (must run on macOS — signing tooling not available in remote container)

1. Open `JARVISBoot_signed2.shortcut` in the Shortcuts app (or edit the source).
2. Locate the **Get Contents of URL** action pointing at `:5082`.
3. Change the URL to `http://100.82.240.6:5081/...` (preserve the existing path/query).
4. Re-sign the shortcut:
   ```sh
   shortcuts sign --mode anyone \
     --input  JARVISBoot.shortcut \
     --output JARVISBoot_signed3.shortcut
   ```
5. Distribute the re-signed file (bump the suffix: `_signed3`) and re-import.

## Verification

- Cold-start the shortcut (kill JARVIS first so trust state is not warm).
- The **first** GET should now return `200` with no retry.
- Confirm no TLS/cert error appears in the JARVIS run log.

## Notes / caveats

- `:5081` (LABH) is plaintext HTTP — acceptable only on the trusted Tailscale
  overlay (`100.82.240.6`). Do **not** expose `:5081` off-mesh. (See MESH-013:
  restrict `:5081` bootstrap routes.)
- A `.shortcut` is a signed, gzipped binary plist; it cannot be regenerated or
  re-signed from a Linux container — step 4 must happen on the Mac.
- Long-term fix (deferred): install/trust the LAB TLS cert on JARVIS so `:5082`
  works on first contact, then revert to HTTPS.

## Related queue items

- **CPenny node** — re-signed shortcut install still pending her tap of the
  iMessage link (UDID `00008140-0014142A3C93001C`).
- **JARVISSendC** — done on Mike's iPhone; Penny's install pending.
- **T016 / Cai** — Apple Shortcut import-restriction research feeds this.
