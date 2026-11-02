Import("env")
import os
import sys

# Nguồn giá trị, ưu tiên theo thứ tự:
#   1) Biến môi trường thật của hệ điều hành (export / $env: / set)
#   2) File ".env" nằm cạnh platformio.ini (đọc trực tiếp bằng script này,
#      KHÔNG cần export tay, nên nút "Upload" trong VS Code cũng tự đọc được)
#
# Lý do cần cách (2): khi bấm nút Upload trong VS Code (PlatformIO extension),
# lệnh build chạy trong một task/terminal RIÊNG, không thừa hưởng biến môi
# trường bạn export ở một cửa sổ terminal khác -> set rồi mà vẫn báo thiếu.
# Đọc thẳng từ file .env giải quyết triệt để vấn đề đó.
#
# Cách dùng nhanh nhất: cp .env.example .env, sửa giá trị trong .env, rồi
# bấm Upload bình thường -- không cần export gì nữa.

PROJECT_DIR = env.get("PROJECT_DIR", os.getcwd())
ENV_FILE = os.path.join(PROJECT_DIR, ".env")


def load_dotenv(path):
    """Đọc file .env dạng KEY=VALUE, trả về dict. Bỏ qua dòng trống / bắt đầu bằng #."""
    data = {}
    if not os.path.isfile(path):
        return data
    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip()
            value = value.strip().strip('"').strip("'")
            if key:
                data[key] = value
    return data


dotenv_values = load_dotenv(ENV_FILE)


def getenv(key, default=None):
    # Ưu tiên biến môi trường thật của OS trước, sau đó mới tới file .env
    val = os.environ.get(key, "").strip()
    if val:
        return val
    val = dotenv_values.get(key, "").strip()
    if val:
        return val
    return default


ap_ssid    = getenv("AP_SSID", "Free WiFi")      # SSID không nhạy cảm, có fallback
admin_user = getenv("ADMIN_USER", "admin")       # username không nhạy cảm, có fallback
admin_pass = getenv("ADMIN_PASS")                # BẮT BUỘC phải set, KHÔNG fallback

if admin_pass is None:
    sys.stderr.write(
        "\n*** LOI: chua co ADMIN_PASS (ca bien moi truong lan file .env deu khong co). ***\n"
        "Cach don gian nhat (khuyen dung, hoat dong ca khi bam nut Upload trong VS Code):\n"
        "  1) copy .env.example thanh .env\n"
        "  2) mo .env, sua dong ADMIN_PASS=... thanh mat khau that\n"
        "  3) bam Upload lai binh thuong, khong can export gi them\n"
        "File .env dang tim: %s\n\n" % ENV_FILE
    )
    env.Exit(1)   # dừng build ngay, không cho ra firmware với mật khẩu đoán được

env.Append(CPPDEFINES=[
    ("DEFAULT_AP_SSID",    env.StringifyMacro(ap_ssid)),
    ("DEFAULT_ADMIN_USER", env.StringifyMacro(admin_user)),
    ("DEFAULT_ADMIN_PASS", env.StringifyMacro(admin_pass)),
])
