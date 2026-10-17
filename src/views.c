#include "views.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void views_format_bytes(int64_t bytes, char *out, size_t out_max) {
    if (bytes < 1024) {
        snprintf(out, out_max, "%lld B", (long long)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(out, out_max, "%.1f KB", (double)bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(out, out_max, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    } else {
        snprintf(out, out_max, "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

static void print_head(const char *title) {
    printf("<!DOCTYPE html>\n");
    printf("<html lang=\"en\">\n");
    printf("<head>\n");
    printf("  <meta charset=\"utf-8\">\n");
    printf("  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, viewport-fit=cover\">\n");
    printf("  <meta name=\"apple-mobile-web-app-capable\" content=\"yes\">\n");
    printf("  <meta name=\"apple-mobile-web-app-status-bar-style\" content=\"black-translucent\">\n");
    printf("  <title>%s | C-Vault</title>\n", title ? title : "Media Vault");
    printf("  <style>\n");
    printf("    :root {\n");
    printf("      --bg: #0f1117;\n");
    printf("      --card-bg: #1a1d27;\n");
    printf("      --card-border: #2b3042;\n");
    printf("      --fg: #e6edf3;\n");
    printf("      --fg-muted: #8b949e;\n");
    printf("      --accent: #3b82f6;\n");
    printf("      --accent-hover: #2563eb;\n");
    printf("      --danger: #ef4444;\n");
    printf("      --success: #10b981;\n");
    printf("    }\n");
    printf("    @media (prefers-color-scheme: light) {\n");
    printf("      :root {\n");
    printf("        --bg: #f8fafc;\n");
    printf("        --card-bg: #ffffff;\n");
    printf("        --card-border: #e2e8f0;\n");
    printf("        --fg: #0f172a;\n");
    printf("        --fg-muted: #64748b;\n");
    printf("        --accent: #2563eb;\n");
    printf("        --accent-hover: #1d4ed8;\n");
    printf("      }\n");
    printf("    }\n");
    printf("    * { box-sizing: border-box; margin: 0; padding: 0; }\n");
    printf("    body {\n");
    printf("      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;\n");
    printf("      background: var(--bg);\n");
    printf("      color: var(--fg);\n");
    printf("      line-height: 1.5;\n");
    printf("      padding-bottom: 80px;\n");
    printf("      -webkit-font-smoothing: antialiased;\n");
    printf("    }\n");
    printf("    header {\n");
    printf("      background: var(--card-bg);\n");
    printf("      border-bottom: 1px solid var(--card-border);\n");
    printf("      position: sticky; top: 0; z-index: 50;\n");
    printf("      padding: 12px 16px;\n");
    printf("    }\n");
    printf("    .nav-container {\n");
    printf("      max-width: 1200px; margin: 0 auto;\n");
    printf("      display: flex; justify-content: space-between; align-items: center;\n");
    printf("    }\n");
    printf("    .brand {\n");
    printf("      font-size: 1.25rem; font-weight: 700; text-decoration: none; color: var(--fg);\n");
    printf("      display: flex; align-items: center; gap: 8px;\n");
    printf("    }\n");
    printf("    .brand span { color: var(--accent); }\n");
    printf("    .nav-stats {\n");
    printf("      font-size: 0.85rem; color: var(--fg-muted);\n");
    printf("    }\n");
    printf("    .container { max-width: 1200px; margin: 0 auto; padding: 16px; }\n");
    printf("    .upload-card {\n");
    printf("      background: var(--card-bg);\n");
    printf("      border: 2px dashed var(--card-border);\n");
    printf("      border-radius: 12px;\n");
    printf("      padding: 24px 16px;\n");
    printf("      text-align: center;\n");
    printf("      margin-bottom: 20px;\n");
    printf("      transition: border-color 0.2s;\n");
    printf("    }\n");
    printf("    .upload-card:hover { border-color: var(--accent); }\n");
    printf("    .btn {\n");
    printf("      display: inline-flex; align-items: center; justify-content: center;\n");
    printf("      padding: 10px 20px; border-radius: 8px; font-weight: 600; font-size: 0.95rem;\n");
    printf("      text-decoration: none; cursor: pointer; border: none; transition: background 0.15s;\n");
    printf("    }\n");
    printf("    .btn-primary { background: var(--accent); color: #fff; }\n");
    printf("    .btn-primary:hover { background: var(--accent-hover); }\n");
    printf("    .btn-secondary { background: var(--card-border); color: var(--fg); }\n");
    printf("    .btn-danger { background: var(--danger); color: #fff; }\n");
    printf("    .progress-bar-container {\n");
    printf("      width: 100%%; background: var(--card-border); border-radius: 6px;\n");
    printf("      height: 8px; margin-top: 12px; overflow: hidden; display: none;\n");
    printf("    }\n");
    printf("    .progress-bar {\n");
    printf("      height: 100%%; width: 0%%; background: var(--accent); transition: width 0.1s;\n");
    printf("    }\n");
    printf("    .filters {\n");
    printf("      display: flex; gap: 8px; overflow-x: auto; padding-bottom: 8px;\n");
    printf("      margin-bottom: 16px; scrollbar-width: none;\n");
    printf("    }\n");
    printf("    .filter-tab {\n");
    printf("      padding: 6px 14px; border-radius: 20px; font-size: 0.85rem; font-weight: 500;\n");
    printf("      text-decoration: none; color: var(--fg-muted); background: var(--card-bg);\n");
    printf("      border: 1px solid var(--card-border); white-space: nowrap;\n");
    printf("    }\n");
    printf("    .filter-tab.active { background: var(--accent); color: #fff; border-color: var(--accent); }\n");
    printf("    .search-bar {\n");
    printf("      display: flex; gap: 8px; margin-bottom: 16px;\n");
    printf("    }\n");
    printf("    .search-input {\n");
    printf("      flex: 1; padding: 8px 14px; border-radius: 8px;\n");
    printf("      border: 1px solid var(--card-border); background: var(--card-bg);\n");
    printf("      color: var(--fg); font-size: 0.9rem;\n");
    printf("    }\n");
    printf("    .media-grid {\n");
    printf("      display: grid;\n");
    printf("      grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));\n");
    printf("      gap: 12px;\n");
    printf("    }\n");
    printf("    @media (max-width: 480px) {\n");
    printf("      .media-grid { grid-template-columns: repeat(2, 1fr); gap: 8px; }\n");
    printf("    }\n");
    printf("    .media-card {\n");
    printf("      background: var(--card-bg);\n");
    printf("      border: 1px solid var(--card-border);\n");
    printf("      border-radius: 10px; overflow: hidden;\n");
    printf("      display: flex; flex-direction: column;\n");
    printf("      position: relative;\n");
    printf("    }\n");
    printf("    .media-thumb-link {\n");
    printf("      aspect-ratio: 1/1; background: #000; display: block; overflow: hidden;\n");
    printf("      position: relative;\n");
    printf("    }\n");
    printf("    .media-thumb {\n");
    printf("      width: 100%%; height: 100%%; object-fit: cover;\n");
    printf("      transition: transform 0.2s;\n");
    printf("    }\n");
    printf("    .media-card:hover .media-thumb { transform: scale(1.03); }\n");
    printf("    .badge-video {\n");
    printf("      position: absolute; bottom: 6px; right: 6px;\n");
    printf("      background: rgba(0,0,0,0.7); color: #fff; padding: 2px 6px;\n");
    printf("      border-radius: 4px; font-size: 0.75rem; font-weight: 600;\n");
    printf("    }\n");
    printf("    .badge-star {\n");
    printf("      position: absolute; top: 6px; left: 6px;\n");
    printf("      background: rgba(0,0,0,0.6); color: #f59e0b; padding: 2px 5px;\n");
    printf("      border-radius: 4px; font-size: 0.8rem;\n");
    printf("    }\n");
    printf("    .media-meta {\n");
    printf("      padding: 8px 10px;\n");
    printf("      display: flex; flex-direction: column; gap: 4px;\n");
    printf("    }\n");
    printf("    .media-title {\n");
    printf("      font-size: 0.85rem; font-weight: 600; white-space: nowrap;\n");
    printf("      overflow: hidden; text-overflow: ellipsis; color: var(--fg);\n");
    printf("    }\n");
    printf("    .media-subtitle {\n");
    printf("      font-size: 0.75rem; color: var(--fg-muted);\n");
    printf("      display: flex; justify-content: space-between;\n");
    printf("    }\n");
    printf("    .media-actions {\n");
    printf("      display: flex; justify-content: space-between; align-items: center;\n");
    printf("      margin-top: 4px; padding-top: 6px; border-top: 1px solid var(--card-border);\n");
    printf("    }\n");
    printf("    .action-btn {\n");
    printf("      background: none; border: none; color: var(--fg-muted); cursor: pointer;\n");
    printf("      font-size: 0.8rem; text-decoration: none; padding: 2px 4px;\n");
    printf("    }\n");
    printf("    .action-btn:hover { color: var(--fg); }\n");
    printf("    .login-box {\n");
    printf("      max-width: 360px; margin: 80px auto; padding: 24px;\n");
    printf("      background: var(--card-bg); border: 1px solid var(--card-border);\n");
    printf("      border-radius: 12px; text-align: center;\n");
    printf("    }\n");
    printf("    .pin-input {\n");
    printf("      width: 100%%; padding: 12px; font-size: 1.5rem; text-align: center;\n");
    printf("      border: 1px solid var(--card-border); border-radius: 8px;\n");
    printf("      background: var(--bg); color: var(--fg); letter-spacing: 4px; margin: 16px 0;\n");
    printf("    }\n");
    printf("    .detail-container {\n");
    printf("      max-width: 900px; margin: 20px auto; background: var(--card-bg);\n");
    printf("      border: 1px solid var(--card-border); border-radius: 12px; overflow: hidden;\n");
    printf("    }\n");
    printf("    .detail-player {\n");
    printf("      background: #000; width: 100%%; max-height: 70vh; display: flex;\n");
    printf("      justify-content: center; align-items: center;\n");
    printf("    }\n");
    printf("    .detail-player img, .detail-player video {\n");
    printf("      max-width: 100%%; max-height: 70vh; object-fit: contain;\n");
    printf("    }\n");
    printf("    .detail-info { padding: 20px; }\n");
    printf("  </style>\n");
    printf("</head>\n");
    printf("<body>\n");
}

static void print_footer(void) {
    printf("</body>\n");
    printf("</html>\n");
}

void views_render_gallery(const cgi_request *req, const file_record *files,
                          int file_count, const vault_stats *stats,
                          const char *active_filter, const char *search_query) {
    (void)req;
    print_head("Gallery");

    char total_size_str[32];
    views_format_bytes(stats->total_bytes, total_size_str, sizeof(total_size_str));

    /* Header */
    printf("<header>\n");
    printf("  <div class=\"nav-container\">\n");
    printf("    <a href=\"/\" class=\"brand\"><span>C</span>-Vault</a>\n");
    printf("    <div class=\"nav-stats\">%lld items &bull; %s</div>\n",
           (long long)stats->total_files, total_size_str);
    printf("  </div>\n");
    printf("</header>\n");

    printf("<div class=\"container\">\n");

    /* Mobile Upload Zone */
    printf("  <div class=\"upload-card\">\n");
    printf("    <form id=\"uploadForm\" method=\"POST\" action=\"/upload\" enctype=\"multipart/form-data\">\n");
    printf("      <input type=\"file\" id=\"filePicker\" name=\"files\" multiple accept=\"image/*,video/*,*/*\" style=\"display:none;\">\n");
    printf("      <button type=\"button\" class=\"btn btn-primary\" onclick=\"document.getElementById('filePicker').click();\">\n");
    printf("        + Upload Photos &amp; Videos\n");
    printf("      </button>\n");
    printf("      <p style=\"font-size:0.8rem; color:var(--fg-muted); margin-top:8px;\">Pick from Camera Roll or drag files here</p>\n");
    printf("    </form>\n");
    printf("    <div id=\"progressContainer\" class=\"progress-bar-container\">\n");
    printf("      <div id=\"progressBar\" class=\"progress-bar\"></div>\n");
    printf("    </div>\n");
    printf("    <div id=\"uploadStatus\" style=\"font-size:0.85rem; margin-top:6px; color:var(--fg-muted); display:none;\"></div>\n");
    printf("  </div>\n");

    /* Search Bar */
    char *escaped_q = search_query ? cgi_html_escape(search_query) : NULL;
    printf("  <form class=\"search-bar\" method=\"GET\" action=\"/\">\n");
    printf("    <input type=\"text\" name=\"q\" class=\"search-input\" placeholder=\"Search filenames...\" value=\"%s\">\n",
           escaped_q ? escaped_q : "");
    printf("    <button type=\"submit\" class=\"btn btn-secondary\">Search</button>\n");
    if (escaped_q && *escaped_q) {
        printf("    <a href=\"/\" class=\"btn btn-secondary\">Clear</a>\n");
    }
    printf("  </form>\n");
    if (escaped_q) free(escaped_q);

    /* Filter Tabs */
    const char *af = active_filter ? active_filter : "all";
    printf("  <div class=\"filters\">\n");
    printf("    <a href=\"/\" class=\"filter-tab %s\">All (%lld)</a>\n",
           strcmp(af, "all") == 0 ? "active" : "", (long long)stats->total_files);
    printf("    <a href=\"/?filter=photo\" class=\"filter-tab %s\">Photos (%lld)</a>\n",
           strcmp(af, "photo") == 0 ? "active" : "", (long long)stats->photos_count);
    printf("    <a href=\"/?filter=video\" class=\"filter-tab %s\">Videos (%lld)</a>\n",
           strcmp(af, "video") == 0 ? "active" : "", (long long)stats->videos_count);
    printf("    <a href=\"/?filter=document\" class=\"filter-tab %s\">Docs (%lld)</a>\n",
           strcmp(af, "document") == 0 ? "active" : "", (long long)stats->docs_count);
    printf("    <a href=\"/?filter=favorites\" class=\"filter-tab %s\">Favorites</a>\n",
           strcmp(af, "favorites") == 0 ? "active" : "");
    printf("    <a href=\"/?filter=trash\" class=\"filter-tab %s\">Trash</a>\n",
           strcmp(af, "trash") == 0 ? "active" : "");
    printf("  </div>\n");

    /* Media Grid */
    if (file_count == 0) {
        printf("  <div style=\"text-align:center; padding:48px 16px; color:var(--fg-muted);\">\n");
        printf("    <p style=\"font-size:1.1rem; font-weight:500;\">No media files found</p>\n");
        printf("    <p style=\"font-size:0.85rem; margin-top:4px;\">Upload photos or 4K videos from your iPhone to get started.</p>\n");
        printf("  </div>\n");
    } else {
        printf("  <div class=\"media-grid\">\n");
        for (int i = 0; i < file_count; i++) {
            const file_record *f = &files[i];
            char sz[32];
            views_format_bytes(f->file_size, sz, sizeof(sz));
            char *escaped_name = cgi_html_escape(f->original_filename);

            printf("    <div class=\"media-card\">\n");
            printf("      <a href=\"/view/%s\" class=\"media-thumb-link\">\n", f->uuid);

            if (strcmp(f->media_type, "photo") == 0) {
                printf("        <img src=\"/raw/%s\" class=\"media-thumb\" loading=\"lazy\" alt=\"%s\">\n",
                       f->uuid, escaped_name ? escaped_name : "");
            } else if (strcmp(f->media_type, "video") == 0) {
                printf("        <video src=\"/raw/%s#t=0.5\" class=\"media-thumb\" preload=\"metadata\" muted playsinline></video>\n", f->uuid);
                printf("        <div class=\"badge-video\">VIDEO</div>\n");
            } else {
                printf("        <div style=\"display:flex; height:100%%; align-items:center; justify-content:center; color:var(--fg-muted); font-weight:bold;\">DOC</div>\n");
            }

            if (f->is_favorite) {
                printf("        <div class=\"badge-star\">&starf;</div>\n");
            }
            printf("      </a>\n");

            printf("      <div class=\"media-meta\">\n");
            printf("        <div class=\"media-title\" title=\"%s\">%s</div>\n",
                   escaped_name ? escaped_name : "", escaped_name ? escaped_name : "");
            printf("        <div class=\"media-subtitle\">\n");
            printf("          <span>%s</span>\n", sz);
            printf("          <span>%s</span>\n", f->created_at[0] ? f->created_at : "");
            printf("        </div>\n");
            printf("        <div class=\"media-actions\">\n");
            printf("          <a href=\"/download/%s\" class=\"action-btn\" title=\"Download original\">Download</a>\n", f->uuid);
            printf("          <form method=\"POST\" action=\"/favorite/%s\" style=\"display:inline;\">\n", f->uuid);
            printf("            <button type=\"submit\" class=\"action-btn\">%s</button>\n", f->is_favorite ? "&starf; Unstar" : "&star; Star");
            printf("          </form>\n");

            if (f->is_deleted) {
                printf("          <form method=\"POST\" action=\"/restore/%s\" style=\"display:inline;\">\n", f->uuid);
                printf("            <button type=\"submit\" class=\"action-btn\" style=\"color:var(--success);\">Restore</button>\n");
                printf("          </form>\n");
            } else {
                printf("          <form method=\"POST\" action=\"/delete/%s\" style=\"display:inline;\" onsubmit=\"return confirm('Move to trash?');\">\n", f->uuid);
                printf("            <button type=\"submit\" class=\"action-btn\" style=\"color:var(--danger);\">Trash</button>\n");
                printf("          </form>\n");
            }

            printf("        </div>\n");
            printf("      </div>\n");
            printf("    </div>\n");

            if (escaped_name) free(escaped_name);
        }
        printf("  </div>\n");
    }

    printf("</div>\n");

    /* Live Progress JavaScript for iPhone Safari Batch Uploads */
    printf("<script>\n");
    printf("  const picker = document.getElementById('filePicker');\n");
    printf("  const progContainer = document.getElementById('progressContainer');\n");
    printf("  const progBar = document.getElementById('progressBar');\n");
    printf("  const statusTxt = document.getElementById('uploadStatus');\n");
    printf("  picker.addEventListener('change', function() {\n");
    printf("    const files = Array.from(this.files);\n");
    printf("    if (!files.length) return;\n");
    printf("    progContainer.style.display = 'block';\n");
    printf("    statusTxt.style.display = 'block';\n");
    printf("    const totalBytes = files.reduce((acc, f) => acc + f.size, 0);\n");
    printf("    const loadedPerFile = new Array(files.length).fill(0);\n");
    printf("    const startTime = performance.now();\n");
    printf("    let nextIdx = 0, completed = 0, failed = 0;\n");
    printf("    const CONCURRENCY = Math.min(4, files.length);\n");
    printf("    function updateProgress() {\n");
    printf("      const loaded = loadedPerFile.reduce((a, b) => a + b, 0);\n");
    printf("      const pct = totalBytes > 0 ? Math.min(100, Math.round((loaded / totalBytes) * 100)) : 100;\n");
    printf("      const elapsedSec = Math.max(0.1, (performance.now() - startTime) / 1000);\n");
    printf("      const mbps = ((loaded / (1024 * 1024)) / elapsedSec).toFixed(1);\n");
    printf("      progBar.style.width = pct + '%%';\n");
    printf("      statusTxt.textContent = 'Uploading (' + completed + '/' + files.length + '): ' + pct + '%% (' + mbps + ' MB/s)';\n");
    printf("    }\n");
    printf("    function runWorker() {\n");
    printf("      if (nextIdx >= files.length) return;\n");
    printf("      const idx = nextIdx++;\n");
    printf("      const fd = new FormData();\n");
    printf("      fd.append('files', files[idx]);\n");
    printf("      const xhr = new XMLHttpRequest();\n");
    printf("      xhr.open('POST', '/upload', true);\n");
    printf("      xhr.upload.onprogress = function(e) {\n");
    printf("        if (e.lengthComputable) {\n");
    printf("          loadedPerFile[idx] = e.loaded;\n");
    printf("          updateProgress();\n");
    printf("        }\n");
    printf("      };\n");
    printf("      xhr.onload = function() {\n");
    printf("        loadedPerFile[idx] = files[idx].size;\n");
    printf("        completed++;\n");
    printf("        updateProgress();\n");
    printf("        if (completed + failed === files.length) {\n");
    printf("          statusTxt.textContent = 'Upload complete! Refreshing...';\n");
    printf("          setTimeout(() => { window.location.reload(); }, 400);\n");
    printf("        } else {\n");
    printf("          runWorker();\n");
    printf("        }\n");
    printf("      };\n");
    printf("      xhr.onerror = function() {\n");
    printf("        failed++;\n");
    printf("        if (completed + failed === files.length) {\n");
    printf("          statusTxt.textContent = 'Finished with ' + failed + ' error(s).';\n");
    printf("        } else {\n");
    printf("          runWorker();\n");
    printf("        }\n");
    printf("      };\n");
    printf("      xhr.send(fd);\n");
    printf("    }\n");
    printf("    for (let w = 0; w < CONCURRENCY; w++) runWorker();\n");
    printf("  });\n");
    printf("</script>\n");

    print_footer();
}

void views_render_view(const cgi_request *req, const file_record *file) {
    (void)req;
    char *escaped_name = cgi_html_escape(file->original_filename);
    print_head(escaped_name);

    printf("<header>\n");
    printf("  <div class=\"nav-container\">\n");
    printf("    <a href=\"/\" class=\"brand\">&larr; Back to Gallery</a>\n");
    printf("    <a href=\"/download/%s\" class=\"btn btn-primary\" style=\"padding:6px 14px; font-size:0.85rem;\">Download</a>\n", file->uuid);
    printf("  </div>\n");
    printf("</header>\n");

    printf("<div class=\"detail-container\">\n");
    printf("  <div class=\"detail-player\">\n");

    if (strcmp(file->media_type, "photo") == 0) {
        printf("    <img src=\"/raw/%s\" alt=\"%s\">\n", file->uuid, escaped_name ? escaped_name : "");
    } else if (strcmp(file->media_type, "video") == 0) {
        printf("    <video src=\"/raw/%s\" controls playsinline autoplay style=\"width:100%%;\"></video>\n", file->uuid);
    } else {
        printf("    <div style=\"padding:48px; text-align:center; color:var(--fg-muted);\">\n");
        printf("      <p style=\"font-size:1.2rem;\">%s</p>\n", escaped_name ? escaped_name : "Document");
        printf("      <p style=\"margin-top:12px;\"><a href=\"/download/%s\" class=\"btn btn-primary\">Download to View</a></p>\n", file->uuid);
        printf("    </div>\n");
    }
    printf("  </div>\n");

    char sz[32];
    views_format_bytes(file->file_size, sz, sizeof(sz));

    printf("  <div class=\"detail-info\">\n");
    printf("    <h2 style=\"font-size:1.2rem; margin-bottom:8px;\">%s</h2>\n", escaped_name ? escaped_name : "");
    printf("    <p style=\"font-size:0.85rem; color:var(--fg-muted); margin-bottom:12px;\">\n");
    printf("      Type: <strong>%s</strong> &bull; Size: <strong>%s</strong> &bull; Uploaded: <strong>%s</strong>\n",
           file->mime_type, sz, file->uploaded_at);
    printf("    </p>\n");
    printf("    <p style=\"font-size:0.8rem; font-family:monospace; color:var(--fg-muted); word-break:break-all; background:var(--bg); padding:8px; border-radius:6px;\">\n");
    printf("      SHA-256: %s\n", file->sha256);
    printf("    </p>\n");
    printf("    <div style=\"display:flex; gap:12px; margin-top:20px;\">\n");
    printf("      <a href=\"/download/%s\" class=\"btn btn-primary\">Download Original</a>\n", file->uuid);
    printf("      <form method=\"POST\" action=\"/delete/%s\" onsubmit=\"return confirm('Move file to trash?');\">\n", file->uuid);
    printf("        <button type=\"submit\" class=\"btn btn-danger\">Delete</button>\n");
    printf("      </form>\n");
    printf("    </div>\n");
    printf("  </div>\n");
    printf("</div>\n");

    if (escaped_name) free(escaped_name);
    print_footer();
}

void views_render_login(const char *error_message) {
    print_head("Unlock Vault");

    printf("<div class=\"login-box\">\n");
    printf("  <h1 style=\"font-size:1.5rem; margin-bottom:6px;\"><span>C</span>-Vault</h1>\n");
    printf("  <p style=\"font-size:0.85rem; color:var(--fg-muted);\">Enter PIN to access your personal storage</p>\n");

    if (error_message && *error_message) {
        printf("  <p style=\"color:var(--danger); font-size:0.85rem; margin-top:12px;\">%s</p>\n", error_message);
    }

    printf("  <form method=\"POST\" action=\"/login\">\n");
    printf("    <input type=\"password\" name=\"pin\" class=\"pin-input\" pattern=\"[0-9]*\" inputmode=\"numeric\" maxlength=\"12\" placeholder=\"&bull;&bull;&bull;&bull;&bull;&bull;\" autofocus required>\n");
    printf("    <button type=\"submit\" class=\"btn btn-primary\" style=\"width:100%%; padding:12px;\">Unlock Vault</button>\n");
    printf("  </form>\n");
    printf("</div>\n");

    print_footer();
}
