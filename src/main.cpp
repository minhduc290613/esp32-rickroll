#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include "esp_wifi.h"

// In ra ngày/giờ lúc build, hiện trong Serial + /status + /admin để biết chắc
// OTA đã chạy đúng bản mới (so sánh trước/sau khi update).
#define FW_BUILD_INFO (__DATE__ " " __TIME__)

// ===== GIÁ TRỊ MẶC ĐỊNH LẤY TỪ BIẾN MÔI TRƯỜNG LÚC BUILD (xem load_env.py) =====
// Chỉ dùng LẦN ĐẦU khi chưa có gì lưu trong flash; sau đó admin đổi qua /admin
// sẽ được lưu vào Preferences và ưu tiên hơn các macro này.
#ifndef DEFAULT_AP_SSID
#define DEFAULT_AP_SSID "Free WiFi"
#endif
#ifndef DEFAULT_ADMIN_USER
#define DEFAULT_ADMIN_USER "admin"
#endif
#ifndef DEFAULT_ADMIN_PASS
#define DEFAULT_ADMIN_PASS "changeme123"
#endif

const IPAddress apIP(192, 168, 4, 1);
const char* VIDEO_PATH = "/video.mp4";
// ================================================================================

DNSServer dns;
WebServer server(80);
Preferences prefs;
File uploadFile;

String videoDownloadUrl; // link mp4 trực tiếp để tải về lần đầu (KHÔNG phải link youtube.com)
String otaUrl;           // link firmware.bin trực tiếp để tự cập nhật qua OTA
String ghUser, ghRepo;   // chỉ để GỢI Ý tự điền otaUrl từ GitHub Pages, không dùng để làm gì khác
String staSSID, staPASS; // wifi thật dùng tạm để tải video / firmware
String caption;
String bgColor1, bgColor2; // màu nền gradient trang login
String lastVisitorName = "";

String apSSID;      // SSID của mạng giả phát ra, đổi được qua admin
String adminUser;   // tài khoản admin, đổi được qua admin
String adminPass;   // mật khẩu admin, đổi được qua admin

String esc(String s) {
  s.replace("&", "&amp;"); s.replace("<", "&lt;");
  s.replace(">", "&gt;");  s.replace("'", "&#39;"); s.replace("\"", "&quot;");
  return s;
}

bool isAdmin() {
  if (!server.authenticate(adminUser.c_str(), adminPass.c_str())) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

void redirectTo(const String& path) {
  server.sendHeader("Location", path, true);
  server.send(302, "text/plain", "");
}

// ---------- Theo dõi sự kiện WiFi qua Serial Monitor (để biết AP có thật sự lên không) ----------
void wifiEventHandler(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_START:
      Serial.println("[WIFI] AP đã bật (ARDUINO_EVENT_WIFI_AP_START)");
      break;
    case ARDUINO_EVENT_WIFI_AP_STOP:
      Serial.println("[WIFI] AP đã tắt (ARDUINO_EVENT_WIFI_AP_STOP)");
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.printf("[WIFI] Có thiết bị kết nối vào AP, tổng số client: %d\n",
                     WiFi.softAPgetStationNum());
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.printf("[WIFI] Một thiết bị rời AP, còn lại: %d\n",
                     WiFi.softAPgetStationNum());
      break;
    case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
      // điện thoại đang dò sóng AP nhưng chưa kết nối - log tắt bớt vì có thể dồn dập
      break;
    default:
      break;
  }
}

