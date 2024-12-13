﻿# light_environment

Энтити для настройки освещения от солнца и неба на локациях.

## Настройки

- **`Name`** - Имя энтити
- **`Pitch Yaw Roll`** - Угол поворота
- **`Fire with light's state`** - Цель которая будет активирована, когда источник перейдёт в состояние вкл или выкл
- **`Brightness`** - Указывается цвет (в формате RGB - красный, зеленый, синий) и яркость света
- **`Appearance`** - Тип поведения лампочки
- **`Custom Appearance`** - Кастомный тип поведения лампочки, тут можно установить параметры мерцания лампочки. Для этого используются комбинации букв английского алфавита от a до z, где "a" - потухшая лампочка, "z" - горящая. Например, комбинация abcdedcba заставит лампочку сначала включиться, потом потухнуть, затем это повторится
- **`TurnOnStyle`** - Номер лайтстиля, используемый при включении света в течение заданного времени
- **`TurnOnTime`** - Время (целое значение, не дробное) в течении которого будет использоваться лайтстиль
- **`TurnOffStyle`** - Номер лайтстиля, используемый при выключении света в течение заданного времени
- **`TurnOffTime`** - Время (целое значение, не дробное) в течении которого будет использоваться лайтстиль
- **`OnStyle`** - Номер лайтстиля, используемый при включённом свете
- **`OffStyle`** - Номер лайтстиля, используемый при выключенном свете
- **`ZHLT Fade`** - Установить уровень затемнения
- **`ZHLT Falloff`** - Установить формулу расчета затемнения
- **`Pitch`** - Угол в градусах, под которым свет направлен к земле
- **`Shade`** - Оттенок освещения