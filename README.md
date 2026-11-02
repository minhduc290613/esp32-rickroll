# Rick Roll Captive Portal — ESP32-C3 + PlatformIO

[Tiếng Việt](README.md) | [English](README.en.md)

![Platform](https://img.shields.io/badge/platform-ESP32--C3-blue?logo=espressif&logoColor=white)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino&logoColor=white)
![Build Tool](https://img.shields.io/badge/build-PlatformIO-orange?logo=platformio&logoColor=white)
![Filesystem](https://img.shields.io/badge/filesystem-LittleFS-9cf)
![License](https://img.shields.io/badge/license-MIT-green)
![Status](https://img.shields.io/badge/status-for%20pranks%20only-ff69b4)

Flash ngay từ trình duyệt, không cần cài gì: **[→ Trang Web Installer](git.tino.io.vn/esp32-rickroll/)**

## Cấu trúc
```
rickroll-portal/
├── .github/workflows/build-firmware.yml   # bot tự build .bin khi có commit mới
├── docs/                                  # trang web flash firmware (GitHub Pages)
│   ├── index.html
│   ├── manifest.json
│   ├── firmware/                          # .bin do bot tự sinh, đừng sửa tay
│   └── README.md
├── platformio.ini
├── load_env.py        # đọc biến môi trường lúc build, nạp vào firmware
├── .env.example        # mẫu file biến môi trường, copy thành .env
├── .gitignore
├── partitions.csv
└── src/main.cpp
```

## Cấu hình SSID & tài khoản admin qua biến môi trường (lúc build)
Thay vì sửa trực tiếp trong code, các giá trị lấy từ biến môi trường khi build,
qua `load_env.py`:

| Biến         | Ý nghĩa                              | Bắt buộc?                         |
|--------------|---------------------------------------|------------------------------------|
| `AP_SSID`    | Tên WiFi giả phát ra                  | Không, mặc định `Free WiFi`        |
| `ADMIN_USER` | Tài khoản đăng nhập `/admin`          | Không, mặc định `admin`            |
| `ADMIN_PASS` | Mật khẩu đăng nhập `/admin`           | **Có** — build sẽ báo lỗi nếu thiếu, để không ai quên đổi mật khẩu mặc định |

### Cách set (khuyên dùng — hoạt động cả khi bấm nút Upload trong VS Code)
`load_env.py` tự đọc file **`.env`** nằm cạnh `platformio.ini`, không cần
export biến môi trường ở terminal nào cả:

1. File `.env` đã có sẵn trong project (giá trị placeholder).
2. Mở `.env`, sửa dòng `ADMIN_PASS=...` thành mật khẩu thật của bạn.
3. Bấm **Upload** như bình thường (CLI hay nút trong VS Code đều được).

```ini
# .env
AP_SSID=Free WiFi
ADMIN_USER=admin
ADMIN_PASS=mat-khau-rieng-cua-ban
```

⚠️ Lý do nên dùng cách này thay vì `export`/`$env:`: khi bấm nút Upload trong
VS Code (PlatformIO extension), lệnh build chạy trong một **task/terminal
riêng**, không thừa hưởng biến môi trường bạn export ở cửa sổ terminal khác —
dễ gặp lỗi "set rồi mà vẫn báo thiếu". Đọc thẳng từ `.env` tránh hẳn vấn đề đó.

### Cách khác: export biến môi trường thật (nếu luôn chạy `pio` bằng CLI)
**Linux/Mac:**
```bash
export AP_SSID="Free WiFi"
export ADMIN_USER="admin"
export ADMIN_PASS="mat-khau-rieng-cua-ban"
pio run -t upload
```
**Windows PowerShell:**
```powershell
$env:ADMIN_PASS="mat-khau-rieng-cua-ban"
pio run -t upload
```
Biến môi trường thật luôn được ưu tiên hơn file `.env` nếu cả hai cùng tồn tại.

⚠️ Những giá trị này **chỉ dùng cho lần build/nạp đầu tiên** (khi flash chưa
lưu gì). Sau đó đổi SSID hay tài khoản admin qua trang `/admin` sẽ được lưu
vào bộ nhớ flash (NVS/Preferences) và có hiệu lực ngay, không cần build lại.

## Cài đặt
1. Mở thư mục này bằng VS Code (extension **PlatformIO** đã cài).
2. Set biến môi trường như trên (ít nhất là `ADMIN_PASS`).
3. Bấm **Upload** (mũi tên →) hoặc chạy `pio run -t upload`.
   Nếu board không được nhận, giữ nút **BOOT** khi cắm USB.

## Đổi SSID / tài khoản admin sau khi đã chạy
Vào `http://192.168.4.1/admin` (đăng nhập bằng user/pass hiện tại), mục
**"🔒 Tài khoản Admin"** và ô **"Tên mạng (SSID) hiển thị"** ở mục đầu tiên.
Lưu xong:
- SSID đổi ngay lập tức (AP khởi động lại tự động, không cần cắm lại nguồn).
- Đổi tài khoản/mật khẩu admin thì lần sau vào `/admin` phải đăng nhập lại
  bằng thông tin mới — trình duyệt sẽ hỏi lại ngay sau khi lưu.

## Thiết lập lần đầu (để portal có video chạy offline)
1. Sau khi nạp xong, thiết bị phát WiFi mở tên theo `AP_SSID` (hoặc "Free WiFi").
2. Dùng điện thoại/máy tính kết nối WiFi đó, mở `http://192.168.4.1/admin`.
3. Trong mục **"WiFi thật"**: nhập SSID + mật khẩu WiFi nhà bạn,
   và dán **link video mp4 trực tiếp** (không phải link youtube.com —
   YouTube không cho tải thẳng mp4, bạn cần tự host file mp4 ở nơi khác,
   ví dụ GitHub raw hoặc hosting cá nhân).
4. Bấm **"Lưu tất cả"**, rồi bấm **"Tải / Tải lại video ngay"**.
   ESP32 sẽ tạm chuyển sang WiFi thật để tải file về flash,
   xong tự quay lại chỉ phát AP giả — từ đó không cần internet nữa.

## Trải nghiệm người dùng
1. Nạn nhân kết nối WiFi giả → tự động hiện trang đăng nhập kiểu
   các mạng công cộng (quán cà phê...): nhập tên, đợi đếm ngược quảng cáo 5s.
2. Bấm "Kết nối ngay" → chuyển sang trang phát video Rick Roll full màn hình,
   lặp lại liên tục.

## Admin có thể chỉnh
- SSID mạng giả, caption trang đăng nhập, màu nền gradient.
- Tài khoản & mật khẩu admin.
- WiFi thật + link video để tải lại bất cứ lúc nào.
- Upload mp4 thủ công (thay cho tải qua mạng).
- Xóa video hiện tại.
- Cập nhật firmware qua OTA — từ URL hoặc upload file `.bin` thủ công.

## Flash qua web (không cần PlatformIO)
Mỗi lần push commit đụng tới `src/`, `platformio.ini`, `partitions.csv` hay
`load_env.py` lên nhánh `main`, bot GitHub Actions (`build-firmware.yml`) tự:
1. Build firmware bằng PlatformIO với giá trị env **mặc định, không bí mật**
   (`admin` / `changeme123` / `Free WiFi`).
2. Gom `firmware.bin`, `bootloader.bin`, `partitions.bin`, `boot_app0.bin`
   vào `docs/firmware/`.
3. Cập nhật `docs/manifest.json` và tự commit `[skip ci]` để không tự kích hoạt lại.

Sau đó trang `docs/index.html` (host qua GitHub Pages) luôn có bản mới nhất
để flash trực tiếp từ Chrome/Edge qua WebSerial — xem hướng dẫn bật Pages
trong `docs/README.md`. **Nhớ đổi SSID/tài khoản admin ngay sau khi flash**
vì bản build công khai này dùng mật khẩu mặc định.

## Giám sát & debug khi WiFi không phát ra
Nếu kết nối USB mà điện thoại không thấy mạng "Free WiFi" xuất hiện:

1. Mở **Serial Monitor** trong PlatformIO (`pio device monitor`, hoặc icon
   phích cắm trong thanh dưới VS Code) — firmware đã được thêm log chi tiết:
   - `[WIFI] ✅ AP '...' đang phát @ 192.168.4.1 ...` → AP đã lên, thử quét lại
     WiFi trên điện thoại hoặc tắt/bật WiFi.
   - `[WIFI] ❌ Không bật được AP sau 3 lần thử` → xem gợi ý ngay dưới dòng đó
     (thường là nguồn USB yếu, hoặc board không đúng ESP32-C3).
   - Mỗi 10 giây có dòng `[MONITOR] uptime=... heap=... clients=... ap=...`
     để biết chắc thiết bị không bị treo.
2. Nguyên nhân hay gặp nhất với board **ESP32-C3 SuperMini** (loại clone giá rẻ):
   - Dùng cáp USB chỉ sạc (không có dây data) → không nạp được, không debug được.
   - Cổng USB máy tính/hub không đủ dòng → ESP32 brownout liên tục, WiFi chập chờn.
     Thử cắm trực tiếp vào cổng USB sau case máy tính hoặc dùng củ sạc 5V/1A+ qua cáp data.
   - Một số bản board thiếu ăng-ten tốt → firmware đã tự set công suất phát tối đa
     (`WiFi.setTxPower(WIFI_POWER_19_5dBm)`), nhưng vẫn cần đứng gần board khi test.
3. Nếu vẫn không lên, thử xoá toàn bộ flash rồi nạp lại:
   ```bash
   pio run -t erase
   pio run -t upload
   ```
4. Vào `http://192.168.4.1/admin` → khối màu đen ở đầu trang (📡) hiển thị
   số client đang kết nối, uptime, heap trống — tự cập nhật mỗi 3 giây, lấy dữ liệu
   từ endpoint `/status` (yêu cầu đăng nhập admin).

`platformio.ini` cũng đã bật `monitor_filters = esp32_exception_decoder, time`:
nếu thiết bị crash/reboot, Serial Monitor sẽ tự dịch địa chỉ bộ nhớ trong
backtrace thành tên hàm, và mỗi dòng log có kèm mốc thời gian.

### Nếu thấy lỗi `Image length ... doesn't fit in partition length ...` / "program size greater than maximum" / bootloop liên tục
Nghĩa là file firmware build ra lớn hơn vùng `app0`/`app1` trong `partitions.csv`.
Hiện mỗi vùng app đang để **1.375MB** (binary hiện tại ~1.33MB, dư khoảng 80KB).
Nếu thêm tính năng mới làm code phình to vượt mức này, tăng cả `app0` lẫn `app1`
cùng một lượng, bớt `spiffs` tương ứng để tổng vẫn khớp 4MB flash, ví dụ:
```
app0,     app,  ota_0,   0x10000,  0x180000
app1,     app,  ota_1,   0x190000, 0x180000
spiffs,   data, spiffs,  0x310000, 0xF0000
```
(offset của `app1` = offset `app0` + size `app0`; offset `spiffs` = offset `app1`
+ size `app1`; `app0` và `app1` **phải cùng kích thước** để OTA hoạt động đúng).
Cũng tránh bật `CORE_DEBUG_LEVEL` lên cao — log `Serial.printf` tự viết trong
code không phụ thuộc giá trị này nên không mất gì khi để `0`.

Sau khi sửa, luôn **xoá sạch flash rồi nạp lại** để chắc chắn không còn firmware
cũ gây xung đột:
```bash
pio run -t erase
pio run -t upload
```

## Cập nhật firmware qua OTA (không cần cắm lại USB)
`partitions.csv` có **2 vùng app** (`app0` + `app1`) để OTA an toàn kiểu A/B:
firmware mới được ghi vào vùng *đang không chạy*, máy vẫn hoạt động bình thường
trong lúc tải/ghi; chỉ khi ghi xong và hợp lệ mới chuyển boot sang bản mới rồi
khởi động lại. Nếu quá trình lỗi giữa chừng, máy tự khởi động lại bằng bản cũ —
không bao giờ bị "biến gạch" (brick) do OTA lỗi nửa chừng.

Vào `/admin`, mục **"🔄 Cập nhật firmware (OTA)"**, có 2 cách:

**Cách 1 — Tải từ URL (khuyên dùng nếu đã bật GitHub Pages ở `docs/`):**
1. Dán link `firmware.bin` trực tiếp vào ô URL — hoặc nhanh hơn: nhập
   **GitHub username** + **tên repo** vào 2 ô nhỏ rồi bấm nút
   **"💡 Gợi ý: điền link từ GitHub Pages"**, nó tự điền
   `https://<username>.github.io/<ten-repo>/firmware/firmware.bin` giúp bạn
   (bot GitHub Actions tự build file này mỗi khi bạn push code mới — xem mục
   "Flash qua web" ở trên). Username/repo được nhớ lại cho lần sau.
2. Bấm **"🔄 Tải & cập nhật firmware qua URL"**.
3. Thiết bị tạm dùng WiFi thật (mục "WiFi thật" đã cấu hình cho video) để tải
   file, ghi vào vùng app rảnh, rồi tự khởi động lại — vẫn giữ AP giả chạy
   song song trong lúc tải nên admin không bị rớt kết nối giữa chừng.

**Cách 2 — Upload file .bin thủ công (không cần WiFi thật/internet):**
1. Build firmware trên máy bằng `pio run` (không `upload`), lấy file tại
   `.pio/build/esp32c3/firmware.bin`.
2. Vào `/admin`, mục **"Hoặc upload file .bin thủ công"**, chọn file đó, bấm
   **"⬆️ Upload & cập nhật"**. Chạy ngay trên AP giả, không cần internet.

Sau khi khởi động lại, kiểm tra khối giám sát (📡) ở đầu `/admin` — dòng
`🧩 Build:` đổi sang mốc giờ build mới là chắc chắn OTA đã thành công.

⚠️ File `.bin` dùng cho OTA phải là **đúng app image** build từ chính
`platformio.ini`/`partitions.csv` của project này (không phải `bootloader.bin`,
`partitions.bin` hay file đã merge) — nạp nhầm loại file sẽ bị từ chối vì
không hợp lệ (`Update.end()` kiểm tra checksum/magic byte trước khi chấp nhận).

## Chuẩn bị video mp4 nhỏ gọn (ffmpeg)
```bash
ffmpeg -i rick.mp4 -t 25 -vf scale=320:-2 -b:v 250k -b:a 40k -movflags +faststart out.mp4
```
Giữ dưới ~1MB để vừa vùng flash `spiffs` trong `partitions.csv` (hiện là 1.1875MB,
vì 2 vùng app0/app1 cho OTA chiếm phần lớn 4MB flash còn lại).

## Lưu ý kỹ thuật & đạo đức
- **iPhone/Safari** cần hỗ trợ HTTP range request để phát video mượt,
  mà `WebServer` chuẩn không hỗ trợ. Muốn chạy tốt trên iOS, cân nhắc
  chuyển sang `ESPAsyncWebServer` (`serveStatic` hỗ trợ range).
- Trình duyệt chặn autoplay có tiếng → có nút "Bật tiếng" sau khi video chạy.
- `WiFiClientSecure::setInsecure()` bỏ qua kiểm tra SSL khi tải video —
  chấp nhận được cho việc cá nhân tải 1 file từ nguồn bạn tự host,
  không nên dùng cách này cho mục đích khác.
- File `.env` chứa mật khẩu thật, đã được thêm vào `.gitignore` —
  đừng commit nó lên git/github công khai.
- Chỉ dùng để troll bạn bè trong phạm vi WiFi do chính bạn phát.
  Đừng đặt tên giống mạng thật (quán cà phê, công ty...), đừng giả logo
  thương hiệu thật, và đừng lưu trữ/khai thác tên người dùng nhập vào —
  làm vậy sẽ biến trò đùa thành hành vi lừa đảo/thu thập dữ liệu trái phép.

# Sponsor
[![Sponsor Me](https://img.shields.io/badge/sponsor-Me-ff69b4?style=for-the-badge&logo=githubsponsors&logoColor=white)](https://donate.protechvn.io.vn) <br/>

# License
This code under [MIT LICENSE](LICENSE)