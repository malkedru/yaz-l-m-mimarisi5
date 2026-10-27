# v5 - Hata yonetimi, dogrulama ve dead-letter

- StageException / PipelineException ayrimi; ConfigError fatal hata alt turu.
- Asama bazinda FAIL_FAST, SKIP, DEAD_LETTER, RETRY(n) politikalari.
- Bagimsiz FailedRecord/JSONL hata hedefi; gecersiz UTF-8 baytlarini kayipsiz saklama.
- ValidationStage: zorunlu alan, sayisal aralik, zaman tutarliligi, IPv4/IPv6.
- RunSummary: giren/cikan/atilan/DL/tekrar/aborted sayilari ve monotonik sure.
- Bozuk ve bos fixture; 4 yeni ornek yapilandirma, v5 mimari diyagrami ve gereksinim tablosu.
- 9 test programi, 367/367 dogrulama; Windows anormal test cikis kodu kontrolu.

# Degisiklik Gunlugu

## v3 — Bilesim: Filtreleme, Projeksiyon ve Pipeline Builder

### Yeni dosyalar
- `PipelineBuilder.h` — akici, tip guvenli builder (`from().via()...to()`)
- `PredicateFilterStage.h`, `Predicates.h` — kosula gore eleme; hazir kosullar
- `ProjectionStage.h` — alanlari alt kumeye indirger (ham satiri ve `attributes`'u atar)
- `FileSink.h` — dosyaya yazar; `open()`/`close()` gercekten kullanilir
- `LogFields.h` — alan adlari ve metin gosterimi (projeksiyon, CLI ve bicimleyici ortak)
- `CommandLine.h` — `--min-status`, `--path-prefix`, `--fields`, `--output`, `--help`
- `Makefile`
- `experiments/OrderExperiment.cpp` — siralama deneyi
- `tests/`: `TestHarness.h`, `TestSupport.h`, `PredicateFilterStageTests.cpp`,
  `ProjectionStageTests.cpp`, `FileSinkTests.cpp`, `PipelineBuilderTests.cpp`,
  `CommandLineTests.cpp`, `IntegrationTest.cpp`, `fixtures/mini.log`

### Degistirilen dosyalar
- `Pipeline.h` — `Pipeline<I,O>` (tek asamali sablon) yerine tipten bagimsiz `Pipeline`;
  yasam dongusu yonetimi (sink once acilir, en son kapanir; hata olsa da `close()` cagrilir)
- `Sink.h` — varsayilan `open()`/`close()` eklendi (`Stage` ile ayni sozlesme)
- `ConsoleSink.h` — istege bagli `RecordFormatter`; zaman damgasi bicimi `LogFields`'a tasindi
- `main.cpp` — komut satiri isleme ve builder ile hat kurulumu; ozet stderr'e
- `README.md`, `ARCHITECTURE.md` — guncellendi; siralama deneyi sonuclari eklendi

### Degismeyen dosyalar
`Emitter.h`, `Source.h`, `Stage.h`, `FileLineSource.h`, `LogRecord.h`, `ParserStage.h`,
`tests/ParserStageTests.cpp`, `tests/CollectingEmitter.h` — uc yeni asama mevcut
arayuzlere dokunmadan eklendi.

### Uyumsuz degisiklik
- `Pipeline<I,O>(&source, &stage, &sink)` kaldirildi; yerine `PipelineBuilder` kullanilir.

### Bilinen sinirlar (sonraki haftalar)
- Hatali satirlarin dogru ele alinmasi 5. haftada.
- Asama listesi bu hafta komut satirindan kurulur; 4. haftada yapilandirmaya tasinacak.
- Zaman araligi kosulu (`Predicates::timestampBetween`) hazir ve testli, ancak henuz
  komut satiri secenegi yok.
- `--fields` ile ekrana/dosyaya yazilan cikti baslik satiri icermez (TAB ile ayrilmis).

## v4 — Yapilandirma odakli birlestirme
- Turlu JSON ayristirici; StageFactory / StageRegistry; PipelineLoader.
- Veriye dayali tek fabrika kayit tablosu; builder uzerinden bilesen sahipligi.
- Akistan once parametre ve topoloji dogrulama; acik ConfigError / StageException ayrimi.
- Uc yapilandirma ve loader testleri; onceki CLI korunur.
- VS Code MSVC build/debug/test gorevleri, PowerShell derleme betigi, VERSION v4.
