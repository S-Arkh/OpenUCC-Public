# Архитектура

## Поток данных

```
Core.u -> Package -> nativeOperators -> target signatures/precedences
*.uc   -> Lexer   -> Parser / typed expressions -> Emitter
                    class/member exports      -> Package::serialize -> *.u
```

В процессе компиляции нет DLL loading, UObject, SDK bridge или вызовов UCC.

## Идентификаторы и размерности

- Name index: индекс в names, от нуля.
- Object reference: export `+(index+1)`, import `-(index+1)`, null `0`.
- Property/function/class owners и Children/Next: те же object references.
- В сериализованном байткоде object/name reference — compact index.
- В memory PC такая ссылка занимает четыре байта независимо от host architecture.
- Jump operands — 16-битные target memory PCs; writer проверяет диапазон.
- Размеры target-полей не выводятся из `sizeof(void*)` хоста.

`Package::serialize` сначала записывает имена, импорты и payload, затем export
table. Поэтому её variable-length compact offsets уже окончательны и не требуют
итерационного перерасчёта. Нумерация ссылок остаётся неизменной.

## Модель и линковка

Package хранит target-таблицы и непрозрачные payload. Полная семантическая модель
всех export kinds пока отсутствует. TargetModel разрешает typed imports между
пакетами и проверяет граф наследования. Совпадение object path не достаточно:
Engine может содержать одноимённые Class и Texture; учитывается класс объекта.
Metadata reader извлекает свойства и полные function signatures, проходя
Children/Next и структурно декодируя сериализованный bytecode до его конца.
Скрипт не исполняется; ScriptText не требуется. Native operators выбираются из
этих метаданных. Поддержанный overload выбирается по точному совпадению типов.
Optional operators исключаются, а не теряют flags. Out-операторы проверяют
writable lvalue; последний skip bool-аргумент `&&`/`||` сериализуется как EX_Skip.
Размер skip — memory size аргумента плюс EndFunctionParms, не размер файла.

Frontend двухпроходный на класс: сначала резервируются declarations/exports,
затем разрешаются выражения и вызовы в телах. Это позволяет forward calls и
рекурсию. Вызовы обычных функций используют virtual-call name reference;
static — final-call export reference. Optional omission сохраняет позицию
EX_Nothing; out требует lvalue и записывает CPF_OutParm.
Несколько независимых классов компонуются в один пакет. Внешнее наследование
использует TargetModel и TargetImports: символы dependency получают типизированные
imports с сохранением owner chain. Int/bool-сигнатуры overrides проверяются;
export Super и serialized UField.Super указывают на родительскую функцию.
Virtual dispatch остаётся виртуальным, super и static используют final call,
индексированные native-функции — native opcode. Наследуемые scalar defaults
записываются по исходным именам свойств без дублирования property exports.
Неверные overrides отвергаются. Frontend также поддерживает class flags,
within/config, составные типы, states, delegates и replication metadata.
