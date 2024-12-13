﻿# player_keycatcher

Данная энтити является логическим расширением функционала, предоставляемого **trigger_impulse**. Позволяет перехватывать кнопки игрока для активации событий на карте.

## Настройки

- **`Name`** - Имя энтити
- **`Master`** - Мастер для блокировки энтити. Заблокированный player_keycatcher не вызывает свои цели
- **`Key To Catch`** - Имя кнопки, которую требуется отслеживать. Один **player_keycatcher** может отслеживать только одну кнопку (любую)
- **`Fire On Pressed`** - Цель, которая будет однократно вызвана, когда игрок нажмёт отслеживаемую кнопку
- **`Fire On Hold`** - Цель, которая будет вызываться каждый кадр, пока игрок держит отслеживаемую кнопку
- **`Fire On Release`** - Цель, которая будет однократно вызвана, когда игрок отпустит отслеживаемую кнопку

## Примечания

- Максимальное количество энтити данного типа на карте — 64 штуки.
- Вы можете создавать несколько **player_keycatcher**, которые отслеживают одну и ту же кнопку. Вы можете отслеживать их состояние (кнопка нажата — STATE_ON, кнопка отпущена — STATE_OFF).
- Таблица соответствия виртуальных кнопок приведена ниже:  
  - attack - Кнопка первичной атаки
  - jump - Прыжок
  - duck - Приседание
  - forward - Движение вперёд
  - back - Движение назад
  - use -  Активация
  - left - Поворот влево  
  - right - Поворот вправо
  - moveleft - Движение влево
  - moveright - Движение вправо
  - attack2 - Вторичная атака
  - run - Бег
  - reload - Перезарядка
  - alt1 -  Свободная кнопка; назначается самостоятельно при помощи консольной команды **`bind <key> +alt1`**
  - score - Cвободная кнопка (только для ксаш-мода); назначается самостоятельно при помощи консольной команды **`bind <key> +score`**
- Расположение **player_keycatcher** в пространстве не имеет значения
- Остерегайтесь использовать энтити в мультиплеере, поскольку она будет отслеживать нажатие кнопок всех игроков на сервере.
- энтити не будет работать, если игрок заморожен при помощи **trigger_playerfreeze** или спаунфлага **Freeze Player** у **trigger_camera**, поскольку зажатые кнопки обнуляются непосредственно в движке и перехватить это дело нет никакой возможности.