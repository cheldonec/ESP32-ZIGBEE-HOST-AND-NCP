# flash_spiffs_all.py
import os
import sys
import subprocess
import re

BUILD_DIR = "build"
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))

PORT = os.environ.get("ESPPORT")

PARTITIONS = [
    {
        "name": "zbm_ui",
        "file": "spiffs_zbm_webui.bin",
        "desc": "Web UI (интерфейс)"
    },
    {
        "name": "zbm_quirks",
        "file": "spiffs_zbm_quirks.bin",
        "desc": "Quirks (правила устройств)"
    },
    {
        "name": "zbm_conf",
        "file": "spiffs_zbm_conf.bin",
        "desc": "Конфигурация"
    },
    {
        "name": "zbm_certs",
        "file": "spiffs_zbm_certs.bin",
        "desc": "Сертификаты"
    }
]


def find_esp_port():
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
                print("⚠️  Ошибка: Доступ к порту запрещён.")
            if "Failed to connect" in output:
                print("⚠️  ESP32 не в режиме прошивки.")
            if "No serial ports found" in output:
                print("⚠️  Не найдено ни одного COM-порта.")
            return None

    except FileNotFoundError:
        print("❌ Не найден esptool.")
    except subprocess.TimeoutExpired:
        print("⏰ Таймаут: esptool не ответил.")
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
    print(f"🔧 Выполняю: {' '.join(cmd)}")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"❌ Ошибка: {' '.join(cmd)}")
        return False
    return True


def main():
    print("🚀 Массовая прошивка SPIFFS-разделов...\n")

    port = get_port()

    build_path = os.path.join(PROJECT_DIR, BUILD_DIR)
    if not os.path.exists(build_path):
        print(f"❌ Папка {build_path} не найдена.")
        print("💡 Выполни: idf.py build")
        sys.exit(1)

    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        print("❌ Не установлен IDF_PATH")
        sys.exit(1)

    parttool = os.path.join(idf_path, "components", "partition_table", "parttool.py")
    if not os.path.exists(parttool):
        print(f"❌ Нет parttool.py: {parttool}")
        sys.exit(1)

    failed = False
    for part in PARTITIONS:
        bin_path = os.path.join(build_path, part["file"])
        if not os.path.exists(bin_path):
            print(f"⚠️  Файл не найден: {bin_path} — пропускаю {part['name']}")
            failed = True
            continue

        print(f"📌 Прошивка: {part['name']} → {part['desc']}")
        cmd = [
            sys.executable, parttool,
            "--port", port,
            "write_partition",
            "--partition-name", part["name"],
            "--input", bin_path
        ]

        if run_command(cmd):
            print("✅ Успешно\n")
        else:
            print("❌ Ошибка\n")
            failed = True

    if failed:
        print("⚠️  Одна или несколько операций завершились с ошибкой.")
    else:
        print("🎉 Все SPIFFS-разделы успешно прошиты!")

    print("🔚 Готово.")


if __name__ == "__main__":
    main()