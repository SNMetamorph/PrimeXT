﻿# env_static_decal
Точечная энтити для размещения на локации HD декалей из папки `gfx\decals` (альтернатива устаревшим декалям из Half-Life 1).

## Настройки
- **`Decal group name`** - Имя декали из файла настроек в папке `gfx\decals`, указывается из файла `decalinfo.txt`
- **`Direction`** - Направление по одной из осей (по умолчанию автоопределение направления работает хорошо)

## Пример использования
```
grafity1 
{
	grafity2	48	48	1
}
```

Данный скрипт означает, что `grafity1` - это имя группы, ниже `grafity2` - это имя текстуры из папки `gfx\decals`. 
Параметры 48 48 - это размер текстуры, чем выше значение, тем больших размеров будет декаль.
Параметр 1 это прозрачность декали от 0 до 1, можно использовать промежуточные значения, такие как 0.2 или 0.1234
