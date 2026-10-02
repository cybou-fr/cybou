# 71 — Сборка CYBOU core (Windows, Qt MinGW + vcpkg)

Это **единственная авторитетная процедура сборки** CYBOU core на Windows.
Воспроизводит конфигурацию `build_cybou_qt_mingw`, на которой собираются
`cybou-core-test.exe`, `cybou-node.exe`,
`cybou-loadgen.exe` и Qt GUI.

## Что требуется (один раз)

| Компонент | Путь на этой машине | Назначение |
| --- | --- | --- |
| Qt MinGW 13.1.0 | `C:\Qt\Tools\mingw1310_64` | компилятор g++, binutils |
| Qt 6.11.2 (mingw_64) | `C:\Qt\6.11.2\mingw_64` | GUI (CMAKE_PREFIX_PATH) |
| Qt Ninja | `C:\Qt\Tools\Ninja` | генератор сборки |
| CMake ≥ 4 | `C:\Program Files\CMake` | конфигуратор |
| vcpkg | `C:\Users\cybou\vcpkg` | зависимости (pinned manifest) |

В `PATH` для шага установки зависимостей должен быть MinGW:

```bat
set PATH=C:\Qt\Tools\mingw1310_64\bin;%PATH%
```

Без этого vcpkg не сможет собрать порты для triplet `x64-mingw-dynamic`
(хост-триплет тоже MinGW — машина без Visual Studio, детект MSVC падает).

Каталог MinGW `bin` должен оставаться в `PATH` и при каждом прямом запуске
CMake build и тестов. `g++.exe` запускает `cc1plus.exe` как дочерний процесс;
если DLL из MinGW `bin` не находятся в `PATH`, дочерний процесс может завершиться
с кодом Windows `0xC0000135`, а Ninja покажет ошибку compile без диагностики.
CMake Preset `cybou-consumer-qt-mingw` задаёт это окружение, если запускать через
preset. Для прямых команд в PowerShell задайте его явно:

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;$env:PATH"
cmake --build build_cybou_qt_mingw --target cybou-core-test cybou-node --parallel 4
```

Для запуска приложения и тестов добавьте также каталоги runtime DLL Qt и vcpkg:

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\6.11.2\mingw_64\bin;$(Resolve-Path build_cybou_qt_mingw/vcpkg_installed/x64-mingw-dynamic/bin);$env:PATH"
build_cybou_qt_mingw\bin\cybou-core-test.exe --log_level=error
```

## Почему именно так

Корневой `CMakeLists.txt` требует **OpenSSL 3.5** (`find_package(OpenSSL 3.5
REQUIRED COMPONENTS Crypto)`) — ML-DSA-65 появился только в 3.5. Системного
OpenSSL 3.5 на машине нет (Git OpenSSL — 3.x старый, Qt не поставляет его).
Единственный pinned-источник OpenSSL 3.5 — `vcpkg.json` (baseline
`120deac30...`, порт openssl 3.5.x).

Системный `libssl-dev` Ubuntu 24.04 — OpenSSL 3.0.13, поэтому CI на
`ubuntu-24.04` без vcpkg падает на configure. См. `.github/workflows/cybou-core.yml`.

## Шаг 1 — зависимости vcpkg (manifest, pinned)

Из корня репозитория:

```bat
set PATH=C:\Qt\Tools\mingw1310_64\bin;%PATH%
C:\Users\cybou\vcpkg\vcpkg.exe install ^
  --triplet x64-mingw-dynamic ^
  --host-triplet x64-mingw-dynamic ^
  --x-feature=tests ^
  --x-install-root=%CD%\build_cybou_qt_mingw\vcpkg_installed
```

Что ставится (фича `tests`, без qt/wallet/zeromq — GUI берёт Qt из
`C:\Qt\6.11.2`, wallet выключен):

```text
boost-asio, boost-multi-index, boost-signals2, libevent (override 2.1.12#7), openssl 3.5.x, boost-test
```

