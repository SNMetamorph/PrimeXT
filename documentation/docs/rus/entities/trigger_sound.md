﻿# trigger_sound

Триггер, заменяющий собой [env_sound](./env_sound) и позволяющий задать зону смены DSP-эффекта точно по области триггера.

## Настройки

- **`Name`** - Имя энтити
- **`Target`** - Имя объекта, который активируется
- **`Delay before trigger`** - Время в секундах до активации объекта
- **`KillTarget`** - Имя объекта, который будет уничтожен после активации
- **`Parent`** - Имя энтити, к которой будет прикреплен
- **`Master`** - Имя мастера для активации
- **`Room Type`** - Тип комнаты (Имитация звука)

## Примечания

- Один **trigger_sound**, подобно **env_sound**, может сменить настройки DSP только для одного эффекта.
- Чтобы сбросить DSP-эффект на ноль, вам потребуется ещё один объект.
- Триггер нельзя активировать через targetname, только касанием игрока.
