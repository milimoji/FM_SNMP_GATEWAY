# HealthMonitor Implementation Checklist

## 0. Scope Lock

1. Potvrditi da je referentni dokument [HealthMonitor Spec v1.1 (Final)](src/HealthMonitor%20Spec%20v1.1%20(Final).md).
2. Zakljucati ABI: bez promene offseta i velicina polja u v1.1.
3. Definisati owner-e po modulu: Collector, Storage, Scorer, API/UI.

## 1. Data Structures

1. Kreirati C/C++ strukture za FileHeaderV11, DeltaRecordV11, CheckpointRecordV11.
2. Dodati static_assert za velicine: 96, 72, 100 B.
3. Dodati static_assert za kriticne offsete.
4. Definisati enum-e za recType, alarmSeverity, statusFlags, qualityFlags.

## 2. CRC Layer

1. Implementirati CRC32 util (IEEE, reflected).
2. Dodati test vektore za CRC32.
3. Implementirati helper-e:
   1. computeHeaderCrc32
   2. computeRecordCrc32
4. Verifikovati da CRC koristi zeroed CRC field tokom racunanja.

## 3. Boot Session Metadata

1. Uvesti bootCounter persistentno u NVS.
2. Uvesti bootSessionId (uint64) generisanje po boot-u.
3. Popuniti firmwareVersion, configVersion, deviceSerial za header.
4. Definisati timeStatus i noRtc ponasanje kada nema validnog vremena.

## 4. Collector Module

1. Implementirati HealthCollector tick na 60 s.
2. Meriti:
   1. freeHeap, minHeap, largestBlock
   2. stack high-water po taskovima
   3. temperature x10
   4. fanDuty, fanRpm, alarmMask, alarmSeverity
   5. cycle avg/max za measurement i communication
3. Delta racun:
   1. Odrzavati prethodni snapshot u RAM-u
   2. Delta saturacija na 0xFFFF i status bit deltaOverflow
4. Pravilo: prvi zapis posle boota mora biti checkpoint.
5. Pravilo: na svaki 60. sample emitovati checkpoint.

## 5. Queue Contract

1. Kreirati queue (min 32, preporuceno 64 elemenata).
2. Producer enqueue je non-blocking (timeout 0).
3. Na queue overflow:
   1. increment droppedSamples
   2. set needCheckpoint
   3. set gapAfterDrop na sledecem uspesnom upisu
4. Dodati metrike queue depth, high-water, drop count.

## 6. Storage Module

1. Kreirati direktorijume health/YYYY/MM.
2. Otvarati dnevni fajl DD.bin.
3. Pri kreiranju fajla upisati validan FileHeaderV11.
4. Upis je append-only.
5. Flush politika:
   1. svakih 8 zapisa ili 30 s
   2. odmah za checkpoint
6. Single-writer pravilo: samo Storage task pristupa fajlu.

## 7. Recovery and Failover

1. Mount fail -> RAM-only mode + event HEALTH_STORAGE_DEGRADED.
2. Write fail -> retry 3 puta, zatim RAM-only mode.
3. Periodic remount attempt (npr. 60 s).
4. Posle oporavka, prvi zapis obavezno checkpoint sa gapAfterDrop=1.
5. Boot recovery:
   1. validirati header CRC32
   2. skenirati recorde do prvog invalidnog
   3. truncate na poslednji validni boundary
6. Header invalid -> rename u .bad i kreirati novi fajl.

## 8. Parser and Export

1. Implementirati parser za mesovite DELTA/CHECKPOINT zapise.
2. Validirati recMagic, recSize, recType, payloadVersion, CRC32.
3. Implementirati rekonstrukciju vremena iz fileStartTimestampMs i uptimeSec kada je timeValid=1.
4. CSV export raditi samo na zahtev korisnika.

## 9. Health Score Module

1. Implementirati poseban HealthScorer modul (bez storage zavisnosti).
2. Ulazi:
   1. heap fragmentacija
   2. reconnect trend
   3. i2c/adc greske
   4. stack margina
   5. cycle latency
   6. alarm pritisak
3. Izlazi:
   1. Health (0-100)
   2. Confidence (0-100)
   3. Trend (Improving/Stable/Degrading)
   4. reason mask
4. Dodati smoothing i hysteresis.

## 10. API/UI Integration

1. Endpoint za poslednjih 6 h iz RAM ring-a.
2. Endpoint za istoriju iz storage-a po opsegu.
3. Endpoint za download bin i CSV.
4. UI prikaz:
   1. score + trend + confidence
   2. key health chart (heap, largestBlock, reconnects, i2c)
   3. alarm timeline

## 11. Testing Matrix

1. Unit test:
   1. CRC32
   2. struct size/offset assertions
   3. delta overflow
2. Integration test:
   1. queue overflow scenario
   2. mount/write fail scenario
   3. remount recovery scenario
3. Longevity test:
   1. 72 h continuous run
   2. no measurement deadline miss
4. Power-loss test:
   1. reset usred append-a
   2. parser recovery i truncate validacija

## 12. Definition of Done

1. Svi acceptance kriterijumi iz [HealthMonitor Spec v1.1 (Final)](src/HealthMonitor%20Spec%20v1.1%20(Final).md) prolaze.
2. Build stabilan bez novih upozorenja visokog prioriteta.
3. Nema dinamicke alokacije u measurement putanji.
4. Dokumentovana procedura oporavka i eksporta logova.
