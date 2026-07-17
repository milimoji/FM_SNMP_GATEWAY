# HealthMonitor Spec v1.1 (Final)

## Status

1. Ovaj dokument je finalni format za implementaciju.
2. Cilj je stabilan ABI, minimalan RAM overhead i pouzdan oporavak posle greske.

## Endian i opsta pravila

1. Endian: little-endian (`endian = 1`).
2. Svi zapisi su binarni, bez JSON/CSV u runtime putanji.
3. Svi zapisi imaju CRC32.
4. Nema dinamicke alokacije u Measurement putanji.
5. `timestamp_ms` je `uint64_t`.
6. Ako vreme nije validno (nema RTC/NTP), `timestamp_ms = 0`, a validnost je u `timeStatus` bitu.

---

## 1) Folder i fajl organizacija

1. Putanja: `health/YYYY/MM/DD.bin`
2. Primer: `health/2026/07/17.bin`
3. Jedan fajl sadrzi:
   1. `FileHeaderV11` (tacno 96 B)
   2. N zapisa (`DeltaRecordV11` ili `CheckpointRecordV11`)

---

## 2) FileHeaderV11 layout (96 B, CRC32 u headeru)

| Offset | Size | Type | Field | Opis |
|---:|---:|---|---|---|
| 0 | 4 | char[4] | magic | "HMON" |
| 4 | 2 | uint16 | specVersion | `0x0101` |
| 6 | 1 | uint8 | endian | `1=little` |
| 7 | 1 | uint8 | headerSize | `96` |
| 8 | 2 | uint16 | deltaRecordSize | `72` |
| 10 | 2 | uint16 | checkpointRecordSize | `100` |
| 12 | 4 | uint32 | headerCrc32 | CRC32 headera, ovo polje tretirati kao 0 tokom racunanja |
| 16 | 8 | uint64 | deviceSerial | Jedinstven ID uredjaja |
| 24 | 4 | uint32 | firmwareVersion | FW build/version |
| 28 | 4 | uint32 | configVersion | Verzija konfiguracije |
| 32 | 4 | uint32 | bootCounter | Persistent boot broj |
| 36 | 8 | uint64 | bootSessionId | Jedinstven ID trenutnog boota |
| 44 | 8 | uint64 | fileStartTimestampMs | 0 ako vreme nije validno |
| 52 | 4 | uint32 | fileStartUptimeSec | Uptime pri otvaranju fajla |
| 56 | 2 | uint16 | samplePeriodSec | Podrazumevano 60 |
| 58 | 2 | uint16 | checkpointEverySamples | Podrazumevano 60 |
| 60 | 4 | uint32 | headerFlags | Rezervisano za feature bitove |
| 64 | 32 | uint8[32] | reserved | Mora biti 0 u v1.1 |

### CRC32 pravilo (header)

1. Polinom: IEEE CRC32 (`0x04C11DB7`, reflektovano `0xEDB88320`).
2. Ulaz: bajtovi `[0..95]`, sa `headerCrc32 = 0`.
3. Rezultat se upisuje u offset `12`.

---

## 3) Record envelope (zajednicki prefix, u oba record tipa)

| Offset | Size | Type | Field | Opis |
|---:|---:|---|---|---|
| 0 | 2 | uint16 | recMagic | `0xA55A` |
| 2 | 1 | uint8 | recType | `1=DELTA`, `2=CHECKPOINT` |
| 3 | 1 | uint8 | recSize | `72` ili `100` |
| 4 | 4 | uint32 | sampleSeq | Monotono u okviru boot session |
| 8 | 4 | uint32 | uptimeSec | Sekunde od boota |
| 12 | 1 | uint8 | timeStatus | Bit0=`timeValid`, Bit1=`timeJump`, Bit2=`noRtc` |
| 13 | 1 | uint8 | qualityFlags | Bit0=`gapAfterDrop`, Bit1=`partialSample` |
| 14 | 2 | uint16 | payloadVersion | `0x0101` |

