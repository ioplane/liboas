# Справочник API

Все публичные функции используют префикс `oas_`. Функции возвращают 0 при успехе
или отрицательное значение errno при ошибке, если не указано иное. Указатели,
помеченные `[[nodiscard]]`, нельзя игнорировать.

## oas_alloc -- Arena-аллокатор

```c
oas_arena_t *oas_arena_create(size_t block_size);
```
Создаёт новую arena. Передайте 0 для размера блока по умолчанию (64 КиБ).
Возвращает `nullptr` при ошибке выделения памяти.

```c
void oas_arena_destroy(oas_arena_t *arena);
```
Уничтожает arena и освобождает все блоки. Безопасен для nullptr.

```c
void *oas_arena_alloc(oas_arena_t *arena, size_t size, size_t align);
```
Выделяет `size` байт с выравниванием `align`. Возвращает `nullptr` при ошибке
или нулевом размере.

```c
void oas_arena_reset(oas_arena_t *arena);
```
Сбрасывает arena для повторного использования без освобождения блоков.
Все предыдущие аллокации становятся невалидными.

```c
size_t oas_arena_used(const oas_arena_t *arena);
```
Возвращает общее количество выделенных байт по всем блокам.

## oas_error -- Накопление ошибок

```c
oas_error_list_t *oas_error_list_create(oas_arena_t *arena);
```
Создаёт список ошибок, размещённый в arena. Возвращает `nullptr` при ошибке.

```c
void oas_error_list_add(oas_error_list_t *list, oas_error_kind_t kind,
                        const char *path, const char *fmt, ...);
```
Добавляет ошибку с форматированием в стиле printf. Путь и сообщение
копируются в arena.

```c
size_t oas_error_list_count(const oas_error_list_t *list);
```
Возвращает количество ошибок.

```c
const oas_error_t *oas_error_list_get(const oas_error_list_t *list, size_t index);
```
Возвращает ошибку по индексу. Возвращает `nullptr`, если индекс вне диапазона.

```c
bool oas_error_list_has_errors(const oas_error_list_t *list);
```
Возвращает true, если список содержит ошибки.

```c
const char *oas_error_kind_name(oas_error_kind_t kind);
```
Возвращает читаемое имя вида ошибки (например, `"TYPE"`, `"CONSTRAINT"`).

## oas_parser -- Разбор документов

```c
oas_doc_t *oas_doc_parse(oas_arena_t *arena, const char *json, size_t len,
                         oas_error_list_t *errors);
```
Разбирает документ OpenAPI 3.2 из JSON-строки. Возвращает `nullptr` при ошибке.

```c
oas_doc_t *oas_doc_parse_file(oas_arena_t *arena, const char *path,
                              oas_error_list_t *errors);
```
Разбирает документ OpenAPI 3.2 из файла. Возвращает `nullptr` при ошибке.

```c
void oas_doc_free(oas_doc_t *doc);
```
Освобождает базовый документ yyjson. Делает невалидными все строковые указатели
с нулевым копированием.

## oas_schema -- Модель JSON Schema

```c
oas_schema_t *oas_schema_create(oas_arena_t *arena);
```
Выделяет и инициализирует нулями схему со значениями-сторожами по умолчанию.

```c
int oas_schema_add_property(oas_arena_t *arena, oas_schema_t *schema,
                            const char *name, oas_schema_t *prop_schema);
```
Добавляет свойство в список свойств объектной схемы.

```c
uint8_t oas_type_from_string(const char *type_name);
```
Преобразует имя типа в битовую маску (например, `"string"` -> `OAS_TYPE_STRING`).
Возвращает 0 для неизвестных типов.

## oas_compiler -- Компиляция схем

```c
oas_compiled_schema_t *oas_schema_compile(const oas_schema_t *schema,
                                          const oas_compiler_config_t *config,
                                          oas_error_list_t *errors);
```
Компилирует дерево схемы в байткод валидации. Возвращает `nullptr` при ошибке.

