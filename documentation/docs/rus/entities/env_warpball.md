﻿# env_warpball

Энтити для визуализации эффекта телепортации Xen. Представляет собой спрайт и два десятка лучей, и имитирует эффект телепорта, из которого появляются враждебные NPC.

## Настройки

- **`Name`** - Имя энтити
- **`Target`** - Имя энтити активируемая при включении
- **`Parent`** - Имя энтити, к которой будет прикреплен
- **`Warp target`** - Имя объекта, где эффект должен появиться; по умолчанию — сама энтити
- **`Damage delay`** - Задержка перед нанесением повреждений

## Спаунфлаги

- **`Remove on fire`** - Удалить после активации
- **`Kill Center`** - Телепортируемый объект будет уничтожен

**Примечание:** Урон от телепорта, представляет собой невидимую сферу радиусом 300 юнитов, которая наносит 300 HP урона всем сущностям, кроме телепортируемой.