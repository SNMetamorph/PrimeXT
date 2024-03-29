---
sidebar_position: 2
description: Цель этой статьи заключается в том, чтобы объяснить, что из себя представляет моддинг Half-Life 1 во втором десятилетии 21 века, и с чего вообще начать погружение в эту тему. Эта статья имеет больше теоретический, нежели практический характер. 
---

# Введение в современный моддинг Half-Life 1
Несмотря на то, что с момента выхода в свет этой игры прошло почти 30 лет, сообщество по сей день продолжает
интересоваться игрой и использовать её как инструмент для своего творчества. Карты, модели, звуки, спрайты, текстуры, 
и даже полноценные законченные проекты, которые включают в себя всё перечисленное ранее: неисчислимое количество гигабайтов контента было создано сообществом, 
и эта цифра с каждом годом растёт. 
  
С момента релиза игры и по сей день, моддинг претерпел множество изменений - появились новые, эффективные и удобные 
инструменты, а старые отчасти ушли на свалку истории. Несведующему в теме человеку будет довольно трудно разобраться сходу самому.
Цель этой статьи заключается в том, чтобы объяснить, что из себя представляет моддинг Half-Life 1 во втором десятилетии 21 века, и с чего вообще 
начать погружение в эту тему. Я не буду подробно вдаваться в детали инструментов и процессов (такое лучше описывать 
в отдельных статьях, что когда-нибудь будет сделано), а эта статья имеет больше теоретический, нежели практический характер. 
Она должна лишь дать поверхностное понимание предмета и направление для дальнейшего изучения, если у читателя возникнет 
такая необходимость.

## Ключевое событие в истории
Пожалуй, самым главным событием в моддинге Half-Life 1 стал релиз движка Xash3D. Он является практически полностью
совместимым с оригинальным движком Half-Life 1, названным GoldSrc. Но Xash3D имеет ряд очень важных отличий: у него полностью 
открытый исходный код, многие внутренние лимиты были расширены либо убраны целиком, добавлено множество нового функционала
для моддинга, а также он поддерживает множество различных платформ, например Nintendo Switch, PS Vita, Android, и многие другие. 
Его даже запускали на смарт-часах, и в браузере (при помощи Emscripten), а также в DOS. 

