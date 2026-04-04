# ZBM Dev Model — Internal Documentation

> 💡 **Цель**: динамическое управление Zigbee-устройствами, эндпоинтами, кластерами и атрибутами.  
> Поддержка стандартных и vendor-specific кластеров (Tuya, Xiaomi и др.)

---

## 📦 Архитектура

- Устройства (`zbm_dev_t`) → Эндпоинты → Кластеры → Атрибуты
- Динамическое создание при первом обновлении
- Безопасный доступ через `*_safe()` функции
- Поиск по `GUID`: `0x1001:1:0402:0000`

---

## 🔧 Как добавить стандартный кластер?

См. инструкцию: [cluster_create_template_instruction.md](./cluster_create_template_instruction.md)

---

## 🔗 Основные функции

| Назначение | Вызов |
|----------|-------|
| Обновить/создать атрибут | `zbm_update_cluster_attribute_safe(...)` |
| Найти устройство | `zbm_find_device_by_short_safe(0x1001)` |
| Найти атрибут по GUID | `zbm_find_attr_by_guid_safe("0x1001:1:0006:0000")` |
| Найти атрибут по ключу | `zbm_find_attr_by_key_safe(addr, ep, cl, attr)` |
| Генерация GUID | `zbm_generate_attr_guid(out, len, addr, ep, cl, attr)` |
| Очистка устройства | `zbm_free_dev_t(dev)` |

---

## 📁 Структура