Первый запуск собирает OpenSSL из исходников (mingw gcc, debug+release) —
это долго (15–30 мин). Повторные запуски мгновенные (vcpkg видит, что всё
установлено). **Прерывать процесс нельзя** — недособранное дерево портов
прибита к консистентному состоянию только повторным запуском той же команды.

Важно: `--x-feature=tests` вместо default-features. Без флага подтянутся
qtbase/qttools/sqlite3/zeromq и vcpkg начнёт собирать Qt (~час) даже при
включённом `BUILD_GUI=ON`, потому что GUI линкует Qt из `CMAKE_PREFIX_PATH`,
а не из vcpkg.

## Шаг 2 — configure (один раз)

```bat
"C:\Program Files\CMake\bin\cmake.exe" -S . -B build_cybou_qt_mingw -G Ninja ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_TOOLCHAIN_FILE=C:/Users/cybou/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic ^
  -DVCPKG_HOST_TRIPLET=x64-mingw-dynamic ^
  -DVCPKG_MANIFEST_FEATURES=tests ^
  -DVCPKG_MANIFEST_INSTALL=OFF ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 ^
  -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe ^
  -DBUILD_GUI=ON ^
  -DBUILD_TESTS=ON ^
  -DWITH_CCACHE=OFF
```

Ключевые решения:

- `VCPKG_MANIFEST_INSTALL=OFF` — зависимости ставятся явной командой из
  шага 1, а не автоматически при configure. Так видно, что и куда ставится,
  и configure не лезет в сеть.
- `VCPKG_HOST_TRIPLET=x64-mingw-dynamic` — на машине без Visual Studio
  хост-триплет `x64-windows` падает с `visualstudio.cpp: Value was null`.
- GUI ON, но Qt — из `C:\Qt\6.11.2\mingw_64`, не из vcpkg.

## Шаг 3 — сборка

```bat
"C:\Program Files\CMake\bin\cmake.exe" --build build_cybou_qt_mingw --target cybou-core-test cybou-node cybou cybou_qt -j 4
```

For a focused Qt GUI rebuild after editing `src/qt`, use:

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;$env:PATH"
cmake --build build_cybou_qt_mingw --target cybou_qt --parallel 4
```

## Шаг 4 — запуск CYBOU-протокольных тестов

```bat
build_cybou_qt_mingw\bin\cybou-core-test.exe --log_level=error
```

## Диагностика типовых падений

| Симптом | Причина | Лечение |
| --- | --- | --- |
| `Could NOT find OpenSSL (at least 3.5)` | не выполнен шаг 1 или поставлены не те фичи | повторить шаг 1 точно с `--x-feature=tests` |
| `visualstudio.cpp(90): Value was null` (vcpkg) | хост-триплет x64-windows без Visual Studio | `--host-triplet x64-mingw-dynamic` + MinGW в PATH |
| `No CMAKE_C_COMPILER could be found` (vcpkg) | MinGW не в PATH при шаге 1 | `set PATH=C:\Qt\Tools\mingw1310_64\bin;%PATH%` |
| Ninja сообщает о падении compile без диагностики | MinGW `cc1plus.exe` не находит DLL из `bin` (`0xC0000135`) | добавить `C:\Qt\Tools\mingw1310_64\bin` в `PATH` и повторить сборку |
| configure пересобирается и теряет OpenSSL | CMake re-run без toolchain | всегда указывать `-DCMAKE_TOOLCHAIN_FILE=...` (кеш хранит его, но регенерация без него его затирает) |
| CI (ubuntu-24.04) красный на configure | `libssl-dev` = OpenSSL 3.0.13 < 3.5 | та же стратегия vcpkg: toolchain + triplet + установка openssl в workflow |

## Правило

Любое изменение процедуры сборки сначала фиксируется в этом документе,
потом применяется в скриптах/CI. Не наоборот.