Этот движок открыл невиданные ранее возможности для игроков и разработчиков модов (вы наверняка уже видели в этом году новость, 
что к HL1 прикрутили настоящий рейтрейсинг). Первоначально, движок был разработан одним хорошо известным в узких кругах человеком 
под ником *g-cont* или *Дядя Миша*. Но в 2019 году он перестал заниматься разработкой движка, сделал многие свои разработки общедоступными 
и переключился на другой проект. 
В данный момент движок поддерживается и разрабатывается командой энтузиастов под названием [FWGS](https://github.com/FWGS), но также в разработке принимают участие 
и другие контрибьюторы, как например *Иван "provod/w23" Авдеев*, который с начала 2021 года занимается разработкой Vulkan-рендерера с поддержкой рейтрейсинга.

Также, в 2016 году командой FWGS был выпущен в открытый доступ порт движка Xash3D под платформу Android, вместе с этим также были сделаны порты оригинального 
Half-Life 1 и Counter Strike 1.6. Это событие вызвало неимоверную волну интереса со стороны игрового сообщества. По итогам этих событий, Xash3D набрал на Google Play свыше миллиона установок и 30+ тысяч отзывов. CS16Client набрал на Google Play свыше миллиона установок и 20+ тысяч отзывов. Немалые цифры для игры почти 30-летней давности, не так ли? 
Помимо HL1 и CS 1.6 также были портированы и многие другие моды: Opposing Force, They Hunger, Afraid of Monsters: Director's Cut, Poke646, и т.д. 

## Базовый моддинг (замена/добавление контента)
Допустим, пришла вам в голову идея в какой-либо игре, базирующейся на движке GoldSrc/Xash3D (как например HL1, CS, HL:OF, HL:BS, TFC) 
заменить звуки, модели, или текстуры. Делается это очень просто: вы просто находите в папке с игрой нужный вам файл и заменяете на желаемый. 
Вы можете как сделать его сами, так и подобрать что-то подходящее на таких сайтах, как [ModDB](https://www.moddb.com/games/half-life/downloads), [GameBanana](https://gamebanana.com/games/34), [Gamer-Lab](https://gamer-lab.com/rus/goldsrc).
И на этом всё. Что касается карт (или локаций/уровней, как их ещё называют), то тут всё сложнее: уже готовую карту 
изменять можно в довольно ограниченных пределах, для этого используется программа `bspguy`. Но на самом деле, обычно и этих пределов достаточно. 

В случае если же вы хотите создавать свои игровые локации полностью с нуля, вам нужно будет разобраться с редактором и компиляцией 
карт. По этой теме позже будут сделаны отдельные, подробные статьи. По работе с моделями тоже нужна отдельная статья, в которой будут освещены 
все аспекты и необходимые для работы инструменты (к слову, всё бесплатное и опенсорсное).

## Продвинутый моддинг
В случае, если вы хотите сделать свой проект на базе HL1, не ограничиваясь при этом только лишь заменой контента, то вам
придётся столкнуться с Half-Life SDK. HLSDK представляет из себя набор исходников утилит, клиентской и серверной библиотеки HL1. Всё это 
написано на языке C++, так что если вы уже знакомы с этим языком, то начать работу вам будет сильно проще. HLSDK попал в общий доступ 
спустя непродолжительное время после релиза HL1, то есть примерно где-то в 1999 году. Люди брали HLSDK за основу, реализовывали свои идеи, и выкладывали 
модифицированные исходники в публичный доступ. В результате, получилось множество разных вариаций HLSDK с разными дополнительными возможностями. Но в этой статье 
я опишу только самые актуальные на данный момент варианты, которые имеет смысл использовать в своих проектах: 

### hlsdk-portable
Представляет из себя обычный HLSDK без дополнительных фич и геймплейных изменений относительно стандартной HL, но портирован под множество 
платформ, и также содержит немалое количество различных багфиксов, которых нет в оригинальном HLSDK. Разрабатывается командой FWGS. 
Неплохой вариант в случае, если вам не нужно выходить за рамки возможностей HL1, и вы, например, хотите просто как-то изменить геймплей или что-то ещё.
Можно использовать для создания мода как под GoldSrc, так и под Xash3D FWGS. Можно использовать как для синглплеерных, так и мультиплеерных модов.

### PrimeXT
Современный вариант HLSDK для движка Xash3D FWGS, адаптированный под множество современных платформ, имеет улучшенную 
графику и интеграцию с физическим движком PhysX, но сохраняя при этом все присущие GoldSrc и Xash3D возможности и подходы к работе. 
Базируется на XashXT, поэтому наследует весь функционал из XashXT и Spirit Of Half-Life. Подходит для создания как синглплеерных, так и мультиплеерных модов. 
Кроме того, содержит в себе огромное множество новых возможностей и инструментов для создания модов, подробнее об этом можно 
прочитать на [сайте проекта](https://snmetamorph.github.io/PrimeXT/docs/rus/intro), так как подобное описание не входит в задачи этой статьи.
Если вы хотите выйти далеко за рамки HL1 технологически и выжать из движка все соки - PrimeXT был создан именно для этого.

## Дополнительные источники
Пока мы работаем над собственными статьями, крайне рекомендуем обратить внимание на уже существующие источники знаний. Вполне вероятно, что среди них вы 
найдёте ответ на ваш вопрос.

- [Туториалы от The303](https://the303.org/tutorials/)
- [Туториалы на GameBanana](https://gamebanana.com/tuts/games/34?_nPage=4)
- [Туториалы на ModDB](https://www.moddb.com/tutorials?filter=t&kw=&subtype=&meta=&game=1)
- [Туториалы на Sourcemodding](https://www.sourcemodding.com/tutorials/goldsrc/)
- [Туториалы по HLSDK от Admer456](https://www.youtube.com/playlist?list=PLZmAT317GNn19tjUoC9dlT8nv4f8GHcjy)
- [Легендарный учебник по маппингу от *Дмитрия Черкашина aka Dmitrich*](https://cs-mapper2.narod.ru/tutorials/index.shtml.htm)
- [HLRA (по большей части устарело)](http://ralertmod.narod.ru/index.htm)
- [TWHL Wiki](https://twhl.info/wiki/index)
- [Форум CSM](https://csm.dev/)
- [Форум HLFX](https://hlfx.ru/forum/)
