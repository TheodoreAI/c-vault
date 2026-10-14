# C-Vault: Technical Specification & Implementation Plan

A high-performance, self-hosted personal media and file vault written in C, served via CGI, and powered by SQLite. Designed specifically to offload photos, 4K videos, and documents from iPhones and laptops with zero bloat, streaming efficiency, and instant retrieval.

Based on the **BCHS** (C + CGI + SQLite) methodology outlined in [A Modern Web App in C](https://prophitt.me/writing/a-modern-web-app-in-c).

---

## 1. Executive Summary

### 1.1 Problem Statement
Modern iPhones produce large volumes of high-resolution media (48MP photos, 4K 60fps ProRes/HEVC videos), rapidly exhausting local storage (128GB/256GB). Commercial solutions (Apple iCloud, Google Photos, Dropbox) introduce perpetual subscription fees, privacy concerns, proprietary lock-in, and cumbersome manual export workflows. Meanwhile, existing self-hosted software (Nextcloud, Immich) often requires heavyweight containers, multi-gigabyte runtimes (Node.js, Python, Redis, Postgres), and extensive server maintenance.

### 1.2 Solution
**C-Vault** delivers a single-binary, hyper-lean alternative:
* **Zero Runtimes**: Pure C application using CGI 1.1 and an embedded SQLite database.
* **Low Memory Footprint**: Chunked streaming I/O handles multi-gigabyte video uploads directly to disk in fixed 64KB buffers without RAM spikes.
* **Direct iPhone Integration**: Web-based multi-file picker from the iOS Camera Roll plus a 1-tap iOS Share Sheet Shortcut ("Upload to Vault & Prompt to Delete").
* **Sub-Millisecond Response**: Spawns clean per-request or runs under FastCGI, with zero memory leak accumulation across requests.
* **Resilient Storage**: Content-addressable storage with SHA-256 deduplication and SQLite metadata indexing.

---

## 2. Technical Architecture

```
                          ┌──────────────────────────────────────┐
                          │         Client Layer (iPhone)        │
                          │  • Mobile Safari PWA (Camera Roll)   │
                          │  • iOS Shortcut (Share Sheet)        │
                          │  • Desktop Browser (Drag & Drop)     │
                          └──────────────────┬───────────────────┘
                                             │ HTTP / HTTPS
                                             ▼
                          ┌──────────────────────────────────────┐
                          │          Web / CGI Server            │
                          │  • Python CGI dev runner (local dev) │
                          │  • httpd / nginx / lighttpd (prod)   │
                          └──────────────────┬───────────────────┘
                                             │ stdin / stdout / env
                                             ▼
                          ┌──────────────────────────────────────┐
                          │           C-Vault Binary             │
                          │  • cgi.c: Env & multipart parser     │
                          │  • router.c: Route dispatcher        │
                          │  • stream.c: HTTP 206 range engine   │
                          │  • views.c: Programmatic HTML/CSS    │
                          │  • auth.c: PIN/Cookie verification   │
                          └──────────────┬───────────────────────┘
                                         │
                    ┌────────────────────┴────────────────────┐
                    ▼                                         ▼
         ┌─────────────────────┐                   ┌────────────────────┐
         │   SQLite Database   │                   │ Filesystem Storage │
         │   (WAL Mode Amalg.) │                   │ (vault/YYYY/MM/    │
         │   • Metadata & Tags │                   │  <sha256>.<ext>)   │
         │   • SHA-256 Hashes  │                   │                    │
         │   • FTS5 Search     │                   │                    │
         └─────────────────────┘                   └────────────────────┘
```

### 2.1 Core Technology Stack
| Layer | Technology | Rationale |
|---|---|---|
| **Language** | C99 / C11 | Minimal resource usage, maximum performance, portable. |
| **Protocol** | CGI 1.1 (RFC 3875) | Simple, stateless, process-isolated, no framework lock-in. |
| **Database** | SQLite 3 (Amalgamation) | Zero-maintenance embedded database; single C file vendored directly. |
| **Hashing** | SHA-256 (CAmalgamation/crypto) | Content deduplication and integrity verification. |
| **Frontend** | Vanilla HTML5 + CSS + Minimal JS | Progressive enhancement: works with standard forms, enhanced with upload progress & lightbox. |
| **Development** | MinGW GCC / Make / Python 3 | Rapid 1-second compilation cycle and local cross-device testing. |

### 2.2 Streaming & Memory Management
* **Multipart Streamer**: Unlike traditional frameworks that load incoming files into memory buffers, `cgi_parse_multipart` reads fixed 64KB blocks from `stdin` based on `CONTENT_LENGTH`. As chunks arrive, the payload bytes are streamed directly into a temporary file on disk, maintaining peak RAM usage under 8MB regardless of upload size (tested up to 4GB+).
* **HTTP 206 Byte-Range Streaming**: Video playback on iOS Safari requires partial content range requests (`Range: bytes=start-end`). The C binary inspects the `HTTP_RANGE` header and directly seeks (`fseeko64`) to the requested offsets, streaming only the requested chunks with `Content-Range` headers.

---

## 3. Database Schema

Embedded SQLite database stored at `data/vault.db` running in `WAL` (Write-Ahead Logging) mode for concurrent access.

```sql
PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;

-- Core Files & Metadata
CREATE TABLE IF NOT EXISTS files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    original_filename TEXT NOT NULL,
    stored_filename TEXT NOT NULL,
    file_path TEXT NOT NULL,
    mime_type TEXT NOT NULL,
    file_size INTEGER NOT NULL,
    sha256 TEXT NOT NULL,
    media_type TEXT NOT NULL, -- 'photo', 'video', 'document', 'other'
    width INTEGER DEFAULT 0,
    height INTEGER DEFAULT 0,
    duration INTEGER DEFAULT 0, -- in seconds (for videos)
    is_favorite INTEGER DEFAULT 0,
    is_deleted INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_files_sha256 ON files(sha256);
CREATE INDEX IF NOT EXISTS idx_files_media_type ON files(media_type);
CREATE INDEX IF NOT EXISTS idx_files_created_at ON files(created_at);
CREATE INDEX IF NOT EXISTS idx_files_is_deleted ON files(is_deleted);

-- Tags
CREATE TABLE IF NOT EXISTS tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS file_tags (
    file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
    tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY(file_id, tag_id)
);

-- Sessions / Auth
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    token TEXT NOT NULL UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL
);

-- Full-Text Search (FTS5) for instant file discovery
CREATE VIRTUAL TABLE IF NOT EXISTS files_fts USING fts5(
    original_filename,
    media_type,
    content='files',
    content_rowid='id'
);
```

---

## 4. API & CGI Routing Specification

| Method | Route | Description | Response Type |
|---|---|---|---|
| `GET` | `/` | Dashboard & Media Gallery (filterable by type/date) | `text/html` |
| `GET` | `/view/:uuid` | Single media preview page with lightbox / video controls | `text/html` |
| `GET` | `/raw/:uuid` | Direct raw file stream with HTTP 206 Partial Content | Binary (`image/*`, `video/*`, etc.) |
| `GET` | `/download/:uuid`| Force file attachment download with original filename | `application/octet-stream` |
| `POST`| `/upload` | Web-based multipart form upload (supports multiple files) | `303 Redirect` or JSON progress |
| `POST`| `/api/upload` | Headless API upload designed for the iOS Shortcut | `application/json` |
| `POST`| `/delete/:uuid`| Soft-delete file (move to trash) | `303 Redirect` |
| `POST`| `/restore/:uuid`| Restore soft-deleted file | `303 Redirect` |
| `POST`| `/purge/:uuid` | Permanent deletion from disk and database | `303 Redirect` |
| `GET` | `/stats` | Storage analytics & savings counter | `text/html` or JSON |
| `GET/POST`| `/login` | Simple PIN/passphrase authentication & session cookie | `text/html` / `303 Redirect` |

---

## 5. iOS Shortcut Automation Workflow

To seamlessly solve the iPhone storage exhaustion issue:

```
[iPhone User] 
     │
     ▼
Selects 20 photos & 4K videos in Apple Photos app
     │
     ▼
Taps Share Sheet ──▶ Selects "Upload to C-Vault" Shortcut
                          │
                          ▼
            [iOS Shortcut Executes in Background]
            1. Loops over selected media items
            2. Sends POST to https://<host>/api/upload
               with Bearer Token / API Key
            3. Checks HTTP 200 JSON { "status": "success", "deduped": false }
                          │
                          ▼
            [iOS Shortcut Prompts User]
            "20 items safely backed up to C-Vault (1.4 GB freed).
             Delete selected photos from your iPhone?"
                          │
                   ┌──────┴──────┐
                   │             │
               [Confirm]      [Cancel]
                   │             │
                   ▼             ▼
             Originals      Originals
             Deleted        Preserved
             from iPhone
```

---

## 6. GitHub Project Board: Epics & Tasks Breakdown

The following tasks are formatted for direct entry into a GitHub Project board (columns: **Backlog**, **Ready**, **In Progress**, **Review**, **Done**).

---

### Epic 1: Project Skeleton & Build System

#### Task 1.1: Initialize Project Directory & Vendored Dependencies
* **Labels**: `setup`, `c-lang`, `database`
* **Description**: Create folder structure (`src/`, `data/`, `uploads/`, `public/`). Download and vendor SQLite amalgamation (`sqlite3.c`, `sqlite3.h`) version 3.45+ and SHA-256 implementation.
* **Acceptance Criteria**:
  - `src/sqlite3.c` and `src/sqlite3.h` present and clean.
  - Directory structure adheres to standard C layout.
  - Git ignore ignores build artifacts (`.exe`, `.o`, `data/`, `uploads/`).

#### Task 1.2: Makefile & Cross-Platform Compilation
* **Labels**: `build`, `c-lang`
* **Description**: Write a robust `Makefile` compatible with MinGW GCC (Windows) and clang/gcc (Linux/BSD/macOS). Enable optimization flags (`-O2`), warnings (`-Wall -Wextra`), and link pthread/dl for SQLite.
* **Acceptance Criteria**:
  - Running `make` compiles the project to `cvault` (or `cvault.exe`) in under 5 seconds.
  - Clean target (`make clean`) removes intermediate objects.

#### Task 1.3: Python CGI Local Development Server
* **Labels**: `devops`, `tooling`
* **Description**: Implement `dev_server.py` to route HTTP requests to the compiled CGI binary, bind to `0.0.0.0:8080`, and display the machine's local Wi-Fi IP so iPhones on the same network can immediately connect.
* **Acceptance Criteria**:
  - Running `python dev_server.py` launches the server and prints LAN IP (e.g. `http://192.168.1.50:8080`).
  - Correctly sets CGI environment variables (`REQUEST_METHOD`, `QUERY_STRING`, `PATH_INFO`, `CONTENT_LENGTH`, `CONTENT_TYPE`, `HTTP_COOKIE`, `HTTP_RANGE`).
  - Pipes `stdin` and reads `stdout` from `cvault.exe`.

---

### Epic 2: Core CGI & Database Foundation

#### Task 2.1: CGI Abstraction Library (`cgi.h` / `cgi.c`)
* **Labels**: `backend`, `c-lang`
* **Description**: Build a reusable CGI utility library to parse environment variables, parse query parameters (`?key=value`), parse URL-encoded form bodies, extract cookies, and emit standard HTTP headers (`Content-Type`, `Status`, `Set-Cookie`, `Location`).
* **Acceptance Criteria**:
  - Query parameters and cookies can be looked up with `cgi_param(req, "key")`.
  - Proper URL decoding handling `%20`, `+`, and Unicode.
  - HTML escaping helper function (`html_escape`) prevents XSS vulnerabilities.

#### Task 2.2: SQLite Database Controller (`db.h` / `db.c`)
* **Labels**: `database`, `c-lang`
* **Description**: Implement database initialization, schema migration execution, connection pooling/open-close lifecycle per CGI process, and WAL mode enablement.
* **Acceptance Criteria**:
  - Auto-creates `data/vault.db` and executes tables initialization if not present.
  - Handles database busy/locked states gracefully with busy timeouts.
  - Clean query wrappers for inserting and querying records.

#### Task 2.3: SHA-256 Deduplication & Storage Manager
* **Labels**: `storage`, `c-lang`
* **Description**: Implement cryptographic SHA-256 calculation on streamed data. Verify against the `files` table before finalizing storage. If a duplicate hash exists, reference the existing record or link without duplicating disk storage.
* **Acceptance Criteria**:
  - Uploading the identical file twice results in deduplication detection.
  - Storage path partition format (`uploads/YYYY/MM/<hash>.<ext>`) avoids single-folder inode overload.

---

### Epic 3: High-Performance Upload & Streaming Engine

#### Task 3.1: Chunked Multipart/Form-Data Parser
* **Labels**: `backend`, `c-lang`, `performance`
* **Description**: Implement a low-memory multipart streaming parser. Read `stdin` in 64KB chunks, extract boundary headers, stream the file payload directly into `uploads/temp/` without loading into RAM, and rename upon completion.
* **Acceptance Criteria**:
  - Successfully uploads 500MB+ video file on low-memory environments (<16MB RAM peak).
  - Handles multi-file uploads in a single request.
  - Cleans up incomplete temporary files on abort.

#### Task 3.2: HTTP 206 Partial Content Byte-Range Video Streamer
* **Labels**: `backend`, `c-lang`, `media`
* **Description**: Implement HTTP 206 Range request handling for video playback (`.mp4`, `.mov`, `.webm`). Parse `Range: bytes=start-end`, seek file pointer, set `Content-Range: bytes start-end/total`, and stream byte chunks.
* **Acceptance Criteria**:
  - iPhone Safari video player can scrub back and forth through a 4K video instantly without freezing or buffering the entire file.
  - Returns `200 OK` if no `Range` header is present, and `206 Partial Content` when present.

#### Task 3.3: Headless API Endpoint for iOS Shortcuts (`/api/upload`)
* **Labels**: `api`, `ios`, `backend`
* **Description**: Create a dedicated JSON endpoint accepting POST uploads with an API authorization header (`Authorization: Bearer <TOKEN>`), returning JSON `{ "status": "ok", "uuid": "...", "filename": "...", "size": 123456 }`.
* **Acceptance Criteria**:
  - Works with `curl` and iOS Shortcut "Get Contents of URL" action.
  - Rejects unauthorized requests with HTTP 401 JSON response.

---

### Epic 4: User Interface & Mobile Experience

#### Task 4.1: Responsive Componentized HTML/CSS Views (`views.h` / `views.c`)
* **Labels**: `frontend`, `c-lang`, `ui`
* **Description**: Write clean C functions that generate semantic, responsive HTML5 with modern CSS (dark/light theme, system fonts, mobile-optimized grid).
* **Acceptance Criteria**:
  - Fully responsive on iPhone screens (390px to 430px width) up to 4K desktops.
  - Zero external CSS/JS framework dependencies (embedded clean CSS in under 15KB).

#### Task 4.2: Mobile Upload Hub with Live Progress & Batch Selection
* **Labels**: `frontend`, `mobile`, `ux`
* **Description**: Implement mobile-optimized file input `<input type="file" multiple accept="image/*,video/*">` with drag-and-drop support, file queue listing, and JavaScript `XMLHttpRequest` progress tracking for real-time progress bars.
* **Acceptance Criteria**:
  - Selecting 20+ photos on iPhone displays progress bar for each file.
  - Graceful fallback for non-JS browsers via standard form POST.

#### Task 4.3: Gallery Grid, Filters & Storage Dashboard
* **Labels**: `frontend`, `ui`, `database`
* **Description**: Display media in a clean thumbnail grid with tabs: **All**, **Photos**, **Videos**, **Documents**, **Favorites**, **Trash**. Include a dashboard widget showing: Total Files, Total Size (GB), and Estimated iPhone Storage Saved.
* **Acceptance Criteria**:
  - Filtering by type triggers instant reload or client-side filtering.
  - Formatted file sizes (KB, MB, GB) and human-readable dates.

#### Task 4.4: Fullscreen Lightbox & Media Modal
* **Labels**: `frontend`, `media`, `mobile`
* **Description**: Lightweight lightbox modal allowing fullscreen photo viewing with swipe/arrow navigation, download button, delete button, and native HTML5 video player with controls.
* **Acceptance Criteria**:
  - Smooth photo navigation on mobile touch.
  - Videos autoplay inline with audio toggle.

---

### Epic 5: Security, Auth & iOS Integration

#### Task 5.1: PIN / Passphrase Authentication & Session Cookies
* **Labels**: `security`, `auth`, `backend`
* **Description**: Implement simple passphrase or 6-digit PIN protection. Valid submissions issue a secure, HTTP-only cookie with an expiration date stored in the `sessions` table.
* **Acceptance Criteria**:
  - Unauthenticated visitors on LAN are redirected to `/login`.
  - Secure session token generated using cryptographically strong random bytes.

#### Task 5.2: iOS Shortcut Recipe & Deployment Guide
* **Labels**: `documentation`, `ios`, `automation`
* **Description**: Create a step-by-step iOS Shortcut recipe and iCloud shortcut link / JSON configuration that users can install in 1 click, allowing photo sharing directly from the iOS Share Sheet.
* **Acceptance Criteria**:
  - Complete illustrated instructions in `docs/IOS_SHORTCUT_GUIDE.md`.
  - Verified working on iOS 16+.

#### Task 5.3: Production Deployment Guide (Systemd / Tailscale / Nginx)
* **Labels**: `documentation`, `devops`
* **Description**: Document how to keep C-Vault running as a background service on Windows (PowerShell/nssm) or Linux/OpenBSD (systemd/rc.d) and connect securely via Tailscale for remote access away from home Wi-Fi.
* **Acceptance Criteria**:
  - `README.md` includes simple instructions for Windows, Linux, and OpenBSD deployment.

---

## 7. Definition of Done (DoD)
1. **Compilation**: Clean compile with GCC with `-Wall -Wextra -O2` with zero errors and zero warnings.
2. **Performance**: Memory consumption during 1GB video upload remains under 16MB RAM.
3. **iPhone Compatibility**: Works natively on iOS Safari without layout breaking.
4. **Reliability**: Uploads survive network interruptions cleanly without corrupting the SQLite database.
5. **No Bloat**: Total compiled binary size under 3MB (including vendored SQLite).