// Khởi động AP giả, có retry + fix các lỗi hay gặp khiến AP không phát sóng:
// - modem sleep làm rớt beacon  -> tắt bằng esp_wifi_set_ps(WIFI_PS_NONE)
// - công suất phát mặc định thấp (hay gặp ở board C3 SuperMini) -> set TX power cao nhất
// - dùng chuỗi rỗng "" thay vì nullptr cho mạng mở, tránh lỗi ở vài bản core
// - thử lại vài lần và in rõ nguyên nhân nếu vẫn thất bại
void startFakeAP() {
  WiFi.persistent(false);     // không ghi cấu hình WiFi vào NVS mỗi lần đổi, đỡ hao flash
  WiFi.disconnect(true, true);
  delay(100);

  WiFi.mode(WIFI_AP);
  delay(100);

  if (!WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0))) {
    Serial.println("[WIFI] ⚠️ softAPConfig thất bại!");
  }

  bool ok = false;
  for (int attempt = 1; attempt <= 3 && !ok; attempt++) {
    ok = WiFi.softAP(apSSID.c_str(), "", 1, 0, 8); // mở, kênh 1, tối đa 8 client
    if (!ok) {
      Serial.printf("[WIFI] softAP() thất bại lần %d/3, thử lại...\n", attempt);
      delay(300);
    }
  }

  if (!ok) {
    Serial.println("[WIFI] ❌ Không bật được AP sau 3 lần thử. Kiểm tra: "
                    "nguồn USB có đủ dòng không (nên dùng cáp data + cổng USB 1A+), "
                    "board có đúng là ESP32-C3 không, thử nhấn nút RESET.");
    return;
  }

  esp_wifi_set_ps(WIFI_PS_NONE);                 // tắt tiết kiệm điện, tránh rớt beacon AP
  WiFi.setTxPower(WIFI_POWER_19_5dBm);           // công suất phát tối đa, hay thiếu trên board C3 SuperMini

  dns.start(53, "*", apIP);
  Serial.printf("[WIFI] ✅ AP '%s' đang phát @ %s (kênh 1, mở, không mật khẩu)\n",
                apSSID.c_str(), WiFi.softAPIP().toString().c_str());
}

// ---------- Tải video thật từ internet (chạy lúc setup hoặc theo yêu cầu admin) ----------
bool downloadVideoOnce() {
  if (videoDownloadUrl.length() == 0) { Serial.println("Chưa có video_url"); return false; }
  if (staSSID.length() == 0) { Serial.println("Chưa có WiFi thật"); return false; }

  Serial.printf("Đang kết nối WiFi thật '%s' để tải video...\n", staSSID.c_str());
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(staSSID.c_str(), staPASS.c_str());

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(300); Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nKhông kết nối được WiFi thật.");
    startFakeAP();
    return false;
  }
  Serial.println("\nĐã có internet, bắt đầu tải video...");

  WiFiClientSecure https;
  https.setInsecure(); // bỏ qua kiểm tra chứng chỉ cho gọn, chấp nhận ở mức cá nhân
  HTTPClient http;
  bool ok = false;

  if (http.begin(https, videoDownloadUrl)) {
    int code = http.GET();
    if (code == 200) {
      File f = LittleFS.open(VIDEO_PATH, "w");
      if (f) {
        WiFiClient* stream = http.getStreamPtr();
        uint8_t buf[1024];
        int total = 0;
        while (http.connected() && (http.getSize() > 0 || http.getSize() == -1)) {
          size_t avail = stream->available();
          if (avail) {
            int c = stream->readBytes(buf, min(avail, sizeof(buf)));
            f.write(buf, c);
            total += c;
          } else if (!http.connected()) break;
          delay(1);
        }
        f.close();
        Serial.printf("Tải xong: %d bytes\n", total);
        ok = total > 1000;
      }
    } else {
      Serial.printf("HTTP lỗi: %d\n", code);
    }
    http.end();
  }

  WiFi.disconnect(true);
  startFakeAP(); // quay lại chỉ phát AP giả, không cần internet nữa
  return ok;
}

