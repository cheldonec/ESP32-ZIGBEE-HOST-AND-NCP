# flash_ui.py
import os
import sys
import subprocess
import re

BUILD_DIR = "build"
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))

BIN_FILE = "spiffs_zbm_webui.bin"
PARTITION_NAME = "zbm_ui"
DESCRIPTION = "Web UI (React интерфейс)"

PORT = os.environ.get("ESPPORT")


def find_esp_port():
    """Ищет ESP32 через esptool — анализирует stderr и stdout"""
    try:
        print("🔧 Запускаю esptool для определения порта...")
        result = subprocess.run(
            [sys.executable, "-m", "esptool", "--before", "default_reset", "chip_id"],
            capture_output=True,
            text=True,
            timeout=5
        )

        output = result.stderr + "\n" + result.stdout

        match = re.search(r'(COM\d+|/dev/ttyUSB\d+|/dev/ttyACM\d+)', output)
        if match:
            port = match.group(1)
            print(f"✅ Найден порт в выводе esptool: {port}")
            return port
        else:
            print("❌ Порт не найден в выводе esptool")
            if "Access is denied" in output:
                print("⚠️  Ошибка: Доступ к порту запрещён. Возможно, он занят.")
            if "Failed to connect" in output:
                print("⚠️  ESP32 не в режиме прошивки. Нажми кнопку BOOT + RESET.")
            if "No serial ports found" in output:
                print("⚠️  Не найдено ни одного COM-порта.")
            return None

    except FileNotFoundError:
        print("❌ Не найден esptool. Убедись, что IDF активирован: source $IDF_PATH/export.sh")
    except subprocess.TimeoutExpired:
        print("⏰ Таймаут: esptool не ответил за 5 секунд.")
        print("💡 Проверь, подключён ли ESP32 и работает ли драйвер.")
    except Exception as e:
        print(f"❌ Ошибка при поиске порта: {e}")
    return None


def get_port():
    if PORT:
        print(f"📌 Используем порт из ESPPORT: {PORT}")
        return PORT

    print("🔍 Автоопределение порта ESP32...")
    detected = find_esp_port()
    if detected:
        return detected

    print("💡 Подключи ESP32 и убедись, что драйверы установлены.")
    print("❌ Не удалось найти порт. Завершение.")
    sys.exit(1)


def run_command(cmd):
    """Выполняет команду и проверяет результат"""
    print(f"🔧 Выполняю: {' '.join(cmd)}")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"❌ Ошибка при выполнении команды: {' '.join(cmd)}")
        sys.exit(1)


def main():
    print("🚀 Прошивка раздела 'zbm_ui'...\n")

    port = get_port()

    build_path = os.path.join(PROJECT_DIR, BUILD_DIR)
    if not os.path.exists(build_path):
        print(f"❌ Папка сборки не найдена: {build_path}")
        print("💡 Сначала собери проект: idf.py build")
        sys.exit(1)

    bin_path = os.path.join(build_path, BIN_FILE)
    if not os.path.exists(bin_path):
        print(f"❌ Файл образа не найден: {bin_path}")
        print("💡 Убедись, что выполнил: idf.py build")
        sys.exit(1)

    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        print("❌ Переменная окружения IDF_PATH не установлена")
        print("💡 Активируй среду ESP-IDF перед запуском")
        sys.exit(1)

    parttool = os.path.join(idf_path, "components", "partition_table", "parttool.py")
    if not os.path.exists(parttool):
        print(f"❌ Не найден parttool.py: {parttool}")
        sys.exit(1)

    cmd = [
        sys.executable, parttool,
        "--port", port,
        "write_partition",
        "--partition-name", PARTITION_NAME,
        "--input", bin_path
    ]

    print(f"📌 Прошивка: {PARTITION_NAME}")
    print(f"   Файл: {bin_path}")
    print(f"   Описание: {DESCRIPTION}")
    print()

    run_command(cmd)

    print("✅ Раздел 'zbm_ui' успешно прошит!")
    print("🎉 Теперь можно перезагрузить устройство.")


if __name__ == "__main__":
    main()