# openUCC

Независимый компилятор UnrealScript и структурные кодеки пакетов Unreal Engine 2 /
Lineage II. На C++17, под Windows и Linux; лицензия MIT.

## Возможности

- **Компиляция `.uc` → `.u`:** классы, функции, состояния, delegates, структуры,
  массивы, `INT64`, defaults, зависимости и поддержанные native-контракты.
  Отдельная команда `compile-objects` собирает поддержанные объектные пакеты
  из `.uobj` с опорой на исходный пакет.
- **Структурная пересборка существующих ресурсов:** поддержанные экспорты
  текстур `.utx`, статических моделей `.usx`, моделей/анимаций `.ukx`,
  звуков `.uax` и встроенных в пакеты ресурсов. Проверяются профили, тела
  объектов, ссылки и переносимые lazy-offsets.
- **Диагностика пакетов:** инвентаризация, модель зависимостей и контейнерный
  roundtrip.

Компилятор не использует `ucc.exe`, Editor.dll, SDK или загрузку игровых DLL.
Целевой `Core.u` нужен как источник типов, операторов и версии пакета;
для конкретной сборки также могут потребоваться исходники, зависимости и
исходный пакет-эталон. Они предоставляются пользователем.

**Важно:** пересборка ресурсов работает с существующим пакетом, а не создаёт
универсальные игровые ресурсы из PNG/FBX/WAV. Поддержка отдельных Model в `.unr`
не означает полной пересборки карт. Компиляция и структурная пересборка сами по
себе не гарантируют корректную работу результата в игре.

## Сборка

Windows x64, Visual Studio с поддержкой C++17, CMake 3.24+:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Код использует только стандартную библиотеку C++17. Поддерживаются сборки
Windows/MSVC и Linux/GCC.

Стиль C++: `clang-format`, пресет Google из `.clang-format`.
Форматирование: `clang-format --style=file -i src/compiler.cpp`;
проверка: `clang-format --style=file --dry-run --Werror src/compiler.cpp`.

## Использование

### Публичный portable Windows x64

При наличии релиза скачайте из **Releases** архив `OpenUCC-<version>-windows-x64.zip`
и распакуйте его в каталог с правом записи. Положите свои `.uc` в `Classes`, а
целевой `Core.u` и необходимые `.u`-зависимости — в `Dependencies`. Затем
укажите имя пакета:

```powershell
.\Compile.cmd -Package MyPackage
```

