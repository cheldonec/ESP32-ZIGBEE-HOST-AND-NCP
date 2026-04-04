# flash_spiffs.py
import os
import sys
import subprocess

# === Настройки ===
PORT = "COM4"  # ← поменяй на свой порт
BUILD_DIR = "build"
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))

PARTITIONS = [
    {
        "name": "spiffs_ui",
        "file": "spiffs_ui.bin",
        "description": "UI файлы (веб-интерфейс)"
    },
    {
        "name": "quirks",
        "file": "spiffs_quirks.bin",
        "description": "Quirks (правила совместимости устройств)"
    }
]

def run_command(cmd):
    print(f"Выполняю: {' '.join(cmd)}")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"❌ Ошибка при выполнении: {cmd}")
        sys.exit(1)

def main():
    print("🚀 Запуск прошивки SPIFFS-разделов...\n")

    # Проверяем, существует ли build/
    build_path = os.path.join(PROJECT_DIR, BUILD_DIR)
    if not os.path.exists(build_path):
        print(f"❌ Папка {build_path} не найдена. Сначала собери проект: idf.py build")
        sys.exit(1)

    # Проверяем IDF_PATH
    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        print("❌ Переменная окружения IDF_PATH не установлена")
        print("Убедись, что ты в активированной среде ESP-IDF")
        sys.exit(1)

    parttool = os.path.join(idf_path, "components", "partition_table", "parttool.py")

    if not os.path.exists(parttool):
        print(f"❌ parttool.py не найден: {parttool}")
        sys.exit(1)

    # Прошиваем каждый раздел
    for part in PARTITIONS:
        bin_file = os.path.join(build_path, part["file"])
        if not os.path.exists(bin_file):
            print(f"❌ Файл не найден: {bin_file}")
            continue

        print(f"📌 Прошивка: {part['name']} → {part['description']}")
        cmd = [
            sys.executable, parttool,
            "--port", PORT,
            "write_partition",
            "--partition-name", part["name"],
            "--input", bin_file
        ]
        run_command(cmd)
        print("✅ Успешно прошито\n")

    print("🎉 Все SPIFFS-разделы прошиты!")

if __name__ == "__main__":
    main()
