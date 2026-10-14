# C-Vault 🗄️

> A high-performance, self-hosted personal media and file vault written in C, served via CGI, and powered by SQLite.

C-Vault is built following the modern C + CGI + SQLite methodology outlined in [A Modern Web App in C](https://prophitt.me/writing/a-modern-web-app-in-c). It is designed to solve smartphone storage exhaustion (specifically iPhones) by enabling fast, stream-efficient offloading of photos, 4K videos, and documents to your own private storage over Wi-Fi or secure tunnels.

---

## ⚡ Highlights

* **Pure C & Zero Runtimes**: No Node.js, Python frameworks, Docker, or heavy background daemons required.
* **Low Memory Footprint**: Chunked 64KB multipart streaming directly to disk from `stdin` handles multi-gigabyte video uploads with <16MB peak RAM.
* **HTTP 206 Byte-Range Video Streaming**: Smooth scrubbing and playback of 4K `.mp4`/`.mov` videos directly inside mobile Safari.
* **Embedded SQLite**: Single-file database with WAL mode and FTS5 full-text search.
* **SHA-256 Deduplication**: Identical media files are recognized instantly to save disk space.
* **1-Tap iOS Shortcut**: Offload media directly from the iOS Share Sheet and prompt to delete originals from the iPhone to reclaim storage.

---

## 📖 Documentation & Specification

See [SPECIFICATION.md](SPECIFICATION.md) for the complete technical specification, database schema, API routes, and the GitHub project board implementation tasks.

