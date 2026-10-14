# C-Vault

C-Vault is a personal media and file vault written in C, served through CGI, and backed by a single SQLite database.

I started this project because my iPhone was constantly running out of storage. When you take a lot of photos and 4K videos, 128 GB fills up surprisingly fast. Apple's default solution is a monthly iCloud subscription, but that does not actually free up local storage in a predictable way, and it locks you into their ecosystem. The self-hosted alternatives I looked at—tools like Immich or Nextcloud—are capable, but they tend to be heavy. Running several Docker containers, a Postgres instance, and a Redis queue just to back up pictures from a phone to a local hard drive felt like too much overhead for what is essentially file transfer and indexing.

I wanted to see how far I could get using a much simpler foundation. This project follows the architecture described by Corey Prophitt in [A Modern Web App in C](https://prophitt.me/writing/a-modern-web-app-in-c): plain C, SQLite via its official amalgamation, and the standard CGI 1.1 protocol.

---

## How it works

The core idea is to keep the application stateless and let the operating system and SQLite do what they are already good at.

1. **Streaming from `stdin` directly to disk:**
   A typical issue when uploading multi-gigabyte 4K iPhone videos to an application server is memory usage. If a framework buffers the entire request payload in RAM before processing it, the process can easily run out of memory or stall. In C-Vault, the multipart parser reads incoming chunks (64 KB at a time) directly from `stdin` based on `CONTENT_LENGTH` and streams the file payload straight into a temporary file on disk. This keeps the process memory footprint small (typically under 16 MB) regardless of file size.

2. **Database and metadata indexing:**
   Once a file finishes uploading, we compute its SHA-256 hash. If the hash matches an existing record in SQLite, we avoid duplicating the physical file on disk. If it is new, we record its original name, MIME type, file size, timestamp, and storage path in `data/vault.db`. SQLite runs in WAL (Write-Ahead Logging) mode, which allows fast concurrent reads while writes are committed.

3. **Two-way access and Apple formats:**
   The application is designed for two-way use. You can offload media from your phone to free up space, and you can browse or retrieve it later:
   - **Photos (`.heic`, `.dng`):** We preserve the untouched original files so no dynamic range or metadata is lost. Mobile Safari renders HEIC natively in `<img>` tags.
   - **Videos (`.mov`, `.mp4`):** When iOS Safari plays a video, it does not download the entire file first. It sends HTTP range requests (`Range: bytes=start-end`) to read the QuickTime MOOV index atom and stream only the needed sections. C-Vault implements HTTP 206 Partial Content so you can scrub through large videos on your phone over Wi-Fi without waiting for a full download.
   - **Retrieval:** Any file can be viewed, streamed, or saved back to the iOS Camera Roll or Files app with a single tap.

4. **iOS Automation:**
   Apple does not allow web pages to silently scan the Camera Roll in the background. To make backing up easy, C-Vault pairs with an iOS Shortcut. From the native Photos app, you select photos, tap the Share Sheet, and choose "Upload to C-Vault." Once the upload completes, the shortcut prompts you to delete the originals from your device, immediately freeing up phone storage.

---

## Building and Running

### Prerequisites
* GCC or Clang (e.g. MinGW-w64 on Windows, or standard GCC on Linux/macOS)
* GNU Make
* Python 3 (used for the local development server harness)

### Quick Start

1. Compile the C-Vault CGI binary:
   ```bash
   make
   ```
   This compiles `cvault` (or `cvault.exe` on Windows) with vendored SQLite in a few seconds.

2. Start the local server:
   ```bash
   python dev_server.py
   ```
   The script prints your local network IP (e.g., `http://192.168.1.50:8080`). Open that URL in Safari on your iPhone connected to the same Wi-Fi network.

---

## Project Structure

```text
c-vault/
├── src/
│   ├── main.c        # Request router and CGI entry point
│   ├── cgi.c / .h    # CGI environment parsing, query/cookie handlers, multipart streaming
│   ├── db.c / .h     # SQLite initialization, schema migrations, and queries
│   ├── stream.c / .h # HTTP 206 byte-range streaming engine
│   ├── views.c / .h  # Semantic HTML/CSS rendering
│   ├── sqlite3.c     # Vendored SQLite amalgamation
│   └── sqlite3.h
├── data/             # SQLite database (vault.db)
├── uploads/          # Partitioned media storage (uploads/YYYY/MM/)
├── dev_server.py     # Local CGI dev runner
├── Makefile          # Build configuration
├── README.md
└── SPECIFICATION.md  # Detailed technical specifications and implementation tasks
```

---

## Documentation

For the full technical breakdown, schema definitions, route specifications, and the task list for the GitHub project board, see [SPECIFICATION.md](SPECIFICATION.md).