---

## 4) DeltaRecordV11 layout (72 B ukupno)

| Offset | Size | Type | Field |
|---:|---:|---|---|
| 16 | 4 | uint32 | freeHeap |
| 20 | 4 | uint32 | minHeap |
| 24 | 4 | uint32 | largestBlock |
| 28 | 2 | uint16 | stackMeasure |
| 30 | 2 | uint16 | stackComm |
| 32 | 2 | uint16 | stackSnmp |
| 34 | 2 | int16 | cpuTemp_x10 |
| 36 | 2 | int16 | boardTemp_x10 |
| 38 | 1 | uint8 | fanDutyPct |
| 39 | 1 | uint8 | alarmSeverity |
| 40 | 2 | uint16 | fanRpm |
| 42 | 2 | uint16 | alarmMask |
| 44 | 2 | uint16 | statusFlags |
| 46 | 2 | uint16 | measureCycleAvgMs |
| 48 | 2 | uint16 | measureCycleMaxMs |
| 50 | 2 | uint16 | commCycleAvgMs |
| 52 | 2 | uint16 | commCycleMaxMs |
| 54 | 2 | uint16 | dEthReconnects |
| 56 | 2 | uint16 | dWifiReconnects |
| 58 | 2 | uint16 | dI2cErrors |
| 60 | 2 | uint16 | dSnmpGetReq |
| 62 | 2 | uint16 | dSnmpSetReq |
| 64 | 2 | uint16 | dHttpReq |
| 66 | 2 | uint16 | dTrapCount |
| 68 | 4 | uint32 | recCrc32 |

### CRC32 pravilo (record)

1. CRC se racuna nad bajtovima `[0..recSize-1]`, sa `recCrc32=0`.
2. Za Delta zapis, CRC polje je na offsetu `68`.

---

## 5) CheckpointRecordV11 layout (100 B ukupno)

| Offset | Size | Type | Field |
|---:|---:|---|---|
| 16 | 4 | uint32 | freeHeap |
| 20 | 4 | uint32 | minHeap |
| 24 | 4 | uint32 | largestBlock |
| 28 | 4 | uint32 | internalFreeHeap |
| 32 | 4 | uint32 | psramFreeHeap |
| 36 | 4 | uint32 | dmaFreeHeap |
| 40 | 2 | uint16 | stackMeasure |
| 42 | 2 | uint16 | stackComm |
| 44 | 2 | uint16 | stackSnmp |
| 46 | 2 | int16 | cpuTemp_x10 |
| 48 | 2 | int16 | boardTemp_x10 |
| 50 | 1 | uint8 | fanDutyPct |
| 51 | 1 | uint8 | alarmSeverity |
| 52 | 2 | uint16 | fanRpm |
| 54 | 2 | uint16 | alarmMask |
| 56 | 2 | uint16 | statusFlags |
| 58 | 2 | uint16 | measureCycleAvgMs |
| 60 | 2 | uint16 | measureCycleMaxMs |
| 62 | 2 | uint16 | commCycleAvgMs |
| 64 | 2 | uint16 | commCycleMaxMs |
| 66 | 2 | uint16 | reserved0 |
| 68 | 4 | uint32 | cEthReconnects |
| 72 | 4 | uint32 | cWifiReconnects |
| 76 | 4 | uint32 | cI2cErrors |
| 80 | 4 | uint32 | cSnmpGetReq |
| 84 | 4 | uint32 | cSnmpSetReq |
| 88 | 4 | uint32 | cHttpReq |
| 92 | 4 | uint32 | cTrapCount |
| 96 | 4 | uint32 | recCrc32 |

---

## 6) Delta + checkpoint pravila