// ---------- OTA: tải firmware.bin mới từ URL và tự cập nhật ----------
// Ghi vào vùng app KHÁC với vùng đang chạy (nhờ partitions.csv có app0+app1),
// nên máy vẫn chạy bình thường trong lúc tải/ghi; chỉ khi ghi xong & hợp lệ
// mới chuyển boot sang bản mới rồi khởi động lại. Nếu quá trình lỗi giữa
// chừng, máy vẫn khởi động lại bằng bản cũ - không bao giờ bị "biến gạch".
bool performOtaFromUrl(const String& url) {
  if (url.length() == 0) { Serial.println("[OTA] Chưa có URL firmware"); return false; }
  if (staSSID.length() == 0) { Serial.println("[OTA] Chưa có WiFi thật"); return false; }

  Serial.printf("[OTA] Đang kết nối WiFi thật '%s' để tải firmware...\n", staSSID.c_str());
  WiFi.mode(WIFI_AP_STA); // giữ AP giả chạy song song, admin không bị rớt kết nối
  WiFi.begin(staSSID.c_str(), staPASS.c_str());

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(300); Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[OTA] Không kết nối được WiFi thật.");
    startFakeAP();
    return false;
  }
  Serial.println("\n[OTA] Đã có internet, bắt đầu tải firmware...");

  WiFiClientSecure https;
  https.setInsecure();
  HTTPClient http;
  bool ok = false;

  if (http.begin(https, url)) {
    int code = http.GET();
    if (code == 200) {
      int len = http.getSize();
      Serial.printf("[OTA] Kích thước file: %d bytes\n", len);

      if (!Update.begin(len > 0 ? (size_t)len : UPDATE_SIZE_UNKNOWN)) {
        Serial.printf("[OTA] Update.begin() lỗi: %s\n", Update.errorString());
      } else {
        WiFiClient* stream = http.getStreamPtr();
        size_t written = Update.writeStream(*stream);
        Serial.printf("[OTA] Đã ghi %u bytes\n", (unsigned)written);

        if (Update.end()) {
          if (Update.isFinished()) {
            Serial.println("[OTA] ✅ Ghi firmware thành công, hợp lệ, sẵn sàng khởi động lại.");
            ok = true;
          } else {
            Serial.println("[OTA] ❌ Update chưa hoàn tất (ghi thiếu dữ liệu).");
          }
        } else {
          Serial.printf("[OTA] ❌ Update.end() lỗi: %s\n", Update.errorString());
        }
      }
    } else {
      Serial.printf("[OTA] ❌ HTTP lỗi: %d\n", code);
    }
    http.end();
  }

  if (!ok) {
    WiFi.disconnect(true);
    startFakeAP(); // tải lỗi -> quay lại AP giả như cũ, firmware hiện tại không bị ảnh hưởng
  }
  // Nếu ok, cố ý KHÔNG gọi startFakeAP()/restart ở đây - để hàm gọi (handleOtaFetch)
  // còn kịp trả lời cho trình duyệt admin biết là đã xong, trước khi tự khởi động lại.
  return ok;
}