```c
void oas_compiled_schema_free(oas_compiled_schema_t *compiled);
```
Освобождает скомпилированную схему и все её ресурсы (инструкции, regex-паттерны).

```c
oas_compiled_doc_t *oas_doc_compile(const oas_doc_t *doc,
                                    const oas_compiler_config_t *config,
                                    oas_error_list_t *errors);
```
Компилирует все схемы в разобранном документе. Возвращает `nullptr` при ошибке.

```c
void oas_compiled_doc_free(oas_compiled_doc_t *compiled);
```
Освобождает скомпилированный документ и все скомпилированные схемы.

## oas_validator -- Движок валидации

```c
int oas_validate(const oas_compiled_schema_t *compiled, yyjson_val *value,
                 oas_validation_result_t *result, oas_arena_t *arena);
```
Валидирует значение yyjson по скомпилированной схеме. Заполняет `result`.

```c
int oas_validate_json(const oas_compiled_schema_t *compiled, const char *json,
                      size_t len, oas_validation_result_t *result,
                      oas_arena_t *arena);
```
Разбирает JSON-строку и валидирует по скомпилированной схеме.

```c
int oas_validate_request(const oas_compiled_doc_t *doc,
                         const oas_http_request_t *req,
                         oas_validation_result_t *result, oas_arena_t *arena);
```
Валидирует HTTP-запрос (путь, параметры, тело) по скомпилированному документу.

```c
int oas_validate_response(const oas_compiled_doc_t *doc, const char *path,
                          const char *method, const oas_http_response_t *resp,
                          oas_validation_result_t *result, oas_arena_t *arena);
```
Валидирует HTTP-ответ по скомпилированному документу. Требует исходный путь
запроса и метод.

## oas_emitter -- Генерация JSON

```c
char *oas_doc_emit_json(const oas_doc_t *doc, const oas_emit_options_t *options,
                        size_t *out_len);
```
Генерирует документ в виде JSON-строки. Возвращает строку, выделенную в куче.
Освобождается через `oas_emit_free()`.

```c
char *oas_schema_emit_json(const oas_schema_t *schema,
                           const oas_emit_options_t *options, size_t *out_len);
```
Генерирует схему в виде JSON-строки. Освобождается через `oas_emit_free()`.

```c
void oas_emit_free(char *json);
```
Освобождает строку, возвращённую функциями генерации. Безопасен для nullptr.

## oas_builder -- Программное построение (Code-First)

```c
oas_schema_t *oas_schema_build_string(oas_arena_t *arena);
oas_schema_t *oas_schema_build_int32(oas_arena_t *arena);
oas_schema_t *oas_schema_build_int64(oas_arena_t *arena);
oas_schema_t *oas_schema_build_number(oas_arena_t *arena);
oas_schema_t *oas_schema_build_bool(oas_arena_t *arena);
oas_schema_t *oas_schema_build_string_ex(oas_arena_t *arena, const oas_string_opts_t *opts);
oas_schema_t *oas_schema_build_integer_ex(oas_arena_t *arena, const oas_number_opts_t *opts);
oas_schema_t *oas_schema_build_number_ex(oas_arena_t *arena, const oas_number_opts_t *opts);
oas_schema_t *oas_schema_build_array(oas_arena_t *arena, oas_schema_t *items);
oas_schema_t *oas_schema_build_object(oas_arena_t *arena);
```
Фабричные функции схем. Каждая возвращает схему с соответствующей `type_mask`.

```c
int oas_schema_set_required(oas_arena_t *arena, oas_schema_t *schema, ...);
```
Устанавливает имена обязательных свойств. Передаётся список строк,
завершённый NULL.

```c
int oas_schema_set_description(oas_schema_t *schema, const char *description);
int oas_schema_set_additional_properties(oas_schema_t *schema, oas_schema_t *additional);
```
Устанавливает метаданные и ограничения схемы.