1. Sampling period je 60 s.
2. Svaki minut se emituje `DELTA` zapis.
3. Svakih 60 uzoraka (podrazumevano 1 h) emituje se i `CHECKPOINT`.
4. Prvi zapis posle boota mora biti `CHECKPOINT`.
5. Ako dodje do queue overflow-a ili storage greske, prvi sledeci uspesan zapis mora biti `CHECKPOINT` sa `qualityFlags.gapAfterDrop=1`.
6. Delta polja su razlika u odnosu na prethodni uspesno upisan sample iste boot session.
7. Delta saturacija: ako delta ne staje u `uint16`, upisati `0xFFFF` i postaviti `statusFlags` bit `deltaOverflow`.
8. Cumulative brojaci se vode u RAM kao `uint32`, u checkpoint idu kao apsolutna vrednost.
9. `timestamp_ms` nije u recordu; rekonstrukcija vremena ide iz `fileStartTimestampMs + (uptimeSec - fileStartUptimeSec)*1000` samo kada je `timeValid=1`.

---

## 7) Queue contract (Measurement -> Storage)

1. Interfejs je striktno producer/consumer.
2. Producer: HealthCollector task.
3. Consumer: HealthStorage task.
4. Queue element je kompletan binarni record (`72` ili `100` B) + metadata (`enqueueTicks`, `priorityHint`).
5. Queue depth: 32 elemenata (minimalno), preporuceno 64.
6. Enqueue je non-blocking (`timeout=0`).
7. Ako je queue puna:
   1. Increment `droppedSamples`.
   2. Set interni `needCheckpoint=1`.
   3. Ne blokirati Measurement task.
8. Storage task prioritet mora biti nizi od Measurement, visi od UI/web taska.
9. Storage task radi batch write (npr. do 8 zapisa po flush ciklusu).

---

## 8) Storage task contract i failover ponasanje

1. Storage task je jedini koji pristupa LittleFS fajlovima za health.
2. Upis je append-only.
3. `fsync/flush` politika:
   1. Na svakih 8 zapisa ili 30 s.
   2. Odmah na checkpoint zapis.
4. Mount fail:
   1. Preci u RAM-only mode.
   2. Postaviti `storageDegraded=1`.
   3. Generisati EventLog dogadjaj `HEALTH_STORAGE_DEGRADED`.
5. Write fail:
   1. Retry do 3 puta.
   2. Ako i dalje fail, preci u RAM-only mode.
   3. Obeleziti `needCheckpoint=1` za prvi oporavljeni upis.
6. Recovery:
   1. Periodicno pokusavati remount (npr. na 60 s).
   2. Nakon uspesnog remount-a otvoriti novi dnevni fajl.
   3. Prvi upis mora biti `CHECKPOINT` sa `qualityFlags.gapAfterDrop=1`.
7. Corrupt tail recovery na boot:
   1. Validirati header CRC32.
   2. Iterirati recorde do prvog invalidnog CRC ili nekompletnog kraja.
   3. Truncirati fajl na poslednji validni record boundary.
8. Ako je header nevalidan:
   1. Fajl se ne koristi za append.
   2. Renamovati u `.bad`.
   3. Kreirati novi fajl sa novim headerom.

---

## 9) Alarm severity kodovi (v1.1)

1. `0=OK`
2. `1=INFO`
3. `2=WARNING`
4. `3=MINOR`
5. `4=MAJOR`
6. `5=CRITICAL`

---

## 10) ABI stabilnost

1. `specVersion` i `payloadVersion` moraju ostati `0x0101` za ovaj dokument.
2. Svaka promena offseta/velicine polja zahteva novu verziju.
3. Reserved polja moraju biti 0 u v1.1 i cuvaju forward kompatibilnost.

---

## 11) Minimalni acceptance test za v1.1

1. 72h rada bez blokiranja Measurement taska.
2. Namerno izazvan queue overflow ne rusi sistem i pravilno oznacava gap.
3. Simuliran power-loss tokom upisa ne ostecuje prethodne validne zapise.
4. Posle remount-a prvi zapis je checkpoint.
5. Parser uspesno cita mesoviti niz DELTA + CHECKPOINT zapisa.