// ---------- Trang login kiểu public wifi (nền CSS thuần, chạy offline hoàn toàn) ----------
void handleLogin() {
  String h = F(R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content='width=device-width,initial-scale=1'>
<title>Free WiFi - Portal</title>
<style>
*{box-sizing:border-box}
body{margin:0;min-height:100vh;font-family:'Segoe UI',sans-serif;
  display:flex;align-items:center;justify-content:center;color:#fff;
  position:relative;overflow:hidden;
  background:linear-gradient(135deg,__BG1__,__BG2__ 100%);
}
.blob{position:absolute;border-radius:50%;filter:blur(40px);opacity:.35;
  animation:float 10s ease-in-out infinite}
.b1{width:260px;height:260px;background:#ff5e62;top:-60px;left:-60px}
.b2{width:320px;height:320px;background:#36d1dc;bottom:-80px;right:-80px;animation-delay:2s}
.b3{width:200px;height:200px;background:#fceabb;top:40%;right:10%;animation-delay:4s}
@keyframes float{0%,100%{transform:translateY(0)}50%{transform:translateY(-25px)}}

.card{position:relative;z-index:2;background:#fff;color:#222;width:92%;max-width:380px;
  border-radius:16px;padding:28px 24px;box-shadow:0 20px 50px rgba(0,0,0,.4);text-align:center}
.logo{font-size:1.6em;font-weight:800;color:#c0392b;margin-bottom:4px}
.sub{color:#888;font-size:.85em;margin-bottom:18px}
input{width:100%;padding:13px;border:1px solid #ddd;border-radius:10px;
  margin-bottom:14px;font-size:1em}
.ad{background:#f3f3f3;border-radius:10px;padding:14px;margin-bottom:16px;font-size:.85em;color:#555}
.ad b{color:#c0392b}
button{width:100%;padding:14px;border:0;border-radius:10px;font-size:1.05em;
  font-weight:700;color:#fff;background:#bbb;transition:.2s}
button.ready{background:#c0392b;cursor:pointer}
.note{font-size:.7em;color:#aaa;margin-top:14px}
</style></head><body>
<div class="blob b1"></div><div class="blob b2"></div><div class="blob b3"></div>
<div class=card>
  <div class=logo>☕ __SSID__</div>
  <div class=sub>__CAPTION__</div>
  <form id=f method=POST action=/connect>
    <input name=name placeholder="Nhập tên của bạn" required>
    <div class=ad>📺 Mạng sẽ kết nối sau <b id=cd>5</b> giây...</div>
    <button id=btn type=submit disabled>Vui lòng đợi (5s)</button>
  </form>
  <div class=note>Bằng việc kết nối, bạn đồng ý Điều khoản sử dụng</div>
</div>
<script>
let s = 5;
const cd = document.getElementById('cd');
const btn = document.getElementById('btn');
const t = setInterval(() => {
  s--;
  cd.textContent = s;
  if (s <= 0) {
    clearInterval(t);
    btn.disabled = false;
    btn.classList.add('ready');
    btn.textContent = '🔗 Kết nối ngay';
  } else {
    btn.textContent = 'Vui lòng đợi (' + s + 's)';
  }
}, 1000);
</script>
</body></html>)HTML");

  h.replace("__BG1__", bgColor1);
  h.replace("__BG2__", bgColor2);
  h.replace("__CAPTION__", esc(caption));
  h.replace("__SSID__", esc(apSSID));
  server.send(200, "text/html; charset=utf-8", h);
}

void handleConnect() {
  lastVisitorName = server.arg("name");
  Serial.println("Nạn nhân vừa 'kết nối': " + lastVisitorName);
  redirectTo("/video-page");
}

// ---------- Trang phát Rick Roll ----------
void handleVideoPage() {
  String h = F("<!doctype html><html><head><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Đang kết nối...</title><style>"
               "body{margin:0;background:#000;color:#fff;font-family:sans-serif;text-align:center}"
               "video{width:100%;max-height:85vh}"
               "h3{padding:10px}"
               "button{font-size:1.1em;padding:.6em 1.4em;margin:1em;border-radius:8px;border:0}"
               "</style></head><body>");
  h += "<h3>✅ Xin chào " + esc(lastVisitorName) + ", đang kết nối Internet...</h3>";
  if (LittleFS.exists(VIDEO_PATH)) {
    h += F("<video id=v src=/video.mp4 autoplay loop playsinline></video><br>"
           "<button onclick=\"v.muted=false;v.play()\">🔊 Bật tiếng</button>");
  } else {
    h += F("<p>Chưa có video vì lí do bản quyền. Admin cần cấu hình &amp; tải video ở /admin</p>");
  }
  h += F("</body></html>");
  server.send(200, "text/html; charset=utf-8", h);
}

void handleVideo() {
  File f = LittleFS.open(VIDEO_PATH, "r");
  if (!f) { server.send(404, "text/plain", "no video"); return; }
  server.streamFile(f, "video/mp4");
  f.close();
}

// ---------- Giám sát (status JSON, dùng cho khối tự refresh trong /admin) ----------
void handleStatus() {
  if (!isAdmin()) return;
  bool hasVideo = LittleFS.exists(VIDEO_PATH);

  String j = "{";
  j += "\"fw_build\":\"" + String(FW_BUILD_INFO) + "\"";
  j += ",\"uptime_sec\":" + String(millis() / 1000);
  j += ",\"free_heap\":" + String(ESP.getFreeHeap());
  j += ",\"clients\":" + String(WiFi.softAPgetStationNum());
  j += ",\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\"";
  j += ",\"ap_ssid\":\"" + apSSID + "\"";
  j += ",\"has_video\":" + String(hasVideo ? "true" : "false");
  j += ",\"video_kb\":" + String(hasVideo ? LittleFS.open(VIDEO_PATH, "r").size() / 1024 : 0);
  j += ",\"fs_used_kb\":" + String(LittleFS.usedBytes() / 1024);
  j += ",\"fs_total_kb\":" + String(LittleFS.totalBytes() / 1024);
  j += ",\"last_visitor\":\"" + lastVisitorName + "\"";
  j += "}";
  server.send(200, "application/json", j);
}

// ---------- Admin ----------
void handleAdmin() {
  if (!isAdmin()) return;

  String h = F("<!doctype html><html><head><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Admin</title><style>body{font-family:sans-serif;max-width:560px;margin:auto;padding:12px}"
               "input,button{width:100%;padding:10px;margin:6px 0;box-sizing:border-box}"
               "input[type=color]{height:44px;padding:4px}"
               "fieldset{margin:14px 0}legend{font-weight:700}"
               ".warn{color:#c0392b;font-size:.85em}"
               ".mon{background:#111;color:#0f0;font-family:monospace;font-size:.82em;"
               "border-radius:10px;padding:12px;margin-bottom:16px;white-space:pre-line}"
               "</style></head><body><h2>🎛 Admin Panel</h2>");

  h += F("<div class=mon id=mon>📡 Đang tải giám sát...</div>"
         "<script>"
         "async function refreshMon(){"
         "try{"
         "const r=await fetch('/status');"
         "const d=await r.json();"
         "const up=d.uptime_sec, h=Math.floor(up/3600), m=Math.floor((up%3600)/60), s=up%60;"
         "document.getElementById('mon').textContent="
         "`🧩 Build: ${d.fw_build}\\n"
         "📡 AP: ${d.ap_ssid} @ ${d.ap_ip}\\n"
         "👥 Client đang kết nối: ${d.clients}\\n"
         "⏱ Uptime: ${h}h ${m}m ${s}s\\n"
         "💾 Heap trống: ${Math.round(d.free_heap/1024)} KB\\n"
         "🎬 Video: ${d.has_video ? d.video_kb+' KB' : 'chưa có'}\\n"
         "🗄 Flash dùng: ${d.fs_used_kb}/${d.fs_total_kb} KB\\n"
         "🙋 Khách gần nhất: ${d.last_visitor || '(chưa có)'}`;"
         "}catch(e){document.getElementById('mon').textContent='⚠️ Mất kết nối tới thiết bị';}"
         "}"
         "refreshMon();setInterval(refreshMon,3000);"
         "</script>");

  h += "<form method=POST action=/save><fieldset><legend>Nội dung &amp; giao diện</legend>"
       "Tên mạng (SSID) hiển thị:<input name=apssid value='" + esc(apSSID) + "'>"
       "Caption:<input name=caption value='" + esc(caption) + "'>"
       "Màu nền 1:<input name=bg1 type=color value='" + esc(bgColor1) + "'>"
       "Màu nền 2:<input name=bg2 type=color value='" + esc(bgColor2) + "'>"
       "</fieldset>";

  h += "<fieldset><legend>WiFi thật (để tải video lần đầu)</legend>"
       "SSID:<input name=ssid value='" + esc(staSSID) + "'>"
       "Mật khẩu:<input name=pass type=password value='" + esc(staPASS) + "'>"
       "Link video mp4 trực tiếp:<input name=vurl value='" + esc(videoDownloadUrl) + "'>"
       "</fieldset>";

  h += "<fieldset><legend>🔄 Cập nhật firmware (OTA)</legend>"
       "Link firmware.bin trực tiếp:"
       "<input id=otaurl name=otaurl value='" + esc(otaUrl) + "'>"
       "<div style='display:flex;gap:6px'>"
       "<input id=ghuser name=ghuser placeholder='GitHub username' value='" + esc(ghUser) + "' style='margin:0'>"
       "<input id=ghrepo name=ghrepo placeholder='tên repo' value='" + esc(ghRepo) + "' style='margin:0'>"
       "</div>"
       "<button type=button onclick=\"fillOtaFromPages()\" style='background:#555'>"
       "💡 Gợi ý: điền link từ GitHub Pages</button>"
       "<script>"
       "function fillOtaFromPages(){"
       "const u=document.getElementById('ghuser').value.trim();"
       "const r=document.getElementById('ghrepo').value.trim();"
       "if(!u||!r){alert('Nhập GitHub username và tên repo trước đã.');return;}"
       "document.getElementById('otaurl').value="
       "`https://${u}.github.io/${r}/firmware/firmware.bin`;"
       "}"
       "</script>"
       "</fieldset>";

  h += "<fieldset><legend>🔒 Tài khoản Admin</legend>"
       "<p class=warn>Đổi xong sẽ phải đăng nhập lại bằng thông tin mới.</p>"
       "Tài khoản:<input name=admin_user value='" + esc(adminUser) + "'>"
       "Mật khẩu mới (để trống nếu không đổi):<input name=admin_pass type=password placeholder='••••••••'>"
       "</fieldset>"
       "<button>💾 Lưu tất cả</button></form>";

  h += F("<form method=POST action=/fetch><button>⬇️ Tải / Tải lại video ngay</button></form>"
         "<form method=POST action=/upload enctype=multipart/form-data>"
         "<fieldset><legend>Hoặc upload mp4 thủ công</legend>"
         "<input type=file name=f accept=video/mp4><button>Upload</button></fieldset></form>"
         "<form method=POST action=/delete><button>🗑 Xóa video</button></form>"

         "<form method=POST action=/ota-fetch onsubmit=\"return confirm('Tải firmware mới từ URL và cập nhật ngay? Thiết bị sẽ khởi động lại.');\">"
         "<button>🔄 Tải &amp; cập nhật firmware qua URL</button></form>"
         "<form method=POST action=/ota-upload enctype=multipart/form-data "
         "onsubmit=\"return confirm('Nạp file .bin này làm firmware mới? Thiết bị sẽ khởi động lại.');\">"
         "<fieldset><legend>Hoặc upload file .bin thủ công (không cần internet)</legend>"
         "<input type=file name=f accept=.bin><button>⬆️ Upload &amp; cập nhật</button></fieldset></form>"

         "<p><a href=/>Xem trang portal</a></p></body></html>");
  server.send(200, "text/html; charset=utf-8", h);
}

void handleSave() {
  if (!isAdmin()) return;

  caption          = server.arg("caption");
  bgColor1         = server.arg("bg1");
  bgColor2         = server.arg("bg2");
  staSSID          = server.arg("ssid");
  staPASS          = server.arg("pass");
  videoDownloadUrl = server.arg("vurl");
  otaUrl           = server.arg("otaurl");
  ghUser           = server.arg("ghuser");
  ghRepo           = server.arg("ghrepo");

  String newApSsid = server.arg("apssid");
  String newUser   = server.arg("admin_user");
  String newPass   = server.arg("admin_pass"); // rỗng = giữ nguyên

  if (newApSsid.length() == 0) newApSsid = apSSID; // không cho để trống SSID
  if (newUser.length() == 0) newUser = adminUser;   // không cho để trống user

  bool apChanged = (newApSsid != apSSID);
  apSSID = newApSsid;
  adminUser = newUser;
  if (newPass.length() > 0) adminPass = newPass;

  prefs.putString("caption", caption);
  prefs.putString("bg1", bgColor1);
  prefs.putString("bg2", bgColor2);
  prefs.putString("ssid", staSSID);
  prefs.putString("pass", staPASS);
  prefs.putString("vurl", videoDownloadUrl);
  prefs.putString("otaurl", otaUrl);
  prefs.putString("ghuser", ghUser);
  prefs.putString("ghrepo", ghRepo);
  prefs.putString("apssid", apSSID);
  prefs.putString("adminuser", adminUser);
  prefs.putString("adminpass", adminPass);

  if (apChanged) startFakeAP(); // áp dụng SSID mới ngay, không cần khởi động lại thiết bị

  redirectTo("/admin");
}

void handleFetch() {
  if (!isAdmin()) return;
  bool ok = downloadVideoOnce();
  server.send(200, "text/html; charset=utf-8",
    ok ? "<p>Tải thành công! <a href=/admin>Quay lại</a></p>"
       : "<p>Tải thất bại, kiểm tra WiFi/URL. <a href=/admin>Quay lại</a></p>");
}

void handleDelete() {
  if (!isAdmin()) return;
  LittleFS.remove(VIDEO_PATH);
  redirectTo("/admin");
}

void handleUploadDone() {
  if (!isAdmin()) return;
  redirectTo("/admin");
}

void handleUploadData() {
  if (!server.authenticate(adminUser.c_str(), adminPass.c_str())) return;
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    LittleFS.remove(VIDEO_PATH);
    uploadFile = LittleFS.open(VIDEO_PATH, "w");
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END || u.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
  }
}

// ---------- OTA qua URL (admin bấm nút, thiết bị tự tải & cập nhật) ----------
void handleOtaFetch() {
  if (!isAdmin()) return;
  bool ok = performOtaFromUrl(otaUrl);
  if (ok) {
    server.send(200, "text/html; charset=utf-8",
      "<p>✅ Tải &amp; ghi firmware mới thành công! Thiết bị sẽ khởi động lại "
      "sau 2 giây, WiFi sẽ mất một chút rồi lên lại bình thường.</p>");
    delay(1500);
    WiFi.disconnect(true);
    ESP.restart();
  } else {
    server.send(200, "text/html; charset=utf-8",
      "<p>❌ OTA thất bại. Kiểm tra WiFi thật / link firmware, hoặc xem Serial "
      "Monitor để biết chi tiết. Firmware hiện tại KHÔNG bị ảnh hưởng. "
      "<a href=/admin>Quay lại</a></p>");
  }
}

// ---------- OTA qua upload .bin thủ công (không cần internet, chạy ngay trên AP giả) ----------
void handleOtaUploadDone() {
  if (!isAdmin()) return;
  if (Update.hasError()) {
    server.send(200, "text/html; charset=utf-8",
      "<p>❌ Upload firmware lỗi, firmware hiện tại KHÔNG bị ảnh hưởng. "
      "<a href=/admin>Quay lại</a></p>");
    return;
  }
  server.send(200, "text/html; charset=utf-8",
    "<p>✅ Nạp firmware thành công! Thiết bị sẽ khởi động lại sau 2 giây...</p>");
  delay(1500);
  ESP.restart();
}

void handleOtaUploadData() {
  if (!server.authenticate(adminUser.c_str(), adminPass.c_str())) return;
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    Serial.printf("[OTA] Upload thủ công bắt đầu: %s\n", u.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (Update.write(u.buf, u.currentSize) != u.currentSize) {
      Update.printError(Serial);
    }
  } else if (u.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[OTA] Upload thủ công xong: %u bytes, hợp lệ.\n", (unsigned)u.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    Serial.println("[OTA] Upload thủ công bị huỷ giữa chừng.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\n[BOOT] Rick Roll Portal - build: %s\n", FW_BUILD_INFO);
  if (!LittleFS.begin(true)) Serial.println("LittleFS lỗi!");

  prefs.begin("portal", false);
  caption          = prefs.getString("caption", "Vui lòng đăng nhập để sử dụng wifi miễn phí");
  bgColor1         = prefs.getString("bg1", "#1e3c72");
  bgColor2         = prefs.getString("bg2", "#c0392b");
  staSSID          = prefs.getString("ssid", "");
  staPASS          = prefs.getString("pass", "");
  videoDownloadUrl = prefs.getString("vurl", "");
  otaUrl           = prefs.getString("otaurl", "");
  ghUser           = prefs.getString("ghuser", "");
  ghRepo           = prefs.getString("ghrepo", "");

  // SSID/admin: ưu tiên giá trị đã lưu trong flash; nếu chưa từng lưu,
  // dùng giá trị mặc định lấy từ biến môi trường lúc build (load_env.py)
  apSSID    = prefs.getString("apssid", DEFAULT_AP_SSID);
  adminUser = prefs.getString("adminuser", DEFAULT_ADMIN_USER);
  adminPass = prefs.getString("adminpass", DEFAULT_ADMIN_PASS);

  WiFi.onEvent(wifiEventHandler); // log mọi sự kiện AP ra Serial Monitor để debug
  startFakeAP();

  // Lần đầu chạy: nếu chưa có video nhưng đã cấu hình wifi+url -> tự tải
  if (!LittleFS.exists(VIDEO_PATH) && staSSID.length() && videoDownloadUrl.length()) {
    downloadVideoOnce();
  }

  server.on("/", handleLogin);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/video-page", handleVideoPage);
  server.on(VIDEO_PATH, handleVideo);
  server.on("/admin", handleAdmin);
  server.on("/status", handleStatus);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/fetch", HTTP_POST, handleFetch);
  server.on("/delete", HTTP_POST, handleDelete);
  server.on("/upload", HTTP_POST, handleUploadDone, handleUploadData);
  server.on("/ota-fetch", HTTP_POST, handleOtaFetch);
  server.on("/ota-upload", HTTP_POST, handleOtaUploadDone, handleOtaUploadData);

  const char* probes[] = {"/generate_204", "/gen_204", "/hotspot-detect.html",
                          "/library/test/success.html", "/connecttest.txt",
                          "/ncsi.txt", "/fwlink", "/redirect", "/canonical.html"};
  for (auto p : probes) server.on(p, [](){ redirectTo("/"); });
  server.onNotFound([](){ redirectTo("/"); });

  server.begin();
}

void loop() {
  dns.processNextRequest();
  server.handleClient();

  // In nhịp tim mỗi 10 giây ra Serial Monitor, để biết chắc AP còn sống,
  // kể cả khi không mở trình duyệt vào /admin để xem /status.
  static unsigned long lastBeat = 0;
  if (millis() - lastBeat > 10000) {
    lastBeat = millis();
    Serial.printf("[MONITOR] uptime=%lus heap=%uKB clients=%d ap=%s@%s\n",
                  millis() / 1000, ESP.getFreeHeap() / 1024,
                  WiFi.softAPgetStationNum(), apSSID.c_str(),
                  WiFi.softAPIP().toString().c_str());
  }
}