```c
oas_doc_t *oas_doc_build(oas_arena_t *arena, const char *title, const char *version);
```
Создаёт минимальный документ OAS 3.2.0 с `info.title` и `info.version`.

```c
int oas_doc_add_server(oas_doc_t *doc, oas_arena_t *arena, const char *url,
                       const char *description);
int oas_doc_add_component_schema(oas_doc_t *doc, oas_arena_t *arena,
                                 const char *name, oas_schema_t *schema);
int oas_doc_add_path_op(oas_doc_t *doc, oas_arena_t *arena, const char *path,
                        const char *method, const oas_op_builder_t *op);
```
Добавляет серверы, компонентные схемы и операции путей в документ.

## oas_adapter -- Фасад интеграции

```c
oas_adapter_t *oas_adapter_create(const char *json, size_t len,
                                  const oas_adapter_config_t *config,
                                  oas_error_list_t *errors);
```
Создаёт адаптер из JSON-строки спецификации (подход spec-first).

```c
oas_adapter_t *oas_adapter_from_doc(const oas_doc_t *doc, oas_arena_t *arena,
                                    const oas_adapter_config_t *config,
                                    oas_error_list_t *errors);
```
Создаёт адаптер из документа, построенного через builder (подход code-first).

```c
void oas_adapter_destroy(oas_adapter_t *adapter);
```
Уничтожает адаптер и освобождает все ресурсы.

```c
const oas_doc_t *oas_adapter_doc(const oas_adapter_t *adapter);
const oas_adapter_config_t *oas_adapter_config(const oas_adapter_t *adapter);
```
Доступ к разобранному документу и конфигурации.

```c
const char *oas_adapter_spec_json(const oas_adapter_t *adapter, size_t *out_len);
```
Получает кэшированный JSON спецификации для раздачи. Возвращает `nullptr`,
если `serve_spec` отключён.

```c
int oas_adapter_validate_request(const oas_adapter_t *adapter,
                                 const oas_http_request_t *req,
                                 oas_validation_result_t *result, oas_arena_t *arena);
int oas_adapter_validate_response(const oas_adapter_t *adapter, const char *path,
                                  const char *method, const oas_http_response_t *resp,
                                  oas_validation_result_t *result, oas_arena_t *arena);
```
Валидация запросов и ответов через адаптер.

```c
int oas_adapter_find_operation(const oas_adapter_t *adapter, const char *method,
                               const char *path, oas_matched_operation_t *out,
                               oas_arena_t *arena);
```
Поиск операции по методу и пути. Возвращает `-ENOENT`, если не найдена.

```c
char *oas_scalar_html(const char *title, const char *spec_url, size_t *out_len);
```
Генерирует HTML документации Scalar API. Вызывающий должен освободить результат
через `free()`.

## oas_regex -- Regex-бэкенд

```c
oas_regex_backend_t *oas_regex_libregexp_create(void);
```
Создаёт regex-бэкенд по умолчанию на основе встроенного QuickJS libregexp.
Возвращает `nullptr` при ошибке.

## oas_problem -- RFC 9457 Problem Details

```c
char *oas_problem_from_validation(const oas_validation_result_t *result,
                                  int status_code, size_t *out_len);
```
Создаёт JSON Problem Details из ошибок валидации. Возвращает `nullptr`,
если валидация успешна. Освобождается через `oas_problem_free()`.

```c
char *oas_problem_to_json(const oas_problem_t *problem, size_t *out_len);
```
Создаёт JSON Problem Details из пользовательской структуры problem.
Освобождается через `oas_problem_free()`.

```c
void oas_problem_free(char *json);
```
Освобождает строку JSON Problem Details.

## oas_negotiate -- Согласование типа содержимого

```c
const char *oas_negotiate_content_type(const char *accept, const char **available,
                                       size_t count);
```
Выбирает наиболее подходящий медиатип из заголовка Accept. Возвращает `nullptr`,
если подходящий тип не найден.
