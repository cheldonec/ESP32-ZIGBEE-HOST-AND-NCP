# flash_quirks.py
import os
import sys
import subprocess

# === Настройки ===
PORT = "COM4"  # ← Замени на свой порт!
BUILD_DIR = "build"
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))

QUIRKS_BIN_FILE = "spiffs_quirks.bin"
PARTITION_NAME = "quirks"
DESCRIPTION = "Quirks (правила совместимости устройств)"


def run_command(cmd):
    """Выполняет команду и проверяет результат"""
    print(f"🔧 Выполняю: {' '.join(cmd)}")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"❌ Ошибка при выполнении команды: {' '.join(cmd)}")
        sys.exit(1)


def main():
    print("🚀 Запуск прошивки раздела 'quirks'...\n")

    # Проверяем, существует ли build/
    build_path = os.path.join(PROJECT_DIR, BUILD_DIR)
    if not os.path.exists(build_path):
        print(f"❌ Папка сборки не найдена: {build_path}")
        print("💡 Сначала собери проект: idf.py build")
        sys.exit(1)

    # Путь к бинарнику с квирками
    quirks_bin = os.path.join(build_path, QUIRKS_BIN_FILE)
    if not os.path.exists(quirks_bin):
        print(f"❌ Файл квирков не найден: {quirks_bin}")
        print(f"💡 Убедись, что файл '{QUIRKS_BIN_FILE}' создан и лежит в папке '{BUILD_DIR}'")
        sys.exit(1)

    # Проверяем IDF_PATH и parttool.py
    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        print("❌ Переменная окружения IDF_PATH не установлена")
        print("💡 Активируй среду ESP-IDF: . $IDF_PATH/export.sh")
        sys.exit(1)

    parttool = os.path.join(idf_path, "components", "partition_table", "parttool.py")
    if not os.path.exists(parttool):
        print(f"❌ Не найден parttool.py: {parttool}")
        print("💡 Убедись, что ESP-IDF установлен корректно")
        sys.exit(1)

    # Команда прошивки
    cmd = [
        sys.executable, parttool,
        "--port", PORT,
        "write_partition",
        "--partition-name", PARTITION_NAME,
        "--input", quirks_bin
    ]

    print(f"📌 Прошивка: {PARTITION_NAME}")
    print(f"   Файл: {quirks_bin}")
    print(f"   Описание: {DESCRIPTION}")
    print()

    run_command(cmd)

    print("✅ Раздел 'quirks' успешно прошит!")
    print("🎉 Готово!")


if __name__ == "__main__":
    main()