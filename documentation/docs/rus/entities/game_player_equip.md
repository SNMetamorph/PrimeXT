﻿# game_player_equip

Энтити позволяет определить начальное вооружение и набор вещей, которые будут выданы игроку, как только он присоединился к игре.
Если Вы поставите этот объект на карту, то игроки будут иметь только то оружие и те вещи, которые указанные в нем.
Если активировать данный объект только для одной команды (используя [game_team_master](./game_team_master)), то игроки другой команды будут экипированы только броней.

## Настройки

- **`Name`** - Имя энтити
- **`Team Master`** - Имя объекта multisource или game_team_master

## Спаунфлаги

- **`Use only`** - Если отмечено, то оружие и прочие вещи будут выдаваться, только при нажатии на "Use"
