# Руководство по разработке

## Предварительные требования

Вся разработка происходит внутри podman-контейнера для разработки. На хосте
необходимы только:

- **Podman** (или Docker) для запуска контейнера разработки
- **Git** для управления версиями

Контейнер предоставляет:

| Инструмент     | Версия  | Назначение                        |
|----------------|---------|-----------------------------------|
| Clang          | 22.1.0  | Основной компилятор               |
| GCC            | 15.1.1  | Компилятор для валидации          |
| CMake          | 4.2.3   | Система сборки                    |
| mold           | latest  | Компоновщик (отладочные сборки)   |
| lld            | latest  | Компоновщик (релизные сборки)     |
| Unity          | 2.6.1   | Фреймворк модульного тестирования |
| cppcheck       | latest  | Статический анализ                |
| PVS-Studio     | latest  | Проприетарный статический анализатор |
| CodeChecker    | latest  | Интеграция Clang SA + clang-tidy  |
| Doxygen        | 1.16.1  | Генерация документации API        |

## Настройка контейнера разработки

Сборка и запуск контейнера:

```bash
# Build the dev image
make -C deploy/podman build-dev

# Run a shell inside the container
podman run --rm -it --security-opt seccomp=unconfined \
    --env-file .env \
    -v $(pwd):/workspace:Z \
    localhost/liboas-dev:latest bash
```

Файл `.env` содержит лицензионный ключ PVS-Studio. Скопируйте из `.env.example`,
если он не существует.

## Команды сборки

CMake-пресеты являются основным интерфейсом сборки:

```bash
# Configure (inside container)
cmake --preset clang-debug

# Build
cmake --build --preset clang-debug

# Run tests
ctest --preset clang-debug

# Format check
cmake --build --preset clang-debug --target format-check

# Apply formatting
cmake --build --preset clang-debug --target format

# Static analysis
cmake --build --preset clang-debug --target cppcheck

# Generate Doxygen docs
cmake --build --preset clang-debug --target docs
```

### Пресеты компиляторов

| Пресет         | Компилятор | Компоновщик | Санитайзеры      | Применение        |
|----------------|------------|-------------|------------------|-------------------|
| `clang-debug`  | Clang 22   | mold        | ASan+UBSan, MSan | Основная разработка |
| `gcc-debug`    | GCC 15     | mold        | ASan+UBSan       | Валидация         |
| `clang-release`| Clang 22   | lld         | Нет              | Релизные сборки   |
| `gcc-release`  | GCC 15     | lld         | LTO              | Релизные сборки   |

## Команды тестирования

```bash
# Run all unit tests
ctest --preset clang-debug

# Run a specific test
ctest --preset clang-debug -R test_schema

# Verbose output
ctest --preset clang-debug --output-on-failure
```

Тесты используют фреймворк Unity. Файлы тестов находятся в `tests/unit/test_*.c`.

## Конвейер качества

После каждой задачи запускайте полный 6-шаговый конвейер качества. Все шаги
должны пройти перед коммитом.

```bash
podman run --rm --security-opt seccomp=unconfined \
    --env-file .env \
    -v $(pwd):/workspace:Z \
    localhost/liboas-dev:latest bash -c "cd /workspace && ./scripts/quality.sh"
```

### Шаги конвейера

| Шаг  | Инструмент   | Что проверяет                           |
|------|--------------|-----------------------------------------|
| 1    | CMake build  | Компиляция со всеми включёнными предупреждениями |
| 2    | ctest        | Прохождение всех модульных тестов       |
| 3    | clang-format | Соответствие форматирования `.clang-format` |
| 4    | cppcheck     | Статический анализ (null deref, утечки, UB) |
| 5    | PVS-Studio   | Проприетарный глубокий статический анализ |
| 6    | CodeChecker  | Clang Static Analyzer + clang-tidy      |

Если шаг не проходит, исправьте найденные проблемы и перезапустите,
пока все 6 не пройдут.

## Соглашения по коду

### Именование

- Функции: `oas_module_verb_noun()` (например, `oas_schema_add_property()`)
- Типы: `oas_module_name_t` (например, `oas_schema_t`)
- Перечисления/макросы: `OAS_MODULE_VALUE` (например, `OAS_TYPE_STRING`)
- Защита от повторного включения: `LIBOAS_MODULE_FILE_H`
- Тестовые функции: `test_module_action_expected()` (например, `test_schema_parse_string_type()`)

