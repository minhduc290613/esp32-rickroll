# Rick Roll Captive Portal — ESP32-C3 + PlatformIO

[Tiếng Việt](README.md) | [English](README.en.md)

![Platform](https://img.shields.io/badge/platform-ESP32--C3-blue?logo=espressif&logoColor=white)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino&logoColor=white)
![Build Tool](https://img.shields.io/badge/build-PlatformIO-orange?logo=platformio&logoColor=white)
![Filesystem](https://img.shields.io/badge/filesystem-LittleFS-9cf)
![License](https://img.shields.io/badge/license-MIT-green)
![Status](https://img.shields.io/badge/status-for%20pranks%20only-ff69b4)

Flash directly from your browser—no installation required: **[→ Web Installer](https://git.tino.io.vn/esp32-rickroll/)**


## Project structure
```
rickroll-portal/
├── .github/workflows/build-firmware.yml   # automatically builds .bin files on new commits
├── docs/                                  # firmware flashing website (GitHub Pages)
│   ├── index.html
│   ├── manifest.json
│   ├── firmware/                          # auto-generated .bin files; do not edit manually
│   └── README.md
├── platformio.ini
├── load_env.py        # reads environment variables during build and embeds them in firmware
├── .env.example        # environment variable template; copy to .env
├── .gitignore
├── partitions.csv
└── src/main.cpp
```

## Configure the SSID and admin account with build-time environment variables
Instead of editing the code directly, values are read from environment variables
during the build via `load_env.py`:

| Variable     | Description                         | Required?                                      |
|--------------|-------------------------------------|------------------------------------------------|
| `AP_SSID`    | Name of the broadcast WiFi network | No; defaults to `Free WiFi`                    |
| `ADMIN_USER` | Login account for `/admin`          | No; defaults to `admin`                        |
| `ADMIN_PASS` | Login password for `/admin`         | **Yes** — the build fails if missing, so you don't forget to change the default password |

### Recommended: use a `.env` file (also works with the VS Code Upload button)
`load_env.py` automatically reads the **`.env`** file next to `platformio.ini`.
There is no need to export variables in a terminal:

1. A `.env` file is already included in the project with placeholder values.
2. Open `.env` and replace the value on the `ADMIN_PASS=...` line with your password.
3. Click **Upload** as usual (from either the CLI or the VS Code button).

```ini
# .env
AP_SSID=Free WiFi
ADMIN_USER=admin
ADMIN_PASS=your-private-password
```

⚠️ This is recommended over `export`/`$env:` because when you click Upload in
VS Code (PlatformIO extension), the build runs in a **separate task/terminal**
and does not inherit environment variables exported in another terminal.
This can lead to "I set it, but it still says it's missing" errors. Reading
the `.env` file directly avoids that problem.

### Alternative: export environment variables (if you always run `pio` from the CLI)
**Linux/macOS:**
```bash
export AP_SSID="Free WiFi"
export ADMIN_USER="admin"
export ADMIN_PASS="your-private-password"
pio run -t upload
```
**Windows PowerShell:**
```powershell
$env:ADMIN_PASS="your-private-password"
pio run -t upload
```
Actual environment variables take precedence over values in `.env` if both exist.

⚠️ These values are **only used for the initial build/flash** (when nothing has
been saved yet). After that, changing the SSID or admin account in `/admin`
saves the values to flash memory (NVS/Preferences) and takes effect immediately,
without rebuilding.

## Installation
1. Open this folder in VS Code with the **PlatformIO** extension installed.
2. Set the environment variables as described above (at least `ADMIN_PASS`).
3. Click **Upload** (the right-arrow button) or run `pio run -t upload`.
   If the board is not detected, hold the **BOOT** button while connecting USB.

## Change the SSID or admin account after setup
Go to `http://192.168.4.1/admin` (sign in with the current username and
password). Use **"🔒 Admin Account"** and the **"Displayed network name (SSID)"**
field in the first section. After saving:
- The SSID changes immediately (the AP restarts automatically; no power cycle needed).
- After changing the admin account or password, you must sign in to `/admin`
  again with the new credentials—the browser will prompt you immediately after saving.

## First-time setup (to play video offline)
1. After flashing, the device broadcasts a WiFi network named according to `AP_SSID`
   (or "Free WiFi").
2. Connect a phone or computer to that network and open `http://192.168.4.1/admin`.
3. In **"Real WiFi"**, enter your home WiFi SSID and password, then paste a
   **direct MP4 video link** (not a youtube.com link—YouTube does not allow
   direct MP4 downloads, so host the MP4 elsewhere, such as GitHub raw or your
   own hosting).
4. Click **"Save all"**, then **"Download / reload video now"**. The ESP32
   temporarily connects to your real WiFi to download the file to flash memory,
   then returns to broadcasting only the prank AP. No internet connection is
   needed after that.

## User experience
1. A target connects to the fake WiFi network and is automatically shown a
   public-WiFi-style login page (like a cafe): enter a name and wait through a
   5-second ad countdown.
2. Click **"Connect now"** to open the full-screen Rick Roll video, which loops continuously.

## Admin settings
- Fake network SSID, login-page caption, and gradient background color.
- Admin username and password.
- Real WiFi credentials and video link for downloading the video again at any time.
- Manually upload an MP4 instead of downloading it over the network.
- Delete the current video.
- Update firmware over OTA, either from a URL or by manually uploading a `.bin` file.

## Flash from the web (no PlatformIO required)
Whenever a commit to the `main` branch changes `src/`, `platformio.ini`,
`partitions.csv`, or `load_env.py`, the GitHub Actions bot (`build-firmware.yml`) automatically:
1. Builds the firmware with **non-secret default** environment values
   (`admin` / `changeme123` / `Free WiFi`).
2. Places `firmware.bin`, `bootloader.bin`, `partitions.bin`, and `boot_app0.bin`
   in `docs/firmware/`.
3. Updates `docs/manifest.json` and commits the changes with `[skip ci]` to
   prevent another workflow run.

The `docs/index.html` page (hosted through GitHub Pages) then always has the
latest version, ready to flash directly from Chrome/Edge using WebSerial. See
`docs/README.md` for instructions on enabling Pages. **Change the SSID and
admin account immediately after flashing**, because this public build uses
the default password.

## Monitor and debug when WiFi is not broadcasting
If your phone cannot see the "Free WiFi" network after connecting the board over USB:

1. Open the **Serial Monitor** in PlatformIO (`pio device monitor`, or click
   the plug icon in the VS Code status bar). The firmware includes detailed logs:
   - `[WIFI] ✅ AP '...' đang phát @ 192.168.4.1 ...` → the AP is running; rescan
     for WiFi on your phone or toggle WiFi off and on.
   - `[WIFI] ❌ Không bật được AP sau 3 lần thử` → check the hint printed below
     the message (usually weak USB power or a board other than ESP32-C3).
   - Every 10 seconds, `[MONITOR] uptime=... heap=... clients=... ap=...` is
     printed to confirm the device has not frozen.
2. Common issues with the **ESP32-C3 SuperMini** (inexpensive clone boards):
   - A charge-only USB cable (without data wires) cannot flash or debug the board.
   - The computer's USB port or hub may not provide enough current, causing
     repeated brownouts and unstable WiFi. Try a rear USB port directly on the
     computer, or a 5V/1A+ charger with a data cable.
   - Some boards have poor antennas. The firmware already sets maximum
     transmit power (`WiFi.setTxPower(WIFI_POWER_19_5dBm)`), but stay close to
     the board while testing.
3. If it still does not start, erase the entire flash and upload again:
   ```bash
   pio run -t erase
   pio run -t upload
   ```
4. Open `http://192.168.4.1/admin`. The black panel at the top (📡) shows the
   number of connected clients, uptime, and free heap. It refreshes every
   3 seconds using the `/status` endpoint (admin login required).

`platformio.ini` also enables `monitor_filters = esp32_exception_decoder, time`.
If the device crashes or reboots, the Serial Monitor translates memory
addresses in the backtrace into function names, and adds timestamps to log lines.

### If you see `Image length ... doesn't fit in partition length ...` / "program size greater than maximum" / repeated bootloops
This means the built firmware is larger than the `app0`/`app1` partition in
`partitions.csv`. Each app partition is currently **1.375 MB** (the current
binary is about 1.33 MB, leaving roughly 80 KB free). If new features make the
code exceed this size, increase both `app0` and `app1` by the same amount and
reduce `spiffs` accordingly so the total still fits the 4 MB flash. For example:
```
app0,     app,  ota_0,   0x10000,  0x180000
app1,     app,  ota_1,   0x190000, 0x180000
spiffs,   data, spiffs,  0x310000, 0xF0000
```
(`app1` offset = `app0` offset + `app0` size; `spiffs` offset = `app1` offset +
`app1` size; `app0` and `app1` **must be the same size** for OTA to work properly.)
Also avoid setting `CORE_DEBUG_LEVEL` too high—custom `Serial.printf` logs in
the code do not depend on it, so leaving it at `0` does not lose those logs.

After making changes, always **erase the entire flash and upload again** to
ensure old firmware does not cause conflicts:
```bash
pio run -t erase
pio run -t upload
```

## Update firmware over OTA (no USB reconnection needed)
`partitions.csv` has **two app partitions** (`app0` + `app1`) for safe A/B OTA:
new firmware is written to the partition **not currently running**, so the
device keeps working during download and writing. Only after the new firmware
is written and validated does the device switch boot partitions and restart.
If the process fails, it restarts using the old firmware—an interrupted OTA
will not brick the device.

Go to `/admin` and open **"🔄 Firmware update (OTA)"**. There are two methods:

**Method 1 — Download from a URL (recommended if GitHub Pages is enabled for `docs/`):**
1. Paste a direct `firmware.bin` link into the URL field. Or enter your
   **GitHub username** and **repository name** in the two small fields, then
   click **"💡 Suggestion: fill in the GitHub Pages link"**. It will populate
   `https://<username>.github.io/<repo-name>/firmware/firmware.bin` for you.
   GitHub Actions automatically builds this file whenever you push new code
   (see **"Flash from the web"** above). The username and repository are
   remembered for next time.
2. Click **"🔄 Download & update firmware via URL"**.
3. The device temporarily uses your real WiFi (configured under "Real WiFi")
   to download the file and write it to the free app partition, then restarts.
   The fake AP stays active during the download, so the admin connection is
   not dropped midway.

**Method 2 — Manually upload a `.bin` file (no real WiFi or internet required):**
1. Build the firmware on your computer with `pio run` (not `upload`), then get
   the file at `.pio/build/esp32c3/firmware.bin`.
2. In `/admin`, find **"Or manually upload a .bin file"**, choose the file,
   then click **"⬆️ Upload & update"**. This runs over the fake AP and does
   not need internet.

After restarting, check the monitor panel (📡) at the top of `/admin`. If the
`🧩 Build:` timestamp has changed to the new build time, the OTA update succeeded.

⚠️ The `.bin` file for OTA must be the **app image** built using this project's
`platformio.ini`/`partitions.csv` (not `bootloader.bin`, `partitions.bin`, or
a merged file). Uploading the wrong type will be rejected because it is
invalid (`Update.end()` checks the checksum/magic byte before accepting it).

## Prepare a compact MP4 video (ffmpeg)
```bash
ffmpeg -i rick.mp4 -t 25 -vf scale=320:-2 -b:v 250k -b:a 40k -movflags +faststart out.mp4
```
Keep it under about 1 MB to fit in the `spiffs` flash partition in
`partitions.csv` (currently 1.1875 MB, since the two app0/app1 partitions for
OTA use most of the remaining 4 MB flash).

## Technical and ethical notes
- **iPhone/Safari** requires HTTP range requests for smooth video playback,
  which the standard `WebServer` does not support. To work well on iOS,
  consider switching to `ESPAsyncWebServer` (`serveStatic` supports ranges).
- Browsers block autoplay with sound, so there is an **"Enable sound"** button
  after the video starts.
- `WiFiClientSecure::setInsecure()` skips SSL verification when downloading
  the video. This may be acceptable for personal use to download one file from
  a source you host yourself, but should not be used for other purposes.
- The `.env` file contains your real password and is included in `.gitignore`—
  do not commit it to a public GitHub repository.
- Use this only to prank friends on a WiFi network that you operate. Do not
  use a name resembling a real network (such as a cafe or company), impersonate
  a real brand, or store/exploit names entered by users; that would turn a
  prank into phishing or unauthorized data collection.

# Sponsor
[![Sponsor Me](https://img.shields.io/badge/sponsor-Me-ff69b4?style=for-the-badge&logo=githubsponsors&logoColor=white)](https://donate.protechvn.io.vn) <br/>

# License
This code under [MIT LICENSE](LICENSE)
