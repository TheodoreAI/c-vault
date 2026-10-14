# C-Vault: Production Deployment & Remote Access Guide

This guide covers running C-Vault as a permanent background service and connecting securely from your iPhone when away from home Wi-Fi.

---

## 1. Running on Windows (Home PC)

### Option A: Local Development / Background Server
To run C-Vault in the background on your Windows machine:

```powershell
# In PowerShell inside the c-vault directory:
Start-Process python -ArgumentList "dev_server.py" -WindowStyle Hidden
```

To stop it later:
```powershell
Get-Process python | Where-Object { $_.CommandLine -like "*dev_server.py*" } | Stop-Process
```

### Option B: Windows Task Scheduler (Start at Login)
1. Open **Task Scheduler** in Windows.
2. Click **Create Basic Task...** $\rightarrow$ name it `C-Vault`.
3. Trigger: **When I log on**.
4. Action: **Start a program**.
   * Program: `python.exe`
   * Arguments: `dev_server.py`
   * Start in: `C:\Users\mateo\Documents\c\c-vault`
5. Finish. C-Vault will now run in the background whenever your PC turns on.

---

## 2. Running on Linux or OpenBSD (Home Server / Mini-PC)

Because C-Vault uses standard C and CGI 1.1, it can run on any Unix system with minimal overhead.

### Building
```bash
make clean
make
```

### OpenBSD `httpd` Configuration (`/etc/httpd.conf`)
OpenBSD serves CGI from a chroot by default (`/var/www`):

```text
server "vault.local" {
    listen on * port 80
    root "/"

    location "/uploads/*" {
        root "/htdocs"
    }

    location "/*" {
        fastcgi socket "/run/slowcgi.sock"
        root "/cgi-bin/cvault"
    }
}
```

### Lighttpd Configuration (`/etc/lighttpd/lighttpd.conf`)
Lighttpd has native support for classic CGI:

```text
server.modules += ( "mod_cgi", "mod_setenv" )
server.document-root = "/var/www/c-vault"
server.port = 8080

$HTTP["url"] =~ "^/" {
    cgi.assign = ( "" => "/var/www/c-vault/cvault" )
}
```

---

## 3. Remote Access Away from Home (Tailscale)

To access your vault and offload photos from your iPhone when you are away from home without port-forwarding or exposing your home IP to the public internet:

### Step 1: Install Tailscale
1. Install [Tailscale](https://tailscale.com) on your computer (Windows, Linux, or Mac).
2. Install the **Tailscale** app on your iPhone from the App Store.
3. Sign in with the same account on both devices.

### Step 2: Connect
1. Turn on Tailscale on your iPhone.
2. Note your computer’s Tailscale IP or MagicDNS hostname (e.g. `100.x.y.z` or `my-pc.tailnet.ts.net`).
3. In iPhone Safari, navigate to:
   ```text
   http://100.x.y.z:8080/
   ```
4. You now have an encrypted, direct WireGuard tunnel between your phone and your vault anywhere in the world. You can upload 4K videos from mobile data or hotel Wi-Fi just as if you were on your home network.