### Обработка ошибок

Функции, которые могут завершиться неудачей, возвращают отрицательные значения errno:

```c
[[nodiscard]] int oas_schema_add_property(oas_arena_t *arena, oas_schema_t *schema,
                                          const char *name, oas_schema_t *prop_schema)
{
    if (!arena || !schema || !name || !prop_schema)
        return -EINVAL;

    oas_property_t *prop = oas_arena_alloc(arena, sizeof(*prop), _Alignof(*prop));
    if (!prop)
        return -ENOMEM;

    /* ... */
    return 0;
}
```

Для функций, захватывающих несколько ресурсов, используйте `goto cleanup`:

```c
int do_work(void)
{
    int rc = -ENOMEM;
    char *buf = malloc(4096);
    if (!buf)
        goto cleanup;

    FILE *f = fopen("file.txt", "r");
    if (!f)
        goto cleanup_buf;

    rc = 0;
    /* ... */

cleanup_file:
    fclose(f);
cleanup_buf:
    free(buf);
cleanup:
    return rc;
}
```

### Возможности C23 (обязательные)

| Возможность      | Использование                           |
|------------------|-----------------------------------------|
| `nullptr`        | Использовать вместо `NULL` повсеместно  |
| `[[nodiscard]]`  | Все функции, возвращающие выделенную память |
| `constexpr`      | Целочисленные и вещественные константы  |
| `bool`           | Использовать ключевое слово, не `_Bool` |
| `_Static_assert` | Проверки инвариантов на этапе компиляции |

### Форматирование

- Ограничение по ширине: 100 символов
- Фигурные скобки: стиль ядра Linux
- Стиль указателей: `int *ptr` (пробел перед `*`)
- Порядок включений: `_GNU_SOURCE` -> соответствующий заголовок -> стандартная библиотека C -> POSIX -> сторонние

## Добавление новых тестов

1. Создайте `tests/unit/test_<module>.c` (или добавьте в существующий файл).
2. Подключите `<unity.h>` и тестируемый заголовок.
3. Реализуйте функции `setUp()` и `tearDown()`.
4. Напишите тестовые функции с префиксом `test_`.
5. Зарегистрируйте тесты в `main()` с помощью `RUN_TEST()`.
6. Добавьте тест в `tests/unit/CMakeLists.txt`.

```c
#include <unity.h>
#include <liboas/oas_schema.h>

void setUp(void) {}
void tearDown(void) {}

void test_schema_create_returns_nonnull(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    oas_schema_t *schema = oas_schema_create(arena);
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_EQUAL_UINT8(0, schema->type_mask);
    oas_arena_destroy(arena);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_schema_create_returns_nonnull);
    return UNITY_END();
}
```

## Добавление fuzz-целей

Fuzz-цели находятся в `tests/fuzz/` и используют LibFuzzer (только Clang).

1. Создайте `tests/fuzz/fuzz_<target>.c`.
2. Реализуйте `LLVMFuzzerTestOneInput()`.
3. Добавьте в `tests/fuzz/CMakeLists.txt`.

```c
#include <liboas/oas_parser.h>
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    oas_arena_t *arena = oas_arena_create(0);
    oas_doc_t *doc = oas_doc_parse(arena, (const char *)data, size, nullptr);
    if (doc)
        oas_doc_free(doc);
    oas_arena_destroy(arena);
    return 0;
}
```

## Интеграция с CI

CI-конвейер проверяет спецификации с помощью внешних инструментов:

- **openapi-generator** -- проверяет, что спецификация может использоваться
  для генерации клиентского/серверного кода на различных языках.
- **openapi-style-validator** -- обеспечивает соблюдение стилевых соглашений API
  (именование, описания, коды ответов).

Эти проверки выполняются как отдельные шаги CI после прохождения конвейера качества.

## Рабочий процесс Git

- Именование веток: `feature/description`, `fix/issue-description`
- Стиль коммитов: conventional commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`)
- Все коммиты должны проходить конвейер качества перед отправкой.
