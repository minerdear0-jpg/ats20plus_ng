reports/
Промежуточная документация ветки ATS-20Plus_next. Не путать с README репозитория.

Спецификации (опора)
- ATS-20_firmware_analysis.txt — аудит v1.18, latency, I2C, encoder, фазы работ
- ATS-20_next_generation_architecture_report.txt — Radio Core / UI Core, deferred tune, dirty OLED
- ATS-20_UI_Sweet_Spot.txt — UI: NORMAL / FOCUS / TRANSIENT, frequency hero, design rules v0.1
- ATS-20_UI_state_machine.txt — слои поверх MAIN, таблица состояний, encoder map (цель после базовых фич)
- ATS-20_I2C_burst.txt — текущая схема 100 / 400-пакет / 100 (не potential; история 965c5d3 отдельно)
- ATS-20_SSB_I2C_burst.txt — SSB deadline 10 ms, не debounce
- ATS-20_S-meter_dirty.txt — page 5; frozen unless hardware RF/perception problem
- ATS-20_I2C_nack_stop.txt — P0: bounded wait; NACK/STOP — способ; пять замеров
- ATS-20_reverse_port_ats-mini.txt — baseline 4fe59be; P0–P3; один эксперимент за раз
- ATS-20Plus_next_gaps.txt — срез архива: bounded I2C, CLKPR, dead state, battery 0%

Журнал ветки
- ATS-20Plus_next_log.txt — что уже в прошивке, что дальше, flash/SRAM, известные компромиссы
- ATS-20Plus_next_2026-08-24.txt — срез дня: idle UI, S-meter S9+, зазоры 7-seg, что на железо
- ATS-20Plus_next_2026-08-25.txt — срез дня: 7c wall + aux SNR/FREQOFF, one-line cave (MODE long), TUNED, flash hunt
- ATS-20_UI_pixel_grid.txt — фактическая раскладка 128×64 в текущем коде

Правило: новые решения кратко в log. Один эксперимент = один commit = один avr-size = один hardware test.
UI overlays / idle: .cursor/rules/ats-ex-ui-overlays.mdc (alwaysApply).
