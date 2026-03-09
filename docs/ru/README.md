# Документация liboas

liboas -- библиотека на C23 для парсинга, валидации и обслуживания спецификаций OpenAPI 3.2.
Целевая платформа -- Linux (ядро 6.7+, glibc 2.39+), лицензия GPLv3.

## Быстрый старт

```c
#include <liboas/oas_adapter.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const char spec[] = "{\"openapi\":\"3.1.0\","
                        "\"info\":{\"title\":\"My API\",\"version\":\"1.0\"},"
                        "\"paths\":{}}";

    oas_adapter_t *adapter = oas_adapter_create(spec, sizeof(spec) - 1, nullptr, nullptr);
    if (!adapter) {
        fprintf(stderr, "Failed to load spec\n");
        return 1;
    }

    /* Validate a request */
    oas_http_request_t req = {
        .method = "GET",
        .path = "/pets",
    };
    oas_arena_t *arena = oas_arena_create(0);
    oas_validation_result_t result = {0};
    oas_adapter_validate_request(adapter, &req, &result, arena);

    if (result.valid) {
        printf("Request is valid\n");
    }

    oas_arena_destroy(arena);
    oas_adapter_destroy(adapter);
    return 0;
}
```

## Содержание

| Файл | Описание |
|------|----------|
| [01-architecture.md](01-architecture.md) | Двухслойная архитектура, модель памяти, проектные решения |
| [02-oas-model.md](02-oas-model.md) | Модель документа, конвейер парсинга, разрешение $ref |
| [03-json-schema.md](03-json-schema.md) | Поддержка JSON Schema 2020-12, система типов, ключевые слова |
| [04-schema-compiler.md](04-schema-compiler.md) | Конвейер компиляции схем в байткод |
| [05-validator.md](05-validator.md) | Виртуальная машина валидации, валидация запросов/ответов |
| [06-integration.md](06-integration.md) | Паттерн адаптера, spec-first и code-first подходы |
| [07-api-reference.md](07-api-reference.md) | Полный справочник публичного API |
| [08-development.md](08-development.md) | Система сборки, тестирование, конвейер качества |

## Зависимости

| Библиотека | Версия | Роль | Лицензия |
|------------|--------|------|----------|
| yyjson | 0.12+ | Парсинг JSON (~2.4 ГБ/с) | MIT |
| libfyaml | 0.9+ | Парсинг YAML 1.2 (опционально) | MIT |
| QuickJS libregexp | latest | Регулярные выражения ECMA-262 (vendored) | MIT |
| Unity | 2.6.1 | Фреймворк модульного тестирования | MIT |

## Соответствие стандартам

- Спецификация OpenAPI 3.2
- JSON Schema 2020-12
- RFC 8259 (JSON), RFC 6901 (JSON Pointer), RFC 3986 (URI)
- RFC 9457 (Problem Details), RFC 9110 (HTTP Semantics)
- ECMA-262 (семантика регулярных выражений для pattern)