Результат находится в `Output`, журнал — в `Logs`. Python, SDK и UCC при запуске не нужны.
**Игровые пакеты и чужие исходники не включены в архив** — необходимые входные
файлы пользователь предоставляет самостоятельно. Для разных клиентов используйте
отдельные копии комплекта, не смешивая их зависимости. Остальные параметры описаны
в `README_RU.txt` внутри архива и в [инструкции](docs/PORTABLE.md#public-portable-release).
Ручной выпуск релиза описан [здесь](docs/PORTABLE.md#manual-github-release).

### Основные команды

Примеры используют предоставленные пользователем входы; замените пути в угловых
скобках. Каталог вывода должен существовать, а имена выходных файлов — быть
свободными.

```powershell
.\build\Release\openucc.exe inspect "<client>/Core.u"
.\build\Release\openucc.exe compile "<client>/Core.u" BootstrapPkg.u tests/fixtures/Bootstrap.uc
.\build\Release\openucc.exe roundtrip "<client>/Interface.u"
.\build\Release\openucc.exe model "<client>/Core.u" "<client>/Engine.u" "<client>/NWindow.u"
.\build\Release\openucc.exe rebuild-resources "<resources>/Example.ukx" "<output>/Example.ukx"
.\build\Release\openucc.exe rebuild-resources "<resources>/Example.usx" "<output>/Example.usx"
```

`compile Core.u output.u first.uc [second.uc ...]` создаёт новый пакет.
Существующий выходной файл не перезаписывается. Диагностика содержит имя файла,
строку и колонку. Код возврата: 0 — успех, 1 — ошибка, 2 — неверная команда.

`roundtrip input.u [new-output.u]` проверяет сохранение таблиц и export payload.
Это контейнерная проверка: она не доказывает корректность внутренних абсолютных
смещений неизвестных ресурсов. Без output запись не производится.

`rebuild-resources input.utx new-output.utx` пересобирает каждый export через
структурный ресурсный кодек с переносом lazy-offsets. Неизвестные типы и хвосты
отклоняются; существующий выходной файл не перезаписывается. Поддерживаемые
профили и форматы перечислены в [docs/RESOURCES.md](docs/RESOURCES.md).
Команда также обрабатывает поддержанные `.usx`, `.ukx` и `.uax`; расширение
не заменяет проверку профиля и каждого объекта. При обычной реконструкции выход —
plain Unreal-пакет. Команда не устанавливает результат в клиент и не запускает
UCC или игровые DLL.

Если при пересборке встречается неподтверждённая структура `Model`
(BSP-геометрии UE2), а остальные экспорты прошли проверку, команда предупреждает
и возвращает `status: unchanged_copy`: это **копия всего пакета без изменений**,
а не пересборка.
[Подробности и ограничения](docs/RESOURCES.md#unverified-model-fallback).

### Пакеты объектов

`compile-objects` — отдельная команда для пересборки поддержанных пакетов,
содержащих экземпляры объектов, например настройки эффектов, а не классы.
Входные `.uobj` описывают имена, вложенность и свойства объектов. Для сборки
нужны соответствующий `Core.u`, исходный пакет-эталон с тем же именем и пакеты,
в которых объявлены классы этих объектов. Формат и поддержка ограничены;
[пример и требования](docs/OBJECT_PACKAGES.md).

Для обычной компиляции своих классов `.uc` используйте `compile`: `.uobj` и
`compile-objects` для этого не нужны.

## Ресурсные кодеки

- Texture, MaskTexture, WetTexture, Palette, Font, группы Package, Cubemap и 19 классов материалов;
  точные биты свойств, Matrix/Plane, G16 и наблюдённые пустые конечные DXT3/RGBA8 mip.
- StaticMesh: наблюдённые профили от 118/0 до 133/40 (точный перечень в документации),
  включая старые коллизии без lazy-offsets, исходные треугольники, условные
  license-поля, ссылки на материалы и профильные auxiliary UV.
- Polys: вершины, векторы, имена и ссылки с переносом таблиц; KMeshProps:
  скалярные поля и наблюдённые четыре пустых массива геометрии.
- Model: структурные кодеки наблюдённых 123/37 и 133/40. Для 133/40 сохраняются
  зоны, листья, BSP-геометрия, render-секции, световые данные, ссылки и переносимые
  lazy-массивы. Для 127/39, 128/39, 130/40 и 132/40 поддержана только
  наблюдённая форма с пустыми дополнительными массивами. Это не доказывает полной
  пересборки карт, мирового размещения или игровой коллизии.
- SkeletalMesh: профили 123/36, 131/40, 132/40 и 133/40, LodMesh 5/6/7; скелет, веса, LOD, attachment,
  исходная геометрия и перенос lazy-offsets.
- VertMesh: наблюдённые профили 123/37 и 133/40, исходные слова вершин/нормалей,
  анимационные последовательности и покадровые границы.
- MeshAnimation 123/36, 131/40 и 133/40 версии 1: треки, последовательности,
  события и точные UTF-16 units текста начиная с версии 131; 32 класса уведомлений.
- Sound 123/23, 128/39, 130/40, 131/40 и 133/40 WAV: свойства, имя формата и
  исходные аудиобайты без перекодирования. Лимит входного контейнера — 1 GiB.

Кроме указанного сохранения неподтверждённого Model, неподдержанный профиль,
непрочитанный хвост или повреждённые данные приводят к явному отказу без записи
выходного файла. Чтение контейнера не означает поддержку всех его native-тел.
Контракты и ограничения: [RESOURCES.md](docs/RESOURCES.md).

## Реализовано

- Чтение наблюдённых контейнерных профилей 118–133, включая исходные
  123/0, 123/37, 126/0, 131/40, 133/40; plain, XOR111 и
  Ver121 с ключом из ASCII-имени файла; правила не-ASCII имён Ver121 не поддержаны.
- Таблицы names/imports/exports, GUID/generations, флаги, проверка границ и циклов.
- Target-граф зависимостей: типизированные импорты, наследование, поиск членов,
  чтение сигнатур функций из бинарных пакетов без ScriptText. Frontend использует
  граф для внешних базовых классов, скалярных полей и сигнатур функций.
- Запись нового контейнера с сохранением идентификаторов и исходного кодирования имён.
- Внешние базовые классы через `--dependency Other.u`, квалифицированный extends,
  наследуемые scalar-поля/defaults, проверка сигнатур overrides. Исходные классы
  одного пакета связываются в порядке наследования независимо от порядка файлов.
- Поля, параметры, локальные переменные и возвращаемые значения `int`, `bool`,
  `byte`, `float`, `string`, `name`; Unicode literals и primitive casts/coercions.
- Статические scalar-массивы; динамические `array<T>` с элементами-примитивами,
  `Length`, `Insert/Remove`, копирование, параметры/out и возврат массивов.
  Indexed defaults и их наследование, включая внешние dynamic array defaults.
- `final`, `simulated`, `exec`, `singular`, `event`, объявления без тела;
  категории var, const/transient и property modifier bits, списки var/local.
- Class `abstract`, `transient`, `config(Name)`, `perobjectconfig`; наследование
  config/transient/perobjectconfig из исходных и внешних классов.
- Объектные типы из исходников/зависимостей, взаимные forward type references,
  `None`, `self`, up/down-casts, объектные массивы, обращения к полям/методам
  и цепочки contexts, включая поля массивов и результаты вызовов.
- Два прохода по классу: объявления, затем разрешение и компиляция тел.
- `class<T>`, class literals/defaults и casts, `new` с outer/name/flags,
  class `static/default` contexts и собственный `default.Member`.
- Виртуальные/static-вызовы, forward calls, рекурсия, optional/out-параметры.
- Внешние inherited virtual/static/native-вызовы, `super.Function()`, `self.Member`.
- Присваивания, `return`, `if/else`, `while`, `for`, `break`, `continue`.
- Арифметические, сравнительные, побитовые и префиксные операторы с выбором
  перегрузки и числовыми преобразованиями; native-индексы/приоритеты берутся из Core.
- Short-circuit `&&`/`||`, префиксные/постфиксные `++`/`--` и поддержанные
  int-операторы с out-параметром (`+=`, `-=`); проверяются writable operands.
- Скалярные `defaultproperties`, UTF-8/BOM, UTF-16 LE/BE BOM, комментарии.
- Скалярные `const` literals: выражения, размеры массивов и defaults;
  наследование из исходных классов и внешних пакетов, экспорт `Core.Const`.
- Отдельные объявления `enum`, значения и `EnumType.Value/EnumCount`,
  именованная типизация, byte/int casts, параметры/out/returns, enum-массивы
  и defaults. Чтение внешних enum и квалифицированных внешних типов.
- `struct`, вложенные объявления и поля, `struct ... extends ...`,
  доступ к полям, копирование, параметры/out/returns и сравнения.
  Метаданные Struct/StructProperty, импорт внешних структур и наследование от них;
  штатные `Vector`/`Rotator` и native-операторы из целевого Core.
- Составные defaults, вложенные структуры и массивы, static/dynamic struct
  arrays, частичное переопределение source/external defaults. Native defaults
  Vector/Rotator/Color; копирование, Insert/Remove, параметры/out/returns массивов структур.
- Раздельный учёт memory PC и compact-reference file offsets.
- Связывание source/external типов и классов: взаимные квалифицированные
  ссылки на struct/enum, inline `var enum`, отложенные defaults, проверки циклов.
- `within`, типизированный `Outer`, внешний контекст вызовов и проверка outer
  для `new`; private/protected/public и проверки доступа/overrides.
- Повторные `dependsOn` и поля, скрывающие унаследованную функцию;
  вызов скрытой функции через `super`, диагностика неоднозначного обращения.
- Native/noexport/safereplace/editinlinenew/collapsecategories, обратные
  модификаторы, Actor placeable/notplaceable, dependsOn и наследование localized.
- Константы `vect`, `rot`, `class` и типизированные object literals из пакетов,
  включая импорт констант и проверку типа объекта.
- `switch/case/default` для int/byte/enum/name/string/bool/object, fall-through,
  вложенные switch/циклы, break и continue к внешнему циклу.
- INT64 как `Core.Object.INT64`: точные signed 64-bit literals/const, поля,
  параметры/out/returns, static/dynamic arrays и defaults Left/Right. Casts
  0x5a..0x63; native-операторы выбираются из целевого Core. Большие литералы
  эмитируются через точный string→INT64 cast, без профильного токена 0x46.
- `state`, `auto state`, наследование source/external states, `ignores`,
  state-функции, `super`/`global`, метки, `goto` и `stop`; State exports,
  probe/ignore masks и выровненные таблицы меток.
- `delegate`: объявления, скрытые DelegateProperty, привязка/очистка/вызов,
  проверка сигнатуры и наследование из исходных и внешних классов.
- Native-объявления с индексами, float suffix `f/F`, неявное завершение
  функций с результатом, перекрытие унаследованных свойств и enum-значений,
   константы через объектный контекст.

- Replication reliable/unreliable: условия, флаги, offsets и наследование RPC;
  сетевое исполнение пока не проверено.
- Enum object literals, contextual enum values, `dynamicrecompile`, INT64 switch,
  covariance объектных массивов, optional delegate bindings, discarded call results.
- Клиентский `Array.Sort` для 133/40: оба направления, дубликаты,
  отрицательные значения и пустые массивы.
- Смешанные Unicode literals разбиваются на ANSI/Unicode фрагменты с `$` из
  целевого Core, обходя подтверждённую ошибку загрузчика UE2.
- `--trace-progress` выводит этапы и время компиляции; индексы символов и имён
  ускоряют полные сборки.

Поля и функции разрешаются после сбора объявлений; одиночные statements
допустимы без `{}`. Идентификаторы пока ASCII. Поля `native`/`transient pointer`
записываются как `PtrProperty`; произвольные преобразования указателей и
арифметика над ними не поддерживаются.

Статические массивы `bool Bits[2]` отклоняются. На синтетическом примере для
динамического `array<bool>` компилируются обращения и индексные defaults,
проверены записанные значения. Если родительский класс во внешнем пакете уже
задаёт defaults такого массива, их переопределение пока отклоняется. Работа
bool-элементов при выполнении зависит от целевой реализации и этой проверкой не
подтверждена. Изменение `Length` через out-операторы также зависит от целевой
реализации. Возврат производной структуры как базовой и переключение state у
`new Object` не подтверждены при выполнении.
Неподдержанный синтаксис или тип завершается ошибкой, не заглушкой.

Пример двух независимых сборок с наследованием:

```powershell
.\build\Release\openucc.exe compile "<client>/Core.u" ParentPkg.u tests/fixtures/ParentProbe.uc
.\build\Release\openucc.exe compile "<client>/Core.u" ChildPkg.u --dependency ParentPkg.u tests/fixtures/ChildProbe.uc
.\build\Release\openucc.exe compile "<client>/Core.u" Interface.u --dependency "<client>/NWindow.u" --source-dir "<sources>/Interface/Classes"
```

`--source-dir` рекурсивно читает `.uc`, не требуя перечислять тысячи путей
в командной строке Windows. Для реального пакета передаются все его зависимости.

## Проверки

Основные C++-тесты проверяют компилятор, чтение и запись пакетов, ресурсные
кодеки и обработку ошибочных входных данных. Python-тесты проверяют вспомогательные
инструменты и состав portable-архива. Игровые файлы для этих тестов не нужны.

Сборка и запуск проверок в Linux:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 -B -m unittest discover -s tests -p 'test_*.py'
```

Для Python-тестов требуется Python 3.10+.

## Документация

- [Запуск portable-версии и выпуск релиза](docs/PORTABLE.md)
- [Пересборка ресурсов и ограничения](docs/RESOURCES.md)
- [Пакеты объектов](docs/OBJECT_PACKAGES.md)
- [Архитектура](docs/ARCHITECTURE.md)

## Лицензия

Код распространяется по лицензии [MIT](LICENSE). Игровые пакеты, SDK и чужие
исходники в репозиторий и публичный portable-архив не входят.
