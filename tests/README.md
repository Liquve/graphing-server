# Unit-тесты сервера

Тесты написаны на QtTest и покрывают модули математики и протокола в соответствии с
«Детальным планом тестирования» (TIMP-project-server).

## Состав

| Каталог        | Тест-кейсы            | Описание                                                              |
|----------------|-----------------------|-----------------------------------------------------------------------|
| `tst_libfn`    | TC-01..04, TC-26..30  | Математическое ядро `libfn/libfn.c`: cosh / ln / гипербола / разрыв.  |
| `tst_protocol` | TC-12, TC-23, TC-35, TC-37 | Парсер/сериализатор `GraphingProtocol.h`: устойчивость к мусору, SQL-инъекциям, спецсимволам. |

## Сборка и запуск

С Qt 6.x и доступным `qmake`:

```bash
cd tests
qmake -recursive
make
./tst_libfn/tst_libfn
./tst_protocol/tst_protocol
```

В Docker-окружении (Ubuntu 22.04 / Qt 6.x):

```bash
docker exec -it <build_container> bash -lc \
    'cd /workspace/tests && qmake -recursive && make && \
     ./tst_libfn/tst_libfn && ./tst_protocol/tst_protocol'
```

Тесты используют `QTEST_APPLESS_MAIN` — `QCoreApplication` не создается,
сетевые подключения и БД не требуются.
