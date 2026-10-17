# iOS Shortcut Automation Guide: Offloading Photos to C-Vault

This guide walks through configuring a native iOS Shortcut that uploads photos and videos directly from the Apple Photos **Share Sheet** to C-Vault, and prompts you to delete the originals from your iPhone to immediately reclaim storage.

---

## Why Use an iOS Shortcut?

Apple's sandbox intentionally prevents web applications running in Safari from silently scanning the Camera Roll in the background. Rather than fighting operating system restrictions or installing heavy third-party sync apps that require background location permissions, a native iOS Shortcut gives you direct, one-tap control:

1. You pick 10, 50, or 100 photos and 4K videos in Apple Photos.
2. You tap **Share** $\rightarrow$ **Upload to C-Vault**.
3. The shortcut uploads the original binary files (including `.heic`, `.mov`, and `.dng`) to your C-Vault server.
4. iOS asks: *"Delete X photos from your iPhone?"*
5. You confirm, and your phone storage is freed up instantly while the untouched originals remain safely stored in your vault.

---

## Step-by-Step Shortcut Setup

### Step 1: Create a New Shortcut
1. Open the **Shortcuts** app on your iPhone.
2. Tap the **`+`** icon in the top-right corner to create a new shortcut.
3. Tap the title at the top (e.g. *New Shortcut*) and rename it to **Upload to C-Vault**.
4. Choose an icon (a vault or cloud icon with a blue background works nicely).

### Step 2: Enable the iOS Share Sheet
1. Tap the **Information (i)** button at the bottom of the screen.
2. Toggle **Show in Share Sheet** to **ON**.
3. Under *Receive*, tap **Any** and uncheck everything except **Images** and **Media** (or leave Any checked so it works with PDFs and documents as well).

### Step 3: Add the Upload Actions
Now build the action flow:

```text
[Receive Images and Media from Share Sheet]
   │
   ▼
[Repeat with Each item in Shortcut Input]
   │
   ▼
   ┌────────────────────────────────────────────────────────┐
   │ Action: Get Contents of URL                            │
   │ • URL: http://<YOUR_SERVER_IP>:8080/api/upload         │
   │ • Method: POST                                         │
   │ • Headers:                                             │
   │     Authorization : Bearer <YOUR_SESSION_TOKEN>        │
   │ • Request Body: Form                                   │
   │     Key   : files                                      │
   │     Type  : File                                       │
   │     Value : Repeat Item                                │
   └────────────────────────────────────────────────────────┘
   │
   ▼
[End Repeat]
   │
   ▼
[Delete Photos: Shortcut Input]
   │
   ▼
[Show Notification: "Photos backed up to C-Vault and storage freed."]
```

> **How to get your Bearer Token:**
> Log into C-Vault in your browser using your PIN. Open your browser developer tools or inspect cookies to find `cvault_session`, or use your configured secret PIN if bearer PIN authorization is enabled.

---

## Nightly Automation (Optional)

If you prefer your phone to back up automatically while you sleep:

1. In the Shortcuts app, tap the **Automation** tab at the bottom.
2. Tap **Create Personal Automation**.
3. Choose **Charger** $\rightarrow$ select **Is Connected**.
4. (Optional) Add a condition: **Network** $\rightarrow$ **Wi-Fi** $\rightarrow$ select your home Wi-Fi network.
5. Set the action to:
   - **Find Photos** where *Date Added is in the last 1 day*.
   - Run the **Upload to C-Vault** shortcut on those photos.
6. Toggle **Ask Before Running** to **OFF** (or leave it ON if you want to confirm before photos are deleted).

---

## Verifying Uploads

Once uploaded:
* Open Safari on your iPhone and go to `http://<YOUR_SERVER_IP>:8080/`.
* Your photos and videos will appear in the gallery grid.
* Tapping any photo displays the full-resolution image.
* Tapping a 4K video streams it inline with native scrubbing via C-Vault's HTTP 206 byte-range engine.

