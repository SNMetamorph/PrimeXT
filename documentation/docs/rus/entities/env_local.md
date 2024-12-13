﻿# env_local

Один из объектов логической системы. Может включаться и выключаться с задержкой. Его состояние может быть считано другими энтитями. Также может активировать и деактивировать объекты с задержкой.

## Настройки

- **`Name`** - Имя энтити
- **`Target`** - Имя объекта, который активируется
- **`Delay Before TURN ON`** - Задержка перед включением (в секундах)
- **`Delay Before TURN OFF`** - Задержка перед выключением (в секундах)
- **`Value to transmit`** - Предустановленное значение, которое можно передать другому объекту при его активации или деактивации

## Спаунфлаги

- **`Start On`** - Изначально включён
- **`Remove on fire`** - После активации,  будет уничтожен

## Примечания

- Объект активирует или деактивирует свою цель с указанной задержкой.
- Может использоваться в качестве мастера для других объектов.
- Может быть источником состояния для [multi_watcher](./multi_watcher).
- Данный объект полностью контролируется в любом из своих состояний и может быть включен и выключен, находясь в любом своём состоянии. Для форсированного перевода в состояние ON (при состоянии Turn On) используйте префикс **`<`** и значение **`1`**. Для форсированного перевода в состояние OFF (при состоянии Turn OFF) используйте префикс **`<`** и значение **`0`**. Все остальные комбинации управляются при помощи обычных префиксов **`+`** или **`-`** либо вообще без таковых. Тогда энтити будет просто переключать состояния при каждой активации.