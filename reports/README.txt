reports/
Промежуточная документация ветки ATS-20Plus_next. Не путать с README репозитория.

Спецификации (опора)
- ATS-20_firmware_analysis.txt — аудит v1.18, latency, I2C, encoder, фазы работ
- ATS-20_next_generation_architecture_report.txt — Radio Core / UI Core, deferred tune, dirty OLED
- ATS-20_UI_Sweet_Spot.txt — UI: NORMAL / FOCUS / TRANSIENT, frequency hero, design rules v0.1
- ATS-20_UI_state_machine.txt — слои поверх MAIN, таблица состояний, encoder map (цель после базовых фич)
- ATS-20_I2C_burst.txt — 400 кГц только коротким burst; Fast when touching, quiet when listening
- ATS-20_SSB_I2C_burst.txt — SSB: UI rate ≠ RF rate, coalescing с deadline ~10 ms, не debounce 70 ms
- ATS-20_S-meter_dirty.txt — S-meter: регион page 5, три частоты, не full redraw; quiet window vs RF
- ATS-20_I2C_nack_stop.txt — изолированный NACK→STOP; четыре замера; glitch = откат
- ATS-20_reverse_port_ats-mini.txt — полировка: v1.18 эталон, diqezit основа, Mini только идеи → 328P

Журнал ветки
- ATS-20Plus_next_log.txt — что уже в прошивке, что дальше, flash/SRAM, известные компромиссы
- ATS-20_UI_pixel_grid.txt — фактическая раскладка 128×64 в текущем коде

Правило: новые решения по архитектуре и UI кратко дописывать в log, полные отчёты не переписывать.
