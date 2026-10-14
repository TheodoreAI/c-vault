# C-Vault: Technical Specification & Implementation Plan

This document outlines the architecture, data structures, and implementation plan for C-Vault—a personal media and file vault written in C, served through CGI, and backed by SQLite. 

The project was conceived to solve a straightforward, recurring problem: local storage exhaustion on iPhones. My goal was to create a lightweight, dependable way to offload photos and 4K videos to a local machine over Wi-Fi, retrieve them whenever needed, and reclaim phone storage without relying on subscription cloud services or heavy containerized software stacks.

The design builds directly on the BCHS (OpenBSD, C, httpd, SQLite) methodology discussed by Corey Prophitt in [*A Modern Web App in C*](https://prophitt.me/writing/a-modern-web-app-in-c).

---

## 1. Problem Context & Design Goals

### 1.1 The Practical Problem
Modern smartphone cameras generate large volumes of data. A short 4K 60fps video can easily take 400 MB to 1 GB, and 48 MP photos quickly consume space on a 128 GB phone. Apple’s default answer is iCloud, but iCloud is designed primarily to synchronize state across Apple hardware rather than act as a pure offload drive; deleting a file on your phone often deletes it everywhere unless you manage settings carefully.

Existing self-hosted alternatives like Nextcloud or Immich work well, but they require substantial infrastructure: multiple Docker containers, PostgreSQL or MariaDB instances, Redis cache layers, and dedicated background workers. For a single-user home setup, running that much software just to transfer files from a phone to a hard drive felt like more complexity than the problem required.

### 1.2 Design Goals
1. **Zero runtime dependencies:** A single compiled C binary. SQLite is vendored as a single source file (`sqlite3.c`), eliminating the need for a separate database process or external package managers.
2. **Predictable, low memory usage:** Streaming uploads directly from standard input (`stdin`) to disk in 64 KB chunks so that a 2 GB video upload uses less than 16 MB of RAM.
3. **Preservation of Apple media formats:** Store untouched originals (`.heic`, `.mov`, `.dng`) byte-for-byte, preserving color profiles, Apple HDR metadata, and EXIF timestamps.
4. **Smooth in-browser media playback:** Implement HTTP 206 Partial Content so iOS Safari can scrub through 4K videos without downloading the whole file first.
5. **Bidirectional workflow:** Allow effortless offloading to free phone space, plus quick on-demand browsing and downloading back to the Camera Roll or Files app.
6. **Simple automation:** Provide a one-tap iOS Shortcut that uploads selected photos through the Share Sheet and prompts to delete the originals from the phone.

---

## 2. Technical Architecture & Fundamentals

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
         │   (WAL Mode Amalg.) │                   │ (uploads/YYYY/MM/  │
         │   • Metadata & Tags │                   │  <sha256>.<ext>)   │
         │   • SHA-256 Hashes  │                   │                    │
         │   • FTS5 Search     │                   │                    │
         └─────────────────────┘                   └────────────────────┘
```

### 2.1 The CGI Execution Model
Before looking at the implementation details, it helps to understand why CGI works well for this use case. 

Modern web development usually places an application server (like Node, Gunicorn, or Puma) behind a reverse proxy. That means you are running and monitoring two separate server processes. CGI eliminates the second long-running process entirely:
* The web server handles network sockets and invokes our C program on each request.
* Request parameters, cookies, and HTTP headers arrive as environment variables (`REQUEST_METHOD`, `QUERY_STRING`, `HTTP_COOKIE`, `CONTENT_LENGTH`).
* The request body is read directly from standard input (`stdin`).
* Response headers, status codes, and HTML or binary data are written directly to standard output (`stdout`).
* Error messages and diagnostics are logged to standard error (`stderr`).

Because each request runs in a fresh process that exits when finished, there is no chance for memory leaks or stale state to accumulate across requests. Deploying an update is as simple as compiling a new binary and replacing the old one; subsequent requests immediately execute the updated code without downtime or server restarts.

### 2.2 Streaming I/O and Memory Control
A common failure mode in web applications handling large media is buffering. If an application attempts to load an entire 1 GB or 2 GB video into memory before writing it to disk, small home servers or mini-PCs will run out of memory quickly.

To prevent this, C-Vault implements a streaming multipart parser:
1. It reads the total payload size from `CONTENT_LENGTH`.
2. It reads chunks of fixed size (64 KB) from `stdin`.
3. It parses the boundary markers in-flight and writes the file payload directly to a temporary file on disk (`uploads/temp/`).
4. While writing, it updates a running SHA-256 calculation.
5. Once the stream ends, it checks the computed hash against SQLite. If the file already exists, the temporary file is discarded to avoid duplicating data. If the file is new, it is moved to its permanent path partitioned by date: `uploads/YYYY/MM/<sha256>.<ext>`.

Through this approach, process memory remains under 16 MB regardless of whether you upload a 2 MB photo or a 4 GB video.

### 2.3 Media Handling & Apple Formats
Working with iPhones introduces specific format requirements that standard web applications often miss:

* **HEIC Images:** Apple devices default to High Efficiency Image Container (`.heic`) format. Converting them to JPEG during upload wastes time, strips Apple dynamic range metadata, and inflates file size. C-Vault stores the raw HEIC file untouched. Mobile Safari has native hardware decoding for HEIC and displays them directly in standard `<img>` tags. For non-Apple desktop browsers that lack native HEIC support, a lightweight client-side preview or embedded JPEG extraction is used.
* **HEVC Videos & HTTP 206 Partial Content:** iPhone 4K and 1080p videos use the H.265 (HEVC) codec inside a `.mov` container. When iOS Safari or QuickTime attempts to play a video, it does not download the entire file first. Instead, it issues a byte-range request (`Range: bytes=0-1`) to inspect the QuickTime **MOOV atom** at the beginning or end of the container, reads index metadata, and then requests only the specific chunk needed for playback. If the server does not support HTTP 206 Partial Content, iOS Safari refuses to play the video. C-Vault handles range requests explicitly using `fseeko64()`, allowing smooth scrubbing through large video files over Wi-Fi.

### 2.4 iOS Sandbox & Automation Strategy
A web application running in Safari cannot silently inspect the Camera Roll in the background while the phone is locked. Apple’s security sandbox intentionally prevents this.

Rather than fighting the operating system, C-Vault works with it through a native **iOS Shortcut**:
1. You select a batch of photos or videos in the Apple Photos app.
2. You tap the Share button and select "Upload to C-Vault."
3. The shortcut sends the items via HTTP POST to `/api/upload` on your C-Vault server.
4. When the server confirms that the files are safely stored, the shortcut prompts: *"Backed up X items. Delete originals from iPhone?"*
5. Confirming deletes the local copies, immediately freeing up phone storage while keeping the full-resolution originals accessible on your server.

---

## 3. Database Schema

We use an embedded SQLite database (`data/vault.db`) running in Write-Ahead Logging (`WAL`) mode to support concurrent reads while writes take place.

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

-- Tags for categorization
CREATE TABLE IF NOT EXISTS tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS file_tags (
    file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
    tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY(file_id, tag_id)
);

-- Authentication Sessions
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    token TEXT NOT NULL UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL
);

-- Full-Text Search (FTS5) for fast filename & tag discovery
CREATE VIRTUAL TABLE IF NOT EXISTS files_fts USING fts5(
    original_filename,
    media_type,
    content='files',
    content_rowid='id'
);
```

---

## 4. API & CGI Routing

| Method | Route | Description | Response Type |
|---|---|---|---|
| `GET` | `/` | Main dashboard and media gallery (filterable by type and date) | `text/html` |
| `GET` | `/view/:uuid` | Single media preview page with video player or photo viewer | `text/html` |
| `GET` | `/raw/:uuid` | Direct media stream with HTTP 206 Partial Content support | Binary (`image/*`, `video/*`) |
| `GET` | `/download/:uuid`| Triggers file attachment download with original filename | `application/octet-stream` |
| `POST`| `/upload` | Multipart form upload for browser UI (handles multiple files) | `303 See Other` redirect |
| `POST`| `/api/upload` | Headless API endpoint designed for the iOS Shortcut | `application/json` |
| `POST`| `/delete/:uuid`| Moves a file to trash (`is_deleted = 1`) | `303 See Other` redirect |
| `POST`| `/restore/:uuid`| Restores a file from trash | `303 See Other` redirect |
| `POST`| `/purge/:uuid` | Permanently deletes a file from disk and database | `303 See Other` redirect |
| `GET` | `/stats` | Summary of total stored files, disk usage, and reclaimed space | `text/html` or JSON |
| `GET/POST`| `/login` | Simple PIN/passphrase authentication and cookie issuance | `text/html` / `303 See Other` |

---

## 5. Implementation Tasks (GitHub Project Board)

The tasks below are organized into sequential epics and structured for direct tracking on a GitHub project board.

---

### Epic 1: Project Skeleton & Build System

#### Task 1.1: Project Directory Structure & SQLite Vendoring
* **Labels**: `setup`, `c-lang`, `database`
* **Description**: Establish the standard directory layout (`src/`, `data/`, `uploads/`, `public/`). Vendor the official SQLite amalgamation (`sqlite3.c`, `sqlite3.h`) directly into `src/` alongside an embedded SHA-256 implementation.
* **Acceptance Criteria**:
  - `src/sqlite3.c` and `src/sqlite3.h` are vendored and build cleanly.
  - Required directories are created and `.gitignore` correctly ignores build artifacts, database files, and uploaded media.

#### Task 1.2: Makefile Configuration
* **Labels**: `build`, `c-lang`
* **Description**: Create a portable `Makefile` supporting MinGW GCC on Windows as well as GCC/Clang on Linux and macOS. Use optimization flags (`-O2`), standard warnings (`-Wall -Wextra`), and appropriate threading/math links for SQLite.
* **Acceptance Criteria**:
  - Running `make` compiles the full project into `cvault` (or `cvault.exe`) in under 5 seconds.
  - Running `make clean` removes all intermediate object files.

#### Task 1.3: Local Development Server Harness (`dev_server.py`)
* **Labels**: `tooling`, `devops`
* **Description**: Write a lightweight Python script that acts as a local CGI server for testing. It should bind to `0.0.0.0:8080`, print the host computer's local Wi-Fi IP address, set standard CGI environment variables, and route requests to the compiled `cvault` binary.
* **Acceptance Criteria**:
  - Running `python dev_server.py` starts the server and displays the local LAN address (e.g. `http://192.168.1.50:8080`).
  - An iPhone on the same Wi-Fi network can connect to the server and receive valid HTTP responses.

---

### Epic 2: Core CGI & Database Foundation

#### Task 2.1: CGI Environment & Request Helpers (`cgi.c` / `cgi.h`)
* **Labels**: `backend`, `c-lang`
* **Description**: Implement helper functions to parse CGI environment variables, decode query parameters (`?key=value`), parse URL-encoded form data, extract cookies, and emit standard HTTP headers (`Content-Type`, `Status`, `Set-Cookie`, `Location`). Include an `html_escape` helper to prevent XSS.
* **Acceptance Criteria**:
  - Request parameters and cookies can be queried by name via helper functions.
  - URL decoding handles spaces, `+`, and special characters properly.
  - Output functions correctly format HTTP header boundaries with standard CRLF line endings.

#### Task 2.2: SQLite Database Controller (`db.c` / `db.h`)
* **Labels**: `database`, `c-lang`
* **Description**: Write database initialization routines that execute schema migrations on startup, enable WAL mode, configure busy timeouts to handle concurrency, and provide clean functions for querying and inserting file records.
* **Acceptance Criteria**:
  - `data/vault.db` is created automatically on first run with all tables and indexes.
  - Concurrent requests do not fail with database lock errors under normal local usage.

#### Task 2.3: SHA-256 Deduplication & Storage Manager
* **Labels**: `storage`, `c-lang`
* **Description**: Compute cryptographic SHA-256 hashes of incoming data streams. Before saving a file permanently, check if the hash already exists in SQLite. If it does, record the new entry without writing duplicate bytes to disk.
* **Acceptance Criteria**:
  - Uploading the exact same image twice results in a single file stored on disk.
  - Files are organized into `uploads/YYYY/MM/<sha256>.<ext>` to avoid single-directory file count limits.

---

### Epic 3: Upload & Media Streaming Subsystem

#### Task 3.1: Chunked Multipart/Form-Data Parser
* **Labels**: `backend`, `c-lang`, `performance`
* **Description**: Implement a streaming multipart parser that reads `stdin` in fixed 64 KB chunks based on `CONTENT_LENGTH`. Stream file contents directly to disk in `uploads/temp/` without loading entire files into memory.
* **Acceptance Criteria**:
  - A 1 GB+ video file uploads successfully while peak process RAM stays below 16 MB.
  - Interrupted or failed uploads clean up temporary files cleanly.

#### Task 3.2: HTTP 206 Partial Content Video Streamer
* **Labels**: `backend`, `c-lang`, `media`
* **Description**: Implement byte-range request handling for video playback (`.mp4`, `.mov`). Parse `Range: bytes=start-end`, seek within the target file using `fseeko64()`, emit `Content-Range: bytes start-end/total`, and stream the requested byte segment.
* **Acceptance Criteria**:
  - An iPhone playing a 4K `.mov` video in Safari can scrub forward and backward smoothly without buffering the entire file.
  - Requests without a `Range` header return standard `200 OK` responses.

#### Task 3.3: Headless API Endpoint for iOS Shortcuts (`/api/upload`)
* **Labels**: `api`, `ios`, `backend`
* **Description**: Build a dedicated JSON endpoint that accepts POST uploads accompanied by an authorization token (`Authorization: Bearer <TOKEN>`) and returns a concise JSON status object with the file UUID, size, and deduplication status.
* **Acceptance Criteria**:
  - Endpoint works reliably with `curl` and the iOS Shortcut "Get Contents of URL" action.
  - Requests with missing or invalid tokens return a `401 Unauthorized` JSON response.

---

### Epic 4: User Interface & Mobile Experience

#### Task 4.1: Responsive Semantic Views in C (`views.c` / `views.h`)
* **Labels**: `frontend`, `c-lang`, `ui`
* **Description**: Create functions in C that generate clean, semantic HTML5 with embedded CSS. Design mobile-first for iPhone viewports (390px to 430px wide) while adapting gracefully to desktop screens. Include dark and light mode support based on system settings.
* **Acceptance Criteria**:
  - UI renders correctly on mobile Safari and desktop browsers without horizontal scrolling or broken layouts.
  - Entire embedded CSS bundle is under 15 KB with zero third-party framework dependencies.

#### Task 4.2: Mobile Upload Hub with Live Progress Bars
* **Labels**: `frontend`, `mobile`, `ux`
* **Description**: Implement a multi-file upload zone with `<input type="file" multiple accept="image/*,video/*">`. Use vanilla JavaScript with `XMLHttpRequest` to show live upload progress bars for each selected file, while preserving standard form submission fallback when JavaScript is disabled.
* **Acceptance Criteria**:
  - Selecting 20+ photos on an iPhone displays real-time percentage progress for each item.
  - Successfully uploaded items trigger an automatic gallery refresh.

#### Task 4.3: Media Gallery Grid, Filters & Storage Metrics
* **Labels**: `frontend`, `ui`, `database`
* **Description**: Render uploaded media in a clean grid layout. Provide tabs to filter by type (**All**, **Photos**, **Videos**, **Documents**, **Trash**) and display a summary card showing total files stored, total gigabytes, and estimated iPhone storage saved.
* **Acceptance Criteria**:
  - Clicking filter tabs updates the media list promptly.
  - File sizes and timestamps are formatted clearly (e.g. "14.2 MB", "Oct 14, 2026").

#### Task 4.4: Fullscreen Lightbox & In-Line Player
* **Labels**: `frontend`, `media`, `mobile`
* **Description**: Build a lightweight media modal for viewing full-size images and playing videos inline. Include swipe or arrow navigation between items, a direct download button, and a delete button.
* **Acceptance Criteria**:
  - Photos can be viewed at full resolution and saved back to the iOS Camera Roll.
  - Videos stream inline with standard audio and playback controls.

---

### Epic 5: Authentication, Security & Deployment

#### Task 5.1: Passphrase Authentication & Session Cookies
* **Labels**: `security`, `auth`, `backend`
* **Description**: Implement simple PIN or passphrase protection. Successful logins issue an HTTP-only, secure session cookie stored in the `sessions` table. Unauthenticated requests from the local network are redirected to `/login`.
* **Acceptance Criteria**:
  - Unauthorized requests cannot view or download media.
  - Session tokens are generated using cryptographically random bytes and expire after a configurable duration.

#### Task 5.2: iOS Shortcut Recipe & Documentation
* **Labels**: `documentation`, `ios`, `automation`
* **Description**: Document the exact step-by-step configuration for creating the iOS Share Sheet Shortcut ("Upload to C-Vault & Prompt to Delete"). Provide clear screenshots and an exportable shortcut file.
* **Acceptance Criteria**:
  - A user following `docs/IOS_SHORTCUT_GUIDE.md` can set up the shortcut on iOS 16+ within 5 minutes.
  - The shortcut uploads selected photos and prompts to delete local copies upon success.

#### Task 5.3: Production Setup & Remote Access Guide
* **Labels**: `documentation`, `devops`
* **Description**: Provide documentation for running C-Vault as a background service on Windows (PowerShell/nssm) and Linux/OpenBSD (systemd/rc.d). Detail how to connect securely away from home using Tailscale or a reverse proxy.
* **Acceptance Criteria**:
  - Documentation covers running the binary behind standard web servers (`httpd`, `nginx`, `lighttpd`).
  - Tailscale setup instructions explain how to access the vault securely without opening router ports.

---

## 6. Definition of Done (DoD)

1. **Build Quality:** The codebase compiles cleanly with GCC using `-Wall -Wextra -O2` with zero errors and zero warnings.
2. **Resource Constraints:** Process memory remains under 16 MB during a 1 GB video upload.
3. **Apple Compatibility:** `.heic` photos display natively in Safari, and `.mov` videos support smooth scrubbing via HTTP 206 range requests.
4. **Data Integrity:** Interrupted uploads do not leave orphan records in SQLite, and duplicate uploads are recognized without wasting disk space.
5. **Self-Contained:** The compiled binary and vendored SQLite require no external runtime dependencies to operate.
