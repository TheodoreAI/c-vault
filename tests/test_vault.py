#!/usr/bin/env python3
import urllib.request
import urllib.parse
import http.cookiejar
import json
import sys

BASE_URL = "http://127.0.0.1:8080"

cj = http.cookiejar.CookieJar()
opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cj))

print("1. Testing unauthenticated access to /...")
try:
    req = opener.open(f"{BASE_URL}/")
    print("  Final URL after redirect:", req.geturl())
    assert "/login" in req.geturl()
    print("  [PASS] Unauthenticated visitor properly redirected to /login")
except Exception as e:
    print("  [FAIL]", e)
    sys.exit(1)

print("2. Testing login with correct PIN 123456...")
data = urllib.parse.urlencode({"pin": "123456"}).encode()
login_req = urllib.request.Request(f"{BASE_URL}/login", data=data, method="POST")
res = opener.open(login_req)
print(f"  Login status: {res.status}, Final URL: {res.geturl()}")

session_cookie = None
for c in cj:
    if c.name == "cvault_session":
        session_cookie = c.value
assert session_cookie is not None
print(f"  [PASS] Session cookie obtained: {session_cookie[:16]}...")

print("3. Testing authenticated GET /...")
dash = opener.open(f"{BASE_URL}/").read().decode()
assert "<title>Gallery | C-Vault</title>" in dash
print("  [PASS] Gallery dashboard rendered successfully!")

print("4. Testing multipart upload of a mock iPhone photo...")
boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW"
body = (
    f"--{boundary}\r\n"
    'Content-Disposition: form-data; name="files"; filename="IMG_9999.HEIC"\r\n'
    "Content-Type: image/heic\r\n\r\n"
    "Fake HEIC iPhone 4K binary image payload 1234567890\r\n"
    f"--{boundary}--\r\n"
).encode()

up_req = urllib.request.Request(
    f"{BASE_URL}/api/upload",
    data=body,
    headers={
        "Content-Type": f"multipart/form-data; boundary={boundary}",
        "Authorization": f"Bearer {session_cookie}",
    },
    method="POST",
)
up_res = json.loads(urllib.request.urlopen(up_req).read().decode())
print(f"  [PASS] Upload result: {up_res['status']}, Files: {up_res['uploaded_count']}")
uuid = up_res["files"][0]["uuid"]
print(f"  File UUID: {uuid}, Deduplicated: {up_res['files'][0]['deduped']}")
assert up_res["files"][0]["deduped"] is False

print("5. Testing deduplication (uploading exact same content with different filename)...")
body_dup = (
    f"--{boundary}\r\n"
    'Content-Disposition: form-data; name="files"; filename="IMG_9999_copy.HEIC"\r\n'
    "Content-Type: image/heic\r\n\r\n"
    "Fake HEIC iPhone 4K binary image payload 1234567890\r\n"
    f"--{boundary}--\r\n"
).encode()

up_dup_req = urllib.request.Request(
    f"{BASE_URL}/api/upload",
    data=body_dup,
    headers={
        "Content-Type": f"multipart/form-data; boundary={boundary}",
        "Authorization": f"Bearer {session_cookie}",
    },
    method="POST",
)
up_dup_res = json.loads(urllib.request.urlopen(up_dup_req).read().decode())
print(f"  [PASS] Deduplicated upload count: {up_dup_res['deduplicated_count']}")
assert up_dup_res["files"][0]["deduped"] is True

print("6. Testing HTTP 206 Partial Content byte-range video/media streaming...")
range_req = urllib.request.Request(
    f"{BASE_URL}/raw/{uuid}", headers={"Range": "bytes=0-10"}
)
range_res = opener.open(range_req)
print(
    f"  [PASS] Range Status: {range_res.status}, Content-Range: {range_res.headers.get('Content-Range')}"
)
chunk = range_res.read()
print(f"  Read bytes count: {len(chunk)}, Value: {chunk}")
assert len(chunk) == 11
assert range_res.status == 206

print("7. Testing single file detail view...")
view_page = opener.open(f"{BASE_URL}/view/{uuid}").read().decode()
assert "IMG_9999.HEIC" in view_page
print("  [PASS] Single file view page loaded cleanly!")

print("8. Testing metrics endpoint (/api/stats)...")
stats_res = json.loads(opener.open(f"{BASE_URL}/api/stats").read().decode())
print(f"  [PASS] Stats: {stats_res['total_files']} files, {stats_res['photos']} photos")
assert stats_res["total_files"] >= 1

print("\n==========================================")
print("  ALL 8 INTEGRATION TESTS PASSED CLEANLY!  ")
print("==========================================")
